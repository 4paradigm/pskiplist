# pskiplist
An implementation of the persistent skiplist based on Intel Optane Persistent Memory. The persistent skiplist data structure is firstly introduced in a [VLDB paper](http://vldb.org/pvldb/vol14/p799-chen.pdf)). It is now being actively developed to support more data storage systems (e.g., PmemKV, RocksDB, Kafka and etc.) as the underlying core data structure.

# Usage
## A new storage engine in PmemKV
Please checkout our forked [PmemKV](https://github.com/4paradigm/pmemkv) to find out [how to](https://github.com/4paradigm/pmemkv/blob/master/doc/ENGINES-experimental.md#pskiplist) use the persistent skiplist storage engine (ENGINE_PSKIPLIST) as the storage engine for PmemKV.

Latest updates will be periodically merged to [Intel's PmemKV](https://github.com/pmem/pmemkv) to keep upstream.

Stay tuned for the integration with more systems/applications.