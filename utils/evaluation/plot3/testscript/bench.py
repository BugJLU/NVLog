#!/usr/bin/python

import subprocess
import time
import os

fstype="ext4"
distribution=["random","zipf:0.99"]
dirpath="/mnt/nvpcssd/"
ssdpath="/dev/nvme0n1"
utils_path="/home/shh/git_clone/nvpc/utils/"
random_bench_path="./random.fio"
zipf_bench_path="./zipf.fio"
log_path="./log"
command_to_execute = "fio "
command_parameter=""
times=3

def mount_ext4(ssd_path,dir_path):
    return "mount -t ext4 " + ssd_path + " " + dir_path

def umount_ext4(ssd_path,dir_path):
    return "umount " + dir_path

def mount_nvpc(ssd_path,dir_path):
    cmd="mount "+ssd_path+" "+dir_path+" ; "
    cmd+=utils_path+"nvpcctl open "+dir_path
    return cmd

def umount_nvpc(ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

mountfs={
    "ext4":mount_ext4,
    "nvpc":mount_nvpc
}

umountfs={
    "ext4":umount_ext4,
    "nvpc":umount_nvpc
}

def print_stats(string):
    print(string)
    return 

def append_to_file(file_path, content):
    try:
        with open(file_path, 'a') as file:
            file.write(content + '\n')
        print("log written")
    except Exception as e:
        print("log write failed")


def execute_shell_command_output_to_file(command, output_file):
    result = os.system(command+" | tee -a "+output_file)


for _distribution in distribution:
    if _distribution=="random" :
        command_to_execute="fio "+random_bench_path
    else :
        command_to_execute="fio "+zipf_bench_path
    print(command_to_execute)
    append_to_file(log_path,command_to_execute)
    execute_shell_command_output_to_file("cat "+utils_path+"nvpc_init_default.conf",log_path)
    time.sleep(2)
    for _iter in range(times) :
        print_stats(fstype+str(_iter))
        time.sleep(2)
        callable=mountfs.get(fstype)
        if callable is not None :
            execute_shell_command_output_to_file("sudo " + callable(ssdpath,dirpath),log_path)
            append_to_file(log_path,"sudo " + callable(ssdpath,dirpath))
            time.sleep(2)
        else :
            continue
        execute_shell_command_output_to_file("nvpcctl usage",log_path)
        time.sleep(2)
        execute_shell_command_output_to_file("sudo free -g",log_path)
        time.sleep(2)
        execute_shell_command_output_to_file("sudo "+command_to_execute, log_path)
        time.sleep(2)
        execute_shell_command_output_to_file("nvpcctl usage",log_path)
        time.sleep(2)
        execute_shell_command_output_to_file("sudo free -g",log_path)
        callable=umountfs.get(fstype)
        if callable is not None :
            execute_shell_command_output_to_file("sudo " + callable(ssdpath,dirpath),log_path)
            append_to_file(log_path,"sudo " + callable(ssdpath,dirpath))
            time.sleep(2)
        else :
            continue

