# NVDLA chiplets over UCIe on RISC-V: build and run guide

This guide covers branch `nvdla-dev-uci-riscv`. It explains how to build the RISC-V gem5
binary with the Verilator NVDLA (nv_full) and the UCIe link model, and how to run a
multi-NVDLA chiplet system with `configs/example/riscv/se_RTL_chiplet.py`.

## 1. What is modelled

A RISC-V MinorCPU (SE mode) runs `nvdla_runtime`, which drives one or more Verilator
nv_full NVDLAs. With the UCIe options the system is split into chiplets. Every
die-to-die crossing goes through a `UCIeLink`, implemented in `src/dev/ucie/`.

**Reference topology: "onehop"** (`--ucie-dla --ucie-cpu --ucie-csb`, without `--ucie-io`).
Every chiplet has exactly one UCIe link to the I/O chiplet. Every UCIe link carries both
directions: a request side and a response side.

```
 CPU chiplet                                   I/O chiplet
 CPU - L1 - L2 - cpu_xbar --[ucie_cpu_link]--> ucie_fabric --+-- MemCtrl/DRAM
   | CSB (per NVDLA)                              ^           +-- iobridge
   +--[ucie_csb_links<i>]--+                      |
                           v                      | [ucie_dla_links<i>]
 DLA chiplet i:  NVDLA<i> cmd       NVDLA<i> dram/dma -> dla_xbars<i>
```

`ucie_fabric` is the crossbar inside the I/O chiplet. It is a `SystemXBar` with the same
latencies as the `membus` of the non-chiplet baseline. A DRAM access therefore crosses a
single link: a CPU access crosses `ucie_cpu_link`, and an NVDLA access crosses
`ucie_dla_links<i>`.

- `--ucie-dla`: puts a link between each DLA chiplet and the I/O chiplet.
- `--ucie-cpu`: puts a link between the CPU chiplet (L2) and the I/O chiplet.
- `--ucie-io`: adds a second hop. With it, `ucie_fabric` becomes a separate fabric, and
  `ucie_io_link` connects it to an `io_xbar` that holds MemCtrl and the iobridge. Every
  DRAM access then crosses two links.
- `--ucie-csb`: the CSB register path from the CPU to each NVDLA also crosses a UCIe
  link, and the CPU stalls for the round trip.
- `--l3` and `--l3-chiplet` add a shared L3. `--ucie-dram` puts the DRAM on its own
  chiplet. DLA traffic in the shared range bypasses the L3.
- The latency of each link comes from the UCIe Delay model:
  - `src/dev/ucie/Delay.cpp`, with a TorchScript latency model
    (`configs/example/ucie/model_traced.pt`);
  - the YAML file `configs/example/ucie/Config.yaml`, which sets the technology, wire
    and PHY parameters.

  `--ucie-link-mode` chooses how packets become flits:

  | Mode | Packing and timing |
  |---|---|
  | `packet` | One delay per packet |
  | `flit256_std` | 256 B flit, 236 B payload |
  | `flit68` | 68 B flit, 64 B payload |
  | `flit256_leandros` | `flit256_std` packing, flit timing from `get_delay()` of the Delay model (bytes in, `fdiWidth` 64 B) |

## 2. One-time setup

### 2.1 Toolchain and environment

Use the clang build recipe from the main README / `NVDLA_NOTES_SYSTEMC.md`, plus the
UCIe variables:

```bash
export VERILATOR_ROOT=<verilator install>            # e.g. .../tools/verilator
export C_INCLUDE_PATH=$VERILATOR_ROOT/include:$C_INCLUDE_PATH
export CPLUS_INCLUDE_PATH=$VERILATOR_ROOT/include:$CPLUS_INCLUDE_PATH
export PATH=$VERILATOR_ROOT/bin:$PATH

export TORCH_ROOT=$PWD/ext/ucie/torch                # set up in 2.2
export YAML_CPP_ROOT=$PWD/ext/ucie/yaml-cpp
export LD_LIBRARY_PATH=$TORCH_ROOT/lib:$LD_LIBRARY_PATH   # needed at run time too
```

You also need the RISC-V cross compiler `riscv64-linux-gnu-gcc` for `m5op.o`.

### 2.2 UCIe dependencies (LibTorch and yaml-cpp)

