#!/usr/bin/python

import subprocess
import time

iosize=["4k"]
threads=["1","2","4","8","16"]
op=["randrw"]
sync=["sync"]
filesize=["1g"]
fstype="nova"
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
multipath="/mnt/nvpcssd/multi"
section_base="[job{}]\nfilename={}/multifile{}\n\n"
base_code="""
[global]
rw=${OP}
bs=${IOSIZE}
size=${FILESIZE}
disk_util=0
disable_lat=1
disable_slat=1
disable_clat=1
lat_percentiles=0
slat_percentiles=0
clat_percentiles=0
;group_reporting
numjobs=1

; 0 for none 1 for sync
sync=${SYNC}
directory=${DIRPATH}
exitall_on_error
pre_read=1
"""

filepath={
    "1g":"file1g",
    "4g":"file4g",
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
    cmd+=utils_path+"nvpcctl open "+dir_path+" s"
    return cmd

def umount_nvpc(pmem_path,ssd_path,dir_path):
    cmd="umount "+dir_path
    return cmd

mountfs={
    "nova":mount_nova,
    "spfs":mount_spfs,
    "ext4":mount_ext4,
    "nvpc":mount_nvpc,
}

umountfs={
    "nova":umount_nova,
    "spfs":umount_spfs,
    "ext4":umount_ext4,
    "nvpc":umount_nvpc
}

def generate_parameter(op,iosize,filesize,sync,dirpath,filename,threads):
    return "OP="+op+" " \
            "IOSIZE="+iosize+" " \
            "FILESIZE="+filesize+" " \
            "SYNC="+sync+" " \
            "DIRPATH="+dirpath+" " \
            "FILENAME="+filename+" " \
            "THREADS="+threads+" " 

def generate_section(_thread):
    return section_base.format(_thread,multipath,_thread)

def generate_benchfile(basecode,threads):
    for _threads in range(int(threads)):
        basecode+=generate_section(_threads)
    return basecode

def generate_option():
    return " "

def print_stats(string):
    print(string)
    return 

def write_to_file(file_path, content):
    with open(file_path, 'w') as file:
        file.write(content)

for _iosize in iosize :
    for _threads in threads :
        tmp=generate_benchfile(base_code,_threads)
        write_to_file(bench_path,tmp)
        for _op in op :
            for _sync in sync :
                for _filesize in filesize :
                    for _iter in range(times) :
                            print_stats(_iosize+_threads+_op+_sync+_filesize+fstype+str(_iter))
                            command_parameter=generate_parameter(_op,_iosize,_filesize,_sync,dirpath,filepath.get(_filesize),_threads)
                            command_option=generate_option()    
                            command_to_execute=command_parameter+"fio "+command_option+bench_path
                            time.sleep(2)
                            callable=mountfs.get(fstype)
                            if callable is not None :
                                #print(callable)
                                append_to_file(log_path,("sudo " + callable(pmempath,ssdpath,dirpath)))
                                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
                                time.sleep(2)
                                pass
                            else :
                                continue
                            execute_shell_command_output_to_file("sudo "+command_to_execute, log_path)
                            time.sleep(2)
                            callable=umountfs.get(fstype)
                            if callable is not None :
                                #print(callable)
                                append_to_file(log_path,("sudo " + callable(pmempath,ssdpath,dirpath)))
                                execute_shell_command_output_to_file("sudo " + callable(pmempath,ssdpath,dirpath),log_path)
                                time.sleep(2)
                                pass
                            else :
                                continue

