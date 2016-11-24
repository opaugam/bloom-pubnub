### Overview

A bit of hacking around the [**Pubnub C SDK**](https://www.pubnub.com/docs/posix-c), wrapped in C++11 exposed as a
Python2.x module. The idea I was toying with revolves around having a little bloom filter object which can be
distributed between processes/containers/etc. In other words any update to the filter will be automatically relayed
via Pubnub to all other instances.

The same idea leveraging Pubnub could be extended to for instance provide distributed metrics caches. Another idea
worth exploring would be to turn a set of Pubnub channels into a distributed hashtable.