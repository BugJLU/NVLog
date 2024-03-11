#!/bin/bash
./nvpcctl config "$(cat ./nvpc_init_default.conf)"
./ndctl/build/daxctl/daxctl reconfigure-device --mode=nvpc all --no-online