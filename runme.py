
import stub
import time

if __name__ == '__main__':

    A = stub.bloom()
    B = stub.bloom()

    #
    # - set a bunch of keys in A
    # - each key will be published to pubnub over the 'bloom' channel
    # - all filter instances will be notified
    #
    for x in range(0, 10000):
        A.set('%d' % x)

    #
    # - B for instance should now be able to tell if a key is unknown or not
    #
    time.sleep(1)
    print B.check('boo')
