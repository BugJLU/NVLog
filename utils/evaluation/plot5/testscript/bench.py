#!/usr/bin/python

import subprocess
import time
import os


def execute_shell_command_output_to_file(command, output_file):
    result = os.system(command+" | tee -a "+output_file)

command_to_execute=["filebench -f fileserver.f",
                    "filebench -f webserver.f",
                    "filebench -f varmail.f"]

osync_command_to_execute=["filebench -f osyncfileserver.f",
                    "filebench -f osyncwebserver.f",
                    "filebench -f osyncvarmail.f"]
fs="nvpc"
log_path="./log"
dirpath="/mnt/nvpcssd/"
fileserverpath="fs/"
videoserverpath="vs/"
webserverpath="wp/"
webproxypath="wp/"
varmailpath="vm"
pmempath="/dev/pmem0"
ssdpath="/dev/nvme0n1p1"
utils_path="/home/shh/git_clone/nvpc/utils/"
times=3

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
    cmd+=utils_path+"nvpcctl open "+dir_path
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

def append_to_file(file_path, content):
    try:
        with open(file_path, 'a') as file:
            file.write(content + '\n')
        print("log written")
    except Exception as e:
        print("log write failed")


def print_stats(string):
    print(string)
    return

def clear_dir(bench_path,log_path):
    execute_shell_command_output_to_file("sudo rm -r "+bench_path+"/*",log_path)
    append_to_file(log_path,"sudo rm -r "+bench_path+"/*")
    return

if fs == "p2cache" :
    command_to_execute=osync_command_to_execute
for _cmd in command_to_execute :
    for _iter in range(times) :
        print_stats(_cmd+fs+str(_iter))
        append_to_file(log_path,_cmd+fs+str(_iter))
        callable=mountfs.get(fs)
        if callable is not None :
            execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
            append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
            time.sleep(2)
        else :
            if fs == "p2cache":
                execute_shell_command_output_to_file("sudo mount /dev/nvme0n1p1 /mnt/nvpcssd ; nvpcctl open /mnt/nvpcssd",log_path)
                append_to_file(log_path,"sudo mount /dev/nvme0n1p1 /mnt/nvpcssd ; nvpcctl open /mnt/nvpcssd")
            else:    
                continue
        append_to_file(log_path,_cmd)
        execute_shell_command_output_to_file("sudo "+_cmd, log_path)
        time.sleep(2)
        callable=umountfs.get(fs)
        if callable is not None :
            execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
            append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
            time.sleep(2)
        else :
            if fs == "p2cache":
                execute_shell_command_output_to_file("sudo umount /mnt/nvpcssd",log_path)
                append_to_file(log_path,"sudo umount /mnt/nvpcssd")
            else:
                continue            


