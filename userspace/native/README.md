# userspace/native - the native Adreno driver

This directory is the beginning of a Metal driver that talks to the Adreno
without Vulkan and without Turnip. The plan, the evidence and the stage list
are in [`docs/native-adreno-driver.md`](../../docs/native-adreno-driver.md).
This file is the practical part: what is here today and how to run it.

## What is here (N0)

| file | what it is |
| --- | --- |
| `include/sweet_gpu.h` | the device API: open, query, BO alloc/import/map/free, CPU map, PM4 submit, fence wait, timestamp |
| `src/sweet_gpu.cpp` | that API over the kernel's NativeAdreno ABI 1, through `IOAcceleratorES` and the `NTNL` tunnel |
| `src/sweet_pm4.h` | checked PM4 type 4 / type 7 emission (CP_NOP, CP_MEM_WRITE, CP_EVENT_WRITE, CP_WAIT_FOR_IDLE) |
| `src/sweet_native_probe.cpp` | the N0 probe: runs inside backboardd, has the GPU write two words, waits on the CP fence, reads them back |
| `test/pm4_test.cpp` | host test for the packet builder; no device needed |

The probe is compiled into `SweetMetal2.bundle` (see
`tools/build-sweet-metal2.py`). It runs from a constructor when asked, before
the Metal device opens Turnip, and closes the user client before returning, so
the normal stack is unaffected.

## Host test

    g++ -std=c++17 -Wall -Wextra -I userspace/native/src \
        userspace/native/test/pm4_test.cpp -o /tmp/sweet-pm4-test
    /tmp/sweet-pm4-test

The macOS CI runs the same command before building the bundle.

## Running the probe on the phone

The kernel admits only a process named `backboardd` to the user client, so
the probe runs inside the Metal bundle, not as a standalone binary.

1. Build the bundle (CI `sweet-metal2`, or a local macOS build):

       python3 tools/build-sweet-metal2.py

2. On the phone, prepare the one-shot flag and the log (root shell; these are
   the only writes, both under `/var/local`):

       echo 1 > /var/local/mtl2-native-probe      # or SWEET_NATIVE_PROBE=1 in /var/local/mtl2-env
       : > /var/local/mtl2-native-probe.log
       chmod 0666 /var/local/mtl2-native-probe.log

3. Install the bundle and restart backboardd with the usual tools:

       tools/sweet-install-metal2.sh <bundle> <backup-dir>

4. Read the result:

       grep 'SWEET NATIVE PROBE' /var/local/mtl2-native-probe.log

   and remove `/var/local/mtl2-native-probe` (or the env line) so the probe
   does not run again.

A passing probe looks like:

    probe: abi=1 input_bits=32 output_bits=36 flags=0x1 max_bo=268435456
    probe: chip=0x... va=0x... bank=... ubwc=...
    probe: submitted 8 words, fence=0x...
    probe: waited, timestamp ... -> ..., got 0x600df00d 0x12345678
    probe: PASS gpu wrote and fence completed

## What this proves, and what it does not

It proves the control plane a native driver stands on: the user client opens
inside backboardd, buffers are allocated and mapped at client IOVAs, plain PM4
is submitted, the kernel's CP fence completes, and the GPU's writes are
visible to the CPU. It does **not** touch render state, shaders or
presentation; those are N1-N4 in the plan.
