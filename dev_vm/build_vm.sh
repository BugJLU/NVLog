#!/bin/bash

source $(dirname "$0")/env.sh

if [ -f $VM_DIR/ubuntu-20.04.6-live-server-amd64.iso ]
then 
	echo "System CDROM exists."
else
	echo `sudo wget --no-check-certificate -P $VM_DIR https://mirrors4.jlu.edu.cn/ubuntu-releases/focal/ubuntu-20.04.6-live-server-amd64.iso`
fi

sudo qemu-img create -f raw $VM_DIR/disk.img 100G
sudo qemu-img create -f raw $VM_DIR/share.img 50G
sudo mkfs ext4 -F $VM_DIR/share.img
sudo qemu-system-x86_64 -smp 8 -m 8G \
	-cpu host \
	-hda $VM_DIR/disk.img \
	-hdb $VM_DIR/share.img \
    -cdrom $VM_DIR/ubuntu-20.04.6-live-server-amd64.iso \
	-enable-kvm \
	-boot order=dc \
    -vnc :1