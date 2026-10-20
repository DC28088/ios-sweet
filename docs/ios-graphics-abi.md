# iOS 16 graphics interface investigation

Status: read-only investigation of the locally prepared iPhone X iOS 16.7.16
20H392 root. No graphics service, renderer, userclient or root modification
has been implemented by this investigation.

## Cache contents located

The root's dyld cache indexes 2710 images across 44 cache-format files. The
actual AGXMetalA11 executable is cached at its normal bundle path, although
no standalone executable is present at that path in the HFS catalog. Cached
images also include IOAccelerator, IOSurface, Metal, OpenGLES/GLEngine, AGX
compiler components and libGPUSupportMercury. IOAccelerator exposes 156
exported names. These observations supersede the earlier unresolved
standalone-executable lookup; they do not establish an Adreno renderer.

Local scripts and extracted metadata remain under ignored
`research/gpu/ios-abi/`. They use the project's read-only HFS fork reader,
Apple's public dyld cache header, bounded cache/Mach-O/export-trie parsing,
and the already installed Capstone disassembler. Only selected headers,
export metadata and small function ranges were read; the cache was not
unpacked in full. The root was never mounted writable.

## Observed IOAccelerator client contract

Shared-cache ARM64 branch stubs were resolved to IOKit exports before
interpreting call arguments. Addresses below are unslid image-relative
function offsets for this exact firmware, not portable ABI constants.

| Entry point | Offset | Observed kernel call |
| --- | --- | --- |
| IOAccelDeviceCreateWithAPIProperty | 0x140c | IOServiceOpen type 1; later IOConnectCallStructMethod selector 2, zero input, 0x258-byte information output |
| IOAccelGLContextCreate | 0x3774 | Passes type 0 to IOAccelContextCreate |
| IOAccelCLContextCreate | 0x4c88 | Passes type 3 to IOAccelContextCreate |
| IOAccelContextCreate | 0x24a0 | Passes its caller-selected type to IOServiceOpen |
| IOAccelCommandQueueCreateWithQoS | 0x3238 | IOServiceOpen type 4 |
| IOAccelContextSubmitDataBuffersExt2 | 0x2710 | IOConnectCallStructMethod selector 2, 0x88-byte input, variable output |

The submit output allocation is calculated from device/context fields; it
must not be replaced with a fixed guessed size. Selector numbers are scoped
to userclient types. The complete information structure, resource handles,
shared-memory formats, synchronization and command encoding have not been
decoded or implemented. The currently probed IOAcceleratorES service is absent.

## Implementation requirement

A usable integration must implement actual resource and context lifetimes,
IOSurface sharing, submission validation, rendering, synchronization and a
compatible userspace graphics backend. The observed Apple command and shader
formats cannot be submitted directly to the private Adreno PM4 ring. Merely
registering the expected name or returning successful selector stubs would
not supply this functionality. Kernel SQE/PAS bring-up remains a prerequisite,
and no native iOS graphics performance claim is supported yet.

## Public format reference

[Apple dyld cache format](https://github.com/apple-oss-distributions/dyld/blob/main/include/mach-o/dyld_cache_format.h)
is used to locate cached images and their mappings. All Apple firmware bytes
and disassembly remain local and excluded from Git.
