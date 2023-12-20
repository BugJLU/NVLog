#!/bin/bash

source $(dirname "$0")/env.sh

sudo qemu-system-x86_64 -smp 8 -m 8G,slots=4,maxmem=128G -cpu host \
    -machine pc,accel=kvm,nvdimm=on \
    -hda $VM_DIR/disk.img \
    -hdb $VM_DIR/share.img \
    -enable-kvm \
    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
    -vnc :1
    #-nographic \
    #-net tap,ifname=tap0,script=no,downscript=no \
    


#-net nic,model=e1000\

    
#qemu-system-arm 
#	-M vexpress-a9 
#	-m 512M 
#	-kernel zImage 
#	-append "rdinit=/linuxrc console=ttyAMA0 loglevel=8" 
#	-dtb vexpress-v2p-ca9.dtb 
#	-nographic 
#	-net nic 
#	-net tap,ifname=tap0,script=no,downscript=no
    #-chardev socket,id=mon0,host=localhost,port=4444,server,nowait \
    #-mon chardev=mon0
