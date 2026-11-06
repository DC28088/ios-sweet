#!/usr/bin/env python3
"""Adapt Xcode 26 generated IORPC access to the unchanged xnu-8796 ABI.

Only generated implementation text is adapted. The kernel's IORPC structure,
dispatch ABI and message layout stay identical to the working Xcode 16 build.
"""
import os
from pathlib import Path
import re
import subprocess
import sys


def normalize(source):
    source = re.sub(r"\brpc\.kernelContent\b",
                    "IORPCMessageFromMach(rpc.message, false)", source)
    source = re.sub(r",\s*\.kernelContent\s*=\s*&[A-Za-z_][A-Za-z0-9_]*\.msg\.content\.__hdr\b",
                    "", source)
    if re.search(r"\.kernelContent\b", source):
        raise ValueError("Unknown generated IORPC kernelContent use; refusing ABI guess")
    return source


def main(args):
    real = Path(os.environ["SWEET_REAL_IIG"]).resolve()
    if real == Path(__file__).resolve():
        raise ValueError("Recursive IIG wrapper")
    result = subprocess.run([str(real), *args])
    if result.returncode:
        return result.returncode
    if "--impl" in args:
        path = Path(args[args.index("--impl") + 1])
        original = path.read_text()
        adapted = normalize(original)
        if adapted != original:
            path.write_text(adapted)
            print("SWEET IIG: normalized generated IORPC access for xnu-8796", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
