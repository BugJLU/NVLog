export VM_DIR=$(dirname $(readlink -f "$0"))
export KERNEL_DIR=$VM_DIR/../linux-5.15.125
export UTILS_DIR=$VM_DIR/../utils