#!/bin/bash

source $(dirname "$0")/env.sh

sudo qemu-system-x86_64 -smp 8 -m 8G,slots=4,maxmem=128G -cpu host \
    -machine pc,accel=kvm,nvdimm=on \
	-hda $VM_DIR/disk.img \
	-hdb $VM_DIR/share.img \
    -enable-kvm \
    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
    -vnc :1 \
    -net nic \
    -net tap,ifname=tap0,script=no,downscript=no \
    -serial tcp::4444,server=on,wait=off \
    # -chardev socket,id=mon0,host=localhost,port=4444,server,nowait \
    # -mon chardev=mon0