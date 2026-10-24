#!/usr/bin/env python3
"""Prepare a separate merged launchd plist with CoreAnimation IOMFB vsync enabled."""
import argparse
import copy
import hashlib
import json
import plistlib
from pathlib import Path

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
matching = [job for job in jobs.values() if job.get('Label') == 'com.apple.backboardd']
if len(matching) != 1:
    raise SystemExit('Expected exactly one BackBoard job')
job = matching[0]
env = job.setdefault('EnvironmentVariables', {})
if not isinstance(env, dict):
    raise SystemExit('Invalid BackBoard environment')
previous = env.get('CA_FORCE_TIMER')
env['CA_FORCE_TIMER'] = '1'
# Do not change other jobs, including the disabled wallpaper registrar.
for key, old_job in before['LaunchDaemons'].items():
    if old_job.get('Label') != 'com.apple.backboardd' and config['LaunchDaemons'][key] != old_job:
        raise SystemExit('Unrelated launch job changed')
output = plistlib.dumps(config, fmt=plistlib.FMT_BINARY)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(output)
print(json.dumps({'input_sha256': hashlib.sha256(original).hexdigest(),
                  'output_sha256': hashlib.sha256(output).hexdigest(),
                  'previous_CA_FORCE_TIMER': previous, 'CA_FORCE_TIMER': '1',
                  'phone_written': False}))
