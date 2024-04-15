#!/usr/bin/python

import subprocess
import time
import os

def execute_shell_command_output_to_file(command, output_file):
    result = os.system(command+" >> "+output_file)

def generate_op_num(total_size,value_size):
    return int(int(total_size)*G/int(value_size))

G=1024*1024*1024
M=1024*1024
K=1024
keysize="16"
valuesizekb="4"
valuesize=str(int(valuesizekb)*K)
totalsize="4"
rrwrthreads="8"
rwwthreads="9"
wsthreads="8"
rsthreads="8"
benchmark="readrandomwriterandom" #benchmark="readseq" #benchmark="fillseq"
opnum=generate_op_num(totalsize,valuesize)
fs="nova"
times=3
compression_level=0
compression_ratio=1.0
readpercent=90
lv1sizemb=512
lv1size=lv1sizemb*M

dirpath="/mnt/nvpcssd/"
db_path="db_bench"
pmempath="/dev/pmem0"
ssdpath="/dev/nvme0n1"
db_bench_path="/home/shh/git_clone/rocksdb-main/db_bench "
log_path="./log"
command_to_execute = ""
command_parameter=""
utils_path="/home/shh/git_clone/nvpc/utils/"
dump_path="/dev/zero"


def append_to_file(file_path, content):
    try:
        with open(file_path, 'a') as file:
            file.write(content + '\n')
        print("log written")
    except Exception as e:
        print("log write failed")

def mount_nova(pmem_path,ssd_path,dir_path):
    return "mount -t NOVA -o init,data_cow " + pmem_path + " " + dir_path

