import os
import argparse
import subprocess
import sys
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

sys.path.append(os.path.dirname(__file__))

from match_reg_trace_addr.parse_qemu_log import *

workload = Workload(
    sys.argv[1],
    sys.argv[2],
    in_compilation=True,
    use_real_data=True,
    dump_results=True,
)
