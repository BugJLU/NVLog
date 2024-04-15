sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/nova/90/512/0
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712540944507973 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    8388608
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    32896.0 MB (estimated)
FileSize:   32896.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      21.017 micros/op 47579 ops/sec 176.306 seconds 8388608 operations;  186.6 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/nova/90/512/1
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712541135644326 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    8388608
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    32896.0 MB (estimated)
FileSize:   32896.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      21.042 micros/op 47524 ops/sec 176.510 seconds 8388608 operations;  186.4 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/nova/90/512/2
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712541326986259 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
Keys:       16 bytes each (+ 0 bytes user-defined timestamp)
Values:     4096 bytes each (4096 bytes after compression)
Entries:    8388608
Prefix:    0 bytes
Keys per prefix:    0
RawSize:    32896.0 MB (estimated)
FileSize:   32896.0 MB (estimated)
Write rate: 0 bytes/second
Read rate: 0 ops/second
Compression: Snappy
Compression sampling rate: 0
Memtablerep: SkipListFactory
Perf Level: 1
WARNING: Assertions are enabled; benchmarks unnecessarily slow
------------------------------------------------
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      20.929 micros/op 47779 ops/sec 175.569 seconds 8388608 operations;  187.4 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
