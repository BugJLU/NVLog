#! /usr/bin/bash

#set -x

dir=$(dirname "$0")

dev_nvme=/dev/nvme0n1
dev_pmem=/dev/pmem0
mnt_dir=/mnt/nvpcssd

db_bench_bin=/home/shh/git_clone/rocksdb-main/db_bench

grub_entry_nova='gnulinux-advanced-6ecf2da2-92b6-411d-9146-f77102984628>gnulinux-5.1.0+-advanced-6ecf2da2-92b6-411d-9146-f77102984628'
grub_entry_nvpc='gnulinux-advanced-6ecf2da2-92b6-411d-9146-f77102984628>gnulinux-5.15.125-advanced-6ecf2da2-92b6-411d-9146-f77102984628'
grub_entry_spfs='gnulinux-advanced-6ecf2da2-92b6-411d-9146-f77102984628>gnulinux-5.1.0-spfs+-advanced-6ecf2da2-92b6-411d-9146-f77102984628'

# Reboot to the kernel that contains the corresponding implementations.
reboot_to() {
	eval entry="\$grub_entry_$1"
	echo "Setting GRUB next boot entry to '$entry'..."
	grub-reboot "$entry"
	echo "Rebooting. It takes about 5min to reboot. Please wait for a while and re-login."
	reboot
}


# Ensure the PMEM is working in $1 mode.
init_pmem() {
	echo "Resetting PMEM to $1 mode..."
	ndctl disable-namespace all
	ndctl destroy-namespace all
	ndctl create-namespace -t pmem -m "$1"
	echo "... Finished."
}

# Initialize EXT4 on an NVME SSD and mount on our testing directory.
init_mount_ext4() {
	echo "Creating EXT4 on '$dev_nvme' and mounting on '$mnt_dir' with options '$@'..."
	yes | mkfs.ext4 "$dev_nvme"
	mount -t ext4 "$dev_nvme" "$mnt_dir" "$@"
	echo "... Finished."
}

# Enable NVLog (our work) on the testing directory.
open_nvlog() {
	echo "Opening NVLog on directory '$mnt_dir'..."
	init_pmem devdax
	cfg=$(cat "$dir/nvpc_init_full_norebuild.conf")
	echo "NVPC config:"
	echo "$cfg"
	nvpcctl config "$cfg"
	/home/shh/git_clone/nvpc/utils/ndctl/build/daxctl/daxctl reconfigure-device --mode=nvpc all --no-online
	nvpcctl open "$mnt_dir"
	echo "... Finished."
}

# Set NVLog ActiveSync mode; 1 - enable, 0 - disable.
set_nvlog_activesync() {
	nvpcctl activesync set "$1"
	echo "NVPC activesync status set to '$1'."
}

# Initialize NOVA on the PMEM and mount on our testing directory.
init_mount_nova() {
	echo "Creating NOVA on '$dev_pmem' and mounting on '$mnt_dir'..."
	modprobe nova
	init_pmem fsdax
	mount -t NOVA -o init,data_cow "$dev_pmem" "$mnt_dir"
	echo "... Finished."
}

# Initialize SPFS on the testing directory.
init_mount_spfs() {
	echo "Mounting SPFS with '$dev_pmem' on '$mnt_dir'..."
	init_pmem fsdax
	mount -t spfs -o "pmem=$dev_pmem,format" "$mnt_dir" "$mnt_dir"
	echo "... Finished."
}

# Unmount any FS on the testing directory.
umount_fs() {
	umount "$mnt_dir"
	echo "Unmounted '$mnt_dir'."
}


# Benchmark for Claim 1.
bench_mixrws() {
	logname="$1"
	sync_ratio=5
	gcc "$dir/../utils/test01_mixrws.c" -o "$dir/test01_mixrws"
	for rw_ratio in 01 37 55 73 ; do
		echo "Running test01_mixrws.c on '$mnt_dir' with sync ratio '$sync_ratio' and R/W ratio '$rw_ratio'..."
		"$dir/test01_mixrws" "$mnt_dir" "$rw_ratio" "$sync_ratio" | tee "mixrws-$logname-$rw_ratio.log"
		echo "... Finished. Output is 'mixrws-$logname-$rw_ratio.log'."
	done
}

# Benchmark for Claim 1 on EXT4.
run_mixrws_ext4() {
	init_mount_ext4
	bench_mixrws "ext4"
	umount_fs
}

# Benchmark for Claim 1 on NVLog.
run_mixrws_nvlog() {
	init_mount_ext4
	open_nvlog
	bench_mixrws "nvlog"
	umount_fs
}

# Benchmark for Claim 1 on NOVA.
run_mixrws_nova() {
	init_mount_nova
	bench_mixrws "nova"
	umount_fs
}

# Benchmark for Claim 1 on SPFS.
run_mixrws_spfs() {
	init_mount_ext4
	init_mount_spfs
	bench_mixrws "spfs"
	umount_fs
	umount_fs
}


# Benchmark for Claim 2.
bench_smallwrites() {
	logname="$1"
	fio_config_file="$dir/bench_smallwrites.fio"
	echo "FIO config: '$fio_config_file'"
	cat "$fio_config_file"
	echo "Running FIO..."
	fio "$fio_config_file" | tee "smallwrites-$logname.log"
	echo "... Finished. Output is 'smallwrites-$logname.log'."
}

# Benchmark for Claim 2 on EXT4.
run_smallwrites_ext4() {
	init_mount_ext4
	bench_smallwrites "ext4"
	umount_fs
}

# Benchmark for Claim 2 on NVLog(O_SYNC).
run_smallwrites_nvlog_osync() {
	init_mount_ext4 -o sync
	open_nvlog
	bench_smallwrites "nvlog_osync"
	umount_fs
}

# Benchmark for Claim 2 on NVLog(ActiveSync)
run_smallwrites_nvlog_as() {
	init_mount_ext4
	open_nvlog
	set_nvlog_activesync 1
	bench_smallwrites "nvlog_as"
	umount_fs
}

# Benchmark for Claim 2 on NVLog(normal).
run_smallwrites_nvlog_normal() {
	init_mount_ext4
	open_nvlog
	bench_smallwrites "nvlog_normal"
	umount_fs
}

# Benchmark for Claim 2 on NOVA.
run_smallwrites_nova() {
	init_mount_nova
	bench_smallwrites "nova"
	umount_fs
}

# Benchmark for Claim 2 on SPFS.
run_smallwrites_spfs() {
	init_mount_ext4
	init_mount_spfs
	bench_smallwrites "spfs"
	umount_fs
	umount_fs
}


# Benchmark for Claim 3.
bench_usage() {
	fio_config_file="$dir/bench_usage.fio"
	echo "FIO config: '$fio_config_file'"
	cat "$fio_config_file"
	rm usage.log
	bash -c 'while true; do sleep 1; nvpcctl usage >> usage.log; done' &
	watcher=$!
	echo "Started NVPC usage watcher [$watcher]."
	sleep 5
	echo "Running FIO..."
	fio "$fio_config_file"
	echo "... FIO finished."
	sleep 30
	kill $watcher
	echo "Watcher [$watcher] terminated. Output is 'usage.log'."
}

run_usage() {
	init_mount_ext4
	open_nvlog
	bench_usage
	umount_fs
}


run_on_nvpc() {
	init_mount_ext4
	bench_mixrws "ext4"
	bench_smallwrites "ext4"
	open_nvlog
	bench_mixrws "nvlog_normal"
	bench_smallwrites "nvlog_normal"
	set_nvlog_gc 1
	bench_usage
	set_nvlog_gc 0
}


"$@"

