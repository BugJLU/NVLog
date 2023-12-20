#!/bin/bash

source $(dirname "$0")/env.sh

sudo mount $VM_DIR/share.img $VM_DIR/swap/
