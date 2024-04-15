sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/1048576/nova/90/512/0
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712451976344048 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
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
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      21.003 micros/op 47612 ops/sec 22.023 seconds 1048576 operations;  186.7 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/1048576/nova/90/512/1
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712452012602351 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
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
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      21.182 micros/op 47210 ops/sec 22.211 seconds 1048576 operations;  185.1 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
sudo mount -t NOVA -o init,data_cow /dev/pmem0 /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/1048576/nova/90/512/2
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=1048576 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712452049058016 because --seed was 0
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
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
Initializing RocksDB Options from the specified file
Initializing RocksDB Options from command-line flags
Integrated BlobDB: blob cache disabled
DB path: [/mnt/nvpcssd/db_bench]
fillseq      :      21.049 micros/op 47509 ops/sec 22.071 seconds 1048576 operations;  186.3 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo umount /mnt/nvpcssd/
