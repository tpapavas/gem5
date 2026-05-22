SystemC SE run.

	export VERILATOR_ROOT=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/verilator/; 
	export C_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/verilatorinclude:$C_INCLUDE_PATH; 
	export CPLUS_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/include:$CPLUS_INCLUDE_PATH; 
	export PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/bin:$PATH; 

	CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc -j21 
	
	./build/ARM/gem5.opt configs/example/arm/se_RTL.py --embed-spm-size=1MiB --embed-spm-lat=2 > stdout 2> stderr
	
	./build/ARM/gem5.opt configs/example/arm/se_systemC.py --embed-spm-size=1MiB --embed-spm-lat=2

	./build/ARM/gem5.opt --debug-flag=NvDlaDevice --debug-file=NvDlaDevice configs/example/arm/se_systemC.py --embed-spm-size=1MiB --embed-spm-lat=2 --add-accel-private-cache

	/data/tpapavasileiou/tools/GEM5-NVDLA/nvdla/gem5-plus/
	
	make library_vcd OPT=1 MULTI_THREAD=0 -j12

	cd /data/ngiannopoulos/Phd/NVDLA/tools/sw-gem5/kumd/umd
	docker run -it --rm -v /data/ngiannopoulos/Phd/NVDLA/tools/:/data/ngiannopoulos/Phd/NVDLA/tools/ edwinlai99/advp:v1
	apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu -y
	cd /data/ngiannopoulos/Phd/NVDLA/tools/sw-gem5/kumd/umd
	export TOP=`pwd`
	make TOOLCHAIN_PREFIX=aarch64-linux-gnu- runtime -j21  or make TOOLCHAIN_PREFIX=aarch64-linux-gnu- compiler -j21
	exit
	sudo chown ngiannopoulos:ngiannopoulos out/apps/runtime/nvdla_runtime/nvdla_runtime
	cp out/apps/runtime/nvdla_runtime/nvdla_runtime ../../../../gem5/binary/runtime_systemC/nvdla_runtime


	beverly -> /data/ngiannopoulos/Phd/NVDLA/nvdla_hw_debug_git_repo


text from thodoris

Σου έβαλα στο Beverly το sw: /data/ngiannopoulos/Phd/NVDLA/tools/sw-gem5
 
1. Για να κάνεις compile το nvdla_runtime:
Shell
# Connect to Docker container
docker run -it --rm -v <home-path>:<home-path> edwinlai99/advp:v1
# VP has arm64 architecture, so we need cross compilers 
apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu -y
# in <path-to-nvdla-stuff>/sw/umd
cd <path-to-sw>/kumd/umd/
export TOP=`pwd`
make TOOLCHAIN_PREFIX=aarch64-linux-gnu- runtime -j<num-threads>
 
2. Για να κάνεις compile το m5ops, αν αλλάξεις κάτι:
Shell
# from inside <gem5-path>/util/m5/build/arm64
---
cd bsc-util/

aarch64-linux-gnu-g++-9 ../util/m5/src/abi/arm64/m5op.S -c -o m5op.o -I../include -fPIC -O3 --static -std=c++11
---

aarch64-linux-gnu-g++-9 ../../src/abi/arm64/m5op.S -c -o m5ops_plus.o -I../../../../include -fPIC -O3 --static -std=c++11
Νομίζω δεν έχει cross-compilers στο Beverly. Αυτό μπορείς να το κάνεις και στο raj.
 
Αν αλλάξεις τα m5ops, πρέπει να περάσεις τα .h του <gem5>/include στο sw μαζί και με το m5ops_plus.o και τα βάζεις όπως φαίνεται στην εικόνα.

/data/ngiannopoulos/Phd/NVDLA/tools/sw-gem5/kumd/umd/core/src/runtime/include/gem5
/data/ngiannopoulos/Phd/NVDLA/tools/sw-gem5/kumd/umd/core/src/runtime/include/gem5/asm/generic