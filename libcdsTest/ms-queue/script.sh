#!/bin/sh

clang++ -o msqueue2 -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so  intrusive_msqueue_hp.cc -I. -I /scratch/benchmarks/libcds -g -std=c++11 -L /scratch/random-fuzzer -lmodel -L /scratch/benchmarks/libcds/build-release/bin -lcds_d -lpthread
