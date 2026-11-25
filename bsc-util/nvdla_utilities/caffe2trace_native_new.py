import os
import argparse
import subprocess
import sys
import shutil
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

sys.path.append(os.path.dirname(__file__))
from match_reg_trace_addr.parse_qemu_log import *

NVDLA_VP_BIN_PATH = "/data/ngiannopoulos/Phd/NVDLA/test_git_framework/git_repo/vp"


def parse_args():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--model-name", help="name of the NN model", required=True
    )
    parser.add_argument(
        "--caffemodel", help="Path to the Caffe model file", required=True
    )
    parser.add_argument(
        "--prototxt", help="Path to the Caffe prototxt", required=True
    )
    parser.add_argument(
        "--nvdla-compiler",
        default="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/sw/prebuilt/x86-ubuntu/nvdla_compiler",
        help="Path to NVDLA compiler",
    )
    parser.add_argument(
        "--qemu-bin",
        default="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/git_repo/vp/build/bin/aarch64_toplevel",
        help="Path to qemu binary. By default it is pointing to the self-built qemu binary.",
    )
    parser.add_argument(
        "--qemu-lua",
        default="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/git_repo/vp/conf/aarch64_nvdla.lua",
        help="Path to configuration file of running qemu.",
    )
    parser.add_argument(
        "--out-dir",
        default="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/git_repo/vp/traces/tp-lenet-caffe/",
        help="directory to put the generated sc.log, register txn and mem traces",
    )
    parser.add_argument(
        "--image",
        default="/data/ngiannopoulos/Phd/NVDLA/test_git_framework/git_repo/vp/network/lenet_files/eight_invert.pgm",
        help="Path to the image for nvdla_runtime to perform inference.",
    )
    parser.add_argument(
        "--true-data",
        action="store_true",
        default=False,
        help="Whether to get true data from sc.log",
    )
    parser.add_argument(
        "--dump-results",
        action="store_true",
        default=False,
        help="Whether to dump results to check results in translated input.txn",
    )
    parser.add_argument(
        "--convert-only",
        action="store_true",
        default=False,
        help="Whether to assume the presence of sc.log and only do processing only",
    )

    args = parser.parse_args()
    return args


def check_dependence(options):
    assert os.path.exists(options.caffemodel)
    assert os.path.exists(options.prototxt)
    assert os.path.exists(options.nvdla_compiler)
    assert os.path.exists(options.qemu_bin)
    assert os.path.exists(options.qemu_lua)

    if options.true_data:
        assert options.image != "" and os.path.exists(options.image)

    if options.dump_results:
        assert options.true_data


def get_loadable(options):
    os.makedirs(options.out_dir, exist_ok=True)
    compiler_path, compiler_name = os.path.split(options.nvdla_compiler)
    assert os.path.exists(options.nvdla_compiler)
    compile_log_path = os.path.abspath(
        os.path.join(options.out_dir, "compile_log")
    )

    curr_lib_path = ""
    if "LD_LIBRARY_PATH" in os.environ.keys():
        curr_lib_path = os.environ["LD_LIBRARY_PATH"]
    os.environ[
        "LD_LIBRARY_PATH"
    ] = f"{curr_lib_path}:{compiler_path}:{NVDLA_VP_BIN_PATH}/systemc-2.3.0a/obj_dir/lib-linux64:{NVDLA_VP_BIN_PATH}/build/lib"

    cmd = (
        "cd "
        + compiler_path
        + " && ./"
        + compiler_name
        + " -o "
        + options.model_name
        + " --cprecision fp16 --configtarget nv_full --informat nchw --prototxt "
        + os.path.abspath(options.prototxt)
        + " --caffemodel "
        + os.path.abspath(options.caffemodel)
        + " > "
        + compile_log_path
        + " 2>&1 && mv fast-math.nvdla "
        + options.model_name
        + ".nvdla && mv "
        + options.model_name
        + ".nvdla "
        + NVDLA_VP_BIN_PATH
        + "/images/linux-4.13.3/ "
        + "&& rm -r "
        + options.model_name
        + "wisdom.dir "
        + "&& rm output.protobuf"
    )

    return_code = os.system(cmd)
    if return_code != 0:
        print("error in generating loadable file; check " + compile_log_path)
        sys.exit(1)


def set_environ_var():
    os.environ[
        "SC_LOG"
    ] = "outfile:sc.log;verbosity_level:sc_debug;csb_adaptor:enable;dbb_adaptor:enable;sram_adaptor:enable"
    os.environ["SC_SIGNAL_WRITE_CHECK"] = "DISABLE"


