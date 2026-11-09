# Set 2: O3 CPU with NVDLA and checkpoints

Builds **on top of set 1**. Runs the NVDLA runtime and all set-1 checkpoints on the O3 CPU as well (`ArmO3CPU`, 2 threads). Minor stays the default and gives the same results as before.

## 1. Tools

`scripts/all_networks.sh` in this folder runs the test on all networks with `CPU_MODEL=o3`, and replaces the set-1 version. The other scripts are those of set 1.

## 2. What changes

1. **The NVDLA code of the CPU moves from `MinorCPU` to `BaseCPU`** (`src/cpu/base.{hh,cc}`), unchanged. Every CPU model now has it. After the move, Minor gives the same final tick, the same cycles and the same output on LeNet int8.
2. **`--cpu-model {minor,o3}`** in `se_RTL.py`, default `minor`. For O3 it sets enough physical registers for 2 threads.
3. **O3 fixes.** These are bugs of O3 with 2 threads, triggered by the second thread of the runtime (`clone()`, futex):

| File | Bug | Effect |
|---|---|---|
| `o3/fetch.cc` `roundRobin()` | Fetches from a thread that was never activated (PC 0) | The pipeline stalls from the start |
| `o3/fetch.cc` `roundRobin()` | A thread woken from a futex is not put back in the fetch list | Deadlock in `pthread_cond_wait` |
| `o3/commit.cc` `roundRobin()` | The same in the commit list | The `svc` stays at the ROB head forever |
| `o3/fetch.cc` `isDrained()` | Also counts threads that are not active | The checkpoint drain never finishes |
| `o3/cpu.cc` `removeThread()` | Flushing the shared time buffers drops the other thread's data | Assert in the decode skid buffer at the end of the run |
| `o3/rob.cc`, `o3/inst_queue.cc` `resetEntries()` | At startup the whole ROB is given to thread 0 | ROB-full assert (resnet34, alexnet) |

In addition, the skid buffer bound in `o3/decode.cc` now also counts the instructions fetch sends before it sees the decode block.

## 3. Build

The RTL model and the wrapper library are **not** rebuilt. Only gem5 is rebuilt, with the same command as in set 1:

```bash
export VERILATOR_ROOT=/data/ngiannopoulos/Phd/NVDLA/tools/verilator
NVDLA_SAVABLE=1 CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt \
    PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc \
    CCFLAGS_EXTRA="-DNVDLA_VERILATED" -j16
```

Ready binary with both sets: `task5_gem5_nvdla_checkpoint/sources/gem5/build/ARM/gem5.opt`, identical to `gem5.opt.savable_exact_pgo_o3`.

## 4. Usage

All set-1 flags and stages work as they are. You only add the CPU:

```bash
# se_RTL.py
gem5.opt ... se_RTL.py <same arguments> --cpu-model o3 [--checkpoint-each-layer ...]
gem5.opt ... se_RTL.py <same arguments> --cpu-model o3 --restore-from <outdir>/cpt.<tick>

# system_ckpt.sh
SE_EXTRA="--cpu-model o3" scripts/system_ckpt.sh baseline lenet_int8
LOADABLE=<.nvdla> IMAGE=<image> SE_EXTRA="--cpu-model o3" scripts/system_ckpt.sh layers <tag>
LOADABLE=<.nvdla> IMAGE=<image> SE_EXTRA="--cpu-model o3" scripts/system_ckpt.sh restore-layer <tag> <n>
# fp16 with O3
SE_EXTRA="--normalize 255 --cpu-model o3" ...

# the test on all networks
CPU_MODEL=o3 scripts/all_networks.sh run 12      # runs/<network>_<precision>_o3/
CPU_MODEL=o3 scripts/all_networks.sh report      # results/all_networks_o3.tsv
```

## 5. Rules

- **A checkpoint loads only with the same CPU model.** O3 and Minor save different state. The same arguments as in set 1 are also required.
- **O3 is a faster CPU,** so the NVDLA waits less between layers. On LeNet int8:
  - final tick 12.9e9 instead of 33.8e9 with Minor,
  - 2.8M NVDLA cycles instead of 6.8M,
  - 8 interrupts instead of 7, and one more CSB write, because the timing changes.

  The output is the same.

## 6. What has been tested

8 networks were tested (alexnet, densenet, googlenet, lenet5, resnet18, resnet34, squeezenet, vgg11), in fp16 and int8, with `nvdla_cache`, `--dlas 1` and SE. Each config ran:
- baseline,
- checkpoint after every layer,
- restore from the middle layer,
- restore from the last layer.

Results:
- **Output:** identical in all 4 runs, in 16/16 configs, and identical to Minor.
- **Cycle deviation:** of the run with checkpoints, up to 0.64% from the baseline (alexnet int8).
- **Host time:** lower than Minor on most networks, e.g. densenet int8 3.6 h instead of 8.1 h.

**Not tested:**
- `dma_spm`, `--dlas 2`, FS.
- O3 with a thread count other than 2.
- **Minor with the final binary:** all networks with checkpoints on Minor were tested before this set. After it, only the LeNet baseline was rerun.
