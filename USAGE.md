# Set 1: checkpoint/restore for gem5 + NVDLA (Verilator) in SE mode, with MinorCPU

This set lets you checkpoint the whole system in SE mode (`se_RTL.py`, one NVDLA, **MinorCPU**):

- before the NVDLA starts,
- at any tick,
- at a given NVDLA cycle, so also inside a layer,
- after every layer, in a single run.

After a restore, `output.dimg` is bit-identical to a run without checkpoints. Cycle counts differ by about 1.4% per restore, because gem5 does not save the caches and they restart empty.

The hw submodule (`src/systemC_NVDLA/model_nvdla/hw`) holds the Verilog of the prebuilt model: PDP **without the PDP fix**, CMAC/SDP from upstream. Checkpoints are only valid for this RTL.

## 1. Tools

In `S1=/data/ngiannopoulos/Phd/tasks/task5_gem5_nvdla_checkpoint/release_steps/step1_checkpoint_minor`:

| File | What it is |
|---|---|
| `pgo/lenet_int8.profdata` | PGO profile, only for rebuilding the model from scratch |
| `scripts/system_ckpt.sh` | Ready-made flow: probe, create, at, every, at-cycle, layers, restore… |
| `scripts/all_networks.sh` | The test on all networks (baseline, layers, 2 restores per config) |
| `scripts/nvdla_timeline.sh` | Shows from a `simout` when the NVDLA is busy, to pick a cycle |
| `scripts/compare_runs.sh` | Compares runs: exit code, output SHA, ticks, cycles, CSB writes |

## 2. Build

You need three things: the savable model, the wrapper library and gem5. All commands run from the gem5 root.

```bash
export VERILATOR_ROOT=/data/ngiannopoulos/Phd/NVDLA/tools/verilator   # Verilator 4.040
```

### 2a. Savable model

**Fast path:** copy the ready model, already built with PGO:

```bash
T=/data/ngiannopoulos/Phd/tasks/task5_gem5_nvdla_checkpoint
mkdir -p ext/rtl/model_nvdla/verilator_nvdla_full_savable
cp $T/sources/gem5/ext/rtl/model_nvdla/verilator_nvdla_full_savable/{VNV_nvdla.h,libVNV_nvdla__ALL.a} \
   ext/rtl/model_nvdla/verilator_nvdla_full_savable/
```

**From scratch:** only if you change the Verilog or the profile. Needs a lot of RAM (>45 GB). Run inside hw:

```bash
./tools/bin/tmake -build vmod
cd verif/verilator && $VERILATOR_ROOT/bin/verilator --cc --exe -f verilator.f \
    --Mdir ../../outdir/nv_full/verilator_savable_pgo/ nvdla.cpp --compiler clang \
    --output-split 30000 --output-split-cfuncs 90000 --noassert -Wno-TIMESCALEMOD -O3 -UVL_DEBUG --savable
cd ../../outdir/nv_full/verilator_savable_pgo && make -j40 -f VNV_nvdla.mk VNV_nvdla__ALL.a \
    CC=clang CXX=clang++ VM_PARALLEL_BUILDS=1 OPT_SLOW=-O0 OPT_FAST=-O3 OPT_GLOBAL=-O3 \
    VM_USER_CFLAGS="-fPIC -DNDEBUG -O3 -Ofast -fprofile-use=$S1/pgo/lenet_int8.profdata -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date"
cp VNV_nvdla.h          <gem5>/ext/rtl/model_nvdla/verilator_nvdla_full_savable/
cp VNV_nvdla__ALL.a     <gem5>/ext/rtl/model_nvdla/verilator_nvdla_full_savable/libVNV_nvdla__ALL.a
```

Three things to watch:

- **`OPT_SLOW=-O0`:** with `--savable` the `__Slow.cpp` is 170 MB, and with `-O3` the compile never finishes.
- **Without PGO:** the model runs 2.2–2.8 times slower. To build a profile from other networks, see BACKGROUND, section «Ταχύτητα: γιατί χρειάζεται PGO».
- **Verilator `-O2`:** do not use it. It removes signals the wrapper reads, and the link fails.

### 2b. Wrapper library and gem5

```bash
cd ext/rtl/model_nvdla && make library_vcd SAVABLE=1 OPT=1 && cd -

NVDLA_SAVABLE=1 CC=clang CXX=clang++ /usr/bin/python3 /usr/bin/scons build/ARM/gem5.opt \
    PYTHON_CONFIG=/usr/bin/python3-config PROTOC=/usr/bin/protoc \
    CCFLAGS_EXTRA="-DNVDLA_VERILATED" -j16
```

Without `SAVABLE=1` / `NVDLA_SAVABLE=1` it builds with the old prebuilt model. Then only the checkpoint before the NVDLA starts works. A checkpoint with the NVDLA active exits with a clear message and does not write a broken checkpoint.

`VERILATOR_ROOT` must also be set for scons, otherwise you get `KeyError: 'VERILATOR_ROOT'`.

A ready set-1 binary already exists: `$T/sources/gem5/build/ARM/gem5.opt.savable_exact_pgo_layers`. The task's `gem5.opt` also includes set 2.

