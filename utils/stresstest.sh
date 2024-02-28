#!/bin/bash

echo testing write...
dd if=/dev/zero of=./testfile bs=1M count=16384

echo testing read...
dd if=./testfile of=/dev/null bs=1M count=16384

echo test done