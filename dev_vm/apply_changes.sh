#!/bin/bash

sudo ./make_kernel.sh withmod
sudo ./copy_kernel.sh withmod
sudo ./run_qemu.sh
