#!/bin/bash

echo testing write...
dd if=/dev/random of=./testfile bs=1M count=32768

echo testing read...
dd if=./testfile of=./dev/null bs=1M count=32768

echo test done