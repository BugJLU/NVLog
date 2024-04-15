#!/usr/bin/python

import subprocess
import time

op=["read","write","randread","randwrite","writes","randwrites"]
pattern=["hot","cold"]
filesize="8g"
fstype="ext4-hdd"
times=3

dirpath="/mnt/nvpcssd/"
pmempath="/dev/pmem0"
ssdpath="/dev/nvme0n1"
hddpath="/dev/sdc"
bench_path="./bench.fio"
log_path="./log"
command_to_execute = "fio "
command_option=""
command_parameter=""

filepath={
    "2g":"file2g",
    "8g":"file8g",
    "16g":"file16g",
    "30g":"file30g"
}

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
    return "mount -t NOVA -o init,data_cow " + pmem_path + " " + dir_path

def umount_nova(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

def mount_ext4_ssd(pmem_path,ssd_path,hdd_path,dir_path):
    return "mount -t ext4 " + ssd_path + " " + dir_path

def umount_ext4_ssd(pmem_path,ssd_path,hdd_path,dir_path):
    return "umount " + dir_path

def mount_ext4_nvm(pmem_path,ssd_path,hdd_path,dir_path):
    return "mount -t ext4 " + pmem_path + " " + dir_path

def umount_ext4_nvm(pmem_path,ssd_path,hdd_path,dir_path):
    return "umount " + dir_path

def mount_ext4_dax(pmem_path,ssd_path,hdd_path,dir_path):
    return "mount -t ext4 -o dax " + pmem_path + " " + dir_path

def umount_ext4_dax(pmem_path,ssd_path,hdd_path,dir_path):
    return "umount " + dir_path

mountfs={
    "nova":mount_nova,
    "ext4-ssd":mount_ext4_ssd,
    "ext4-dax":mount_ext4_dax,
    "ext4-nvm":mount_ext4_nvm
}

umountfs={
    "nova":umount_nova,
    "ext4-ssd":umount_ext4_ssd,
    "ext4-dax":umount_ext4_dax,
    "ext4-nvm":umount_ext4_nvm
}

def print_stats(string):
    print(string)
    return 

def generate_parameter(op,iosize,filesize,dirpath,filename,preread,sync):
    return "OP="+op+" " \
            "IOSIZE="+iosize+" " \
            "FILESIZE="+filesize+" " \
            "DIRPATH="+dirpath+" " \
            "FILENAME="+filename+" " \
            "PREREAD="+preread+" " \
            "SYNC="+sync+" "

def generate_option():
    return ""

def remove_file(path):
    execute_shell_command_output_to_file("sudo rm "+path,log_path)
    return

for _op in op :
    if _op.startswith("rand"):
        _iosize="4K"
    else :
        _iosize="16K"
    if _op.endswith("s"):
        _sync="sync"
        _op=_op[:-1]
    else :
        _sync="none"
    for _pattern in pattern :
        if _pattern == "cold":
            command_parameter=generate_parameter(_op,_iosize,filesize,dirpath,filepath.get(filesize),"0",_sync)
        else:
            command_parameter=generate_parameter(_op,_iosize,filesize,dirpath,filepath.get(filesize),"1",_sync)
        append_to_file(log_path,command_parameter)
        command_option=generate_option()
        command_to_execute=command_parameter+"fio "+command_option+bench_path
        time.sleep(2)
        for _iter in range(times) :
            print_stats(_iosize+_op+filesize+fstype+str(_iter))
            callable=mountfs.get(fstype)
            if callable is not None :
                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,hddpath,dirpath),log_path)
                append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,hddpath,dirpath))
                time.sleep(2)
                pass
            else :
                continue
            execute_shell_command_output_to_file("sudo "+command_to_execute, log_path)
            time.sleep(2)
            remove_file(dirpath+filepath.get(filesize))
            time.sleep(2)
            callable=umountfs.get(fstype)
            if callable is not None :
                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,hddpath,dirpath),log_path)
                append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,hddpath,dirpath))
                time.sleep(2)
                pass
            else :
                continue