## 3. Usage with the `se_RTL.py` options

Append them to the command you already run, with the same `--loadable`, `--image`, `--dlas` and cache flags. Each checkpoint is written to `<outdir>/cpt.<tick>/`.

| I want | Options |
|---|---|
| Checkpoint at a tick and continue | `--checkpoint-at <tick>` |
| Checkpoint at a tick and stop | `--checkpoint-at <tick> --checkpoint-exit` |
| Periodic checkpoints, for runs that may get killed | `--checkpoint-every <ticks>` |
| Checkpoint after every layer (at its interrupt), in one run | `--checkpoint-each-layer`, plus `--checkpoint-at <tick>` if you also want one before the NVDLA starts |
| Checkpoint at an NVDLA cycle, e.g. inside a layer | `--checkpoint-at-nvdla-cycle <cycle>` (plus `--checkpoint-exit` to stop) |
| Restore | `--restore-from <outdir>/cpt.<tick>` |
| Find the tick of the first NVDLA access | `--exit-on-first-nvdla-access` |

The three checkpoint modes (`--checkpoint-at`, `--checkpoint-every`, `--checkpoint-at-nvdla-cycle`) cannot be combined with each other. `--checkpoint-each-layer` combines only with `--checkpoint-at`.

Example: checkpoint inside layer 5 (ip1) of LeNet, then restore.

```bash
G=<your gem5 with set 1>
ARGS="--loadable .../int8_onnc.nvdla --image .../vp_in_000.pgm --dlas 1 \
      --add-accel-private-cache --accel-pr-cache-size=1MiB --accel-pr-cache-assoc=16 \
      --accel-pr-cache-tag-lat=12 --accel-pr-cache-dat-lat=12 --accel-pr-cache-resp-lat=5"

$G/build/ARM/gem5.opt -e -r -d out_ckpt $G/configs/example/arm/se_RTL.py $ARGS \
    --checkpoint-at-nvdla-cycle 3300000 --checkpoint-exit
# simout: [NVDLA CHECKPOINT CYCLE] nvdla#0 cycle=3300000 tick=30331091000
#         Checkpoint written to .../out_ckpt/cpt.30331091000

$G/build/ARM/gem5.opt -e -r -d out_restore $G/configs/example/arm/se_RTL.py $ARGS \
    --restore-from out_ckpt/cpt.30331091000
```

**To resume a run that was killed:** if it ran with `--checkpoint-every`, `--restore-from` the last `cpt.*` in its outdir.

## 3b. Checkpoint before the NVDLA and after every layer

```bash
scripts/system_ckpt.sh layers        <tag>      # one run, writes runs/<tag>/layers/layers.txt
scripts/system_ckpt.sh restore-layer <tag> <n>  # restore and run to the end from layer n
```

The run does the `probe` itself if it does not exist yet. In `layers.txt` each line is one checkpoint:

```
layer cycle tick checkpoint
0 0 27023911000 .../layers/cpt.27023911000        # before the NVDLA starts
1 623468 27657315000 .../layers/cpt.27657315000   # after layer 1
...
```

- **Layer 0:** the checkpoint before the NVDLA, like `create`, with no RTL state.
- **Layer n:** written when the NVDLA raises the interrupt of the layer. By then its outputs are written. One interrupt corresponds to one HW layer, i.e. a group of fused ops (e.g. a CONV together with its SDP). LeNet has 7.
- **Without the script:** `--checkpoint-each-layer --checkpoint-at <tick before the first access>`. The simout prints lines `[NVDLA LAYER DONE] nvdla#0 layer=N cycle=C tick=T`.
- **Size:** about 17 MB per checkpoint, 8 for LeNet (136 MB).
- **Cost:** the run takes as long as without checkpoints (1127 s vs 1176 s with one checkpoint), because each checkpoint takes only seconds.

## 4. Picking a cycle for a layer

The NVDLA clock stops when it is idle, which is most of the time, while the CPU writes registers. So an NVDLA cycle does not map to a fixed tick, and with `--checkpoint-at <tick>` you will rarely land inside a layer. That is why `--checkpoint-at-nvdla-cycle` exists.

1. Run the network once without checkpoints, as a baseline.
2. Extract its timeline:
   ```bash
   $S1/scripts/nvdla_timeline.sh <baseline>/simout
   ```
   It prints:
   - **op enables:** which engine starts and at which cycle (CDMA for CONV, SDP, PDP, CDP),
   - **interrupts:** when a group of ops finishes,
   - **busy windows:** when the NVDLA reads or writes memory.
3. Pick a cycle **inside** a busy window. Layer `k` is usually the `k`-th busy window after the matching op enable.

   The ops → layer mapping is also in the `simout`, under `[dumpOneTask] Operations`, with the `=== OP i (CONV/SDP/PDP) ===` entries and their consumers. A CONV with its SDP is one layer, because they are fused.

Example, LeNet int8 with `nvdla_cache`:

