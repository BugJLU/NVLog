# Artifact Evaluation

We provide our machine equipped with required hardwares for the artifact evaluation. For security concerns, the login info is provided elsewhere.

When logged in, the working directory should be automatically set to `~/nvpc/ae`. The script `ae.sh` contains all of our artifact evaluation procedures.

## Getting Started Instructions

To evaluate the NVLog implementation, the system must be booted with the specific kernel. Run:

```sh
sudo ./ae.sh reboot_to nvpc
```

It takes about 5min to reboot. Please wait for a while and re-login to the system.

After the reboot, you can first run the following commands:

```sh
sudo ./ae.sh init_mount_ext4
sudo ./ae.sh open_nvlog
```

This mounts Ext4 and enables NVLog on `/mnt/nvpcssd`. Run `nvpcctl usage` to check the NVM usage by NVLog at any time.

## Detailed Instructions

### Claim 1: "mixrws"

We have a test tool `../utils/test01_mixrws.c`. It performs mixed read/write/syncwrite operations on the target filesystem, with different R/W ratio and sync ratio. The outputted test result contains throughput and bandwidth.

The following experiments run `../utils/test01_mixrws.c` on different filesystem configurations (Ext4, Ext4+NVLog, NOVA, Ext4+SPFS) with different R/W ratio (0/1, 3/7, 5/5, 7/3). The sync ratio is fixed at 50%.

#### Ext4

In case of any bug, it is recommended to reboot now to ensure a clean kernel state. Run:

```sh
sudo ./ae.sh reboot_to nvpc
```

After reboot, run:

```sh
sudo ./ae.sh run_mixrws_ext4
```

This formats `/dev/nvme0n1` to Ext4, mounts it on `/mnt/nvpcssd` and performs mixrws tests on it. The result is written to `mixrws-ext4-{01,37,55,73}.log`.

#### Ext4+NVLog

Run:

```sh
sudo ./ae.sh run_mixrws_nvlog
```

This formats `/dev/nvme0n1` to Ext4, mounts it on `/mnt/nvpcssd`, opens NVLog on `/dev/pmem0` for the test directory and performs mixrws tests on it. The result is written to `mixrws-nvlog-{01,37,55,73}.log`.

#### NOVA

Reboot to the NOVA kernel:

```sh
sudo ./ae.sh reboot_to nova
```

After reboot, run:

```sh
sudo ./ae.sh run_mixrws_nova
```

This configures `/dev/pmem0` in fsdax mode, formats it to NOVA, mounts it on `/mnt/nvpcssd` and performs mixrws tests on it. The result is written to `mixrws-nova-{01,37,55,73}.log`.

#### Ext4+SPFS

Reboot to the SPFS kernel:

```sh
sudo ./ae.sh reboot_to spfs
```

After reboot, run:

```sh
sudo ./ae.sh run_mixrws_spfs
```

This formats `/dev/nvme0n1` to Ext4, mounts it on `/mnt/nvpcssd`, mounts SPFS with `/dev/pmem0` on the test directory and performs mixrws tests on it. The result is written to `mixrws-spfs-{01,37,55,73}.log`.

#### Results Summary

Run this command to summarize the results:

```sh
for i in ext4 spfs nova nvlog; do echo ::$i::; grep bw: mixrws-$i-{01,37,55,73}.log; done
```

The results should demonstrate that Ext4+NVLog has the highest bandwidth.

### Claim 2: "smallwrites"

The following experiments use FIO to perform 64B small writes on the test directory. The FIO job description file is `bench_smallwrites.fio`.

#### Ext4

Reboot to the NVPC kernel:

```sh
sudo ./ae.sh reboot_to nvpc
```

After reboot, run:

```sh
sudo ./ae.sh run_smallwrites_ext4
```

Like described above, this formats and mounts an Ext4 filesystem and performs FIO on it. The result is written to `smallwrites-ext4.log`.

#### Ext4+NVLog(normal)

Run:

```sh
sudo ./ae.sh run_smallwrites_nvlog_normal
```

This opens NVLog on the Ext4 test directory with default configurations. The result is written to `smallwrites-nvlog_normal.log`.

#### Ext4+NVLog(O_SYNC)

It is recommended to reboot now to ensure a clean kernel state. Run:

```sh
sudo ./ae.sh reboot_to nvpc
```

After reboot, run:

```sh
sudo ./ae.sh run_smallwrites_nvlog_osync
```

This mounts Ext4 with `-o sync`. The result is written to `smallwrites-nvlog_osync.log`.

#### Ext4+NVLog(ActiveSync)

It is recommended to reboot now to ensure a clean kernel state. Run:

```sh
sudo ./ae.sh reboot_to nvpc
```

After reboot, run:

```sh
sudo ./ae.sh run_smallwrites_nvlog_as
```

This enables the "ActiveSync" feature of NVPC with `nvpcctl activesync set 1`. The result is written to `smallwrites-nvlog_as.log`.

#### NOVA

Reboot to the NOVA kernel:

```sh
sudo ./ae.sh reboot_to nova
```

After reboot, run:

```sh
sudo ./ae.sh run_smallwrites_nova
```

The result is written to `smallwrites-nova.log`.

#### Ext4+SPFS

Reboot to the SPFS kernel:

```sh
sudo ./ae.sh reboot_to spfs
```

After reboot, run:

```sh
sudo ./ae.sh run_smallwrites_spfs
```

The result is written to `smallwrites-spfs.log`.

#### Results Summary

Run this command to summarize the results:

```sh
for i in ext4 nova spfs nvlog_normal nvlog_osync nvlog_as; do echo ::$i::; grep bw= smallwrites-$i.log; done
```

The results should demonstrate that Ext4+NVLog outperforms other filesystem configurations.

### Claim 3: "usage"

The following experiment performs a normal FIO benchmark (`bench_usage.fio`) that writes 80G data on Ext4+NVLog, while using `nvpcctl usage` to trace the NVM usage.

Reboot to the NVPC kernel:

```sh
sudo ./ae.sh reboot_to nvpc
```

After reboot, run:

```sh
sudo ./ae.sh run_usage
```

This firstly launches a subshell to run `nvpcctl usage` every 1s. Then 5s later, FIO starts. After FIO finishes, the usage tracer keeps tracing for 30s and then terminates. The usage log is written into `usage.log`.

Run this command to filter the usage tracing output:

```sh
grep "nvpcctl: used" usage.log
```

It should demonstrate:

- In the first 5s, the usage is 0;
- In the middle, the usage increases slowly, but far lower than 80G (20971520 pages);
- In the last 30s, the usage decreases until reaching a very low level.
