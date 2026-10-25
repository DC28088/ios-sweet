#!/usr/bin/env python3
"""Prepare a separate merged launchd plist in which bluetoothd uses the kernel's H4 transport.

bluetoothd picks its HCI transport from BT_HCI_TRANSPORT (TRANSPORT[:DRIVER[:DEVICE]]); with
H4BC:POSIX:com.apple.uart.bluetooth,3000000 it connects to the kernel control that
bsd/dev/sweet_bt_hci.c registers (sweetbt=1). Same setting as Bergamota's root_tweaks18
bluetooth(). BT_DEVICE_ADDRESS is dropped (the kernel set the BD_ADDR). Every other job, and
every other key of the bluetoothd job, must stay as it was.
"""
import argparse
import copy
import hashlib
import json
import plistlib
from pathlib import Path

LABEL = 'com.apple.bluetoothd'
ENV = {'BT_HCI_TRANSPORT': 'H4BC:POSIX:com.apple.uart.bluetooth,3000000'}
REMOVE = ('BT_DEVICE_ADDRESS',)

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('input', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
if a.output.exists() or a.input.resolve() == a.output.resolve():
    raise SystemExit('Output must be a new file; input remains unchanged')
original = a.input.read_bytes()
config = plistlib.loads(original)
before = copy.deepcopy(config)
jobs = config.get('LaunchDaemons')
if not isinstance(jobs, dict):
    raise SystemExit('Missing merged LaunchDaemons dictionary')
keys = [k for k, j in jobs.items() if isinstance(j, dict) and j.get('Label') == LABEL]
if len(keys) != 1:
    raise SystemExit('Expected exactly one %s job, found %d' % (LABEL, len(keys)))
job = jobs[keys[0]]
env_before = dict(job.get('EnvironmentVariables', {}))
env = job.setdefault('EnvironmentVariables', {})
env.update(ENV)
for k in REMOVE:
    env.pop(k, None)
for key, old_job in before['LaunchDaemons'].items():
    if key == keys[0]:
        new_job = dict(config['LaunchDaemons'][key])
        old = dict(old_job)
        new_job.pop('EnvironmentVariables', None)
        old.pop('EnvironmentVariables', None)
        if new_job != old:
            raise SystemExit('bluetoothd job changed beyond its environment')
    elif config['LaunchDaemons'][key] != old_job:
        raise SystemExit('Unrelated launch job changed')
if {k: v for k, v in config.items() if k != 'LaunchDaemons'} != {k: v for k, v in before.items() if k != 'LaunchDaemons'}:
    raise SystemExit('Non-daemon section changed')
output = plistlib.dumps(config, fmt=plistlib.FMT_BINARY)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(output)
print(json.dumps({'input_sha256': hashlib.sha256(original).hexdigest(),
                  'output_sha256': hashlib.sha256(output).hexdigest(),
                  'job': keys[0], 'environment_before': env_before, 'environment_after': env,
                  'phone_written': False}))
