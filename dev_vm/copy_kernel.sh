#!/bin/bash

source $(dirname "$0")/env.sh

sudo mount $VM_DIR/share.img $VM_DIR/mnt/

# copy kernel
sudo cp $KERNEL_DIR/arch/x86_64/boot/bzImage $VM_DIR/mnt/
sudo cp $KERNEL_DIR/System.map $VM_DIR/mnt/
sudo cp $KERNEL_DIR/vmlinux $VM_DIR/mnt/
echo "kernels are copied"

if [[ ! -z "$1" ]] && [[ "$1" == "withmod" ]]; 
then
    sudo cp -r -u  $KERNEL_DIR/modules_build $VM_DIR/mnt/
    echo "modules are copied"
else
    echo "modules are not copied"
fi

sudo cp $VM_DIR/_install_kernel.sh $VM_DIR/mnt/install_kernel.sh
sudo chmod 755 $VM_DIR/mnt/install_kernel.sh
echo "scripts are copied"

# copy utils
sudo cp -r $UTILS_DIR $VM_DIR/mnt/
echo "utils are copied"

sudo umount $VM_DIR/mnt
echo "done"
