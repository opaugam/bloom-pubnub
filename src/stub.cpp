#include <array>
#include <bitset>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <pubnub_helper.h>
#include <pubnub_sync.h>
#include <pubnub_timers.h>
#include <pybind11/pybind11.h>

using namespace std;

//
// - mickey-mouse way to get the 2 hashes used in the (Ha(k) + i*Hb(k)) double-hashing scheme
// - need to dig a bit on hashing theory
//
#define A (0x00000000FFFFFFFF)
#define B (0xFFFFFFFF00000000)

//
// - trivial bloom filter implementation synchronized on pubnub
// - keys that are set on this object within the python runtime will be published to a pubnub channel
// - all instances will then receive them after a variable delay and update their bit array
//
template<size_t M, size_t K> struct bloom
{
    struct context
    {
        context() : pb(pubnub_alloc())
        {
            pubnub_init(pb, "demo", "demo");
            pubnub_set_transaction_timeout(pb, 750);
            pubnub_set_non_blocking_io(pb);
        }
    
        ~context()
        {
            pubnub_free(pb);
        }

        pubnub_t *pb;
    };

    bool shutdown;
    bitset<M> bits;
    list<string> fifo;
    array<thread, 2> inner;
    condition_variable wq;
    mutex mtx;
    mutex m;

    bloom() : shutdown(false), inner({ {thread(&bloom::_in, this), thread(&bloom::_out, this)} })
    {}

    ~bloom()
    {
        //
        // - set the trigger & notify the wait queue
        // - join both threads
        //
        shutdown = true;
        wq.notify_one();
        inner[0].join(), inner[1].join();
    }

    void set(const pybind11::handle &key) throw (runtime_error, pybind11::key_error)
    {
        unique_lock<mutex> lck(mtx);
        auto s = key.cast<string>();
        for(const char *p = s.c_str(); *p != '\0'; ++p) if(!isalnum(*p)) throw new pybind11::key_error("invalid key");
        fifo.push_front(key.cast<string>());
        wq.notify_one();
    }

    bool check(const pybind11::handle &key) throw (runtime_error)
    {
        unique_lock<mutex> lck(m);
        auto H = hash<string>{}(key.cast<string>());
        for(size_t n = 0; n < K; ++n) if(!bits[_hash(n, H&A, H&B)]) return false;
        return true;
    }

    void _set(const string &key)
    {
        unique_lock<mutex> lck(m);
        auto H = hash<string>{}(key);
        for(size_t n = 0; n < K; ++n) bits[_hash(n, H&A, H&B)] = true;
    }

    inline unsigned long _hash(size_t n, size_t H1, size_t H2) const
    {
        return (H1 + n * H2) % M;
    }

    void _in()
    {
        context ctx;
        while(!shutdown)
        {
            //
            // - sub on our channel
            // - backoff until ready
            //
            pubnub_subscribe(ctx.pb, "bloom", 0);
            for(int ms = 1; pubnub_last_result(ctx.pb) == PNR_STARTED && !shutdown;)
            {
                this_thread::sleep_for(chrono::milliseconds(ms));
                if(ms < 8) ms *= 2;
            }

            for(;;)
            {
                const char *msg = pubnub_get(ctx.pb);
                if(!msg) break;

                //
                // - incoming json payload as a string array
                // - tokenize quick on '"'
                // - hash each token
                //
                const char *last = 0;
                for(bool toggle = 0;; toggle ^= 1)
                {
                    for(; *msg != '\0' && *msg != '"'; ++msg);
                    if(*msg == '\0') break;
                    if(!toggle) ++msg, last = msg;
                    else
                    {
                        _set(string(last, msg - last));
                        ++msg;
                    }
                }
            }
        }
    }

    void _out()
    {
        context ctx;
        array<char, 32768> buf;
        while(1)
        {
            //
            // - wait on the fifo
            // - dequeue the next batch of keys
            //
            string payload;
            unique_lock<mutex> lck(mtx);
            while(!shutdown && fifo.empty()) wq.wait(lck);
            if(shutdown && fifo.empty()) return;

            //
            // - use a flat buffer to turn 1+ keys into a serialized json array
            // - pack as many keys as possible
            //
            char *p = buf.data();
            *p++ = '[';

            for(size_t i = 32768 - 2; i && fifo.size();)
            {
                auto next = fifo.back();
                auto n = next.size() + 3;
                if(n > i) break;
                *p++ = '"';
                next.copy(p, next.size());
                p += next.size();
                fifo.pop_back();
                i -= n, *p++ = '"', *p++ = ',';
            }
            --p, *p++ = ']', *p++ = '\0';
            assert((p - buf.data) <= 32768);

            lck.unlock();

            //
            // - publish our json buffer
            // - backoff until ready
            //
            pubnub_publish(ctx.pb, "bloom", buf.data());
            for(int ms = 1; pubnub_last_result(ctx.pb) == PNR_STARTED;)
            {
                this_thread::sleep_for(chrono::milliseconds(ms));
                if(ms < 8) ms *= 2;
            }

            //
            // - right now the payload will be lost if ever the transaction failed
            // - a proper implementation would be to use a state-machine-like strategy
            //
        }
    }
};

PYBIND11_PLUGIN(stub) {

    pybind11::module m("stub", "python/cxx11 stub");

    //
    // - instantiate a filter with 192K bits and 13 hashes
    // - this should give us a false positive probability of ~0.0001 given a set of 10K keys
    //
    typedef bloom<192000, 13> _bloom;

    pybind11::class_<_bloom>(m, "bloom")
        .def(pybind11::init<>())
        .def("set",     &_bloom::set)
        .def("check",   &_bloom::check);

    return m.ptr();
}