`src/dev/ucie/SConscript` links against LibTorch and a static yaml-cpp. Both are large,
so they are not in git. Install them under `ext/ucie/`:

```bash
# expects ../Raw/.venv (pip torch, Python 3.8) and ../Raw/yaml-cpp-install next to gem5/
./install_ucie_dependencies.sh
```

Symlinks to an existing install work as well. The script also adds
`ext/ucie/{torch,yaml-cpp}` to `.git/info/exclude`. Do the same by hand for
`ext/ucie/build/` and the Verilator output directories.

### 2.3 Verilated nv_full model

`ext/rtl/SConscript` links `ext/rtl/model_nvdla/verilator_nvdla_full/`, which must contain:

- `VNV_nvdla.h`
- `libVNV_nvdla__ALL.a`

This is the Verilator output of the nv_full RTL from the `hw` submodule. Copy it from an
existing build, or regenerate it with Verilator.

`NV_SMALL_EN` must stay commented out in `ext/rtl/model_nvdla/wrapper_nvdla.hh`, which is
the default on this branch.

## 3. Build

Run every step from the gem5 root.

```bash
# (a) UCIe delay library: ext/ucie/build/libucie_delay.a
mkdir -p ext/ucie/build
for f in Delay delay_bridge; do
  clang++ -std=c++17 -O2 -fPIC -D_GLIBCXX_USE_CXX11_ABI=0 -I src/dev/ucie \
    -I "$TORCH_ROOT/include" -I "$TORCH_ROOT/include/torch/csrc/api/include" \
    -I "$YAML_CPP_ROOT/include" -c src/dev/ucie/$f.cpp -o ext/ucie/build/$f.o
done
ar rcs ext/ucie/build/libucie_delay.a ext/ucie/build/Delay.o ext/ucie/build/delay_bridge.o

# (b) m5 ops for the RISC-V runtime
riscv64-linux-gnu-gcc util/m5/src/abi/riscv/m5op.S -Iinclude -march=rv64gc -mabi=lp64d \
    -fPIC -c -o include/gem5/m5op.o

# (c) NVDLA wrapper library: ext/rtl/model_nvdla/libVerilatorNVDLA.a
(cd ext/rtl/model_nvdla && make library_vcd OPT=1 MULTI_THREAD=0 -j8)

# (d) gem5
CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/RISCV/gem5.opt \
    PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc -j12
```

The Makefile of step (c) does not track header dependencies. After you change a header
in `ext/rtl/model_nvdla/`, `touch` the `.cc` files there (`csbMaster`, `axiResponder`,
`embeddedBuffer`, `wrapper_nvdla`) before you run `make` again.

## 4. Run

### 4.1 Reference run: onehop, 2 DLA chiplets, LeNet-5, batch 2

```bash
./build/RISCV/gem5.opt -e -r -d m5out/onehop2dla_lenet \
  configs/example/riscv/se_RTL_chiplet.py \
  --dlas 2 --batches 2 \
  --loadable <nn_suite>/batch_2/LeNet5/lenet5_nv_full_fp16_dla_only_batch_2.nvdla \
  --image    <nn_suite>/input_images/grayscale/eight_invert_batch_2.pgm \
  --runtime  <riscv nvdla_runtime with --batches support> \
  --embed-spm-size=4MiB --embed-spm-lat=2 \
  --l2-size 4MB \
  --ucie-dla --ucie-cpu --ucie-csb --ucie-link-mode flit256_leandros
```

What the run produces:

- `simerr` ends with `Test pass`.
- The network output is written to `output.dimg` in the current directory. Run gem5 from
  a directory you can write to.
- On this branch the run above gives `simTicks = 65732436000`, and top-1 = 8 for both
  batches.

### 4.2 Main options

