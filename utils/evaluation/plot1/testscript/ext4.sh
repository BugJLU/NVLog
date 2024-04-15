#!/bin/bash

# 可执行程序路径
executable="./mix_rws"

# 参数列表
param1_values=(01 37 55 73)  # 第一个参数可能取值
param2_values=(0 2 4 6 8 10)  # 第二个参数可能取值

# 输出文件
output_file="./log"

# 清空输出文件
> $output_file

# 循环遍历所有参数可能性，并将输出追加到输出文件中
for value1 in "${param1_values[@]}"
do
    for value2 in "${param2_values[@]}"
    do
	for i in {1..3}
	do
        # 执行可执行程序并传入参数，并将输出追加到输出文件
            echo "Executing $executable with parameters: $value1 $value2" | tee -a $output_file
            sleep 1
	    sudo mount /dev/nvme0n1 /mnt/nvpcssd
	    sleep 1
	    sudo $executable /mnt/nvpcssd $value1 $value2 | tee -a $output_file
	    sleep 1
	    sudo umount /mnt/nvpcssd
	    sleep 1
    	done
    done
done