def umount_nova(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

def mount_spfs(pmem_path,ssd_path,dir_path):
    cmd = "mount -t ext4 " + ssd_path + " " + dir_path + " ; "
    cmd += "sudo mount -t spfs -o pmem=" + pmem_path + ",format,consistency=meta " + dir_path + " " + dir_path
    return cmd

def umount_spfs(pmem_path,ssd_path,dir_path):
    cmd = "umount " + dir_path + " ; "
    cmd += "sudo umount " + dir_path
    return cmd

def mount_ext4(pmem_path,ssd_path,dir_path):
    return "mount -t ext4 " + ssd_path + " " + dir_path

def umount_ext4(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

def mount_nvpc(pmem_path,ssd_path,dir_path):
    cmd="mount "+ssd_path+" "+dir_path+" ; "
    cmd+=utils_path+"nvpcctl open "+dir_path+" s ; "
    return cmd

def umount_nvpc(pmem_path,ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

mountfs={
    "nova":mount_nova,
    "spfs":mount_spfs,
    "ext4":mount_ext4,
    "nvpc":mount_nvpc
}

umountfs={
    "nova":umount_nova,
    "spfs":umount_spfs,
    "ext4":umount_ext4,
    "nvpc":umount_nvpc
}

def print_stats(string):
    print(string)
    return 

def fillseq(benchmark,threads,dirpath,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size):
    return "--benchmarks="+"fillseq"+" " \
            "--threads=1"+" " \
            "--wal_dir="+dirpath+" " \
            "--db="+dirpath+" " \
            "--key_size="+str(keysize)+" " \
            "--value_size="+str(valuesize)+" " \
            "--sync"+" " \
            "--num="+str(opnum)+" " \
            "--compression_level="+str(compression_level)+" " \
            "--compression_ratio="+str(compression_ratio)+" " \
            "--target_file_size_base="+str(lv1size)+" " \
            "--cache_size=0"+" " \
            "-compressed_cache_size=0"+" " 

def rrwr(benchmark,threads,dirpath,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size):
    return "--benchmarks="+benchmark+" " \
            "--threads="+str(threads)+" " \
            "--wal_dir="+dirpath+" " \
            "--db="+dirpath+" " \
            "--key_size="+str(keysize)+" " \
            "--value_size="+str(valuesize)+" " \
            "--sync"+" " \
            "--num="+str(opnum)+" " \
            "--compression_level="+str(compression_level)+" " \
            "--compression_ratio="+str(compression_ratio)+" " \
            "--readwritepercent="+str(readpercent)+" " \
            "--target_file_size_base="+str(lv1size)+" " \
            "--use_existing_db=true"+" " \
            "--cache_size=0"+" " \
            "-compressed_cache_size=0"+" " 

def rww(benchmark,threads,dirpath,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size):
    return "--benchmarks="+benchmark+" " \
            "--threads="+str(threads)+" " \
            "--wal_dir="+dirpath+" " \
            "--db="+dirpath+" " \
            "--key_size="+str(keysize)+" " \
            "--value_size="+str(valuesize)+" " \
            "--sync"+" " \
            "--num="+str(opnum)+" " \
            "--compression_level="+str(compression_level)+" " \
            "--compression_ratio="+str(compression_ratio)+" " \
            "--target_file_size_base="+str(lv1size)+" " \
            "--use_existing_db=true"+" " \
            "--cache_size=0"+" " \
            "-compressed_cache_size=0"+" "

def ws(benchmark,threads,dirpath,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size):
    return "--benchmarks="+benchmark+" " \
            "--threads=1"+" " \
            "--wal_dir="+dirpath+" " \
            "--db="+dirpath+" " \
            "--key_size="+str(keysize)+" " \
            "--value_size="+str(valuesize)+" " \
            "--sync"+" " \
            "--num="+str(opnum)+" " \
            "--compression_level="+str(compression_level)+" " \
            "--compression_ratio="+str(compression_ratio)+" " \
            "--target_file_size_base="+str(lv1size)+" " \
            "--cache_size=0"+" " \
            "-compressed_cache_size=0"+" "

def rs(benchmark,threads,dirpath,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size):
    return "--benchmarks="+benchmark+" " \
            "--threads=1"+" " \
            "--wal_dir="+dirpath+" " \
            "--db="+dirpath+" " \
            "--key_size="+str(keysize)+" " \
            "--value_size="+str(valuesize)+" " \
            "--sync"+" " \
            "--num="+str(opnum)+" " \
            "--compression_level="+str(compression_level)+" " \
            "--compression_ratio="+str(compression_ratio)+" " \
            "--target_file_size_base="+str(lv1size)+" " \
            "--use_existing_db=true"+" " \
            "--cache_size=0"+" " \
            "-compressed_cache_size=0"+" "

def clear_dir(db_path,log_path):
    execute_shell_command_output_to_file("sudo rm "+db_path+"/*",log_path)
    append_to_file(log_path,"sudo rm "+db_path+"/*")
    return 

for _iter in range(times) :
    callable=mountfs.get(fs)
    if callable is not None :
        execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
        append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
        time.sleep(2)
    else :
        continue
    clear_dir(dirpath+db_path,log_path)
    time.sleep(2)
    if benchmark=="fillseq":
        pass
    else :
        command_to_execute=db_bench_path+" "+fillseq(benchmark,"1",dirpath+db_path,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size)
        append_to_file(log_path,command_to_execute)
        execute_shell_command_output_to_file("sudo "+command_to_execute, dump_path)
        time.sleep(2)
    append_to_file(log_path,"ls "+dirpath+db_path)
    execute_shell_command_output_to_file("ls "+dirpath+db_path, log_path)
    time.sleep(2)
    if benchmark=="readwhilewriting" :
        command_to_execute=db_bench_path+" "+rww(benchmark,rwwthreads,dirpath+db_path,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size)
        print_stats(benchmark+"/"+str(rwwthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
        append_to_file(log_path,benchmark+"/"+str(rwwthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
    elif benchmark=="readrandomwriterandom" :
        print_stats(benchmark+"/"+str(rrwrthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
        command_to_execute=db_bench_path+" "+rrwr(benchmark,rrwrthreads,dirpath+db_path,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size)
        append_to_file(log_path,benchmark+"/"+str(rrwrthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
    elif benchmark=="fillseq" :
        clear_dir(dirpath+db_path,log_path)
        print_stats(benchmark+"/"+str(wsthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
        command_to_execute=db_bench_path+" "+ws(benchmark,wsthreads,dirpath+db_path,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size)
        append_to_file(log_path,benchmark+"/"+str(wsthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
    elif benchmark=="readseq" :
        print_stats(benchmark+"/"+str(rsthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
        command_to_execute=db_bench_path+" "+rs(benchmark,rsthreads,dirpath+db_path,keysize,valuesize,opnum,compression_level,compression_ratio,readpercent,lv1size)
        append_to_file(log_path,benchmark+"/"+str(rsthreads)+"/"+str(keysize)+"/"+str(valuesize)+"/"+str(opnum)+"/"+fs+"/"+str(readpercent)+"/"+str(lv1sizemb)+"/"+str(_iter))
    append_to_file(log_path,command_to_execute)
    execute_shell_command_output_to_file("sudo "+command_to_execute, log_path)
    time.sleep(2)
    clear_dir(dirpath+db_path,log_path)
    time.sleep(2)
    append_to_file(log_path,"ls "+dirpath+db_path)
    execute_shell_command_output_to_file("ls "+dirpath+db_path, log_path)
    time.sleep(2)
    callable=umountfs.get(fs)
    if callable is not None :
        execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
        append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
        time.sleep(2)
    else :
        continue
