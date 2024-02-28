#!/bin/bash

echo "log start" > log

bench_type=("webserver.f" "webproxy.f" "varmail.f" "fileserver.f")

for item in "${bench_type[@]}"
do
    echo $item | tee -a log
    for ((j=0; j<5; j++))
    do
        echo "test" $j
        filebench -f /mnt/filebench/$item | tee -a log
    done
done