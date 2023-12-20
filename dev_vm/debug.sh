#!/bin/bash

source $(dirname "$0")/env.sh

#/home/shh/git_clone/nvpcnew/buildroot/output/images/rootfs.ext4

#good version for build root
#sudo qemu-system-x86_64 \
#    -machine pc,nvdimm=on \
#    -smp 8 \
#    -m 4G,slots=4,maxmem=32G \
#    -nographic \
#    -kernel /home/shh/git_clone/nvpcnew/linux-5.15.125/arch/x86_64/boot/bzImage \
#    -drive format=raw,file=$VM_DIR/../buildroot/output/images/rootfs.ext4,if=virtio \
#   -drive format=raw,file=$VM_DIR/share.img,if=virtio \
#    -append "root=/dev/vda console=ttyS0 kgdboc=ttyS1,115200 nokaslr kgdbwait" \
#   -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
#    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
#    --serial telnet::3441,server,nowait \
#    --serial telnet::3442,server,nowait \
#    --parallel none

sudo qemu-system-x86_64 \
    -machine pc,nvdimm=on \
    -smp 8 \
    -m 2G,slots=4,maxmem=32G \
    -nographic \
    -kernel /home/shh/git_clone/nvpcnew/linux-5.15.125/arch/x86_64/boot/bzImage \
    -drive format=raw,file=$VM_DIR/tmp_disk.img,if=virtio \
    -drive format=raw,file=$VM_DIR/share.img,if=virtio \
    -append "root=/dev/vda3 console=ttyS0 kgdboc=ttyS1,115200 nokaslr kgdbwait" \
    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=2G \
    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
    --serial telnet::3441,server,nowait \
    --serial telnet::3442,server,nowait \
    --parallel none

##############################################################    
#   -chardev socket,id=mon0,host=localhost,port=4444,server,nowait \
#   -mon chardev=mon0 \
#   -serial mon:stdio \






#-drive format=raw,file=$VM_DIR/../buildroot/output/images/rootfs.ext4,if=virtio \
#    -kernel $VM_DIR/vmlinuz-5.15.125-nvpc \
#    -initrd $VM_DIR/initrd.img-5.15.125-nvpc 
#    -kernel /home/shh/git_clone/nvpcnew/linux-5.15.125/arch/x86_64/boot/bzImage \
#    -object memory-backend-file,id=mem1,share=on,mem-path=$VM_DIR/nvdimm0,size=8G \
#    -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
#    -drive format=raw,file=linux-disk.raw,if=virtio \
#    -hda $VM_DIR/disk.img \
#    -hdb $VM_DIR/share.img \
#    -append "nokaslr root=/dev/mapper/ubuntu--vg-ubuntu--lv rw console=ttyS0" \