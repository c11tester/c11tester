#!/bin/sh

# C test cases
# clang -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so -c -I/scratch/random-fuzzer/include/  /scratch/random-fuzzer/test/userprog.c

# gcc -o userprog userprog.o -L/scratch/random-fuzzer -lmodel


DIR=/scratch/random-fuzzer/pthread_test/CDSPass/dummy

export LD_LIBRARY_PATH=/scratch/random-fuzzer

# compile with CDSPass 
if [ "$2" != "" ]; then # C++
  clang++ -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so -g -o $DIR/$1.o -I /scratch/random-fuzzer/include -L/scratch/random-fuzzer -lmodel $1
else # C
  clang -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so -o $DIR/$1.o -I/scratch/random-fuzzer/include/ -L/scratch/random-fuzzer -lmodel $1
fi
