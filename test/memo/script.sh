#!/bin/sh

clang -Xclang -load -Xclang /scratch/llvm/build/lib/libSkeletonPass.so -c -I/scratch/random-fuzzer/include/  /scratch/random-fuzzer/test/userprog.c

gcc -o userprog userprog.o -L/scratch/random-fuzzer -lmodel

#clang -Xclang -load -Xclang /scratch/llvm/build/lib/libSkeletonPass.so -c -I/scratch/random-fuzzer/include/  /scratch/random-fuzzer/test/double-read-fv.c

#gcc -o double-read-fv double-read-fv.o -L/scratch/random-fuzzer -lmodel

#clang -Xclang -load -Xclang /scratch/llvm/build/lib/libSkeletonPass.so -c -I/scratch/random-fuzzer/include/  /scratch/random-fuzzer/test/rmw2prog.c

#gcc -o rmw2prog rmw2prog.o -L/scratch/random-fuzzer -lmodel

#clang -Xclang -load -Xclang /scratch/llvm/build/lib/libSkeletonPass.so -c -I/scratch/random-fuzzer/include/  /scratch/random-fuzzer/test/fences.c

#gcc -o fences fences.o -L/scratch/random-fuzzer -lmodel

export LD_LIBRARY_PATH=/scratch/random-fuzzer
