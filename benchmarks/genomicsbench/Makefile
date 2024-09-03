CXX=g++
CC=gcc
ARCH=avx2
#VTUNE_HOME=/opt/intel/oneapi/vtune/2021.1.1
MKLROOT=/opt/intel/oneapi/mkl/2021.1.1
MKL_IOMP5_DIR=/opt/intel/oneapi/compiler/2021.1.2/linux/compiler/lib/intel64_lin
CUDA_LIB=/usr/local/cuda

.PHONY: clean

all:
	$(info Starting build..this may take a while..)
	cd tools/htslib && autoreconf -i && ./configure && $(MAKE)
	cd tools/bwa-mem2; $(MAKE) CXX=$(CXX) arch=$(ARCH) VTUNE_HOME=$(VTUNE_HOME)
	cd benchmarks/fmi; $(MAKE) CXX=$(CXX) arch=$(ARCH) VTUNE_HOME=$(VTUNE_HOME)
	cd tools/spoa; mkdir build; cd build; cmake -DCMAKE_BUILD_TYPE=Release ..; $(MAKE)
	cd benchmarks/poa; $(MAKE) CXX=$(CXX) arch=$(ARCH) VTUNE_HOME=$(VTUNE_HOME)

gpu:
	cd benchmarks/abea; $(MAKE) CUDA_LIB=$(CUDA_LIB)

clean:
	cd tools/bwa-mem2; $(MAKE) clean
	cd benchmarks/fmi; $(MAKE) clean
	cd benchmarks/poa; $(MAKE) clean
