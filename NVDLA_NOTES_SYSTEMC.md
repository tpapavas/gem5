```bash
export VERILATOR_ROOT=/data/ngiannopoulos/Phd/NVDLA/tools/verilator/;
export C_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator/verilatorinclude:$C_INCLUDE_PATH;
export CPLUS_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator/include:$CPLUS_INCLUDE_PATH;
export PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator/bin:$PATH;

CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc NVDLA_CONFIG=nv_small  -j21

./build/ARM/gem5.opt configs/example/arm/se_systemC.py --embed-spm-size=1MiB --embed-spm-lat=2
