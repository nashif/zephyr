#!/usr/bin/env python3

import sys
import os
from pprint import pprint

ZEPHYR_BASE = os.getenv("ZEPHYR_BASE")
if not ZEPHYR_BASE:
    sys.exit("$ZEPHYR_BASE environment variable undefined")

sys.path.insert(0, os.path.join(ZEPHYR_BASE, "scripts", "dts"))
import edtlib

dts_path = "./sanity-out/frdm_k64f/samples/hello_world/sample.basic.helloworld/zephyr/frdm_k64f.dts.pre.tmp"

edt = edtlib.EDT(dts_path, [os.path.join(ZEPHYR_BASE, "dts", "bindings")], warn_reg_unit_address_mismatch=False)

for node in edt.nodes:
    print(node.bus)
