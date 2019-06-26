#!/bin/sh

clang++ -o msqueue2 -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so intrusive_msqueue_hp.cc -I. -I /scratch/benchmarks/libcds -g -O1 -std=c++11 -L /scratch/new-fuzzer -lmodel -L /scratch/benchmarks/libcds/libcds-llvm/bin -lcds_d -lpthread

export LD_LIBRARY_PATH=/scratch/benchmarks/libcds/libcds-llvm/bin:/scratch/new-fuzzer/
