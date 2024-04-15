#!/usr/bin/python

import subprocess
import time

iosize=["16k","4k","1k","100b"]
threads=["1"]
op=["append","write","randwrite"]
sync=["sync"]
filesize=["2g"]
fstype="ext4"
times=3

dirpath="/mnt/nvpcssd/"
pmempath="/dev/pmem0"
ssdpath="/dev/nvme0n1"
bench_path="./bench.fio"
log_path="./log"
csv_path="./csv"
command_to_execute = "fio "
command_option=""
command_parameter=""
utils_path="/home/shh/git_clone/nvpc/utils/"

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
    cmd+=utils_path+"nvpcctl open "+dir_path+" s ;"
    return cmd

def umount_nvpc(pmem_path,ssd_path,dir_path):
    return "umount " + dir_path

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

def generate_parameter(op,iosize,filesize,sync,dirpath,filename,threads,append):
    return "OP="+op+" " \
            "IOSIZE="+iosize+" " \
            "FILESIZE="+filesize+" " \
            "SYNC="+sync+" " \
            "DIRPATH="+dirpath+" " \
            "FILENAME="+filename+" " \
            "THREADS="+threads+" " \
            "APPEND="+append+" "

def generate_option():
    return ""

def remove_file(path):
    execute_shell_command_output_to_file("sudo rm "+path,log_path)
    return

for _iosize in iosize :
    for _threads in threads :
        for _op in op :
            for _sync in sync :
                for _filesize in filesize :
                    if _op == "append":
                        command_parameter=generate_parameter("write",_iosize,_filesize,_sync,dirpath,filepath.get(_filesize),_threads,"1")
                    else :
                        command_parameter=generate_parameter(_op,_iosize,_filesize,_sync,dirpath,filepath.get(_filesize),_threads,"0")
                    append_to_file(log_path,command_parameter)
                    command_option=generate_option()
                    command_to_execute=command_parameter+"fio "+command_option+bench_path
                    time.sleep(2)
                    for _iter in range(times) :
                        print_stats(_iosize+_threads+_op+_sync+_filesize+fstype+str(_iter))
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
                        remove_file(dirpath+filepath.get(_filesize))
                        time.sleep(2)
                        callable=umountfs.get(fstype)
                        if callable is not None :
                            execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
                            append_to_file(log_path,"sudo " + callable(pmempath,ssdpath,dirpath))
                            time.sleep(2)
                            pass
                        else :
                            continue
