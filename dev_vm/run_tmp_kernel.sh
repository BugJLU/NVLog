#!/bin/bash

source $(dirname "$0")/env.sh

sudo qemu-system-x86_64 \
    -machine pc,accel=kvm,nvdimm=on \
    -smp 8 \
    -m 4G,slots=4,maxmem=32G \
    -nographic \
    -enable-kvm \
    -kernel /home/shh/git_clone/nvpc/linux-5.15.125/arch/x86_64/boot/bzImage \
    -drive format=raw,file=$VM_DIR/tmp_disk.img,if=virtio \
    -drive format=raw,file=$VM_DIR/share.img,if=virtio \
    -append "root=/dev/vda3 console=ttyS0 nokaslr" \
    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
   # -net tap,ifname=tap0,script=no,downscript=no \
   # -net nic\
    --parallel none \
    #--serial telnet::3441,server,nowait \
#   --serial telnet::3442,server,nowait \
#    --serial telnet::3443,server,nowait \

#sudo qemu-system-x86_64 -smp 8 -m 4G,slots=4,maxmem=128G \
#	-cpu host \
#	-hda $VM_DIR/tmp_disk.img \
#	-enable-kvm \
#   -vnc :2	\
#	-machine pc,accel=kvm,nvdimm=on \
#	-hdb $VM_DIR/share.img \
#    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
#    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
#	-chardev socket,id=mon0,host=localhost,port=4444,server,nowait \
#    -mon chardev=mon0 \
