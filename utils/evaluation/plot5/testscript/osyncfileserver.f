set $dir=/mnt/nvpcssd/fs
set $nfiles=10000
set $meandirwidth=20
set $filesize=cvar(type=cvar-gamma,parameters=mean:131072;gamma:1.5) #128K
set $nthreads=16
set $iosize=1m
set $meanappendsize=16k
set $runtime=20

set mode quit alldone

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=80

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop createfile name=createfile1,dsync,filesetname=bigfileset,fd=1
    flowop writewholefile name=wrtfile1,dsync,srcfd=1,fd=1,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop openfile name=openfile1,filesetname=bigfileset,dsync,fd=1
    flowop appendfilerand name=appendfilerand1,iosize=$meanappendsize,dsync,fd=1
    flowop closefile name=closefile2,fd=1
    flowop openfile name=openfile2,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$iosize
    flowop closefile name=closefile3,fd=1
    flowop deletefile name=deletefile1,filesetname=bigfileset
    flowop statfile name=statfile1,filesetname=bigfileset
    }
}

echo  "File-server Version 3.0 personality successfully loaded"

run $runtime
