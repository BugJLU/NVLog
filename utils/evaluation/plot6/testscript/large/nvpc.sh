sudo mount /dev/nvme0n1p1 /mnt/nvpcssd
sleep 2
sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=fillseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --cache_size=0 -compressed_cache_size=0 > /dev/zero
sleep 2
sudo umount /mnt/nvpcssd
sleep 2
sudo mount /dev/nvme0n1p1 /mnt/nvpcssd
sleep 2
sudo nvpcctl open /mnt/nvpcssd
sleep 2
sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=readseq --threads=1 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --target_file_size_base=536870912 --use_existing_db=true --cache_size=0 -compressed_cache_size=0 | tee -a log
sleep 2
sudo /home/shh/git_clone/rocksdb-main/db_bench  --benchmarks=readrandomwriterandom --threads=8 --wal_dir=/mnt/nvpcssd/db_bench --db=/mnt/nvpcssd/db_bench --key_size=16 --value_size=4096 --sync --num=8388608 --compression_level=0 --compression_ratio=1.0 --readwritepercent=90 --target_file_size_base=536870912 --use_existing_db=true --cache_size=0 -compressed_cache_size=0 | tee -a log
sleep 2
sudo umount /mnt/nvpcssd
sleep 2

