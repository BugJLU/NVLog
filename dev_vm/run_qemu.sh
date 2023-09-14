#!/bin/bash

source $(dirname "$0")/env.sh

sudo qemu-system-x86_64 -smp 8 -m 32G -cpu host \
	-hda $VM_DIR/disk.img \
	-hdb $VM_DIR/share.img \
    -enable-kvm \
    -vnc :1 -s