| # | Layer | Busy (NVDLA cycles) |
|---|---|---|
| 1 | conv1 | 623,265 – 644,213 |
| 2 | pool1 | 1,893,412 – 1,895,852 |
| 3 | conv2 | 2,344,053 – 2,353,016 |
| 4 | pool2 | 2,830,620 – 2,831,261 |
| 5 | ip1 | 3,262,102 – 3,359,740 |
| 6 | relu1 | 3,547,344 – 3,547,518 |
| 7 | ip2 | 3,975,310 – 3,976,799 |

The cycles change with the network, the image, the cache flags and `--dma-enable`. After a restore they shift as well, because the caches start cold. For the same reason, always take the timeline from a run **from the start** with the same arguments.

The cycle is `wr->tickcount`, i.e. the number `(N)` at the start of the `simout` lines.

## 5. Checking that a restore is correct

```bash
$S1/scripts/compare_runs.sh <baseline_dir> <checkpoint_dir> <restore_dir>
```

- **`output_sha256`:** must be identical in all. `Test pass` is not enough; it only means the run finished.
- **`nvdla_cyc`, `final_tick`:** absolute, also after a restore. The deviation is about +1.4% per restore.
- **`csb_w` and the other stats:** after a restore they count only from the restore onward.

## 6. The ready script (`system_ckpt.sh`)

The gem5 paths are those of task5 (`TASK=` at the top of the script). `<tag>` names the folder `runs/<tag>`. `lenet_int8` is preconfigured. For any other input:

```bash
LOADABLE=<.nvdla> IMAGE=<image> system_ckpt.sh create <tag>         # e.g. tag = network_image
LOADABLE=<.nvdla> IMAGE=<image> system_ckpt.sh restore <tag>
SE_EXTRA="--normalize 255" LOADABLE=... IMAGE=... system_ckpt.sh ...  # extra arguments, e.g. fp16
```

**One checkpoint per loadable/image pair.** A checkpoint contains the process memory, with the loadable and the image already loaded, so two pairs cannot share one. Each pair gets its own `<tag>`. A restore needs the same `LOADABLE`, `IMAGE` and `SE_EXTRA`.

Tested with LeNet int8 and the image `vp_in_007.pgm`: restoring from its own checkpoint (`cpt.27013167000`) gave the same output as its baseline (`2e0e9690...`, different from that of image 000).

**The checkpoint before the NVDLA saves little:** the part up to the first NVDLA access takes about 35 s of host time out of the roughly 20 minutes of the run. The time savings come from checkpoints inside the inference (`every`, `at-cycle`).

```bash
system_ckpt.sh baseline      <tag>            # full run
system_ckpt.sh at            <tag> <tick>     # checkpoint at the tick, continues
system_ckpt.sh restore-at    <tag> <tick>     # restore from at_<tick>
system_ckpt.sh every         <tag> <period>   # checkpoint every <period> ticks
system_ckpt.sh layers        <tag>            # checkpoint before the NVDLA and after every layer
system_ckpt.sh restore-layer <tag> <n>        # restore from layer n (0 = before the NVDLA)
system_ckpt.sh at-cycle      <tag> <cycle>    # checkpoint at an NVDLA cycle, continues
system_ckpt.sh restore-cycle <tag> <cycle>    # restore from at_cycle_<cycle>
system_ckpt.sh probe|create|restore <tag>     # checkpoint before the NVDLA starts
```

- `GEM5=<absolute path>` runs another binary.
- `RUN_NAME=<name>` changes the run folder.
- `CPT=<dir>` picks another checkpoint for `restore-at` and `restore-cycle`.
- The script never reuses an existing folder.

## 7. Rules and limits

- **Same binary and same arguments:** a restore needs the same binary (same RTL model) and the same `--loadable`, `--image`, `--dlas`, `--dma-enable` and cache flags. A checkpoint from a binary with different Verilog, e.g. with the PDP fix, does not load correctly.
- **Size:** each checkpoint is about 17 MB. The RTL is 1.9 MB in `system.nvdla.nvdla.rtl` and the wrapper 66 KB in `.wrap`. Checkpoints are never deleted automatically.
- **Tested:** all 8 networks (alexnet, densenet, googlenet, lenet5, resnet18, resnet34, squeezenet, vgg11), in fp16 and int8, with `nvdla_cache`, `--dlas 1`, SE mode and **Minor CPU**. For each: `layers`, and restore from the middle and from the last layer. In all of them the output matched the baseline. Details in `../BACKGROUND.md`, section «Τεστ σε όλα τα δίκτυα». The whole test reruns with `scripts/all_networks.sh run`.
- **Not tested:** `dma_spm`, `--dlas 2`, FS and any CPU other than Minor (O3 is set 2). The SPM/DMA code exists but has not run. Before relying on it, do one checkpoint/restore with `compare_runs.sh`.
- **Size and timing on large networks:** each checkpoint holds the whole process memory, 18 MB for densenet int8 but 130–170 MB for resnet34 and alexnet fp16. `--checkpoint-each-layer` produces 347 checkpoints for densenet. The run with checkpoints has different cycle counts from the baseline, up to −7.9% (densenet int8), without changing the output.
- **Cycle trigger:** applies only to `nvdla#0`.
- **Trace mode and `use_fake_mem`:** not supported. The checkpoint exits with `fatal`.
