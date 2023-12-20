#!/bin/bash

export MY_DIR=$(dirname $(readlink -f "$0"))

if [[ ! -z "$1" ]] && [[ "$1" == "withmod" ]]; 
then
    sudo cp -r -u $MY_DIR/modules_build/lib/modules/5.15.125 /lib/modules/
    sudo ln -s /lib/modules/5.15.125 /lib/modules/5.15.125-nvpc
else
    echo "modules are not installed"
fi

sudo installkernel 5.15.125-nvpc $MY_DIR/bzImage $MY_DIR/System.map /boot

sudo mkinitramfs -o /boot/initrd.img-5.15.125-nvpc
sudo update-grub
echo "kernels installed, done"

reboot