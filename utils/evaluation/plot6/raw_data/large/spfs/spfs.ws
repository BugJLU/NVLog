sudo mount -t ext4 /dev/nvme0n1p1 /mnt/nvpcssd/ ; sudo mount -t spfs -o pmem=/dev/pmem0,format,consistency=meta /mnt/nvpcssd/ /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/spfs/90/512/0
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712544789980070 because --seed was 0
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
fillseq      :      15.365 micros/op 65081 ops/sec 128.895 seconds 8388608 operations;  255.2 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
000030.log
000032.log
MANIFEST-000005
sudo umount /mnt/nvpcssd/ ; sudo umount /mnt/nvpcssd/
sudo mount -t ext4 /dev/nvme0n1p1 /mnt/nvpcssd/ ; sudo mount -t spfs -o pmem=/dev/pmem0,format,consistency=meta /mnt/nvpcssd/ /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/spfs/90/512/1
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712544944117206 because --seed was 0
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
fillseq      :      15.688 micros/op 63741 ops/sec 131.604 seconds 8388608 operations;  250.0 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
000030.log
000032.log
MANIFEST-000005
sudo umount /mnt/nvpcssd/ ; sudo umount /mnt/nvpcssd/
sudo mount -t ext4 /dev/nvme0n1p1 /mnt/nvpcssd/ ; sudo mount -t spfs -o pmem=/dev/pmem0,format,consistency=meta /mnt/nvpcssd/ /mnt/nvpcssd/
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
sudo rm /mnt/nvpcssd/db_bench/*
fillseq/8/16/4096/8388608/spfs/90/512/2
/home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 
Set seed to 1712545100955245 because --seed was 0
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
fillseq      :      15.502 micros/op 64507 ops/sec 130.042 seconds 8388608 operations;  253.0 MB/s
sudo rm /mnt/nvpcssd/db_bench/*
ls /mnt/nvpcssd/db_bench
000030.log
000032.log
MANIFEST-000005
sudo umount /mnt/nvpcssd/ ; sudo umount /mnt/nvpcssd/
