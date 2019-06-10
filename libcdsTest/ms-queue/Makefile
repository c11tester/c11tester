all: intrusive_msqueue_hp.o

#%.o : %.cc 
#	$(CXX) -c $@

CFLAGS=-I. -I /scratch/benchmarks/libcds -std=c++11

CDSDIR=/scratch/benchmarks/libcds/build-release/bin
CDSLIB=-lcds_d -lpthread

intrusive_msqueue_hp.o: intrusive_msqueue_hp.cc
	$(CXX) -o msqueue intrusive_msqueue_hp.cc $(CFLAGS) -L $(CDSDIR) $(CDSLIB)

clean:
	rm -f *.o
