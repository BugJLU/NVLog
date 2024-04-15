# NVPC: It's Time to Drop NVM File Systems: A Case for A Transparent NVM Page Cache

This repository contains artifacts of the system **NVPC** we implemented in the paper. NVPC is an NVM-enhanced page cache that can absorb sync writes and extend the DRAM page cache for existing disk file systems to improve their performance transparently. For more information, please refer to the paper. 

> **WARNING:** Only use this work for experimental tests. DO NOT install it under production environment! 

<!-- ## Description

We propose NVPC, a transparent **N**on-**V**olatile Memory **P**age **C**ache that integrates NVM devices into the system page cache, and helps almost all file systems store their data need synchronized into storages in NVM instead of lower-level slow storage.

Our implementation is based on Linux 5.15.125, and our core code has undergone `~7000` lines of changes, and the test and utility code amounts to `~3000` lines. -->

## Artifacts Structure

Directory structure:

```dir
NVPC
├── dev_vm                  --- Scripts to build kernel, etc.
├── linux-5.15.125          --- Our modified linux kernel
├── utils                   --- Evaluation scripts and utilities code
└── README.md               --- Doc you are watching
```

## Getting Started

Now we will start guiding you step by step on how to deploy and run the code we've implemented in the repository.

### Prerequisites

<!-- Since our code implementation is based on a specific linux kernel (5.15.125), please check first if the environment you are using requires support from other specific versions of the kernel. If so, please resolve the version-specific dependencies first. -->

NVPC is implemented on kernel version 5.15.125. Make sure that your Linux distribution supports this kernel version if you want to install this kernel to your machine.

If you just want to test the functionality we have implemented in VM, the hardware required is as follows:

- CPU: x86_64 CPU with multiple cores
- Memory: 16GB+
- Disk: 200GB+
- Virtualization: KVM enabled
- QEMU version: 4.2.1+

But if you need to reproduce the evaluation part of our article, the hardware required is as follows:

- CPU: Intel Xeon 5218R
    
    or other Intel Xeon Processor (2nd Gen or later) with Intel Optane DCPMM support
- Memory: 128GB DDR4 ECC Memory
- NVM: 1st Gen Intel Optane DCPMM 256GB (128GB*2)
- Disk: Samsung PM9A3 (NVMe PCI-e 3.0 SSD 1.92TB)

    or any other disks, but the result may be different from our report in the paper

### Installation

#### OS Setup

We run and evaluate our code on Linux distribution Ubuntu 20.04.6 LTS Server. When installing Ubuntu, just follow Ubuntu's installer instructions and try not to use the SWAP partition.

Once you have installed the operating system, you will need to use `root` for all operations to avoid permission-related issues.

#### Install Kernel

<!-- First, you need to compile the Linux Kernel on another computer, with the same requirements as for normal Kernel compilation.

Then, copy the compiled kernel and modules to a disk accessible on the target system and install the kernel on the target system. Simple scripts are provided for installing the kernel. Copy `env.sh` and `_install_kernel.sh` to the kernel directory and run `_install_kernel.sh` directly to install the kernel and its modules automatically.

```shell
sh ./_install_kernel.sh
```

Note that the path names in `env.sh` are the same as your current kernel paths, which may need to be changed. -->

Make sure that you have the relevant tools installed to compile a Linux kernel. 

- Move to `utils/ndctl` dir, make the ndctl toolchain first. This tool is a modified version to support NVPC, so you need to do this step even if you already have the relevant tools installed. 

- Move to `linux-5.15.125` dir, copy `.config.example` as `.config`. This is the example config file we provide. You can make some further modification on it if you want. 

- Move to `dev_vm` dir and run `./make_kernel.sh`. Check the output to see if the kernel is compiled successfully. 

- If you want to use QEMU to evaluate NVPC:

    - Run `build_vm.sh` in `dev_vm`, it will prepare a QEMU VM for you with Ubuntu 20.04. Follow the step on the VM screen to install Ubuntu first.

    - When the VM is installed successfully, close it. Then run `copy_kernel.sh`. This script helps you to move the relevant files, including the compiled kernel, useful scripts, and utils, to the `shared.img` virtual disk, so that you can access it in the VM.

    - Run `run_qemu.sh`, it will start the VM with proper hardware settings. 

    - Inside the VM, mount `/dev/sdb` (or other path that describes the `shared.img` vdisk). Then move to the mounted dir, run `./install_kernel.sh && reboot` to install the kernel.

- If you want to install NVPC on your physical machine, just move to `dev_vm` dir, and run `./_install_kernel.sh && reboot`.


### Test Basic Functions in VM (QEMU)

Note that deploying NVPC needs NVM devices, whether virtual or real hardwares. Be sure that you have NVM and the NVM is **NOT IN USE** and **DOES NOT CONTAIN ANY USEFUL DATA**. The following steps may erase and modify the data on the NVM. 

#### Initialize NVPC

Move to the utils directory (`utlis` dir under this workspace for physical machine, or the `utils` dir under shared disk for VM), then run the init script `./nvpc_init.sh`.

Or if you need to set up NVPC with specific configuration, modify the `nvpc_init_default.conf` in `utils` dir *BEFORE* running `nvpc_init.sh`.

#### Open NVPC on a Mount Point

Run `./nvpcctl open <path>` under `utils` dir. This will open NVPC on the mount point file system of the given path. 

Then all FS operations on the opened mount point will be handled by NVPC. E.g. write something to a file with sync, NVPC will absorb the sync (if absorb_syn is on in the configuration). `./nvpcctl usage` will display the pages used by NVPC. 


## Evaluation

Refer to [this link](utils/evaluation/README.md).

