all: intrusive_msqueue_hp.o

CXX = clang++

#%.o : %.cc 
#	$(CXX) -c $@

INCLUDE = -I. -I /scratch/benchmarks/libcds

INCLUDE += -Xclang -load -Xclang /scratch/llvm/build/lib/libCDSPass.so -I. -I /scratch/benchmarks/libcds

CPPFLAGS = $(INCLUDE) -g -O1 -std=c++11

CDSDIR=/scratch/benchmarks/libcds/build-release/bin
CDSLIB=-lcds_d -lpthread

intrusive_msqueue_hp.o: intrusive_msqueue_hp.cc
	$(CXX) -o msqueue intrusive_msqueue_hp.cc $(CPPFLAGS) -L $(CDSDIR) $(CDSLIB)

clean:
	rm -f *.o
