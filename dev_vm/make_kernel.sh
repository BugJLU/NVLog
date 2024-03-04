#!/bin/bash

source $(dirname "$0")/env.sh

cd $KERNEL_DIR
sudo make -j88
echo "kernels built"

if [[ ! -z "$1" ]] && [[ "$1" == "withmod" ]]; 
then
    sudo make INSTALL_MOD_PATH=$KERNEL_DIR/modules_build modules_install
else
    echo "modules are not extracted"
fi
echo "done"