#!/usr/bin/python

import subprocess
import time

fstype="nvpc-inactive"
iosize=["1k","4k","16k"]
op=["append","write","randwrite"]

dirpath="/mnt/nvpcssd/"
pmempath="/dev/pmem0"
ssdpath="/dev/nvme0n1"
utils_path="/home/shh/git_clone/nvpc/utils/"
bench_path="./sync.fio"
log_path="./log"
csv_path="./csv"
command_to_execute = "fio "
command_option=""
command_parameter=""
times=3

def execute_shell_command_output_to_file(command, output_file):
    try:
        with open(output_file, 'a') as f:
            result = subprocess.run(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            f.write(result.stdout)
            f.write(result.stderr)
        print("log written")
    except Exception as e:
        print("log write failed")

def write_to_file(text, file_path):
    try:
        with open(file_path, 'w') as file:
            file.write(text)
    except Exception as e:
        print("bench.f write failed")

def append_to_file(file_path, content):
    try:
        with open(file_path, 'a') as file:
            file.write(content + '\n')
        print("log written")
    except Exception as e:
        print("log write failed")

def mount_nova(pmem_path,ssd_path,dir_path):
    return "mount -t NOVA -o init " + pmem_path + dir_path

def umount_nova(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

def mount_spfs(pmem_path,ssd_path,dir_path):
    cmd = "mount -t ext4 " + ssd_path +" "+ dir_path + " ; "
    cmd += "mount -t spfs -o pmem=" + pmem_path + ",format,consistency=meta " + dir_path + " " + dir_path
    return cmd

def umount_spfs(pmem_path,ssd_path,dir_path):
    cmd = "umount " + dir_path + " ; "
    cmd += "umount " + dir_path
    return cmd

def mount_ext4(pmem_path,ssd_path,dir_path):
    return "mount -t ext4 " + ssd_path + " " + dir_path

def umount_ext4(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

def mount_nvpc_inactive(pmem_path,ssd_path,dir_path):
    cmd="mount "+ssd_path+" "+dir_path+" ; "
    cmd+=utils_path+"nvpcctl open "+dir_path+" s ; "
    cmd+="sudo "+utils_path+"nvpcctl activesync set 0 ; "
    cmd+="sudo "+utils_path+"nvpcctl activesync show"
    return cmd

def umount_nvpc_inactive(pmem_path,ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

def mount_p2cache(pmem_path,ssd_path,dir_path):
    cmd="mount -o sync "+ssd_path+" "+dir_path+" ; "
    cmd+=utils_path+"nvpcctl open "+dir_path+" s"
    return cmd

def umount_p2cache(pmem_path,ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

def mount_nvpc_active(pmem_path,ssd_path,dir_path):
    cmd="mount "+ssd_path+" "+dir_path+" ; "
    cmd+=utils_path+"nvpcctl open "+dir_path+" s ;"
    cmd+="sudo "+utils_path+"nvpcctl activesync set 1 ; "
    cmd+="sudo "+utils_path+"nvpcctl activesync show"
    return cmd

def umount_nvpc_active(pmem_path,ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

mountfs={
    "nova":mount_nova,
    "spfs":mount_spfs,
    "ext4":mount_ext4,
    "nvpc-inactive":mount_nvpc_inactive,
    "p2cahe":mount_p2cache,
    "nvpc-active":mount_nvpc_active
}

umountfs={
    "nova":umount_nova,
    "spfs":umount_spfs,
    "ext4":umount_ext4,
    "nvpc-inactive":umount_nvpc_inactive,
    "p2cache":umount_p2cache,
    "nvpc-active":umount_nvpc_active
}

def generate_parameter(iosize,op,append):
    return "IOSZ="+str(iosize)+" "+\
            "OP="+str(op)+" "+\
            "APPEND="+str(append)+" "

def generate_option():
    return ""

def print_stats(string):
    print(string)
    return 

def remove_file(path):
    execute_shell_command_output_to_file("sudo rm "+path,log_path)
    return

for _sz in iosize:  
    for _op in op:
        if _op == "append":
            command_parameter=generate_parameter(_sz,"write","1")
        else :
            command_parameter=generate_parameter(_sz,_op,"0")
        command_to_execute=command_parameter+"fio "+command_option+bench_path
        print(command_to_execute)
        for _iter in range(times) :
            print_stats(fstype+str(_iter)+_sz+_op)
            time.sleep(2)
            callable=mountfs.get(fstype)
            if callable is not None :
                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
                append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
                time.sleep(2)
                pass
            else :
                continue
            execute_shell_command_output_to_file("sudo "+command_to_execute, log_path)
            time.sleep(2)
            remove_file(dirpath+"file2g")
            time.sleep(1)
            callable=umountfs.get(fstype)
            if callable is not None :
                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
                append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
                time.sleep(2)
                pass
            else :
                 continue
