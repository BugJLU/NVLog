# NVPC: It's Time to Drop NVM File Systems: A Case for A Transparent NVM Page Cache

This repository contains artifacts of the system **NVPC** we implemented in the paper.

## Description

We propose NVPC, a transparent **N**on-**V**olatile Memory **P**age **C**ache that integrates NVM devices into the system page cache, and helps almost all file systems store their data need synchronized into storages in NVM instead of lower-level slow storage.

Our implementation is based on Linux 5.15.125, and our core code has undergone `~7000` lines of changes, and the test and utility code amounts to `~3000` lines.

## Artifacts Structure

Directory structure:

```dir
NVPC
├── dev_vm                  --- Test code in QEMU
├── linux-5.15.125          --- Our modified linux kernel
├── utils                   --- Evaluation scripts and utilities code
└── README.md               --- Doc you are watching
```

## Getting Started

Now we will start guiding you step by step on how to deploy and run the code we've implemented in the repository.

### Prerequisites

Since our code implementation is based on a specific linux kernel (5.15.125), please check first if the environment you are using requires support from other specific versions of the kernel. If so, please resolve the version-specific dependencies first.

If you just want to test that the functionality we have implemented in VM, the hardware required is as follows:

- CPU: x86_64 CPU with multiple cores
- Memory: 16GB and more
- Disk: 100GB and bigger disk(s)
- Virtualization: KVM enabled

But if you need to reproduce the evaluation part of our article, the hardware required is as follows:

- CPU: 2nd Generation Intel Xeon Processor Scalable Processors and newer CPUs which support Intel Optane DCPMM
- Memory: 128GB DDR4 ECC Memory
- NVM: 1st Gen Intel Optane DCPMM 256GB (128GB*2)
- Disk: NVMe PCI-e 3.0 SSD 1.92TB

### Installation

#### OS Setup

We run and evaluate our code on Linux distribution Ubuntu 20.04.6 LTS Server. When installing Ubuntu, just follow Ubuntu's installer instructions and try not to use the SWAP partition.

Once you have installed the operating system, you will need to use `root` for all operations to avoid permission-related issues.

#### Install Kernel

First, you need to compile the Linux Kernel on another computer, with the same requirements as for normal Kernel compilation.

Then, copy the compiled kernel and modules to a disk accessible on the target system and install the kernel on the target system. Simple scripts are provided for installing the kernel. Copy `env.sh` and `_install_kernel.sh` to the kernel directory and run `_install_kernel.sh` directly to install the kernel and its modules automatically.

```shell
sh ./_install_kernel.sh
```

Note that the path names in `env.sh` are the same as your current kernel paths, which may need to be changed.

### Test Basic Functions in VM (QEMU)

Deploying NVPC needs NVM devices, whether virtual or real hardwares.

#### Initialize NVPC

You need to finish building and installing our kernel. You also need to finish building our `daxctl` tool in the utils. Now move your step to the utils directory, then run the init script:

```shell
cd <workdir>/utils
bash ./nvpc_init.sh
```

#### Open NVPC on a Mount Point

This will open NVPC on the mount point file system of the given file. 

```shell
./nvpcctl open <path_to_a_file>
```


## Evaluation

### Figure 1

### Figure 2


<!-- ### Cite

```bibtex
@inproceedings{guoyuNVPC2024,
    title = {It's Time to Drop {NVM} {File Systems}: A Case for A {Transparent} NVM {Page Cache}},
    isbn = {978-x-xxxxxx-xx-x},
    url = {https://xxxxxxxxxxxxxxxxxxxxxxxxxxxxx},
    language = {en-US},
    urldate = {2024-05-05},
    author = {xxxxxxxxxxxxxxx},
    year = {2024},
    keywords = {},
    pages = {xxxxxxxxx-xxxxxxxxxx},
}
``` -->

## References

[1] ...