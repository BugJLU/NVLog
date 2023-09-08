#!/usr/bin/env python3

import os

IO_ENGINE = "sync" # "libaio" # "sync" # "io_uring"

TEST_DEV_PATH = ["/dev/libnvpc"]
RUNNING_TIME = ["30s"]


DIRECT_IO = [0]
BLOCK_SIZE = ["4k", "1M"]
ALL_SIZE = ["4G"]
RW = ["read", "write", "randread", "randwrite"]
JOBS = [1]
THREAD = [1, 8]
IO_DEPTH = [1, 16]


def FIO(
    testfilepath, size, directio, rw, block_size, jobs, io_depth, runtime, threads, ioengine
) -> str:
    name = "FIO_Test_" + str(testfilepath.replace("/", "@"))+"_"+str(size)+"_"+("directio" if directio == 1 else "")+"_"+str(rw)+"_"+str(block_size)+"_"+str(jobs)+"_"+str(io_depth)+"_"+str(runtime)+"_"+str(threads)

    sh = "sudo fio"
    sh += " --filename=" + str(testfilepath)
    sh += " --ioengine=" + str(ioengine)
    sh += " --size=" + str(size)
    sh += " --direct=" + str(directio)
    sh += " --rw=" + str(rw)
    sh += " --bs=" + str(block_size)
    sh += " --numjobs=" + str(jobs)
    sh += " --iodepth=" + str(io_depth)
    sh += " --runtime=" + str(runtime)
    sh += " --name=" + str(name)
    sh += " --thread=" + str(threads)
    sh += " --time_based --norandommap --group_reporting"
    sh += " --output=" + "./" + str(name) + ".log"
    return sh

if __name__ == '__main__':
    for _path in TEST_DEV_PATH:
        for _rt in RUNNING_TIME:
            for _bs in BLOCK_SIZE:
                for _as in ALL_SIZE:
                    for _rw in RW:
                        for _jobs in JOBS:
                            for _iodep in IO_DEPTH:
                                for _dio in DIRECT_IO:
                                    for _th in THREAD:
                                        cmd = FIO(_path, _as, _dio, _rw, _bs, _jobs, _iodep, _rt, _th, IO_ENGINE)
                                        print("[NOTICE] Now starting test: " + cmd)
                                        os.system(cmd)