| Option | Meaning |
|---|---|
| `--dlas N` | Number of NVDLAs (DLA chiplets) |
| `--batches B` | Batch size, passed to the runtime as `--batches B` when B > 1. The loadable must be built for that batch size |
| `--loadable`, `--image` (`--input-image`), `--normalize` | Model loadable, input image and normalization value for the runtime. `--normalize` defaults to 255 |
| `--runtime` | RISC-V `nvdla_runtime` binary |
| `--embed-spm-size`, `--embed-spm-lat`, `--embed-spm-assoc` | Embedded buffer (SPM) in the NVDLA wrapper, filled through DMA. This config always enables DMA (`dma_enable=True`), so `--dma-enable` is not needed |
| `--buffer-mode`, `--shared-spm` | How the SPM is used: `all` (all-in-one buffer, the default), `pft` (prefetch buffer only) or `pft-cut` (prefetch buffer that skips tensors too large to fit). `--shared-spm` makes one SPM shared by all NVDLAs |
| `--l1i-size`, `--l1d-size`, `--l1i-assoc`, `--l1d-assoc`, `--l2-size`, `--l2-assoc` | CPU caches. Defaults come from `caches.py`: 32kB L1 and 256kB L2 |
| `--l3`, `--l3-size`, `--l3-assoc`, `--l3-lat`, `--l3-chiplet` | Shared L3. With `--l3-chiplet` the L3 is a separate chiplet |
| `--ucie-dla`, `--ucie-cpu`, `--ucie-io`, `--ucie-csb`, `--ucie-dram` | Which crossings get a UCIe link |
| `--dla-xbar-only` | Control run: adds the DLA IOXBar without any UCIe link |
| `--ucie-link-mode` | `packet`, `flit256_std`, `flit68` or `flit256_leandros` |
| `--ucie-phy-model` | `leandros`, `spec` or `none`: the PHY latency added after a flit is accumulated |
| `--ucie-config`, `--ucie-{dla,io,cpu,csb,l3,dram}-config` | Delay YAML for all links, or for one kind of link. The default is `configs/example/ucie/Config.yaml` |
| `--ucie-queue-size`, `--ucie-header-bits`, `--ucie-lanes`, `--ucie-gtps`, `--ucie-retry-buffer` | Link parameters. Lanes and GT/s do not apply to `flit256_leandros` |
| `--ucie-ber`, `--ucie-ber-deterministic`, `--ucie-seed` | Bit errors on the link, which cause flit retries |
| `--no-sf l3bus,fabric` | Turns off the snoop filter on the listed crossbars |

`configs/example/ucie/test_ucie.py` and `test_ucie_tg.py` are small traffic-generator
tests of a single link.

## 5. Statistics (`stats.txt`)

- **UCIe links**, `system.ucie_<kind>_link[s<i>].{req,resp}.*`:
  - `pkts`, `bytes`, `bits`: traffic on the link;
  - `busyTicks`: time the link was busy;
  - `totLatency`, `totQueueing`, `totDelivery`: latency, split into queueing and delivery;
  - `retriesSent`, `blockedByPeer`, `maxQueueOcc`: retries, back-pressure and peak queue
    occupancy;
  - `latencyHist`: latency histogram.
- **NVDLA**, `system.nvdla<i>.*`: cycles, reads and writes, and `num_dma_rd`/`num_dma_wr`.
- **Embedded buffer**, `system.nvdla<i>.spm.*`:
  - `read_hits`, `read_misses`, `read_miss_merged`;
  - `write_hits`, `write_misses`, `write_bypass`;
  - `line_fills`, `fills_dropped`, `evictions`, `dirty_evictions`, `writeback_bytes`;
  - `flushes`, `flush_writeback_bytes`;
  - `prefetch_issued`, `prefetch_used`;
  - `readHitRate`, `writeHitRate`.

  The list is defined once, in `ext/rtl/model_nvdla/spmStats.hh`. With DMA and the SPM
  in all-in-one mode, the counters satisfy:
  `line_fills == num_dma_rd == (read_misses - read_miss_merged) + write_misses`.

## 6. Troubleshooting

- **`libtorch.so` not found at run time:** add `$TORCH_ROOT/lib` to `LD_LIBRARY_PATH`.
- **scons stops on `TORCH_ROOT` / `YAML_CPP_ROOT`:** export both variables before you
  build (see 2.1).
- **Link errors for `get_delay` / `ucie_delay_*`:** build `ext/ucie/build/libucie_delay.a`
  first (step 3a). Rebuild it whenever `Delay.cpp` or `delay_bridge.cpp` changes.
- **Undefined `m5_*` symbols in the runtime build:** run step 3b.
- **`Test pass` is missing:** compare `output.dimg` with the golden output. Also check
  that the loadable is nv_full FP16 and that `--batches` matches the loadable.
