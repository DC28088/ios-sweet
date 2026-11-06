#!/usr/bin/env python3
"""Regression vectors for the generated-code-only IORPC compatibility adapter."""
import importlib.util
from pathlib import Path

path = Path(__file__).with_name("sweet-iig-compat.py")
spec = importlib.util.spec_from_file_location("iig_compat", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
old = "IORPCMessage * msg = IORPCMessageFromMach(rpc.message, false);\n"
assert module.normalize("IORPCMessage * msg = rpc.kernelContent;\n") == old
old_init = "IORPC rpc = { .message = &buf.msg.mach, .replySize = sizeof(buf.rpl) };\n"
new_init = old_init.replace("sizeof(buf.rpl)", "sizeof(buf.rpl), .kernelContent = &buf.msg.content.__hdr")
assert module.normalize(new_init) == old_init
assert module.normalize(old + old_init) == old + old_init
assert module.normalize(module.normalize(new_init)) == old_init
assert module.normalize("IORPCMessageFromMach(rpc.reply, true);") == "IORPCMessageFromMach(rpc.reply, true);"
try:
    module.normalize("IORPC rpc = {.kernelContent = unknown()};")
except ValueError:
    pass
else:
    raise AssertionError("Unknown generated ABI usage must fail closed")
print("PASS: generated IORPC compatibility, old output identity, idempotence and unknown-field rejection")
