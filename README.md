# pskiplist
An implementation of the persistent skiplist based on Intel Optane Persistent Memory. The persistent skiplist data structure is firstly introduced in a [VLDB'21 paper](http://vldb.org/pvldb/vol14/p799-chen.pdf) ("Optimizing In-memory Database Engine for AI-powered On-line Decision Augmentation Using Persistent Memory". Cheng Chen, Jun Yang, Mian Lu, Taize Wang, Zhao Zheng, Yuqiang Chen, Wenyuan Dai, Bingsheng He, Weng-Fai Wong, Guoan Wu, Yuping Zhao, Andy Rudoff). 

With the simplicity and good performance, skiplist is widely used in many scenarios where the sorted keys are needed to support efficient scan operations. For example, Redis uses skiplist to implement its 'sorted set' data structure, RocksDB's memtable has a InlineSkiplist implemetation. Internally, our RTIDB, an in-memory database system specifically designed for feature engineering and online feature extraction for AI applications, uses a double-layered skiplist to manage all the in-memory data.

It is now being actively developed to support more data storage systems (e.g., PmemKV, RocksDB, Kafka and etc.) as the underlying core data structure.

# Usage
## PmemStore - PmemKV with the new storage engine PSKIPLIST
Please checkout our [PmemStore](https://github.com/4paradigm/pmemstore) to find out how to use the persistent skiplist storage engine (ENGINE_PSKIPLIST) as the storage engine for a persistent key-value store.

Latest updates will also be periodically merged to [Intel's PmemKV](https://github.com/pmem/pmemkv) to keep upstream.

Stay tuned for the integration with more systems/applications.