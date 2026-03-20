```bash
export VERILATOR_ROOT=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/verilator/;
export C_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/verilatorinclude:$C_INCLUDE_PATH;
export CPLUS_INCLUDE_PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/include:$CPLUS_INCLUDE_PATH;
export PATH=/data/ngiannopoulos/Phd/NVDLA/tools/verilator_4.040/bin:$PATH;

CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc -j21

CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc EXTRAS=util/systemc/systemc_within_gem5/systemc_tlm/ -j21

./build/ARM/gem5.opt configs/example/arm/se_systemC.py --embed-spm-size=1MiB --embed-spm-lat=2

/data/tpapavasileiou/tools/GEM5-NVDLA/nvdla/gem5-plus/
```
