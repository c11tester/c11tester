#g++ -o test.o test.cc -Wall -g -O3 -I.. -I../include -L.. -lmodel

export LD_LIBRARY_PATH=/scratch/random-fuzzer

if [ "$2" != "" ]; then
  g++ -o "$1.o" $1 -Wall -g -O3 -I.. -I../include -L.. -lmodel
else
  gcc -o "$1.o" $1 -Wall -g -O3 -I.. -I../include -L.. -lmodel
fi