def run_qemu(options):
    image_str = ""
    if options.true_data:
        path_name, file_name = os.path.split(os.path.abspath(options.image))
        image_str = " --image /mnt/images/linux-4.13.3/" + file_name + " "
        os.system(
            "cp "
            + os.path.abspath(options.image)
            + " "
            + NVDLA_VP_BIN_PATH
            + "/images/linux-4.13.3/"
        )

    qemu_run_template = """#!/usr/bin/expect -f

# starts guest vm, run benchmarks, poweroff
set timeout -1

# Assign a variable to the log file
log_file %(path_to_vp_bin)s/qemu_log

# Start the guest VM
spawn %(bin)s -c %(lua)s

# Login process
expect "login: "
send "root\\r"

expect "Password: "
send "nvdla\\r"

expect "# "
send "mount -t 9p -o trans=virtio r /mnt && cd /mnt\\r"
expect "# "
send "insmod /mnt/images/linux-4.13.3/drm.ko && insmod /mnt/images/linux-4.13.3/opendla_1.ko\\r"
expect "# "
send "export LD_LIBRARY_PATH=/mnt/images/linux-4.13.3; /mnt/images/linux-4.13.3/nvdla_runtime --loadable /mnt/images/linux-4.13.3/%(loadable)s %(image)s\\r"

expect "# "
send "shutdown -h now\\r"
""" % {
        "path_to_vp_bin": NVDLA_VP_BIN_PATH,
        "bin": options.qemu_bin,
        "lua": options.qemu_lua,
        "loadable": options.model_name + ".nvdla",
        "image": image_str,
    }
    qemu_run_template.format(options.qemu_bin, options.qemu_lua)
    with open(
        NVDLA_VP_BIN_PATH + "/images/linux-4.13.3/qemu_run.exp",
        "w",
    ) as f:
        f.write(qemu_run_template)
    expect_binary = shutil.which("expect")
    if expect_binary is None:
        print(
            "error: expect is not installed; install it with: "
            "sudo apt-get install -y expect"
        )
        sys.exit(1)
    if os.path.exists(NVDLA_VP_BIN_PATH + "/qemu_log"):
        os.system(
            "mv "
            + NVDLA_VP_BIN_PATH
            + "/qemu_log "
            + NVDLA_VP_BIN_PATH
            + "/qemu_log_bkp"
        )
    qemu_proc = subprocess.run(
        [expect_binary, "images/linux-4.13.3/qemu_run.exp"],
        cwd=NVDLA_VP_BIN_PATH,
        env=os.environ.copy(),
    )
    if qemu_proc.returncode != 0:
        print("error: VP/expect failed with code " + str(qemu_proc.returncode))
        sys.exit(qemu_proc.returncode)


def parse_mixed_type_trace(rd_wr_trace_file):
    rd_address_x_coords = []
    rd_address_y_coords = []
    wr_address_x_coords = []
    wr_address_y_coords = []
    with open(rd_wr_trace_file) as fp:
        lines = fp.readlines()
    for i, line in enumerate(lines):
        words_in_line = line.split()
        if "r" in words_in_line[0]:
            rd_address_x_coords.append(i)
            rd_address_y_coords.append(int(words_in_line[1], 16))
        elif "w" in words_in_line[0]:
            wr_address_x_coords.append(i)
            wr_address_y_coords.append(int(words_in_line[1], 16))
        else:
            print("unknown line: %s" % line)
            exit(1)

    fig = plt.figure(figsize=(16, 9))
    ax = fig.gca()
    plt.scatter(
        rd_address_x_coords,
        rd_address_y_coords,
        s=1,
        c="red",
        label="read trace",
    )
    plt.scatter(
        wr_address_x_coords,
        wr_address_y_coords,
        s=1,
        c="blue",
        label="write_trace",
    )

    ax.yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda value, _position: "0x%08x" % int(value))
    )
    ax.legend()
    plt.tight_layout()
    fig.savefig(
        os.path.join(
            os.path.dirname(rd_wr_trace_file), rd_wr_trace_file + ".png"
        ),
        dpi=240,
    )


def process_log(options):
    if not options.convert_only:
        sc_log = os.path.join(NVDLA_VP_BIN_PATH, "sc.log")
        qemu_log = os.path.join(NVDLA_VP_BIN_PATH, "qemu_log")
        if not os.path.exists(sc_log) or not os.path.exists(qemu_log):
            print("error: VP did not generate sc.log and qemu_log")
            sys.exit(1)
        os.system(
            "cd "
            + NVDLA_VP_BIN_PATH
            + " && mv sc.log "
            + options.out_dir
        )
        os.system(
            "cd "
            + NVDLA_VP_BIN_PATH
            + " && mv qemu_log "
            + options.out_dir
        )

    workload = Workload(
        options.out_dir,
        in_compilation=True,
        use_real_data=options.true_data,
        dump_results=options.dump_results,
    )
    print(options.out_dir)
    assert os.path.exists(os.path.join(options.out_dir, "VP_mem_rd_wr"))
    parse_mixed_type_trace(os.path.join(options.out_dir, "VP_mem_rd_wr"))


def main():
    options = parse_args()

    if not options.convert_only:
        check_dependence(options)
        get_loadable(options)
        set_environ_var()
        run_qemu(options)

    process_log(options)


if __name__ == "__main__":
    main()
