shh@a317-server1:~/git_clone/nvpcfigure/figure522$ sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=readseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --use_existing_db=true --cache_size=0 -compressed_cache_size=0
Set seed to 1712044646017136 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
RocksDB:    version 9.2.0
Date:       Tue Apr  2 07:57:26 2024
CPU:        40 * Intel(R) Xeon(R) Gold 5218R CPU @ 2.10GHz
CPUCache:   28160 KB
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    1048576
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    4112.0 MB (estimated)
FileSize:   4112.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
DB path: [/mnt/nvpcssd/db_bench]
readseq      :       3.537 micros/op 282724 ops/sec 3.709 seconds 1048576 operations; 1108.7 MB/s
shh@a317-server1:~/git_clone/nvpcfigure/figure522$ sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=readseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --use_existing_db=true --cache_size=0 -compressed_cache_size=0
Set seed to 1712044669682834 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
RocksDB:    version 9.2.0
Date:       Tue Apr  2 07:57:49 2024
CPU:        40 * Intel(R) Xeon(R) Gold 5218R CPU @ 2.10GHz
CPUCache:   28160 KB
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    1048576
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    4112.0 MB (estimated)
FileSize:   4112.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
DB path: [/mnt/nvpcssd/db_bench]
readseq      :       3.475 micros/op 287785 ops/sec 3.644 seconds 1048576 operations; 1128.6 MB/s
shh@a317-server1:~/git_clone/nvpcfigure/figure522$ sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=readseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --use_existing_db=true --cache_size=0 -compressed_cache_size=0
Set seed to 1712044681671333 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
RocksDB:    version 9.2.0
Date:       Tue Apr  2 07:58:01 2024
CPU:        40 * Intel(R) Xeon(R) Gold 5218R CPU @ 2.10GHz
CPUCache:   28160 KB
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    1048576
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    4112.0 MB (estimated)
FileSize:   4112.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
DB path: [/mnt/nvpcssd/db_bench]
readseq      :       3.452 micros/op 289718 ops/sec 3.619 seconds 1048576 operations; 1136.1 MB/s
