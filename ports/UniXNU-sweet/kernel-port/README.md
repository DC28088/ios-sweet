# Darwin22 source adaptation

UniXNU at `89801d6e9265ba9aa49e56de642565288d679f83` applies 51 XNU patches
sorted from `patches/xnu`. The selected Apple source is XNU `8796.141.3`
(`1b191cb5`, Darwin 22.6.0), while upstream targets XNU `10063`/Darwin 23.

`progress.json` records the full audit. Every selected patch applies
cumulatively to a pristine source tree:

- 44 apply: 40 unchanged upstream, 4 with replacement patches here.
- 7 are deliberately deferred; `skip.json` records why for each.
- `tools/unixnu-kernel-port.py` reproduces the check. It never compiles code.

The four replacements are context/layout adaptations, not behavior changes:

| Patch | Darwin22 difference |
| --- | --- |
| `apple-only-cpu-tunables.patch` | guards the actual iOS16 `start.s` MIDR/tunables block |
| `external-archive-dependencies.patch` | targets `.unstripped`, the real Darwin22 link product (not `.noctf`) |
| `native-aes-personality.patch` | inserts the personalities after the shorter iOS16 `KernelConfigTables.cpp` table |
| `source-machine-layer.patch` | adds `CONFIG_APPLE_AMCC`, the missing `nos_arm_asm/nos_arm_pmap` bits, and gates the absent AMCC sources |

The device's `../patches/xnu` is the staged queue that UniXNU assembles as
`tree/patches/xnu`; it already contains the replacements and omits the deferred
patches. `../scripts/build-xnu-4k-baseline.sh` is the pinned upstream build
script with the deployment floor changed from iOS 17.0 to 16.0.

Applying a patch is not a compile, a link, a prelink or a hardware result. The
first macOS kernel build is in `.github/workflows/unixnu-kernel.yml`; prelink
against the 20H392 kernelcache is intentionally not part of that first run.
