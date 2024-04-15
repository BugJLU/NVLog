# Evaluation of NVPC Paper

> Authored by Haoyang Wei

This repository contains scripts to reproduce the microbenchmark and macrobenchmark results from the NVPC paper.

> **Note:**
> To reproduce the performance of NOVA and SPFS, you need to install the corresponding kernels and run the scripts inside them. Refer to the link for [NOVA](https://github.com/NVSL/linux-nova) and [SPFS](https://github.com/DICL/spfs). For NVPC, Ext-4, and P2CACHE(Sim), just run the scripts in our kernel. 


## Prerequisites
Before you run, something must be set up. You need to

#### Build Fio
The version of Fio used in this experiment is fio-3.36-90-g5ae4 , and the installation process for Fio can be found at this website: <https://github.com/axboe/fio>.

#### Build Filebench
The version of Filebench used in this experiment is 1.5-alpha3 , and the installation process for Filebench can be found at this website: <https://github.com/filebench/filebench>.

#### Build db_bench 
The version of RocksDB used in this experiment is version 9.2.0 , and the installation process for RocksDB can be found at this website: <https://github.com/facebook/rocksdb/blob/master/INSTALL.md>.

## Reproduce results from the paper

### Directories

In this directory, each `plot<x>` dir contains the resources to reproduce the experiments of a specific figure in the paper, and we will provide a detailed list below. Inside each `plot<x>`, there are three dirs: the `testscript` dir contains the scripts to run the experiment; the `raw_data` dir contains the result from our testbed; the `draw` dir contains the script to draw this figure. 

### Run experiments

> **Note:**
> Please run each script in the directory where the script is located.

- Run experiment with script (under `testscript` dir), the scripts wil output the experimental results to log file.

- You can view the output in the directory where the script is located . When the experiment is done , the results will be output to a file named 'log'.

- Each experiment's process.py file is located in the testscript directory. After running the test script, without any modifications, simply run process.py to view the test data in the output file in the current directory.

- Fill the processed data into the plotting script (under `draw` dir), and run the plotting script to reproduce the experimental results in the paper.

### Figure 1. The throughput on different file systems and different storage devices. C and H indicate that the page cache is cold (cache miss) or heated (cache hit). S means sync writes.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot0/testscript/bench.py`  |
| Raw log path  | `nvpc_diagrams/plot0/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot0/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot0/draw/plot0.py`  |
| Description  |  For specific file system performance testing, you need to modify the value of the `fstype` variable in the bench.py file. Choose it from `nova`, `ext4-ssd`, `ext4-dax`, and `ext4-nvm`.  |

### Figure 6. Read, write, and sync mixed tests under 4KB random access.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot1/testscript/ext4.sh` |
|                          | `nvpc_diagrams/plot1/testscript/nvpc.sh` |
|                          | `nvpc_diagrams/plot1/testscript/spfs.sh` |
|                          | `nvpc_diagrams/plot1/testscript/nova.sh` |
|                          | `nvpc_diagrams/plot1/testscript/p2cache.sh` |
| Raw log path  | `nvpc_diagrams/plot1/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot1/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot1/draw/plot1.py`  |
| Description  |  First compile the source of the test tool `mix_rws.c` to `mix_rws`. Then for specific file system performance testing, run the shell script corresponding to the file system name.  |

### Figure 7. Sync performance under different I/O sizes.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot2/testscript/fsync/fsync.py` |
|                          | `nvpc_diagrams/plot2/testscript/osync/osync.py` |
| Raw log path  | `nvpc_diagrams/plot2/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot2/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot2/draw/plot2.py`  |
| Description  |  For specific file system performance testing, you need to modify the value of the `fstype` variable in the fsync.py and osync.py file. Refer to the `mountfs` var in each script first, then choose `fstype` from it.  | 

### Figure 8. Performance and cost-effectiveness of NVPC NVM-extended page cache. Bars show the throughput of each item; crosses show the cost-effectiveness, which is the throughput divided by the memory cost in USD.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot3/testscript/bench.py`  |
| Raw log path  | `nvpc_diagrams/plot3/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot3/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot3/draw/plot3.py`  |
| Description  |  Ensure that the hardware requirements (i.e. DRAM and NVM size) are met before running the script.  |

### Figure 9. Scalability under random r/w test.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot4/testscript/bench.py`  |
| Raw log path  | `nvpc_diagrams/plot4/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot4/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot4/draw/plot4.py`  |
| Description  |  The fio script file used in this test is generated automatically. Also, modify `fstype` according to `mountfs` for each FS.  |

### Figure 10. Filebench performance results.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot5/testscript/bench.py`  |
| Raw log path  | `nvpc_diagrams/plot5/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot5/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot5/draw/plot5.py`  |
| Description  |  In order to simulate the behavior of P2cache more accurately, P2cache uses separate filebench scripts, although the logic of the script remains unchanged. Modify `fs` for each FS, choose between `nova`, `spfs`, `ext4`, `nvpc`, and `p2cache`.  |

### Figure 11. RocksDB performance results.
|  Figure info   | Contents  |
|  ----  | ----  |
| Experiments script path  | `nvpc_diagrams/plot6/testscript/normal/bench.py`  |
|                          | `nvpc_diagrams/plot6/testscript/large/ext4.sh`  |
|                          | `nvpc_diagrams/plot6/testscript/large/nova.sh`  |
|                          | `nvpc_diagrams/plot6/testscript/large/nvpc.sh`  |
|                          | `nvpc_diagrams/plot6/testscript/large/spfs.sh`  |
| Raw log path  | `nvpc_diagrams/plot6/testscript/log`  |
| Process script path  | `nvpc_diagrams/plot6/testscript/process.py`  |
| Plot script path  | `nvpc_diagrams/plot6/draw/plot6.py`  |
| Description  |  The scripts in the "normal" directory are used to test scenarios where the dataset is smaller than DRAM, while the scripts in the "large" directory are used to test scenarios where the dataset is larger than DRAM. Set `fs` according to `mountfs` for normal test. Be sure to set the right DRAM and NVM size before this test.  |