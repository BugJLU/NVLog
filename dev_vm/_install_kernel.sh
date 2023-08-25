#!/bin/bash

export MY_DIR=$(dirname $(readlink -f "$0"))

sudo installkernel 5.15.125-nvpc $MY_DIR/bzImage $MY_DIR/System.map /boot

sudo mkinitramfs -o /boot/initrd.img-5.15.125-nvpc
sudo update-grub