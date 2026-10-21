# Darwin22 source adaptation

This directory stages replacements for the current UniXNU kernel patches on
XNU8796.141.3. It is not wired into the generic kernel build: that build still
needs a complete replacement patch module and matching archive/prelink scripts.
The board readiness flag remains set.

The first upstream patch, apple-only-cpu-tunables, rejects the iOS16 start.s
context. Its replacement preserves the same non-Apple CPU guard using the
actual iOS16 source layout. The archive-dependency replacement targets
Darwin22's .unstripped link rule rather than Darwin23's .unstripped.noctf.
Source application was checked; no kernel was compiled or executed.
progress.json records the next unresolved patch. Do not
interpret a patch applying cleanly as a working4KiB kernel or driver.

Reference: UniXNU89801d6e9265ba9aa49e56de642565288d679f83 patches/xnu;
Apple XNU1b191cb58250d0705d8a51287127505aa4bc0789. The source patch retains
the Apple source's license and the upstream patch's attribution/provenance.
