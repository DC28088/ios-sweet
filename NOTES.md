# iOS 16 on sweet (Redmi Note 10 Pro, SM7150) - working notes

## Current direction and last known device state (2026-10-24, read first)

- **ACTIVE: 0119 regresses performance; 0120 fixes in preparation, phone left in iOS.**
  User says "home screen. Significantly worse." Completed240s live capture
  logs/live-0119-usb.txt is429837bytes. Replayed startup is the same boot.
  Scoped method17 submissions complete(result0), normally~2..5ms per call;
  observed GLfinish windows avg48.7..77.5ms,max375.8ms. These are not FPS.
  No remaining Mesa layout failure in this capture, but backboardd[56] later
  SIGSEGVs(far0x1c), immediately after BoMap method3 returns0xe00002be
  (NoResources), BO_NEW16384 fails, last logged imported BO1004.
  Shared VM limit1024 includes kernel allocations; client limit1024 likewise.
  This fits object exhaustion but old log has no count to exclude span pressure.
  Do not claim the exact exhausted limit is proven. 0119 rendering is a regression.
  Source f26a9f8; kernel40058371440/GLES40058371530, both23a1fc8 builds succeeded.
  Installed/readback-verified36chunks/20185088bytes; originals in
  research/gles-install-0119/delta. Temp image
  shim/out/shim-boot-0119-scoped-gles-120hz.img SHA256
  2646376c31502337a2fcac8b659d596d134a6aaae5e9c0b267fbefbdc61d13d5.
  Kernel xnu/out-0119/kernel.development.sweet SHA256
  554684a99a1489d8b9cff266a98bdfc670a5afa46ffdcc3a07e072bd84a5d282;
  library userspace/out/0119-gles-23a1fc8-range/sweetgl.dylib SHA256
  f21332e828b8da34130d46880c8f5e6d18d2b1d63c184383b74bef98ea0cea28.
  CPU fallback remains available, same installed library inactive with sweetgles=0:
  shim/out/shim-boot-0118-cpu-fallback-120hz.img SHA256
  84c745e436a3095da8eb4821d557c00c4970cac5f7ab1c1433ea1f34a49e9a1f.
  No phone reboot/write performed after user's regression report.
  0120 source separates live BO budget4096(+32 shared kernel reserve) from
  unchanged per-submit1024 residency limit, capability-gated to reject old kernels;
  logs actual VM objects/spans on pressure. Mesa retries allocation once after
  releasing unused caches and guards failed heap metadata/backing/maps.
  Reuses copy-upload texture storage on same-layout seed refresh. Removes
  inherited A530 forced-sysmem workaround: A6xx fd6_gmem set_blit_scissor rounds
  height4/width16, owned surfaces have patch0117 backing and imports keep Mesa
  size/pitch validation. IR3 nofp16 retained. GMEM selection is unqualified.
  Do not skip glFinish presentation waits: object completion queries currently
  also block, but their actual call frequency/ownership semantics remain unaudited.
  Core VM/UserClient/Submit C++ syntax compilation passed with IOKit type shims;
  frontend generation and all four Mesa patches apply to the pinned sources.
  Kernel patch applies to the post0119 source. No tests added/run.
  Pushed source380691712dca8a8c19854164ffffcbd96eb6e8fe; matching CI
  kernel40062657768 and GLES40062657825 running with run_tests=false.
  Additional60s live capture saved1061bytes to logs/live-0119-after-regression.txt;
  USB is alive, framebuffer callbacks continue, no new GPU/present evidence.
  User asked to open OrangeFox for fresh staging; answer pending.
  Fresh research/gles-install-0120 scripts/cache/initrd prepared; no old cached
  blocks, overlay or delta copied. Package helper research/gpu/package-0120.py.
  Next: finish CI/download/package matching0120 pair; fresh
  staging/explicit approval required for any new raw userdata installation.


- **ACTIVE: 0119 kernel and GLES builds succeeded; downloads/staging pending.**
  User asked to fix the GPU regression and provided BLUETOOTH-SUMMARY.md.
  Phone stays in the confirmed working CPU fallback; no new device writes.
  New patch0117-sweet-gles-residency-cache-and-surface-padding adds an optional
  ABI1 capability/method17 carrying the complete Mesa BO residency/access set.
  It validates ownership/access and IB membership, pins every listed BO until
  a real fence, publishes only that set, acquires only GPU-written BOs, and
  retains resources on uncertain completion. Legacy submit stays supported.
  Large requests are copied from IOKit's out-of-line descriptor before use.
  Owned, nonplanar IOSurfaces get backing for A6xx's final4-row block; caller
  memory is not extended. Userspace preserves READ/WRITE flags (DUMP is only
  diagnostic) and requires the new capability, so the pair must be installed
  together. Tight caller-owned sampled images use the existing upload path.
  CORRECTION to earlier timeout diagnosis: method6 timeout on a zero-duration
  poll is normal pending work, not proof of a GPU hang. New trace suppresses
  those poll messages. Long glFinish calls remain observed, but they include
  CPU submission/cache work, not just GPU execution. Entire-VM cache scans
  are confirmed in the old source, not timed independently by0118's log.
  macos-14 scheduled brownout has ended; use normal kernel runner, no IIG
  substitution. Previous generated-file reuse experiment was discarded.
  Core Submit/VM/UserClient C++ syntax compilation passed with host IOKit
  type/allocation shims (no runtime tests). New patch applies to the actual0116
  CI buildtree. Initial kernel run40058111623 on6733e9d dispatched after API TLS
  failures. Follow-up adds OOL descriptor prepare/complete before copying and
  independent GLES/kernel workflow concurrency groups. New source/build pair
  superseded that initial run. Both replacements use23a1fc821e561c52d61279feda8920a1c7d977d6:
  kernel40058371440 build jobSUCCESS (artifact12546134862,60572112bytes,
  ZIP SHA256759ca7a3181cb5e6124cac3019f1fb056e14e83b7503665acd317a7566d07ba5),
  GLES40058371530 SUCCESS (artifact12546685371,4553091bytes,
  ZIP SHA25691573d9c7f978a420a211574cb945415a222481e76b4afe17f468a20ad05721f).
  Normal gh artifact download timed out; first GLES range download failed at
  offset1048576 after retries. Resumable64KiB GLES downloader now running,
  saves verified-length chunks in research/gpu/gles-0119-download-ranges;
  helper research/gpu/fetch-gles-0119-resume.py, output when fully verified
  userspace/out/0119-gles-23a1fc8-range. Kernel member-only range helper
  research/gpu/fetch-kernel-0119.py downloads kernel+SHA256SUMS to xnu/out-0119.
  No downloaded artifact is accepted without final CRC/hash validation.
  API TLS failures are intermittent; alternate process-local US API routing
  retains TLS/SNI verification (research/gpu/github-read-us.py), no system DNS
  changes. User asked to switch networks; reply pending.
  Fresh research/gles-install-0119 directory prepared, no old block cache,
  overlay or delta reused. User asked to enter OrangeFox for read-only staging;
  no ADB device at last check. No0119 userdata writes or hardware boot.
  Hardware rendering remains unverified.

- **ACTIVE: CPU fallback reaches home screen; user confirms previous speed restored.**
  Branch codex/adreno-power-audit, original iOS16.7.16 HFS+ / NativeAdreno ABI1.
  CI39988038341 SUCCESS onb2cd6ea4b60eb151cd10a56b801a2337d21a4542.
  Artifact12517400420 downloaded to userspace/out/0118-gles-b2cd6ea;
  every SHA256SUMS entry passed. ARM64/iOS16.0/signature load command and
  matching dyld-cache UUID verified. Library18791008bytes SHA256
  c1af3c165dda6249b1125b3d9a65ee2e5790faaa3521ae32ab52f141149d1fcd.
  Image shim/out/shim-boot-0118-gles-bo-size-120hz.img, SHA256
  f35775f444e62765daabfa4041aad24032713c0920bc969d0ed8e03e645f99b5.
  Same compiled0116 kernel and sRGB ADT; boot tag sweetbuild=118.
  User reported FASTBOOT; product sweetin verified. Temporarily booted existing
  OrangeFox recovery image, sendOKAY3.846s/bootOKAY0.130s; no recovery flashing.
  Saved persistent0117 trace to logs/gpu-0117-after-20261023.txt(.bin), confirming
  BGRA/pitch4608 plus Mesa layout failure and later fence timeouts.
  Fresh staging research/gles-install-0118 uses new read-only ADB proxy/cache
  /tmp/sweet-gles118-root.sock. Basic HFS reader could not hash the fragmented
  old dylib (overflow extents); mounted guest verified actual0117 library and
  record SHA256 before replacement. New library staged only in local qcow2,
  final filesystem check/read-only remount/hash/executable checks passed.
  User explicitly chose "Apply and temporarily boot0118". apply-delta.py
  freshly matched all35 original chunks before writing20119552bytes and
  verified ALL35 written chunks by readback SHA256. delta.json now
  installed-readback-verified; before/after bytes remain under delta/.
  adb reboot bootloader; product sweetin; temporary fastboot boot sent16992KB
  OKAY0.515s and BootingOKAY0.130s. No boot/recovery/GPT flashing.
  Live receiver logs/live-0118-usb.txt captured 456029 bytes in 240 seconds.
  USB reconnection replayed startup messages, so do not count duplicate sections
  as separate boots. A618 startup CP fence passed29/29; Mesa/EAGL starts and
  main1125x2436 BGRA/pitch4608 buffers import with extent11239424bytes. This
  resolves the main framebuffer size rejection, not the whole renderer.
  Later method6 returns0xe00002d6 (fence timeout), GL finish blocks for hundreds
  of milliseconds (maximum1804172us), and281x609/pitch1152 imports still fail
  layout validation. Flush counts are NOT measured display FPS.
  User explicitly reports significantly worse performance, no Wi-Fi, no haptic,
  no sound. Treat0118 as a regression; do not call it an acceleration fix.
  Those peripheral drivers remain unfinished, not enabled by this library.
  Prepared shim/out/shim-boot-0118-cpu-fallback-120hz.img, SHA256
  84c745e436a3095da8eb4821d557c00c4970cac5f7ab1c1433ea1f34a49e9a1f.
  Same verified kernel/ADT/shim/DTB; sweetgles=0 and sweetgpu=0 disable injection
  and the GPU worker. User returned phone to FASTBOOT to boot this fallback.
  Product sweetin verified; temporary fastboot boot sent16992KB OKAY0.518s,
  BootingOKAY0.130s. No partition flashing or raw userdata writes. Receiver
  logs/live-0118-cpu-fallback-usb.txt started before boot (150s capture).
  Live USB confirms correct sweetgpu=0/sweetgles=0 boot args, GPU worker
  disabled, and launchd/SpringBoard exec; capture finished183787bytes. User
  confirms "Home screen; back to previous speed". This establishes the fallback
  as a working recovery option, not GPU acceleration. Phone remains in this
  temporary CPU-rendering boot. Installed0118 library remains on disk, inactive.
  Next GPU work must address submission/fence stalls and small-surface layout
  separately. Do not remove glFinish without a real presentation fence or bypass
  Mesa's allocation bounds checks. Preserve current live trace before changes.
  Immediate raw rollback to0117 requires changed blocks still matching; no old
  block delta may be replayed blindly after iOS has modified the filesystem.

### 0117 runtime and source diagnosis (historical)
- **ACTIVE:0117 installed and booted to userspace, still black UI;0118 fix staged.**
  Original ios-sweet branch codex/adreno-power-audit, iOS16.7.16 HFS+.
  User approved raw0117 install and temporary boot. All48 chunks/21233664bytes
  matched before writing and passed readback after writing; delta state is
  installed-readback-verified, originals retained in research/gles-install-0117.
  Library SHA2562eb01550cbddc2619c7feaf36485935052c934acd577ee55eccaeedfafd83b09.
  Boot image SHA256225203f037160ce67f549a1d11104eab97b22f86d58bfa73eff802184a8a8b17.
  adb reboot bootloader, product sweetin, temporary fastboot boot sent16992KB
  OKAY0.514s and BootingOKAY0.130s. No boot/recovery/GPT flashing.
  Live USB capture now WORKS: logs/live-0117-usb.txt,312416bytes in210s.
  Provider ready1, CP fence passes, Mesa FD618/EAGL loaded. Formats corrected
  to BGRA1125x2436, pitch4608. Actual error now "mesa layout failed" on
  surface2/3; user confirms black screen. No acceleration claim.
  Pinned Mesa source confirms fd_screen_bo_from_handle passes0 size for KMS
  handle imports; a6xx layout_resource_for_handle rejects layout.size greater
  than fd_bo_size. Our new IOSurface handles were never Mesa BOs before import,
  so need their real allocation size from the transport BO table. New Darwin
  patch passes that extent into fd_bo_from_handle, with zero/UINT32 checks;
  retain Mesa layout bounds checks. Prepared research/gles-0118-prepared.
  Follow-up CI39988038341 dispatched onb2cd6ea (C-compatible size logging);
  it supersedes canceled39987963423. This is a source correction, not
  hardware-verified rendering.
  Phone remains running0117 with black UI. Need recover/read trace as needed,
  build/verify replacement library, stage fresh delta against current root
  (do not reuse0117 before-state), obtain new raw-write approval before update.
  Full Metal, Wi-Fi, audio and calibrated battery remain unfinished ports.

- **Runner correction:** run39981853003 was queued on macos-14 during its
  scheduled2026-10-23 14:00–00:00 UTC brownout (actions/runner-images#13518).
  Move the GLES job to macos-15, preserve the pinned sources and iOS16 minimum,
  cancel the queued run and dispatch its replacement. Fresh root snapshot and
  local-overlay HFS check completed; no installed GLES library after user's
  manual userdata restore. No device writes.

- **0117 local staging:** fresh proxy and overlay inspected the restored root;
  guest HFS+ check repaired its metadata in the local overlay and mounted it
  read-only, confirming NO_GLES_LIBRARY and NO_GLES_RECORD. Guest exited with
  VERIFIED_UNMOUNTED. Phone userdata remains unchanged. The first guest startup
  had local initramfs ownership wrong; regenerated archive with owner0:0 and
  reran successfully. Image integrity comparison confirms exact shim, DTB and
  payload; firmware-bearing ADT differs only at product/artwork-display-gamut.
  Last GitHub check: replacement run39982823642 queued, no runner assigned;
  no library compilation/artifact yet. Watch transcript:
  research/gpu/gles-0117-build-watch-macos15.log. Do not install old0116 library
  as though it included the new non-planar metadata correction.

- **Recovery log retrieved 2026-10-23:** read-only userdata-tail pull saved
  logs/gpu-0116-return-20261023.txt(.bin), total1308564bytes flush4664.
  Filename describes the collection intent, NOT proven kernel identity.
  This trace's boot-args omit sweetgles=1 and sweetnull=backboardd (line274).
  GPU GMU/HFI/SQE and CP29/29 fence0x618c0de pass (line2412), but render
  gate still expects UCHE0xe19=0x81/read1 (line2414); provider stays ready0.
  Thus this is a pre0116 configuration, not evidence the corrected0116
  GLES pair failed. No sweetgl startup marker. Verified local0116 image
  SHA2569ac29bf16277d7b41eb28690f6181832feabcb80def8d66b1cbb337c70f981c3
  against saved manifest; its cmdline contains sweetgles=1. Need a fresh
  temporary boot of this exact image and recovery/live log. Phone left in
  OrangeFox; no writes, reboot, or new tests performed.

- **0116 temporary boot explicitly authorized 2026-10-23:** verified image
  SHA2569ac29bf16277d7b41eb28690f6181832feabcb80def8d66b1cbb337c70f981c3,
  adb reboot bootloader from recovery, fastboot product sweetin. Temporary
  fastboot boot sent16992KB OKAY0.530s, BootingOKAY0.130s. No flashing.
  Await actual iOS startup and new GPU/GLES trace; transport success is not
  proof of boot completion or acceleration.

- **Fresh0116 black-screen evidence2026-10-23:** user reported no boot/black
  screen. Host detected stockFASTBOOT; temporarily booted existing OrangeFox
  recovery.img (send3.891s/boot0.130s), ADB recovery returned. Read-only saved
  logs/gpu-0116-failed-20261023.txt(.bin),236720bytes flush34; different from
  prior log. This DOES reach iOS userspace: SpringBoard/HID running. Cmdline
  has sweetgles=1. PERFSEL selector1 accepted, NativeAdreno ready1, ABI1 opened,
  Mesa screen ready and EAGL shim installed (lines2567–2596). Compositor fails
  attaching surface2/3 b3a8 1125x2436 pitch0 (line2802+); framebuffer reports
  same surfaces bpr4544 (line3040+). Later method6 returns0xe00002d6 and texture
  copies reportGL0x506. No visible UI; this is userspace/render-path failure,
  not evidence XNU failed to boot. Inspect IOSurface plane0 metadata retrieval
  in Sailfish eagl.c bind_surface versus Sweet IOSurface implementation; do
  not simply ignore format/fence errors. Phone left OrangeFox, no flashing.

- **HackiOS reference port requested2026-10-23:** keep originalios-sweet
  iOS16.7.16/HFS+/NativeAdreno; user asks GPU, battery, Wi-Fi, audio reference
  adaptation and firmware collection. No original-root restore occurred (the
  earlier erase request never confirmed). Existing sweetgl remains installed.
  Current HackiOS kernel d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c and
  userland a79ff0772a3454fd1ef127e181e5b3bce6c9ce18. README still says GLES
  compositor, Metal apps incomplete. Kernel changes since previous8e61187
  onlycommon/setup.sh and xnuloader/loader.c, no new hardware drivers.
  ConcreteGPU corrections taken from its loader/eagl: advertise sRGB for the
  actual8-bitBGRA path (P3 requested unsupportedb3a8/w30r), and use ordinary
  IOSurface metadata/base getters for non-planar images. Existing bridge
  usedOfPlane0 and gotpitch/BPEzero. Preserve allfirmwareADT properties while
  editinggamut withtools/sweet-gles-adt.py. NewGLES build pending; no hardware
  rendering result yet. Recoverymodem/vendor mounted read-only forfirmware.
  Sweet observesqcom,qpnp-qg gauge,ICNSS18800000,AW8624haptic/AW882xxamps;
  Redwood usesPMICGLINK battery,WCN6750/WPSS,AW8697. Do notcopythoseaddresses
  or a660 blobs toSweet. No phone writes, firmwareexecution or reboot.

- **Firmware/port milestone2026-10-23:**103 files/88008754bytes pulled and
  remote/hostSHA256verified toignoredresearch/sweet-firmware-20261023
  (GPU7/audio31/WLAN34/modem29/haptic2). GPUidenticaltoearliercollection.
  Temporaryread-onlymodemVFAT/vendorEROFS mounts unmounted. NoWLANMAC,
  credentials,EFS/persist copied. PublicPMOSDTS confirmsWCN3990/ath10k,
  notRedwoodWCN6750/ath11k. Batteryrecoverydriverqcom,qpnp-qg; PMOSampsAW88263S.
  See docs/HACKIOS-SWEET-REFERENCE.md. Source00f535a pushed; generated
  research/gles-0117-prepared andfirmware-bearingADT
  shim/out/sweet-0117-gles-srgb.adt. GLESbuildNOTstarted:directgles.yml404
  becauseworkflowisbranch-only,correctxnu.ymlgles_onlydispatchthen422
  "Actions has been disabled for this repository". PermissionsAPIenabledtrue.
  AskeduserforActionsUInotice; do notreportqueuedorsuccess. ExistingGPUlibrary
  remainsinstalled, earlieroriginalrootrestorewasneverapproved/executed.
  WiFi/audio/batteryhardwareportsremainunfinished; noMetalcompletionclaim.

### Historical HackiOS detour and earlier installation records

- **USER CHANGED PROJECT TO HACKiOS (2026-10-22):** user reports0116 booted
  with zero improvement, initially stopped, then explicitly requested a new
  HackiOS-based Sweet repository and compiled boot image. Original branch
  and installed userdata preserved; no further device command performed.
  New project /home/dan/hackios-sweet, https://github.com/DC28088/hackios-sweet.
  Kernel/loader CI39773636216 succeeded on659167c; Sweet UEFI firmware job
  CI39773154339 succeeded; HackiOS Mesa/EAGL CI39772020663 succeeded.
  Package CI39774118533 succeeded. Image newrepo/out/boot-39774118533/
  hackios-sweet-boot.img,7833600bytes,
  SHA25614ab4eb6d16d503eabe3a67d037f24b83d3b6bc7b8a81d6c1dff176c74d83072.
  This is the resident HackiOS USB loader + compiled XNU, not an iOS root.
  Requires separate iOS16.5 APFS system/helper images. Current16.7.16 HFS+
  userdata and its sweetgl library are not compatible with HackiOS ABI.
  Root extraction not started (needs~8GB working space, host~4.7GB free).
  Hardware boot/GPU acceleration NOT verified; no tests/device writes.
  New repo NOTES.md and ports/sweet/README.md contain the current handoff.

- **GLES LIBRARY INSTALLED AND READBACK VERIFIED (2026-10-22):** user
  explicitly approved the staged userdata write. apply-delta.py freshly
  matched ALL67 original chunks before the first write, applied22740992bytes
  and verified every changed chunk by readbackSHA256. delta.json state is
  installed-readback-verified;67/67 written_verified. Active root now has
  /usr/lib/sweetgl.dylib SHA2560ae1789bb55795dec09edd3c741cba0687fdde0e36da1db2ad2dcc8fd377083d
  and /private/var/db/sweet-gles-install.json. Exact original blocks retained
  under research/gles-install-0115/delta; apply.log saved. Recovery/bootloader
  partitions unchanged. Phone remains OrangeFox; NO reboot or runtime test.
  NEXT: temporary boot shim/out/shim-boot-0116-gles-compositor-120hz.img
  (not0115), then collect startup/provider/EAGL/submission evidence before
  claiming acceleration. Existing Metal bundle retained; original boot
  without sweetgles=1 does not inject the new library. All older pending
  installation entries below are historical and superseded by this result.

- **GLES INSTALLATION READY / WRITE APPROVAL PENDING:** fresh overlay full
  fsck-fy passed, read-only mount verified library hash/executable mode and
  install record. research/gles-install-0115/delta.json contains67 disjoint,
  4KiB-aligned chunks totaling22740992bytes (21.6875MiB). ALL before/after
  bytes and hashes saved under delta/. Both read-only NBD servers are up:
  /tmp/sweet-gles-root.sock and /tmp/sweet-gles-edited.sock. No phone writes.
  Asked user to approve this concrete raw userdata delta per AGENTS.md.
  preflight.py currently freshly compares every original phone block;
  apply-delta.py repeats all checks then writes/readback-verifies each chunk.
  restore-delta.py refuses rollback after unrelated root changes. Current
  phone remains OrangeFox. On approval, applydelta then update installed
  status and offer matching0116 temporary boot; no runtime acceleration yet.

- **0116 AND GLES PAIR BUILT (2026-10-22):** kernel CI39688634861 passed
  on53b3f58c25d80fe318e2bbb8c01e564deaf84791. Artifact12387816032 checksum
  verified. Image shim/out/shim-boot-0116-gles-compositor-120hz.img,
  17399808bytes, SHA2569ac29bf16277d7b41eb28690f6181832feabcb80def8d66b1cbb337c70f981c3.
  Local unpack checks: unchanged shim/DTB, payload match/deterministic repack,
  required markers and stage9/120Hz/GLES/backboardd flags. Mesa/EAGL library
  CI39688037690 on4e701be succeeded; userspace/out/0115-gles-4e701be/sweetgl.dylib,
  18756112bytes, SHA2560ae1789bb55795dec09edd3c741cba0687fdde0e36da1db2ad2dcc8fd377083d.
  Mach-O iOS platform2 minimum16.0; IOKit Versions/A installname is present
  in the actual cache. Active phone root read-only inspection matches iOS
  16.7.16/20H392/cacheUUID8f970452c9e33e88b114feb1864bc9e0, no prior GLES
  library. Fresh qcow2 overlay contains library and install record; final
  filesystem validation/delta preparation ongoing. NO phone writes or boot.
  Recovery connected; read-only nbdkit /tmp/sweet-gles-root.sock running.
  Host stage research/gles-install-0115 (freshcache). fsck540.1 -fn has known
  header-cache false damage report; finalize with full-fy then read-only
  mount/hash check, as documented in tools/mkiosroot.sh. See docs/GLES_0116.md.
  No tests run, no runtime acceleration claim. Need concrete write approval
  once block delta/rollback are fully prepared, per AGENTS.md.

- **0114 LOG RECOVERED / GLES BUILD SUCCESS (2026-10-22):** user entered
  OrangeFox with USB. Read-only recovery pull logs/gpu-0114-before-gles.txt
  proves RPMh correction passed, GMU/HFI/SQE startup passed, CP29/29 and
  fence0x618c0de completed. Actual provider blocker now UCHE_CLIENT_PF0xe19:
  expected0x81, read1. Driver refused publication; zero improvement explained
  by that log, not an inferred userspace-only fault. Pinned downstream
  vendor-adreno_a6xx.c:943 writes1; XML names this8-bit field PERFSEL.
  Patch0116 programs vendor selector1 and validates all8 field bits, retaining
  the rest of the profile gates.0115 also gets an argument-space bounds check
  after env-string copy. New companion build pending (will be image0116).
  GLES CI39688037690 succeeded on4e701be. Artifact downloading to
  userspace/out/0115-gles-4e701be. First companion kernel39687265147 passed
  on266325e; packaged0115 image exists but is superseded by0116 before boot.
  No phone writes/reboots performed. Current phone is OrangeFox; userdata
  unmounted, HFS+ volume17179869184bytes in114970046464byte partition.
  Fresh installer staging research/gles-install-0115; never reuse the older
  Metal installer block cache for writes against this later filesystem.

- GLES first build39687042106 compiled all Mesa static libraries and the
  EAGL/dispatch frontend, then failed on sweet_native.cpp's IOSurface.h
  include (iOS SDK provides IOSurfaceRef.h). Corrected. Source review also
  removed user cache-maintenance assumptions: every Mesa BO now uses
  task-owned VM pinned through Native BoImport, so publishMapped and
  acquireImported maintain actual physical cache lines at submission/fence
  retirement. This avoids nonexistent sys_dcache_invalidate and ineffective
  user-level flushes on this ARM platform. Disabled unsupported PRIME/native
  fence-FD capabilities, bounded logical queue IDs, real fault counters.
  Companion kernel39687265147 builds266325e; GLES rebuild pending.

- **0115 GLES KERNEL INTEGRATION STAGED:** patch0115 adds explicit
  sweetgles=1 boot mode: backboardd-only fixed DYLD_INSERT_LIBRARIES,
  scoped AMFI dyld policy, no MetalPluginName on the live A618 provider,
  and IOSurfaceAddress/client-readonly memory wiring with exact bitmap rows.
  Normal boots retain Metal discovery; GLES library is loaded only with
  the new flag. Full ordered patch staging passed for all5 touched files.
  GLES frontend CI39687042106 is building source1549316. Companion kernel
  CI pending. No hardware test or root write. Source integration is not a
  claim of successful compositor rendering; actual A618 service readiness
  still needs the0114/0115 startup trace.

- **GLES USERSPACE IMPLEMENTATION STARTED (2026-10-22):** user reports0114
  booted with zero visible improvement and explicitly requests finished
  Sailfish-style userspace integration. No0114 trace collected yet; do not
  claim hardware readiness. Implemented userspace/gles/sweet_native.cpp
  adapting Sailfish BO/import/submit/wait operations to our NativeAdreno
  ABI1, real chip/GMEM/aperture query, cached CPU maps, resource cache
  maintenance, retained IOSurfaces, fenced frees and ordered16-IB batches.
  tools/prepare-sweet-gles.py fetch-source adaptation preserves the EAGL/GLI
  frontend and shader fixes. Original iPhoneX root cache was read only:
  UUID8f970452c9e33e88b114feb1864bc9e0. QuartzCore table/switch and12 prologues
  match the Sailfish source at +0x4000; recorded userspace/gles/cache-profile.json.
  Runtime UUID gate added before EAGL/QuartzCore changes. Mesa pinned to
  f2a70d066e69c0ad62070d2f25e175a4198d273b, all3 Sailfish Mesa hunks apply.
  Reproducible macOS builder tools/build-sweet-gles.py, GLES workflow, and
  reversible offline root installer prepared. No source-level/hardware tests
  added or run. Kernel GLES discovery/load policy and IOSurface source
  wrapping still being implemented. Library compilation pending; nothing
  installed and no new phone command beyond empty adb device listing.

- **0114 BUILD SUCCESS / PACKAGED, NOT BOOTED (2026-10-22):** CI39683698934
  passed on source231196d88d541ac7d43be007e5bce8c5b5f480f4. Portable tests
  SKIPPED as requested by session instructions. Artifact12385946044 fetched
  with ZIP CRC and published SHA verification. Kernel xnu/out-0114/
  kernel.development.sweet SHA256
  944f53f2a40edc65e7bf866e4c9cf43ee240443ac6be05b123bc3e9bc9d63b36.
  Image shim/out/shim-boot-0114-rpmh-completion-120hz.img,17399808bytes,
  SHA2564d16341c9c016f9c788a8aa4fc8a1e6aa86887c18c10d8be2cf56c741d327351.
  Manifest research/gpu/0114-boot-manifest.json; watcher/package helpers
  finish-0114.py/package-0114.py in research/gpu. Boot-v2 unpack verified
  exact original shim+DTB, ramdisk==payload, deterministic repack from new
  kernel+existing firmwareADT+cmdline. New RPMh marker present. Stage9,
  120Hz scheduler, USB logging retained; old installed Metal bundle remains
  ABI-compatible. No GLES frontend built/included, no hardware boot or root
  changes; acceleration NOT established. Initial run39683486855 was cancelled
  and superseded before packaging to preserve touchscreen regulator behavior.
  Next: user-controlled temporary boot of this image and collect the fresh
  latch/GIC/CX/GMU/CP/provider result, then port Mesa/GLES userspace transport.

- **0114 GPU IMAGE BUILD PREPARATION (2026-10-22):** user requested a new
  GPU image. Mesa documentation confirms Gallium freedreno supports Adreno
  6xx GLES and Turnip supports 6xx Vulkan. Neither replaces our XNU hardware
  transport. New patch0114 corrects RPMh completion polling: enable the
  selected RSC completion source, require a fresh latch plus idle/command
  completion, and preserve timeout quarantine. Verify GIC INTID37 is disabled
  and inactive before enabling the RSC source; make no GIC writes. Addresses
  verified against saved live FDT: apps_rsc SPI5, distributor0x17a00000.
  This addresses the observed race hypothesis where masked IRQ_STATUS stays
  zero and a short busy transition is missed; hardware success not proven.
  Exact ordered patch application reproduces intended source. CI marker
  added, portable tests will be disabled. Packaging helper prepared at
  research/gpu/package-0114.py, retains stage9/120Hz/liveUSB and existing
  ADT/firmware/shim. Existing installed Metal bundle ABI unchanged. This
  kernel image does NOT include a ported GLES userland; do not claim GLES
  or UI acceleration. No tests or device commands run; CI build pending.
  Follow-up scopes the new completion mode to ARC/GPU only; preserve the
  established ldoc11 touchscreen transaction path. Supersede initial
  run39683486855 with the scoped version before packaging.

- **PLAN CHANGED BACK: DIRECT SHIM + iOS16 + GLES (2026-10-22):** user
  explicitly returned to main/adreno-power-audit and supplied the Pixel1
  sailfish GPU reference. Active branch is `codex/adreno-power-audit`,
  starting from 4e84f4e. Fetched myfork/main at 8cfc234; that main has no
  commits absent from this branch, so no merge was needed. Preserve the
  UniXNU branch at 8cba86f; its unfinished KPC edits were saved in stash
  `Preserve unfinished UniXNU Darwin22 KPC edits before requested return to
  Adreno branch`. No discard/reset, no device commands or new build.
  Clean reference checkout research/ios-sailfish pinned to
  ce637521bacc06fab5cc29cf144febee2fca9ff8. Public Mesa/EAGL/DRM source
  exists, unlike the earlier private Topaz renderer. Author reports GLES
  UI rendering at 15–35fps, up to60Hz presentation, NO Metal. These are
  source/README findings, not measurements on sweet. New direction audit:
  docs/sailfish-gles-port.md; selected source hashes:
  userspace/gles/reference-inputs.json. No Mesa revision is pinned by the
  reference setup script; resolve one before building.
  Plan: retain our A618 kernel backend; port CoreAnimation GLES/EAGL through
  Mesa Gallium freedreno, adapting Pixel scalar SweetGPU selectors to our
  NativeAdreno structured ABI. Do not import A530 power/firmware/addresses,
  or trust its dsc-8 QuartzCore patch addresses on our installed root.
  First hardware blocker remains saved0113 RPMh cx.lvl completion timeout;
  accepting stale CMD_STATUS is not a fix. Need fresh completion evidence
  before GPUCC/provider initialization, then GLES BO/import/fence transport,
  IOSurface/dispatch/cache ABI and reversible backboardd dylib loading.
  No new acceleration image or dylib compiled/installed; no tests run.
  Phone state was NOT inspected this turn. Most recent recorded operation
  on the separate UniXNU branch was a temporary UEFI boot on2026-10-21;
  current screen/USB state unknown. Installed HFS+iOS16 root preserved.

- **0113 TEMPORARY BOOT / RPMh CX TIMEOUT (2026-10-21):** user authorized
  boot; fastboot product sweetin verified, image SHA matched. fastboot boot
  sent16992KB OKAY0.514s, BootingOKAY0.130s. No partition flashing/root writes.
  User confirms iOS still running. Live capture logs/live-0113-usb.txt:
  initial host helper crashed on udev usb0->enp0s20f0u3 rename (nmcli device
  lookup exit10). Restarted receiver session11777, actual UDP data confirmed.
  User unplugged/reconnected USB on request. USB reset/reconfiguration and
  backlog replay worked; host new-link RX282packets/errors0 at inspection.
  Replay shows current failure EARLIER than render profile: ARC READ mx.lvl
  completes selector3/level321; following cx.lvl READ TCS0 times out (errno60),
  TCS quarantined/no retry. CX preconditions fail; no GPUCC write, no GMU/SQE
  or renderer publication. 0113 optional-register correction NOT REACHED, so
  do not claim its runtime success. Need examine RPMh/TCS transaction lifecycle
  and its CX request before another register/Metal change; preserve quarantine
  semantics, do not blindly reset or reuse uncertain TCS.
  Captured trace remains logs/live-0113-usb.txt; helper session11777 bounded
  1800seconds active at last check. Host helper now retries nmcli discovery
  failures so interface renaming cannot terminate capture; Python compilation
  passed, no tests added/run. Running receiver is older loaded source; use
  updated helper on next start. Phone left running0113, no further reboot.

- **0113 BUILD SUCCESS / READY, NOT BOOTED (2026-10-21):** CI39630488384
  succeeded on source133087d; portable test stepSKIPPED. Kernel artifact
  12360100686 CRC/publishedSHA verified. KernelSHA25694c85d755f64520bb663356dd0dcba82c9cce6f5f2c05e56d0b5980411eade3c.
  Imageshim/out/shim-boot-0113-native-adreno-renderer-120hz.img 17399808bytes,
  SHA256d9ba2cc6091adb9062fe430d2550cc29a9a2f1f583b392ff61dea1d9dfee35ef. Manifest research/gpu/0113-boot-manifest.json.
  Corrects inappropriate optional0x534 access and detects sentinel readbacks;
  hardware outcome/provider publication/iOS acceleration remain UNVERIFIED.
  Existing installed0110Metal bundle is still ABI-compatible; no root changes.
  This turn changed no device state: phone remains running0112; live0112capture
  receiver session85796 was still active at last check, bounded1800seconds.
  Before any0113boot, ensure a receiver is active and separate capture by boot.
  Topaz reference audit committed; useful IOSurface support remains to port with
  memory ownership review and actual compositor trace. No complete public
  Topaz GPU renderer was found. No sweet Wi-Fi driver was installed or enabled.

- **Topaz source comparison detail:** reconstructed current SweetSurface.cpp:
  sweet only0046+0068; lacks IOSurfaceAddress, client wrapping6/7 and real
  values9..11/bulk27; release trap immediately drops maps. Topaz0131..0139
  supplies these operations and detach-view lifetime handling. Concrete future
  integration source identified; do not import Topaz scanout-pool addresses.
  Public radio ready-hook declared/invoked but no consumer or IO80211 class
  found in pinned tree. README reports working Settings Wi-Fi, but public
  radio patches alone do not establish a complete iOS-facing interface.
  Audit updated;0113 kernel still building. Phone remains running0112.

- **Topaz reference refreshed /0113 source correction (2026-10-21):** fetched
  origin/topaz89451b811a4e6cc5cdcf734c45887163d92c599e; clean local reference
  checkout now detached at that exact revision. Public0113..0130 Wi-Fi code exists;
  author README reports WPA2/HTTPS. Reported Topaz GPU uses private HackiOS
  GLES patches; README explicitly excludes GPU source and lists Metal later.
  Public0131..0139 are IOSurface support,0138 empty discovery service. Audit:
  docs/topaz-reference-2026-10-21.md. Sweet WLAN MMIO/SID/reserved pool differ;
  no radio initialization/root changes. Current GPU blocker resolved in source
  by0113: downstream vendor a6xx_start only touches0x534 whenchannel==8;
  sweet has no such override. Skip this optional access, preserve reset state.
  Required registers now explicitly reject0xdeafbead before masked comparison.
  No tests run; patch staging passed. CI39630488384 builds source133087dc748a2f3f9bb2b71df2b465ce7832e123.
  finish-0113.py watches/downloads/packages; existing Metal ABI unchanged.

- **0112 TEMPORARY BOOT / LIVE USB WORKING (2026-10-21):** user explicitly
  authorized ADB/fastboot commands. adb reboot bootloader succeeded from
  OrangeFox; fastboot boot0112 sent16992KB/BootingOKAY. No partition flashing.
  Live UDP console receives real data from192.168.7.2, logs/live-0112-usb.txt
  >216KiB. Host fixed-MAC interface has correct profile; capture session85796
  remains active. Reset lifecycle across a subsequent replug is not yet proven.
  GMEM validation now passes; CP still completes29/29, fence0x618c0de.
  NEW BLOCKER live trace: SWEET RENDER reg0x534 expected0 read0xdeafbead
  (RBBM_NC_MODE_CNTL). Render profile fails0xe00002ca; provider NOT published;
  GMUstage9 ends errno5. Do not claim acceleration. Need determine A618
  support/access for this macrotile-mode register against pinned Linux and
  downstream/vendor configuration before changing its validation or access.
  Phone left running temporary0112; home-screen state not visually confirmed.
  No root image changes. Prior manual-boot preference superseded for this boot
  by explicit user authorization. Existing Metal bundle stays compatible.

- **0112 BUILD SUCCESS / READY, NOT BOOTED:** CI39625892744 succeeded on
  sourcefbb439533ba3c0dc7bd84d4a6a883a576dc8d537; portable test stepSKIPPED.
  Artifact12356753481 published checksum and ZIP CRC verified by fetch helper.
  Kernel SHA256 044f900704fad68c0e4c030094b4aa36d2c84e695edf1c6589dffa7cd891b722.
  Image shim/out/shim-boot-0112-native-adreno-renderer-120hz.img 17399808bytes,
  SHA256 c31cb71ffe3d03ac395bf9b0df2e006919d591afa8c7cb375e84337a31662ef2.
  Manifest research/gpu/0112-boot-manifest.json; staged source saved in
  research/gpu/0112-work. No device writes/reboot; phone left OrangeFox.
  Existing0110 installed Metal bundle remains ABI-compatible; do not reinstall
  root. User prefers manual temporary fastboot boot. Bounded USB receiver
  session85796 waits1800seconds onUDP5140, logs/live-0112-usb.txt and
  logs/live-0112-capture.out/status. A listening socket is not proof of transfer.
  After boot expect GMEM profile acceptance, provider publication, backboardd
  client connection and ABI submit/wait results; none yet hardware-verified.
  USB reset/reconnect correction and UI acceleration remain UNVERIFIED.

- **0111 SAVED LOG FOUND /0112 correction staged (2026-10-21):** OrangeFox
  confirmed, pulled logs/gpu-0111-paired-startup.txt600600bytes, flush121.
  0111 S2CR quirk confirmed; live CB0 has1027pages/20bootstraptranslations.
  SQE ME_INIT completes12/12; real CP timestamp fence completes29/29,
  value0x618c0de, no CP fault. NEW BLOCKER: renderprofile reg0xe0d expects
  inclusive0x17ffff but reads0x17f000. Provider publication stops here, so
  installed Metal still has no GPU to open. 0112 preserves Linux's GMEM limit
  write and validates upper20addressbits; low12RAZ inferred from hardware log.
  USB log proves no endpoint reconfiguration after later bus resets. 0112
  tracks data transfer resource IDs, ends active transfers before DMA reuse,
  clears configuration/queues on reset/disconnect/SET_CONFIGURATION0, locks
  setup against TX, ignores unconfigured RX completion. Failed stop disables
  data path for rest of boot. Console replays last1MiB after a valid host frame
  per configuration, bounded16datagrams/100ms, restores cursor on send error.
  Host helper sends bounded subnet probe to initiate ARP. Added bounded actual
  Metal compositor open/method-result telemetry. Kernel patch0112 applies to
  reconstructed0072..0077USB and0110render/client sources. CI39625892744 builds exact
  sourcefbb439533ba3c0dc7bd84d4a6a883a576dc8d537; finish-0112.py watches,
  downloads the kernel with published SHA verification and packages0112.
  No local ARM compiler, no tests run. No phone writes/reboots/root changes.

- **User reports0111 booted, performance unchanged; live capture FAILED:** user
  requested debug collection from current iOS. Old bounded receiver is gone,
  logs/live-0111-usb.txt was0bytes. Restarted receiver; correct hostIPv4 exists.
  USB1209:0001 enumerates, but RX/TX packets0, RX errors144900+ climbing,
  TX errors climbing; no ARP/ping response. NM link down/up did not help.
  User ran authorized sudo usbreset1209:0001; re-enumerated device9->11,
  still zero RX/TX valid packets and thousands of new errors. Captured ZERO
  GPU console bytes; do not claim0111 SMMU result or infer Metal discovery.
  Source review identifies missing configured/queue reset onUSB bus reset:
  0073 reset handles onlyDCFG address;0075 ud_configure returns immediately
  when ud.configured remains true. Likely explains endpoint loss on reconnect;
  not yet hardware-proven as sole cause. No new kernel patch/build this turn.
  dumpcap launch denied by host capabilities; no usable PCAP captured.
  Asked user to manually enterOrangeFox to pull0111 saved startup, await reply.
  Active bounded receiver session2642 (600s) writes logs/live-0111-usb.txt,
  logs/live-0111-capture.out/status. New helper now distinguishes receiver-ready
  from first actual packet and reports no-data/RX-error status every30s.
  No device writes/reboot or root changes this turn. Need actual saved GPU
  trace before driver changes. A live connection needs USB reset/reconnect
  handling and backlog replay, not just a desktop socket listener.

- **0111 BUILD SUCCESS / READY, NOT BOOTED:** CI39537556638 success on exact
  source6cf3c60c393a0ac5b82ef8fe311be5475d5d8d6e; portable testsSKIPPED.
  finish-0111 helper verified published artifact checksum and packaged image.
  Kernel SHA256 519ccd305ad132ca1b8dcacc54e319cb354ad5ba3e1d6666a241cea14df56a06.
  Image shim/out/shim-boot-0111-native-adreno-renderer-120hz.img17383424bytes,
  SHA256 eeb53575f3e316461d00656d2d77caf28a52ec2643b36aa30a2d6c72378fb5da.
  Binary contains new S2CR probe/quirk and existing renderer publication markers.
  Existing installed0110 Metal bundle remains matching (ABI unchanged).
  Phone LEFT ORANGEFOX, no device writes/reboot this turn. User previously
  preferred manual boots. Live USB receiver READY session96141,1800s bound,
  logs/live-0111-usb.txt; stdout/tmp/sweet-usb-0111.out and statusfile. Dedicated
  NM sweet-usb-log should assignIPv4 automatically when iOS re-enumerates.
  Watch probe/readback, CB0 translation count, CP fence and provider publication;
  then actual Metal discovery/commands. Hardware correction and acceleration
  UNVERIFIED until boot. No root reinstall needed. Fresh diagnostic/persistent
  log0110 SHA25674ce09c2f10e8edd21e4c0c860a9fc3270e6a715ca1298b385a1df89aa5e0c15.

- **0110 STARTUP BLOCKER FOUND /0111 code correction:** user entered OrangeFox.
  Pulled logs/gpu-0110-paired-startup.txt273477bytes, flush55. GMU/HFI succeeds,
  then SWEET NATIVE VM route preflight failed0xa=ArmSmmu4KBusy; CP SMMU errno5,
  stage9 ends, NO render-provider publication. Installed Metal cannot initialize
  when provider never exists. Baseline invalid slots show S2CR0x200ff. Strict
  upstream preflight expects0x20000; Linux qcom_smmu_cfg_probe/write_s2cr documents
  firmware ignoring FAULT writes, rewriting BYPASS to FAULT with CBNDXff.
  Added0111: probe ONLY the leased INVALID stream slot, determine firmware
  encoding, normalize subsequent FAULT requests at board MMIO adapter. Require
  actual FAULT readback, no global reset/no GMU CB1 changes. Exact sweetID and
  polling profile permits ignored low CBNDX byte only for FAULT readback;
  translated routes still exact. Better startup detail log before failure.
  Vendor Streams C compilation with-Wall-Wextra-Werror and patch staging pass;
  Full kernel CI39537556638 runs on6cf3c60c393a0ac5b82ef8fe311be5475d5d8d6e,
  testsfalse. research/gpu/finish-0111.py polls/downloads/packages automatically
  (session96448, finish-0111.log). No tests/hardware run. Bundle ABI unchanged.
  Phone LEFT ORANGEFOX; do not reflash root. Source manifest in
  docs/native-adreno-0111-sources.json. This fixes a measured initialization
  blocker; it does not prove later Metal calls or acceleration work.

- **0110 paired boot still slow; USB live logging NOW VERIFIED:** user says
  installed bundle +0110 boots with unchanged performance. Current device iOS,
  enumerated1209:0001, USB Ethernet MAC02:00:5e:77:ee:01. Host enp0s20f0u3 had
  onlyIPv6, no UDP5140 receiver. sudo requires password, but nmcli device modify
  succeeded as desktop user; assigned192.168.7.1/24 and ping192.168.7.2 answered.
  Received actual console into logs/live-0110-usb.txt; now captures daemon execs,
  FB timing and USB counters. Observed1200callbacks/10s but present_calls=0;
  this is scheduler evidence, NOT GPU rendering proof or measured frame rate.
  Created dedicated persistent NM profile sweet-usb-log, matches only phoneMAC,
  manualIPv4/no default route/IPv6 disabled/autoconnect priority100. Replaced
  sudo+nc shell helper with Python UDP receiver binding BEFORE configuring NM,
  source-filtering phoneIP, append log, optional --duration and reconnect wait.
  Current capture launched with300s duration; original90s capture also received
  data. No kernel/root writes/reboot this turn. Startup already missed before
  address setup; requested user manually enter OrangeFox withPower+VolUp to
  pull full persistent0110 startup trace. Awaiting reply. Do not assume provider
  published/Metal loaded from boot success, and do not build another speculative
  driver image before reading startup result. NSLog is not automatically in
  kernel stream; frontend diagnostic routing remains an instrumentation gap.

- **METAL BUNDLE INSTALLED into phone userdata (2026-10-21):** user manually
  entered OrangeFox with Power+VolUp after software recovery boot returned to
  FASTBOOT. ADB recovery root now works. User explicitly authorized copying the
  matching bundle into the installed root. OrangeFox has FUSE but no HFS kernel
  driver; host sudo needs password. Used a read-only ADB-backed NBD export of
  the first16GiB HFSX volume, Linux guest + qcow2 overlay, to install
  /System/Library/Extensions/NativeAdrenoMetal.bundle from0110-1fae147.
  Confirmed actual iOS16.7.16 /20H392 SystemVersion.plist. No old bundle existed.
  Set root:wheel0:0, executable755. All53 bundle SHA256s passed twice, including
  after a read-only remount; all guest mounts cleanly unmounted before applying.
  fsck repair mode repaired preexisting hard-link metadata and post-copy volume
  header, then reportedOK. Read-only -fn continues the known (1,0) header warning
  documented for these journaled volumes; do not claim -fn clean.
  Saved ALL before/after chunks in research/metal-install-0110/delta/ with
  delta.json and guarded restore-delta.py (refuses unexpected changed blocks).
  Live preflight matched every original chunk. Applied285 chunks,241303552bytes,
  only inside first16GiB of userdata; every written chunk readback SHA verified.
  Volume headers written last; final sync complete. No other partition writes,
  no root reflash, no GPT change; persistent log tail outside volume untouched.
  Full guest/stage/apply logs and helper sources under research/metal-install-0110/.
  Local userspace/out/0110-1fae147/installation.json records evidence.
  Device LEFT IN ORANGEFOX, not rebooted. Matching0110 boot image is ready.
  GPU/Metal rendering performance still UNVERIFIED. Next step is paired kernel
  boot with live log capture; user earlier preferred manual boot. Do not restore
  raw before blocks after any subsequent iOS write without consistency analysis.

- **Recovery boot attempted after sandbox disabled (2026-10-21):** user
  asked to run fastboot directly. Device detected; fastboot boot
  recovery/recovery.img sent131072KB OKAY3.965s, Booting OKAY0.130s,
  total4.407s. Subsequent checks still detect FASTBOOT, no ADB recovery device.
  This proves command acceptance only, not successful recovery startup.
  Stopped the pending adb wait. No partitions flashed, no root writes, Metal
  bundle NOT installed. Host sudo requires a password. Need determine why
  temporary recovery returns/remains in fastboot before installing bundle.

- **PAIRED0110 BUILD COMPLETE (not installed/booted):** kernel39532260702
  and graphics39532264309 both SUCCESS on1fae147d3bcba8a2377b8bc3995ab0d66c97022f.
  Graphics artifact12315364871 ZIP58511001bytes. Final bundle licensing precedes
  codesign seal; codesign --verify --strict passed in CI. Downloaded to
  userspace/out/0110-1fae147/; verified all53 recorded bundle file hashes,
  SweetAdrenoMetalDevice principal class, included dylib/licenses/signature.
  Same directory now contains0110 boot image, boot-build.json, metal-build.json,
  paired-build.json and README.md. Kernel full build log saved under
  research/gpu/build-0110.log (gh HTTP2 download failed twice; urllib GET worked).
  Actual compiler invocations forClient/Render confirmed; no hardware/tests.
  Frame acceleration/performance remains UNVERIFIED. Matching bundle must be
  installed into active root /System/Library/Extensions before evaluating this
  kernel. No root, USB, reboot or partition operations performed this turn.
  docs/native-adreno-0110-deployment.md and docs/usb-live-logging.md record
  deployment/rollback/logging; do not ask another kernel-only diagnostic boot.

- **0110 kernel build and packaging SUCCESS:** CI39532260702 on exactsource
  1fae147d3bcba8a2377b8bc3995ab0d66c97022f; portable test step SKIPPED.
  Kernel artifact12315548449 ZIP60534401bytes; downloaded selected files and
  verified published SHA256. Kernel SHA256
  e7aee71d91fb4b9a51ff2ee353ca4b40adf3fb44849909753eae1528e284dbef.
  Image shim/out/shim-boot-0110-native-adreno-renderer-120hz.img17383424bytes
  SHA256 ed9a809ae4c5980c90c4ab2955e49325beb60f538daef98bf6759374b78029a8.
  Existing0083 shim and0096 firmwareADT reused; XNU sweetgpu9,120Hz target,
  sweetusb2,sweetusblog1. Manifest research/gpu/0110-boot-manifest.json.
  Binary contains actual provider/Metal discovery/live VM strings. Paired
  graphics CI39532264309 subsequently succeeded (see above). Do not boot the kernel
  alone: matching root Metal bundle still required. No phone/root access.

- **Build resumed, USB guide reviewed:** graphics run39472089044 on3ced921
  succeeded. Kernel run39472076417 failed at SweetNativeAdrenoClient.cpp:196:
  IOPlatformExpert was only forward declared. Pending local patch includes
  IOKit/IOPlatformExpert.h to provide inheritance definition. Earlier missing
  NativeAdrenoABI.h was fixed in3ced921. Fix bundle signing order: licenses
  must be copied BEFORE sealing bundle; require codesign --verify --strict.
  Rebuild both on the new commit, run_tests=false. No device access/boot.
  User-provided USB-SHIM-GUIDE.md is an MSM8953 CDC-ACM design description,
  not implementation sources. Existing sweet patches0072-0077 implement
  CDC-ECM logging (sweetusb=2, sweetusblog=1, UDP5140); retain sweet addresses.
  Transport has not been newly hardware-qualified this turn.


- **0110 connected renderer implementation (2026-10-20, NOT booted):** new
  SweetNativeAdrenoRender programs all A618 3D address modes,512KiB GMEM and
  SM7150 UBWC2 (bank14/swizzle6/min32/four-channel) with register readback.
  SweetNativeAdrenoClient publishes IOAcceleratorES/AGXAccelerator discovery
  only after liveVM/CP/render profile connect; implements actual NativeAdreno
  BO/import/map/IB/fence ABI and exact Darwin Turnip tunnel. One backboardd
  connection, same GPU IOLock, real fence quiescence before maps/frees/submits,
  retained original BO descriptors for CPU mappings; no fake render success.
  Kernel imports actual NativeAdrenoUserClient/Timer/Status. Initial WC disabled
  for explicit noncoherent cache maintenance; no process-isolation claim.
  Added actual iOS16 Turnip+metal2vulkan/NativeMetal build and a minimal plugin
  entry, removing target-specific QuartzCore probes by not compiling the old
  plugin. Base528 static ABI guard is unqualified at runtime. Mesa build-script
  pin85fda3b differs from survey pin c823a83: build follows compiler manifest.
  New graphics workflow invoked through existing xnu.yml graphics_only=true;
  run39469296081 onf56d421 currently compiling. First direct metal.yml dispatch
  was404 because new workflows are not registered on defaultbranch.0109 kernel
  run39468676639 was CANCELLED by old shared workflow concurrency when starting
  graphics. Concurrency now separates kernel/graphics. New full0110 kernel build
  pending. No root edits, installs, USB access or new boot images. User asked to
  keep phone in FASTBOOT until the connected rendering path is ready.

- **0108 user result /0109 connected-VM work (2026-10-20):** user confirms
  0108 reached iOS with unchanged performance, now FASTBOOT. User explicitly
  accepted implementation work without another unconnected-image boot. Do not
  access the phone or request a boot until an actual rendering path exists.
  Latest Topaz fetched d8c82d9dd5925136fa226e8e23bf773eeac2d69d: only new0139
  IOSurface address publication; no public GPU driver/Turnip/Metal path found.
  NativeMetal HEAD remains6dcd4cb.0109 now replaces pre-SQE fixed20-leaf CB0
  setup with an owned2GiB4K BO arena (16K host), maps the same boot SQE/ring/
  fence allocations, probes their20 translations plus guard, then connects
  NativeVm to NativeSubmit after the proven CP fence/adopt. Adds an explicit
  measured A618 context/polling profile; no unhandled IRQ enable or CB1 changes.
  Boot storage borrowed NC with permanent owner, never freed or cache-remapped.
  Portable SMMU compilation and patch staging passed; full XNU compile pending.
  Not hardware qualified; still no userclient, Turnip or Metal deployment.

- **0108 temporary boot sent (2026-10-20):** user reported FASTBOOT.
  Approved direct fastboot boot of
  shim/out/shim-boot-0108-native-adreno-core-120hz.img succeeded:
  Sending16928KB OKAY0.510s, Booting OKAY0.130s, total0.662s. No partition
  flashing or root edits. No /dev/bus/usb filesystem permission grants were
  requested. Waiting for user's on-screen result; fastboot success does not
  establish iOS boot or rendering. Full GPU acceleration remains unfinished.

- **0108 kernel build SUCCESS / image packaged (2026-10-20):** CI39464554694
  exactsource11e8daf2b57fa01c5e954eb3284aa095cf23e82a, testsSKIPPED. New
  memory/CPU/VM/submit and SMMU compile commands confirmed in saved CI log
  research/gpu/build-0108.log. Kernel artifact12283999169 ZIP60359513 bytes;
  kernel17318512 bytes SHA256
  84919178dc601d422abac875eb432f2a6f046a2a69b736b67ba91d7970ec406f,
  matches published checksum. Image
  shim/out/shim-boot-0108-native-adreno-core-120hz.img17334272 bytes SHA256
  6809ac8ad2fb224649265364912bc61f7c0b1108b1101051fa476c9f4ca2b7a8.
  Bootv2 kernel/payload unpack roundtrip matches. Manifest
  research/gpu/0108-boot-manifest.json. No USB, hardware tests, flashes or root
  edits. Full acceleration STILL UNFINISHED: the imported resource core is
  compiled but not attached to live CB0; no userclient/Turnip/Metal renderer.
  Do not expect UI lag improvement from this image alone. Next implementation
  is pre-CP dynamic table arena plus live VM ownership, then NativeAdreno ABI
  and iOS16 rendering frontend; docs/native-adreno-0108.md has exact boundaries.

- **0108 full kernel CI running:** run39464554694 exactsource
  11e8daf2b57fa01c5e954eb3284aa095cf23e82a, testsfalse. First run39464251721
  on76a874a failed before compilation because source-list hunk began at1;
  corrected full-file context in11e8daf. Actual patch staging outside the
  parent checkout applies0107/0108 and matches all committed vendor bytes.
  research/gpu/finish-0108.py fetches/packages exactsource only, log
  research/gpu/finish-0108.log; exec helper session29223. No device access.
  Do not describe the expected0108 native-adreno-core image as fullacceleration.

- **0108 core implementation / build-only (2026-10-20):** execution restored
  through approved repository/network commands; NO USB access. Imported MIT
  NativeAdrenoMemory/Cpu/SmmuMemory/Vm/Submit and ArmSmmu4K into third_party;
  patch0108 compiles the core into XNU. Ported complete16KiB CPU ownership,
  separate logical4KiB GPU extents, padding-inclusive budgets/pool allocations,
  PAGE_SHIFT cache attributes and guarded physical maintenance. A6xx framed
  submissions use pinned Linux cache/CCU packets and legacy fences rather than
  A7xx BR/BV state. Local portable SMMU C and ring C++ compilation passed;
  ARM-only MMIO syntax compiled, no tests or hardware runs. Full XNU CI pending.
  This is not a lag fix: dynamic BO arena is not bound to CB0, no userclient,
  Darwin Turnip/Metal plugin or IOSurface rendering deployed. Keep0106 cold-SQE
  fix and existing GMU CB1. docs/native-adreno-0108.md records exact gaps;
  docs/native-adreno-0108-sources.json records source/adapted hashes. Original
  mode-only working changes must remain unstaged. User boots manually.

- **0107 build SUCCESS /image ready (2026-10-20):** CI39458079908 exactsource
  cc09e1f4f779dbffdc3b4e92388aeddfd70b32a2; portable testsSKIPPED. Job118750074042,
  kernel artifact12280989818 ZIP60138774. Kernel17243456 bytes SHA256
  8f1c304069f7004568b4098b201fb2b7c0d6d3710d2bff1309441f43678bdd36,
  published artifact checksum verified; both NativeAdreno adapter log markers
  present. Image shim/out/shim-boot-0107-native-adreno-a618-120hz.img17276928
  bytes SHA256 cb84c79382dd110734e2a37edba6870e4dfb1644dbdb655b82506507ba069148.
  Boot v2 stub and payload roundtrip match. Manifestresearch/gpu/0107-boot-manifest.json;
  finish-0107.log complete, helpersfetch/packageonly (no device commands).
  sweetusb=2 added for existing live-console transport; sweetgpu9/fbtarget120.
  NO0107hardwarebootyet: normal fastboot devices returns empty; asked user to
  connectFASTBOOT or leavebuildunbooted. Wait for reply before dependent device
  work; do not claim handoffreadyonhardware. No flashing/rootedits/radiopower.
  Full GPU acceleration NOT implemented: next code is16KiB BO/IOSurface/VM,
  A6xx context/IBs, NativeAdreno ABI userclient, Darwin Turnip and iOS16 Metal.
  Before proceeding with hardware, check async answer and device visibility.

- **0107 corrected build /Metal ABI:** CI39458079908 on exactsource
  cc09e1f, testsfalse; first CI39457578705 failed during patch application.
  finish-0107.py/package-0107.py updated source/run; first helper log preserved.
  New payload adds sweetusb=2 for existing live-console transport. No device
  action or root edits. Read-only Metal Objective-C class_ro inspection verified
  _MTLDevice name, instanceStart8, instanceSize528 for20H392; upstream expects
 536 foriPadOS17.7.11. _MTLAddDevice exported. docs/native-metal-ios16-profile.json
  records partial static evidence; do not merely change536to528 and publish.
  Upstream plugin has hardcodediPadOS17 display probes that require replacement.

- **0107 initial CI39457578705 failed before compilation:** required patch
  application rejected the one-context-line files.arm64 insertion. Corrected
  to the existing seven-line surrounding context; no hardware image resulted.
  This was patch staging, not a C++/GPU execution result. Job118748365910
  diagnostics saved research/gpu/build-0107-tail.log.

- **UniXNU port started /0107 (2026-10-20):** user supplied NativeAdreno,
  NativeMetal and MakrSas/ios-topaz. Cloned/read and pinned exact commits/hashes
  in docs/unixnu-port-sources.json; details docs/unixnu-sweet-port.md. MIT ring,
  ABI and PM4 imports in third_party/unixnu.0107 embeds adapted A618 ring and
  SweetNativeAdreno adapter into XNU, adopting the completed0106 ring without
  reset, halt, base reprogramming or clearing memory. Preserves sequence, uses
  event4 CACHE_FLUSH_TS/WHERE_AM_I and separate fence/shadow, blocks A7 framing.
  Private synchronous kernel submit only; no userclient/Metal renderer published.
  No extra command submitted by adoption. Host ring C++14 and PM4 C compilation
  passed-Wall-Wextra-Werror (ring upstream formatting warning suppressed); no
  tests run. Need full XNU compilation. NativeMemory assumes4K CPU and cannot
  run unchanged on sweet16K; NativeMetal build targets iOS17 and depends on
  patched Turnip/metal2vulkan. No userspace/root edits or acceleration claim.
  Topaz has newer station/WPA2 patches beyond README; sweet ICNSS18800000
  differs from topaz c800000, so no hardcoded Wi-Fi register/firmware port.
  Phone remains FASTBOOT per0106 handoff; no device action in reference audit.

- **0106 hardware: CP startup and PM4 execution succeed (2026-10-20 09:00
  UTC):** user booted the temporary image to the iOS home screen (performance
  unchanged, expected without a renderer) and returned FASTBOOT; a temporary
  recovery boot pulled logs/gpu-0106-cp-startup-trace.txt 257946 bytes flush62
  SHA256 fd36a5767d03b269550140641992ff6316fe9daddc9f01b413c3275a65a67829.
  All 14 CP SNAP steps show IRQ=0; phase2 reads SQECTL=0/INSTR=0/RB_CNTL=0, so
  the guarded early halt correctly did not write 3 and no error latched. After
  instr_base INSTR=0x70000000, RB_CNTL=0x800020c, auth clean, release SQECTL=1,
  bootstrap CP2GMU=0x4 at 10us. ME_INIT errno=0 phase=3 12/12 submitted/
  completed RPTR=WPTR=0xc status=0x1 IRQ=0 fault=0. leave-secure + fence
  errno=0 phase=6 29/29 fence=1 value=0x0618c0de, proving CP consumed PM4 and
  wrote memory, not merely fetched. CB0FSR=0x400 (previous builds 0x402),
  FAR=0 FSYNR0=0 FSYNR1=0. Root cause confirmed: the unconditional
  CP_SQE_CNTL=3 write to a cold (SQECTL=0) engine latched
  CP_INTERRUPT_STATUS bit7; 0106's removal fixed startup. iOS still
  software-rendered; no renderer or iOS graphics ABI work. Phone is in
  FASTBOOT. Added research/gpu/analyze-0106.py. Live console is already
  available via boot-arg sweetusb=2 + tools/sweet-usbnet.sh (patches
  0072/0073/0077); future images should include it for live logs during boot.

- **0106 built and temporarily booted (2026-10-20 09:45 UTC):** CI
  39397948195 SUCCESS on 6f74eecc4e1527d6c88ca7ea0fcb88d14e393e30;
  portable transitions SKIPPED. Kernel artifact 12253851262 (60101595 ZIP
  bytes); xnu/out-0106/kernel.development.sweet 17241680 bytes SHA256
  1f503f968984b061c5f7f6e344c97c9a4e2bc7fbfaae2d397f87a21f24816677,
  artifact checksum verified. Image
  shim/out/shim-boot-0106-cp-startup-trace-120hz.img 17272832 bytes SHA256
  e10f2ab2d61c3b41a2b9e78cc2a349d1b9603312f1a7addb56299d4cc8f682f6;
  manifest research/gpu/0106-boot-manifest.json. bootv2 unpack round-trip
  matched payload 3a13828e9ad8114fef27362ceadc056245d9ef3695cff4e7c84e6c7d2a8aa23a
  and stub kernel. `fastboot boot` Sending/Booting OKAY 0.656s, no flash/root
  edits. Waiting for user iOS confirmation, then temporary recovery boot and
  pull logs/gpu-0106-cp-startup-trace.txt; summarize with
  research/gpu/analyze-0106.py (per-step CP SNAP tags, phase2 IRQ,
  first_fault, CP2GMU/SQECTL/INSTR). Note: if phase2 SQECTL reads 0x3 the
  guarded early-halt still rewrites 3; snapshots distinguish that from a
  pre-existing latch.

- **0105 hardware result /0106 implemented (2026-10-20):** user booted the
  temporary image to the iOS home screen with unchanged slow performance and
  returned FASTBOOT. Log pulled with a temporary OrangeFox boot:
  logs/gpu-0105-sqe-bootstrap.txt, 224962 bytes, flush9, SHA256
  95cb36a4aa5ee872ef558c558f04f4e2d7984b05b5ae691ea7939050a41230d4.
  GMU clocks/HFI, SMMU CB0/CB1, CP firmware staging/readback and signed ZAP
  PAS phase5/status0 all passed. The 0105 gate worked and localized the fault:
  CP_INTERRUPT_STATUS bit7=0x80 was already latched before authentication and
  before any CP_SQE_CNTL release (power=0 before_auth=0x80 after_auth=0x80,
  sqe_started=0, bootstrap_us=0, CP2GMU=0), so this is an inherited setup
  fault, not an ME_INIT execution failure. CB0FSR=0x402, FAR=0, FSYNR0=1,
  FSYNR1=0x4030000, SID=0 again. Unconditional early `CP_SQE_CNTL=3` is now
  the prime suspect; Linux uses 3 only to halt a running SQE, and the first
  fetch in the disassembly still comes from CP_SQE_INSTR_BASE. 0106
  (`xnu/patches/0106-sweet-adreno-cp-startup-snapshots.patch`) removes that
  write, halts only when the phase-2 SQECTL readback says the engine is
  enabled, and records IRQ/SQECTL/INSTR_BASE/RB_CNTL/CP2GMU after each setup
  block plus a first-fault tag. It does not clear faults, reset, retry or
  advertise a renderer. Host CP compile with -Wall -Wextra -Werror passed;
  local patch application matches 0106-work byte for byte. No full XNU build
  or hardware run yet. Phone is in FASTBOOT.

- **0104 hardware result /0105 next:** user reached iOS with unchanged slow
  performance and returned FASTBOOT. Temporary recovery boot OK; log pulled
  logs/gpu-0104-sqe-zap.txt (304784 console bytes, flush117). Full8033-word
  physical SQE alias verification passed. Signed ZAP PAS phase5/status0 passed.
  CP local IRQ80, ME_INIT errno5 with0 submitted/completed, SQE_STAT PC0,
  CB0FSR402/FAR0/FSYNR0=1/FSYNR1=4030000. SQE cache captured; first8 words0,
  later SRAM contents do not establish valid firmware fetch. No rendering.
  0105 adds IRQ snapshots after GPU power, before/after PAS and gates submission
  on firmware CP_CP2GMU_STATUS bit2 (bounded10ms), distinguishing an inherited
  setup/auth error from a post-release execution fault. Does not clear faults,
  add reset/retry or advertise a renderer. Host C compilation-Wall-Wextra-Werror
  passed, no regression tests run. Full XNU build pending.
  Investigated ACTLR303 generic Qualcomm SMMU-500 profile: rejected because
  pinned SM7150 qcom_smmu_v2_data has no client_match. ACTLR unchanged; prior
  commentary proposing it was corrected before build. Phone left in recovery.

- **0104 compiled and temporarily booted (2026-10-20 08:06:28 UTC):**
  CI39389711266 SUCCESS onbcf90e4cf7b5593fefd61d74b389c256504716fb;
  portable regressions SKIPPED. Selected artifact kernel CRC and published
  SHA verified. Kernel xnu/out-0104/kernel.development.sweet17241656 bytes SHA
  4fdfb29c48dd760498b8cf63625afc498a5faf93925a37d2e7737c9d5f793015.
  Image shim/out/shim-boot-0104-sqe-zap-120hz.img17272832 bytes SHA256
  d4c73f7726d9d9f0b57baa188a13b7b1253396345c869dac481bc94d7d3d74a3.
  Manifest research/gpu/0104-boot-manifest.json; helper finish-0104.log after
  transient metadata retry. Job118518552305; tail helper build-0104-tail.log
  session24432 may still run. ADB reboot bootloader, temporary fastboot boot
  Sending/Booting OKAY0.657s. No flash/root edits. Wait90s then user returns
  FASTBOOT; boot recovery and pull logs/gpu-0104-sqe-zap.txt. Inspect full
  physical readback words/heads, pre-SQE PAS attempted/error/phase/status,
  local_irq, sqe_started, FAR, SQE_STAT and SQE_CACHE after a failure. If PAS
  fails SQE stays halted; don't mistake that for instruction-fetch improvement.
  CPU alias readback does not prove GPU fetch; cache/CP progress still required.
  Full iOS renderer remains unimplemented; no lag/acceleration success claimed.

- **Native iOS ABI investigation advanced during0104 build:** read-only HFS
  cache indexing found actual AGXMetalA11 executable in dyld cache (standalone
  file remains absent), plus IOAccelerator/IOSurface/Metal/GLEngine/GPUSupport.
  2710 images across44 cache-format files; selected headers and export tries
  parsed without extracting2.7GB cache. Local ios-abi/index-cache.py,
  cache-exports.py, trace-ioaccel.py and JSON/disassembly outputs stay ignored.
  Resolved ARM64 shared-cache adrp/add/br stubs to actual IOKit exports before
  interpreting calls. GL client type0, Device1, CL3, Queue4; ContextSubmitExt2
  selector2 input0x88 with variable output; Device info selector2 output0x258.
  docs/ios-graphics-abi.md records verified partial contract and limitations.
  No service stubs, resources/command ABI, shader translation or renderer were
  implemented; no root changes. Capstone was already installed. Previous
  standalone AGX lookup uncertainty is resolved by cached-image identification.
  Helper finish-0104-first-attempt.log records transient GH network failure;
  restarted finish-0104.py with metadata retries in finish-0104.log.

- **0104 compiling:** CI39389711266 exactsourcebcf90e4cf7b5593fefd61d74b389c256504716fb,
  run_tests=false. Host CP compilation-Wall-Wextra-Werror succeeds.0103 CI
  39389309335 canceled, no0103 image booted. Helpers finish-0104.py/package-0104.py
  fetch/hash/package exact source only; never device commands. Current phone
  recovery, plan temporary boot of final signed-ZAP sequence after successful CI.

- **0104 vendor ordering /0103 superseded before boot:** pinned matching
  Lineage device kernel at e03e5ba22586be70673afb5d3c3f70510524e658,
  drivers/gpu/msm/adreno_a6xx.c: a6xx_rb_start calls microcode_load including
  signed ZAP subsystem load BEFORE SQE_CNTL1/CP_INIT. Source/hash recorded in
  docs/adreno-vendor-startup-source.json.0104 follows that device vendor order:
  fixed synchronous adapter callback after GPU_SET/DCVS/aperture/base programming
  authenticates existing signed ZAP, then releases SQE only on success. Logs
  attempted/error/PAS phase/status/response. Metadata/reserved mapping retained;
  no repeat auth after ME_INIT, CP_SET_SECURE_MODE still occurs after ME_INIT.
  0103 diagnostics/physical verification retained.0103 CI39389309335 superseded,
  no0103 image booted. Need final0104 compile/result, full iOS rendering absent.

- **0102 hardware: aperture succeeds; SQE illegal instruction at PC0:**
  User reached iOS and returned FASTBOOT. Temporary recovery boot; pulled
  logs/gpu-0102-cp-startup.txt225639 console bytes flush9. InitialTSB all0,
  aperture errno0 phase3 status0 response0. ME_INIT still timed out:0/12words,
  shadowRPTR0; CB0FSR402 FAR0, FSYNR0=1 FSYNR1=4030000. SQE_STAT PC0 and
  all registers0 except word29=70000000. CP_STATUS1=81000006,
  CP_INTERRUPT_STATUS=80 = CP_ILLEGAL_INSTR_ERROR (XMLbit7). RBBM IRQ0
  masked summary failed to reveal this; CP backend must check local status.
  This is failure before established firmware startup, not proof ME_INIT runs.
  0103 stages SQE separately with full volatile CPU readback and independent
  physical IO alias check before mapping/start; start rechecks without overwriting.
  Moves optional SCM aperture before SQE/ring base programming as Linux order.
  Adds CP local error mask f7, captures on any started-SQE failure, halts only
  poisoned engine then captures bounded1024-word instruction-cache bank plus
  ACTLR/HOST_AP/SQE_AP. No reset/retry, page-zero mapping or speculative SMMU
  change. Optional availability-query transport failures now skip like Linux;
  advertised configuration failure remains fatal. Full acceleration absent.
  Phone currently recovery. Need CI/hardware result before claiming any repair.

- **0102 compiled and temporarily booted (2026-10-20 07:39:45 UTC):**
  CI39386882928 SUCCESS on478dd39d6543c3744662a3dcfee10ea3bec7c7bc;
  portable regressions SKIPPED. Artifact12247599799 ZIP60126914 bytes,
  selected kernel CRC and published SHA matched. Kernel17241536 bytes
  xnu/out-0102/kernel.development.sweet SHA256
  9408a441d344f4177cd9b6662cf5df93e320b33eab2e39450491d22b0e1e8fd4.
  Image shim/out/shim-boot-0102-cp-startup-120hz.img17272832 bytes SHA256
  2d247c903d7614175fcb14a57c256cffa9cb52b6d79c33ed9d938c0d75ab2170.
  Manifest research/gpu/0102-boot-manifest.json; finish-0102.log. Job log
  tail helper build-0102-tail.log may still be downloading (session27605).
  ADB reboot bootloader; fastboot boot Sending/Booting OKAY0.693s. No flash
  or root edits. Wait90s then user returns FASTBOOT; boot recovery and pull
  logs/gpu-0102-cp-startup.txt. Inspect initialTSB, aperture phase/status,
  CP progress/FAR and SQE_STAT[0..50] on failure. Decode captured PC against
  local a630-sqe.disasm; do not infer CPU-shadow null fault was fixed without
  evidence. PAS/fence remain unobserved. Native iOS renderer still absent.

- **Offline SQE disassembly prepared:** cloned official Freedreno/envytools
  revision9836b7b79233f038dd2159f7480417ea54290d12 into ignored research/gpu/.
  Built afuc-build/afuc-disasm directly with GCC/rnn sources and system libxml2;
  no assembler, Lua, Mesa full build or global package installation required.
  Fedora libxml2-devel2.12.10-5.fc43.x86_64 RPM signature checked OK and headers
  extracted only into research/gpu/host-tool-root (runtime2.12.10 matches).
  Local build.py/compile.log and sqe-disassembler-manifest.json record tool/SHA.
  Actual device a630_sqe.fw decoded to research/gpu/a630-sqe.disasm, all local.
  Firmware bootstrap starts atPC0; PC16 reads CP_SQE_INSTR_BASE, PC1a..1d
  reads that memory to locate the dispatch table.0102 SQE_STAT will locate the
  null reader. Decoding is not proof that hardware executes those instructions.
  Tool compile has a harmless old-source unreachable macro warning; no kernel
  dependencies changed. DNF downloaded headers only, did not import rejected
  unrelated repository key or install packages. Keep firmware/disassembly out of git.

- **0102 compiling:** CI39386882928 exactsource478dd39d6543c3744662a3dcfee10ea3bec7c7bc,
  run_tests=false.0101 CI39386538128 canceled/superseded, no0101 image booted.
  Host CP-Wall-Wextra-Werror compilation passes. Helpers finish-0102.py and
  package-0102.py only fetch/hash/package on exact CI success, never device actions.
  Temporary boot planned after success; phone OrangeFox. Full rendering absent.

- **0102 further startup audit:** pinned a6xx hw_init also initializes TSB
  control/base/size and64-bit TSB mode before SQE.0102 adds that omitted step
  under acknowledged GPU_SET, records initial values and requires zero range
  plus address-mode readback1. Byte offsets3e000/4/8/c,3e040 from XML.
  Does not write SECVID_TRUST_CNTL; authenticated ZAP remains required to
  leave secure mode. Null reader cause remains unproved until hardware log.
  Supersede0101 CI39386538128 with0102 so the execution attempt includes both
  audited startup prerequisites and full failure-state capture. No0101 boot.

- **0101 compiling:** CI39386538128 sourcebfb7028d1fdc0135c0c57c53cf9013af458af4cc,
  run_tests=false. Host CP-Wall-Wextra-Werror compilation passes. ARM SCM
  assembly and XNU adapter compile via CI (local ARM clang absent). Helpers
  finish-0101.py/package-0101.py only fetch/hash/package exact source; no
  device actions. Phone in recovery, temporary boot planned after CI success.

- **0100 hardware: shadow fixed, null READ persists /0101 implemented:**
  User reached iOS unchanged, FASTBOOT confirmed by read-only enumeration.
  Temporary recovery boot; logs/gpu-0100-cp-shadow.txt239079 console bytes,
  flush13. HFI passes; CP SHADOW70010100 is now correct. ME_INIT still errno60,
  submitted12 completed0 shadowRPTR0 WPTR12; CB0FSR402 FAR0 FSYNR0=1
  FSYNR1=4030000. Missing shadow was NOT established as the null reader.
  0101 adds fixed CB0 SCM aperture: optional INFO availability query forMP0c/1b,
  advertised call uses4 value args{ffff0000,ffffffff,ffffffff,ffffffff} inx2..5.
  Normal STD64 SIP convention retained (0x42000000 base is correct; do not
  accidentally change to FAST). Unsupported query/call availability skips as
  Linux; advertised call failure stops SQE. Existing PAS calls gain zero fourth
  arg; no generic SCM exposure or secure-register fallback. Logs phase/status.
  After a submitted CP failure, captures51 SQE_STAT words (selector2420/data2424)
  and directRPTR/CP_STATUS1/CP_INTERRUPT_STATUS. First SMMU FAR still captured
  before selector writes. Need CI/hardware result; full rendering absent.
  Phone currently OrangeFox; no partition writes or iOS root edits.

- **0100 compiled and temporarily booted (2026-10-20 07:20:01 UTC):**
  CI39384752447 SUCCESS on3a679fbe99902045b6fb34f4bf6be2f2a9950bc5;
  portable regressions SKIPPED. Artifact CRC/published SHA verified.
  Kernel xnu/out-0100/kernel.development.sweet SHA256
  1d4c230cc82b7145219abb7829cd99cbf681cb610fada9354741ba67db641c32.
  Image shim/out/shim-boot-0100-cp-shadow-120hz.img17272832 bytes SHA256
  f14a66683795414bda2aa65b414adc8e86e58b1c1b0f194bc1548dde28b76cc9.
  Manifest research/gpu/0100-boot-manifest.json; helper finish-0100.log.
  ADB reboot bootloader; fastboot boot Sending/Booting OKAY0.697s. No flash
  or root changes. Wait90s for worker then user returns FASTBOOT; boot recovery
  and pull logs/gpu-0100-cp-shadow.txt. Compare FAR, SHADOW70010100, shadow
  RPTR/WPTR12 and CB0FSR; if ME_INIT completes inspect subsequent PAS/fence.
  Full renderer/native iOS ABI still absent; no acceleration/lag claim.

- **Further CP audit, not implemented:** pinned adreno_hw_init optionally
  queries and invokes qcom_scm_set_gpu_smmu_aperture(CB0) before SQE. Protocol
  qcom_scm.h: MP service0x0c, command0x1b,4 value args{ffff0000,ffffffff,
  ffffffff,ffffffff}; INFO query as usual. Current restricted PAS transport only
  supports3 args and does not implement this prerequisite. If0100 still faults,
  audit this path and capture SQE_STAT before changing mappings. Do not claim
  that SHADOW0 caused FAR0: syndrome is a READ, and shadow writes alone are
  not proof of the null reader. Linux CP progress shadow path is nevertheless
  a real missing implementation fixed by0100. No SCM aperture call was attempted.
  Local pinned a6xx_gpu_state.c documents indexed-register auto-increment;
  use bounded51-word CP_SQE_STAT capture (addr0x2420,data0x2424) after failure
  if needed, then decode PC with official Mesa qrisc tools. No raw memory probe.

- **0100 compiling:** CI39384752447 source3a679fbe99902045b6fb34f4bf6be2f2a9950bc5,
  run_tests=false. Host CP compilation-Wall-Wextra-Werror succeeds with latest
  0089 core header and0097 DMA header (an initial attempt selected outdated
  0088 header and failed on the missing GMU fence macro; corrected include path).
  Helpers finish-0100.py/package-0100.py only fetch/hash/package exact source;
  no device actions. Current device recovery, plan temporary boot after success.

- **0099 hardware fault captured /0100 shadow fix (2026-10-20):**
  User reached iOS unchanged, returned FASTBOOT. Temporary recovery boot and
  logs/gpu-0099-cp-fault.txt pull:229110 bytes flush10. GMU/HFI still passes.
  ME_INIT errno60, FAR0 FSYNR0=1(read,L1), FSYNR1=0x4030000, SID0;
  SQE70000000/RING70008000 correct, SHADOW0, CB0FSR402, RPTR0/WPTR9.
  Null access established, shadow as its cause remains a hypothesis.
  Pinned Linux allocates shadow for WHERE_AM_I-capable SQE and reads its memory
  instead of CP_RB_RPTR.0100 programs mapped shadow70010100 (CPUoffset14100),
  initializes it before SQE, verifies register readback, appends WHERE_AM_I to
  every publish and uses validated shadow for ring progress. Same20 mapped
  pages, no zero-page mapping, no generic trust-register bypass. Records real
  submitted/completed DWORDs including3-word WHERE_AM_I. Fence offsets0/4
  stay separate. Need CI then hardware result; no rendering fix claimed.
  Also audited preemption: pinned Linux returns immediately for single ring,
  so no invented preemption records added. Phone currently OrangeFox.

- **0099 compiled and temporarily booted (2026-10-20 07:03:13 UTC):**
  CI39383140126 SUCCESS on6a4de0a69b69e146141ce92970429953ec357d2c,
  portable tests SKIPPED. Artifact CRC and published SHA verified.
  Kernel xnu/out-0099/kernel.development.sweet17241488 bytes SHA256
  ed9798dddad9da9fc2ac12b686b42655c7b8fc04dd0320fb4d788408e5320cfc.
  Image shim/out/shim-boot-0099-cp-fault-120hz.img17256448 bytes SHA256
  cad9736b94b1652ee6db0a4ced5d5fa9cebcb56f06245e34555aa0f669d3f595.
  Manifest research/gpu/0099-boot-manifest.json, helper finish-0099.log.
  ADB reboot bootloader, fastboot boot Sending/Booting OKAY0.699s. No flash
  or root edits. Wait90s then user must return FASTBOOT; boot recovery and
  pull logs/gpu-0099-cp-fault.txt. Inspect first FAR/syndrome and SQE/ring bases
  to resolve0098 CB0FSR0x402. This is a diagnostic image, not a lag fix.

- **0099 fault capture compiling:** exactsource6a4de0a69b69e146141ce92970429953ec357d2c,
  CI39383140126, run_tests=false. Earlier39383070320 canceled to ensure GPU
  diagnostic register pairs use32-bit bus accesses. Includes all0098 changes
  plus first-fault VA/syndrome/address readback; no speculative mapping changes.
  Local helpers finish-0099.py/package-0099.py prepare integrity-verified image
  only after exact CI success. They never boot the phone. Current device recovery.

- **0098 hardware result: HFI passes, CP translation fault (2026-10-20):**
  User reached iOS with unchanged performance and returned to FASTBOOT.
  Temporary recovery boot succeeded; logs/gpu-0098-sqe-cp.txt contains234926
  console bytes, flush13. GMU boot errno0 phase6, FW0x10110000; all5 HFI
  replies succeeded, no drops, queue indices drained. CP map20 pages+guard
  passed (CB0 slot1); ME_INIT timed out phase2, submitted9 completed0,
  RPTR0 WPTR9 status0x800005 IRQ0 CPfault0. CB0FSR0x402 vs CB1FSR0x400:
  actual GPU translation fault, not the earlier Format-only false failure.
  Fault VA/syndrome were not logged by0098. Patch0099 captures FAR, FSYNR0/1,
  stream SID, TTBR0/TCR/SCTLR and GPU SQE/ring/shadow register readback before
  any clearing/probe. Do not infer which address faulted without this evidence.
  PAS and fence path never reached; full acceleration remains absent.
  Phone in OrangeFox; no partitions flashed, no root edits.0098 manifest updated.
  Local build-0098.log fetch failed HTTP2; CI success and artifact hashes were
  checked, but do not claim the incomplete file is a saved complete build log.

- **0098 compiled and CP attempt booted (2026-10-20 06:45:06 UTC):**
  CI39381215265 SUCCESS on d110d0d; includes0093 high-bank fix and0095-0098
  SMMU/HFI/CP changes. Portable regressions SKIPPED. Kernel artifact12245856852
  xnu-kernel ZIP60106156 bytes, selected entry CRC/SHA matched.
  Kernel xnu/out-0098/kernel.development.sweet SHA256
  97c6ad6496ef1ac9af374723c241299c1c18dfdac976ef3c2882fbea5c5e84b4 (17241488 bytes).
  Stage9 shim/out/shim-boot-0098-sqe-cp-120hz.img SHA256
  804c87ff169ca5a6857efcf2e3914b73635c07494821857ab1f66c44123fd613 (17256448 bytes).
  Manifest research/gpu/0098-boot-manifest.json, build log build-0098.log,
  helper output finish-0098.log. Firmware ADT sweet-0096-cp-firmware.adt.
  ADB rebooted recovery to bootloader; fastboot boot returned Sending/Booting
  OKAY (0.720s). No flash/root edits. **Await fresh driver log after90s**;
  bootloader acceptance does not prove iOS boot, HFI, SQE/CP, ZAP or fences.
  User requested attempting execution. Full graphics ABI/renderer and lag fix
  remain unfinished; no accelerator service or performance success is claimed.

- **0098 final CP audit build dispatched:** commit
  d110d0d (full source hash in git) pushed; CI39381215265 compiles patches
  through0098 with portable regressions skipped. Older0096 CI39380282997 and
  0097 CI39380737829 superseded/canceled; no images from those runs booted.
  Local firmware ADT shim/out/sweet-0096-cp-firmware.adt contains verified
  device GMU/SQE/signed ZAP metadata/code. Helpers finish-0098.py and
  package-0098.py prepare a stage9 image when the exact CI source succeeds.
  Both helpers perform only download/integrity/packaging, never device commands.
  Device is in recovery after reading0095. Native iOS integration investigation
  confirms AGXMetalA11 and Apple Metal/OpenGLES bundle identities; a standalone
  AGX executable is not resolved by the HFS reader. No userclient/renderer ABI
  has been decoded, and no fake accelerator class is registered. Full rendering
  and the requested lag fix remain unfinished.

- **CP clock/fence audit (0098):** source audit added the required legacy
  DCVS request selecting performance index1 (180MHz) before GPU_SET/GX access.
  ARC voltage selection alone does not set GX frequency. Bounded OOB23/ACK31
  handshake, DCVS_RETURN check. Fence visibility is polled separately after
  ring drain; posted AXI writes cannot be assumed immediately CPU-visible.
  Added post-ME_INIT SMMU fault snapshots. Host CP C compilation succeeds;
  local ARM compile helper is unavailable because ~/swift/usr/bin/clang is
  absent. Full compile must use macOS CI. Source3005b17 CI39380737829 is
  superseded for CP hardware work by this final audit refinement. No CP
  execution or native iOS acceleration has been observed yet.

- **0095 hardware: GMU actually runs; HFI INIT blocked (2026-10-20):**
  user reported iOS boot and returned toFASTBOOT. Temporarily booted recovery;
  pulled logs/gpu-0095-gmu-hfi.txt (256695 console bytes, flush19), SHA256
  b6f74f35c647c652f94c677f0024917c3d3d068bcebf07168d979f60a1809a64.
  All16 SMMU pages/guard passed; FSR0x400 now correctly treated as metadata.
  CB0 SCTLR=0, stream slots1..4 free, slot0 correctly routes5 toCB1.
  xo.lvl cmdDB address confirmed **0x30090**, levels0,32,80,128.
  MIN_SVS votes both0x400202. GMU boot errno60 phase5, HFICTRL_STATUS1:
  M3 init, GX OOB and SPTPRAC passed; HFI INIT/version exchange did not.
  No CP commands ran.0096 implements CP backend, initial CI39380282997.
  Patch0097 aligns HFI ACK timeout with Linux's1s (ours incorrectly10ms),
  matches echoed command ID+sequence (actual ACK type/length still checked),
  and captures queue/doorbell/SMMU/response diagnostics. Rebuild before
  attempting stage9. Phone currently in recovery; no flash/root modifications.

- **SQE/CP backend implementation in progress (2026-10-20):** patch0096
  adds private 96K wired GPU allocation, 4K tables for inactive CB0/exact SID0
  (ASID0x62), 20 page probes + guard, RO/executable SQE and RW/NX ring/fence.
  Separate PAS metadata page cannot overlap GMU/GPU data. Kernel-only CP path
  waits for GMU GPU_SET acknowledgement, starts SQE with 32K ring and A618
  protection ranges, submits ME_INIT, then requires signed ZAP PAS13 auth
  before CP_SET_SECURE_MODE and memory-write/cache-flush fence readback.
  No direct SECVID_TRUST_CNTL fallback, generic SCM access or userspace PM4.
  Live reserved ZAP range is 0x97f15000/0x2000, not mainline's different range.
  Firmware tool now accepts optional checksum-verified vendor SQE/MDT/B02;
  blobs stay local. Host CP/SMMU C compilation succeeded; not yet exercised.
  This is command-engine bring-up, not native iOS rendering. Do not register
  IOAcceleratorES as a successful stub: backboardd expects an actual userspace
  rendering/driver ABI. Captured log repeatedly matches IOAcceleratorES and
  IOAcceleratorESLegacy -> none. Native graphics integration remains a real
  separate implementation requirement; no lag fix is claimed.
  0095 CI39379234572 succeeded on46ea342; image
  shim/out/shim-boot-0095-gmu-hfi-120hz.img SHA256
  1100ea6d93ed99a2cbebd58b18d76eea3232eb6907ddd979fc06f5077981ab5a.
  Rebooted recovery to bootloader and temporarily booted0095 (Sending/Booting
  OKAY,0.675s). Awaiting user return toFASTBOOT to read GMU result while
  implementing CP. No flash/root changes. Earlier build-only direction was
  superseded by user request to execute GPU commands and fix graphics.

- **0094 booted iOS; SMMU false failure found (2026-10-20):** user reports
  successful home screen, unchanged performance; now requests actual SQE/CP
  execution and iOS graphics integration. User returned to FASTBOOT on request.
  Temporarily booted recovery and pulled `logs/gpu-0094-gmu-hfi.txt`: 239956
  console bytes, flush #15. DMA allocated PA=0x1047e8000; previous truncation
  panic did not recur. All 16 pages translated and unmapped guard faulted.
  Stage 8 stopped errno=5 because cleanup wrongly required the entire FSR to
  clear; read-only Format[10:9]=2 leaves 0x400. ARM IHI0062D.c 16.5.9 confirms
  Format is not a W1C fault. Patch0095 masks actual fault bits, keeps stalled
  status fatal, and logs nonsecure CB0/stream baseline for the CP backend.
  GMU stayed reset; no HFI transaction or GPU command executed on 0094.
  The latest instruction authorizes attempting GPU command execution, superseding
  the earlier no-test/build-only direction. Full acceleration is still absent;
  do not claim a lag fix without rendering and iOS integration evidence.

- **0094 temporary boot sent (2026-10-20):** user explicitly superseded
  build-only direction and requested booting the new image from FASTBOOT.
  Rechecked image SHA-256
  `053fda6fa9e9515c05ef1006d6532e60914f05c7f63c5bf79111c789518cb31a`.
  `fastboot boot shim/out/shim-boot-0094-gmu-hfi-120hz.img` returned Sending
  OKAY (0.525 s), Booting OKAY (0.130 s), total 0.673 s. No partition flashed.
  Bootloader accepted the image; iOS startup and GMU/HFI results pending.
  Allow at least 90 seconds for the deferred GPU worker (scheduled after 60 s).
  Need screen result and a fresh log before claiming startup success or that
  the previous panic is resolved. Full GPU acceleration remains unfinished.

- **0094 compile-only artifact complete (2026-10-20):** implementation
  commits `ca70c43` and `4a6565767c523387f1fa3dbbdea3909ab16febeb` are pushed to
  `myfork/codex/adreno-power-audit`. Includes existing 0093 high-bank repair.
  CI `39377419088` **SUCCESS** on exact source 4a65657; kernel build/link and
  embedded-code checks succeeded, portable test step **SKIPPED** (run_tests=false).
  Kernel artifact `12243978150` (xnu-kernel, ZIP 60117274 bytes); selected kernel
  entry CRC and published SHA-256 matched. Kernel
  `xnu/out-0094/kernel.development.sweet` (17273616 bytes), SHA-256
  `5cb7fcbe81ff39b06b7429dfe9bdcafb5da3a7f7c5ea6e2243dfb4d3ffbab69b`.
  Local stage-8 image `shim/out/shim-boot-0094-gmu-hfi-120hz.img`
  (17215488 bytes), SHA-256
  `053fda6fa9e9515c05ef1006d6532e60914f05c7f63c5bf79111c789518cb31a`.
  Manifest `research/gpu/0094-boot-manifest.json`; finish/package helpers
  `research/gpu/finish-0094.py`, `research/gpu/package-0094.py` and log
  `research/gpu/finish-0094.log`. Reused known shim/base DTB and unchanged
  firmware ADT (SHA-256 73f9c127...76b320). Presentation target stays 120 Hz;
  this proves neither panel scan rate nor rendered FPS. **No tests, temporary
  boots or flashes; installed root image and phone untouched this turn.**
  New opt-in sweetgpu=8 executes GMU/PDC/RSC cold boot plus HFI initialization;
  it is untested on hardware. **Full GPU acceleration is still absent**:
  ZAP/SQE, GPU stream mappings, CP submission/fences, host interconnect policy,
  recovery/thermal management and the native iOS graphics ABI remain unfinished.
  Do not present this artifact as a lag fix or as verified to boot. Next code
  work is the protected firmware / SQE / CP hardware backend, then actual
  rendering and iOS integration. User's latest direction remains build only.

- **Current request: build only, no tests (2026-10-20):** user supplied a
  phystokv_range panic photo after another agent's 0092 attempt. Reviewed new
  commits through `9de2ec6` (also remote branch HEAD). Photo reports an illegal
  low PA with physical base 0x100000000. The original cache-attribute flush
  shifts a 32-bit ppnum_t before widening; `0093-sweet-highmem-cache-sync.patch`
  correctly uses ptoa(), whose XNU macro casts to vm_address_t before shifting.
  This explains a high-bank DMA allocation panic, but has not been confirmed
  by a subsequent boot. Preserve this fix. Latest local stage-7 capture stops
  after SMMU IDs, before TCM/DMA completion markers; it does not itself record
  the pictured panic or prove an exact failing call stack. Stage-6 control
  capture records 32768 verified TCM bytes and errno=0. Neither is evidence of
  GPU execution. User explicitly requests implementation/build without tests:
  **do not boot/flash, run portable regressions or device tests this turn.**
  Implemented ordered patch 0094 (`ca70c43`, refinement follows): polled legacy HFI RPC with echoed-header
  validation, bounded ACK/debug queues, fault/timeout poisoning, INIT/version/
  performance/bandwidth/end-init exchange, and A618 PDC/RSC/M3/GX handshake.
  Capture `gpu-0092-dma-hfi.txt`: 170228 bytes, SHA-256
  `c84980e78eb2a8723aebfd7fcca806deedf903c3337c7aa4a024715613cc31b8`.
  Capture `gpu-0092-stage6.txt`: 269447 bytes, SHA-256
  `a93b8c64ef39395868f21900af29f2c2748c484dc5da26e1cdb322d36d96d213`.
  New opt-in `sweetgpu=8`; stages 1-7 keep their prior behavior. Uses documented
  minimum 180 MHz GPU / 200 MHz GMU OPPs and raw command-DB ARC votes. This is
  GMU firmware startup infrastructure, **not CP/SQE submission or native iOS
  graphics acceleration**. Host interconnect bandwidth, protected ZAP/SQE,
  GPU stream mappings, command execution/fences, recovery/thermal management,
  and the iOS graphics ABI remain unfinished. Editable sources:
  `research/gpu/0094-work/`; CI tests are now separately selectable. Initial
  build `39377178087` on ca70c43 was dispatched with run_tests=false; its test
  step is confirmed skipped. Final source audit pinned the RPMh binding:
  MIN_SVS=48 (GFX/CX raw tables round to64, MX to192); adjusted selector to use
  the binding value, added RSC idle and AO/CM3 fault checks. Rebuilding that
  refinement on `4a6565767c523387f1fa3dbbdea3909ab16febeb`, CI
  `39377419088` (run_tests=false). C HFI/GMU compilation passed with
  -Wall -Wextra -Werror; no binaries/tests were executed. Superseded initial
  CI was explicitly canceled; no hardware operations. Older 0093-only CI `39375252490` on
  9de2ec6 also succeeded, but no repaired hardware boot is recorded.

- **0091 hardware pass / 0092 DMA backend (2026-10-20):** user reported
  iOS boots but performance remains poor; requested working driver using
  latest information. Temporarily booted OrangeFox and pulled
  `logs/gpu-0091-gmu-tcm.txt` (229047 bytes, flush #13), SHA-256
  `f2bc97421bb61a8dd1ceef684691360401cede2f8e991e30175ad23e631697ac`.
  **All six GMU/SMMU clocks PASSED**, GMU SYSRESET=1, POWER_STATUS=0xd4,
  AHB_FENCE_STATUS=0. SMMU IDR0=0x48016605, IDR1=0x20008005, IDR2=0x7511.
  **All 32768 GMU firmware bytes uploaded and read back correctly.** M3
  remains reset; no commands execute, so no performance increase expected.
  Implementing patch 0092 with actual uncached wired DMA allocation, 4K
  SMMUv2 translation tables and bounded legacy HFI queue transport.
  Stage 7 maps only the 64K GMU shared region, using inactive CB1 and a
  free exact-match stream slot for GMU kernel stream 5. CB0/GPU stream 0,
  user stream 4 and protected banks remain untouched. Hardware ATOS must
  verify all 16 pages and an unmapped guard before publishing stream 5.
  Requires fresh M3 reset, GX collapse/clamp, measured ID profile, free
  context and no existing matching stream. Buffers retained on failure.
  HFI has bounds/space/packet checks; no firmware execution or accelerator
  service is implemented. Also inspects `xo.lvl` read-only for PDC resource
  identity (write whitelist unchanged). Host C compilation passed for
  SMMU/HFI, source list and BSD patch application checks passed. Next:
  CI `39352692300` rejected the new-file diff headers before compiling
  (`dev/null: No such file or directory`). Added explicit new-file modes;
  complete patch application including all new files now passes and matches
  editable sources byte for byte. Replacement CI `39353002324` superseded
  before boot: allocator now clears inhibited-cache memory with aligned
  volatile words instead of bzero (avoids Device-memory DC ZVA faults).
  Complete patch revalidated. CI `39353648283` also superseded before boot:
  ARM SMMUv2 spec audit found AFE/TRE readback is UNK/SBOP for long
  descriptors; compare only defined control fields, and test CB1.M for
  inactive state rather than requiring every reset bit zero. Reject all-ones
  PAR even on guard probes. CI `39354082431` succeeded on exact source
  `f0c5dab` (kernel artifact `12233929276`, `xnu-kernel`); the artifact was
  re-downloaded and its ZIP CRC passed. Kernel SHA-256
  `1ff583218737c86ecac55ae829301671b51c6011ad4b33af9e6a303a1a1b73b7`
  (17257032 bytes) matches `xnu/out-0092` byte for byte. An independent
  `tools/mkpayload.py` rebuild produced payload SHA-256
  `a4cec076aaaddb14f300a217075478095d2f3aa6d346cdfa82bec996ab07d822`,
  and a fresh shim rebuild produced stage-7 image
  `shim/out/shim-boot-0092-dma-hfi-120hz.img` (17215488 bytes, SHA-256
  `325cae1dc265472941a67fdf7dbfb354c5f7e0c1ce867b8707d230adbd481750`);
  both are byte-identical to the artifacts codex built. Bootv2 unpack round
  trip matches the payload. Embedded ADT SHA-256
  `73f9c127a087755491fe2cd01fdb81d8b6b35007fa0410bab2dafe928a76b320`;
  GMU firmware CRC32 `0x574c0d4b`/SHA-256 `9ef10b2b...bbacbf2e` matches the
  local 32768-byte blob and stored ADT properties. Manifest
  `research/gpu/0092-boot-manifest.json`. Next: temporary boot on the
  connected FASTBOOT phone, wait at least 90 s, then pull
  `logs/gpu-0092-dma-hfi.txt`. Editable sources `research/gpu/0092-work/`.
  Root image unchanged, no flash.

- **0091 verified build / temporary boot (2026-10-20):** CI
  `39350600573` succeeded on `2743562bdff764a27cccdb105d84577d5d1c8c4c`;
  portable regressions, XNU compile and built-kernel markers all passed.
  Kernel artifact `12232857474` (ZIP 60034907 bytes), CRC/SHA verified.
  Kernel `xnu/out-0091/kernel.development.sweet` SHA-256
  `913878692b86b2f37dd85fb8e75b4f55e8b1d54e04c6899a529a9c89e79930c2`.
  Stage-6 image `shim/out/shim-boot-0091-gmu-tcm-120hz.img` SHA-256
  `63de6f0be72a48a5856466f1919095e6ca94548ff1f2c49465430c3224dc61b3`.
  Packaging kept the known shim/base DTB and 0089 firmware ADT; bootv2
  unpack round trips passed. Manifest `research/gpu/0091-boot-manifest.json`.
  Rebooted OrangeFox to bootloader and `fastboot boot` stage 6 returned
  Sending/Booting OKAY (0.690 s). Await at least 90 seconds for the 60-second
  worker and user boot result; return to FASTBOOT, temporarily boot OrangeFox,
  pull `logs/gpu-0091-gmu-tcm.txt`. No flash or root-image changes. Hardware
  clock/firmware results pending; rendering is still unimplemented.

- **0090 hardware result / 0091 correction (2026-10-20):** user confirmed
  iOS reached home screen and returned to FASTBOOT. Temporarily booted
  OrangeFox and pulled `logs/gpu-0090-gmu-tcm.txt` (242594 bytes, flush #12),
  SHA-256 `2e49f50db1c9e57e87adfb7df627387e4c16c1fc09d0f29cbe88da08f48cfbee`.
  CX passed. GCC three branches, required GPUCC CXO and AHB passed
  (`completed=5`). Next extra GPUCC CX_SNOC_DVM at `0x108c` returned zero
  after enable: `errno=5`. No GMU/SMMU ID read or firmware upload.
  Audited both full clock lists: GMU uses CX_GMU, CXO, DDRSS AXI, MEMNOC;
  SMMU uses GPUCC AHB, GCC MEMNOC and GCC SNOC_DVM_GFX. Neither uses
  GPUCC CX_SNOC_DVM. Patch 0091 removes this second extra dependency;
  six unique required branches remain, including GCC SNOC. Host regressions
  pass with both unused gates zero and assert no access to either; required
  CXO failure still stops setup. Next: build and temporarily boot 0091,
  capture `logs/gpu-0091-gmu-tcm.txt`. Commit `2743562`; CI
  `39350600573` passed; stage-6 temporary boot underway, see above.

- **0090 built and temporarily booted (2026-10-20):** CI
  `39349325350` succeeded on exact source `2054211`, including portable
  regressions, XNU compile and kernel marker checks. Kernel artifact
  `12233165630` (ZIP 60034241 bytes) CRC/SHA verified; local kernel
  `xnu/out-0090/kernel.development.sweet` SHA-256
  `9e47bad1b65e5e0a2d5e207d966bc70557939dec0f609fe2193c02af0e843d30`.
  Packaged images with preserved shim/base DTB and 0089 firmware ADT;
  bootv2 unpack round trips passed. Stage-6 image
  `shim/out/shim-boot-0090-gmu-tcm-120hz.img` SHA-256
  `acf0b27a1a0870ab7a535ac2685bbba299ff990848ac74a8c06235442c9d134a`.
  Manifest `research/gpu/0090-boot-manifest.json`.
  Rebooted OrangeFox to bootloader and temporarily booted stage 6:
  Sending/Booting OKAY (0.674 s). User reached iOS; retained log collected
  in OrangeFox, see above. No flash; no rendering acceleration.

- **0089 hardware log / 0090 correction (2026-10-20):** user confirmed
  stage-6 image reached iOS and returned to FASTBOOT. Temporarily booted
  OrangeFox and pulled `logs/gpu-0089-gmu-tcm.txt` (248135 bytes, flush #14),
  SHA-256 `d6f20d210f4e295b35b1428893574a8e84d997720e16114dd8a601e3166f2705`.
  CX passed again; GCC PLL checks and three bus branches passed. Clock setup
  stopped with `errno=5 block=0 offset=0x1004 value=0 completed=3`:
  CXO_AON enable did not read back. No GMU/SMMU status read or firmware upload
  occurred. This is a failed clock-stage check, not a firmware failure.
  Pinned SM7150 GMU DT lists GMU, CXO, DDRSS AXI and MEMNOC clocks; it does
  not request CXO_AON. Patch 0090 removes that extra branch, retaining
  required CXO/GMU and bus checks. Regression covers zero-valued unused AON
  with no access, and required CXO stuck-off still failing. Portable checks
  passed. Commit `2054211`; CI `39349325350` running on that exact source.
  Next: capture the 0090 hardware log after its temporary boot; see above.

- **0089 verified build artifacts:** CI `39347214431` succeeded on
  `b1a4b3c`, including portable checks, XNU compile and kernel marker checks.
  Kernel `xnu/out-0089/kernel.development.sweet` SHA-256
  `6d0f9903c9c0f97c1ecce4c56e0d24955900af21b1669b23e5edfbd08549fda8`.
  Tested stage-6 image `shim/out/shim-boot-0089-gmu-tcm-120hz.img` SHA-256
  `c92cf13ed47b6fcb60f5559d1023cd836da3d1a7cadf2c181ac10fef57440911`.
  Manifest `research/gpu/0089-boot-manifest.json`; firmware ADT
  `shim/out/sweet-0089-firmware.adt`. Rendering remains unavailable.
  Next execution research: `research/gpu/smmu-gmu-next.md`.

- **0089 postmarketOS GMU port in progress (2026-10-20):** user requested
  continuing toward full acceleration using postmarketOS's Redmi Note 10 Pro
  sources. Pinned sm7150-mainline/linux revision
  `fd78d17954a86ea6ceb0c535e618ea129e09f8dd` (v7.2 branch); reference files and
  SHA-256 manifest in `research/gpu/pmos-pinned/`. Important correction:
  Linux deliberately uses a no-op GX power-domain enable; the GMU owns GX
  startup. Do not restore the old manual GX reset experiment.
  Patch 0089 implements the GPUCC/GCC GMU 200 MHz clock sequence, validates
  the inherited fixed GPLL0 rate, retains shared votes and checks branch/RCG
  status with deadlines. `sweetgpu=5` runs CX plus clocks and reads only
  CX-side GMU status and SMMU IDs; `sweetgpu=6` additionally uploads the raw
  32 KiB firmware into TCM only while SYSRESET is already asserted, checking
  CRC and every uploaded word. It never releases the M3. GPU core accesses,
  firmware execution, SMMU configuration and rendering remain absent.
  Firmware ADT packaging: `tools/sweet-adreno-firmware.py`, local blob only,
  mandatory expected SHA-256; no blobs committed or included in CI.
  `python3 tools/check-adreno-core.py` passed host checks for power/PLL
  preconditions, bit preservation, stuck bus/RCG/UPDATE, invalid reads,
  malformed/corrupt firmware, reset ownership and partial upload failure.
  Patch applied to complete pre-0089 sources and host compilation passed.
  First CI `39346823712` is superseded by the PLL-mode correction: follow
  Linux ACTIVE polling in vote mode and accept an inherited running direct
  mode without changing its controls. Both cases pass host checks. Replacement
  CI pending; no 0089 hardware result. Phone absent from host ADB and fastboot
  on this continuation; user was asked to connect it in FASTBOOT.
  Editable sources: `research/gpu/0089-work/bsd/dev/`.


- **0088 new core / fresh audit (2026-10-19):** user requested a new driver
  based on Linux/Freedreno after repeated failed attempts. Pulled
  `logs/gpu-reaudit-20261019.txt` from OrangeFox; SHA matches `gpu-cx-power.txt`
  exactly, so there is no newer captured hardware result. Audited all seven
  text logs: no GPU command completion, no accelerator service. Patch 0088
  adds a separate portable A618 CX/PM4/ring core and XNU power adapter; removes
  unsafe core aperture reads and disables the incomplete stage-5 GX sequence.
  It adds fresh MX/CX resource checks, all-ones rejection and power settling.
  PM4/ring code is not attached to hardware: firmware, SMMU, clocks and the iOS
  graphics interface remain absent. **This is not full acceleration.** See
  `docs/adreno-port.md` for exact reuse, audit and remaining dependencies.
  Portable C host compilation succeeded (`gcc -Wall -Wextra -Werror`), and
  patch application to the 0087 sources succeeded. Cross-GCC has no libc
  headers here; first CI run `39298707871` rejected the short-context bsd/conf/files
  patch hunk before compiling. Expanded it to the full surrounding entries;
  replacement CI `39299055941` succeeded on `7321f15`, including the
  built-kernel marker checks. Kernel artifact downloaded and checksum-verified:
  `xnu/out-0088/kernel.development.sweet`, 17,240,104 bytes, SHA-256
  `0d1750b798bb569ed6d7071b764c84782092052c2df77579f47d59fed8066439`.
  Packaged `shim/out/shim-boot-0088-a618-cx-120hz.img` (stage 4, 120 Hz,
  same ADT as 0087, cmdline ends `sweetfb_noconsole=1 sweetfb_hz=120
  sweetgpu=4`), SHA-256
  `9d495db439da798f2013cbc79b0d4da7a354aebf27b3f0b5735b0d2251fdedce`
  (payload `634822b8862377c84349caba6b2626c70a5aac0c24277995f2dcf4ca050687ff`);
  bootv2 unpack round-trip matches the payload.
  **Booted 2026-10-19 ~23:53 local; stage 4 CX power PASSED on hardware.**
  Touch worked (GT9896, pid 0x39383936 = "9896", fw 03.02.00.0c). At 60 s
  the A618 core logged `SWEET ADRENO: CX result errno=0 control=0xf8228000
  status=0x80001fe0 waited_us=1` (status bit 31 set) and finished; no panic,
  display and touch survived. This is the first verified GPUCC CX power-up
  under XNU. Rendering is unchanged: `backboardd` still matches
  `IOAcceleratorES -> 0 found`; 0088 performs no GPU core access and stage 5
  remains disabled. Log `logs/gpu-0088-a618-cx.txt` (277,866 bytes, flush
  #25, SHA-256
  `d2d9582d458b68a118db580a31e085f06acf17be7fa4826d47ed3e9c33e77946`,
  ignored); raw ring `logs/gpu-0088-a618-cx.txt.bin`. Phone returned to
  fastboot; no flash.
  Sources: `research/gpu/0088-work/bsd/dev/` (editable), patch 0088 (tracked).


- **0087 stage-4 attempt inconclusive (2026-10-19 ~23:07 local, read first):**
  `fastboot boot shim/out/shim-boot-0087-cx-power-120hz.img` booted and
  reached SpringBoard, but the 45 s SweetTouch probe read an all-zero Goodix
  info page (`0x4022` zeros, `pid=00000000`) and logged
  `SWEET TOUCH: no unique supported IC (goodix=0 fts=0 dead=0); stopping`;
  touch was dead. The 60 s marker `GPUCC CX power v1 starting` is absent
  from the log, so the CX GDSC write never executed: this is **not** a CX
  gate result, and the touch code is unchanged from 0086 (whose probe
  selected GT9896). No panic. The phone was force-rebooted before 60 s and
  is now in OrangeFox. Read-only log: `logs/gpu-cx-power.txt` (167,754
  bytes, flush #5, SHA-256
  `daa90d47582dcd4f8decb37ea8792e667bb596c758853ee70d812ca972ee1876`).
  Next: cold-boot and retry stage 4, or fall back to the 0086 image; stage 5
  remains blocked.
- **0086 hardware result: raise-only `gfx.lvl` passed (2026-10-19 ~21:35
  local):** the 0086 image booted on the phone; iOS reached SpringBoard and
  stayed usable (lag remains, as expected with no GPU). At
  60 s the kernel re-read `mx.lvl` (`0x3`, level 321) and `cx.lvl` (`0x5`,
  level 257) read-only, then read `gfx.lvl` (`0x0`, level 1), wrote selector
  2 (consumer level 65, LOW_SVS), and read back `0x2`; the log records
  `ARC gfx.lvl raise verified selector=2 vlvl=65`. No panic; touch and
  SpringBoard continued after the write; GPUCC stayed collapsed and no
  rendering changed. Read-only pull: `logs/gpu-gfx-raise.txt` (flush #27).
  This proves the APPS ARC level vote is stored/readable on this board; it
  does not prove the rail voltage moved. Patch `0086-sweet-adreno-gfx-raise`
  commit `065acbd`, CI run `39277061501` succeeded. Kernel SHA-256
  `f7f95c9f4ff87a0856e2b5149577cd974c1a292b2220ab524fed99503a31aa95`;
  image `shim/out/shim-boot-0086-gfx-raise-120hz.img` SHA-256
  `536da97e93168dfb361b4ceb9bf1c576fe2066a2a99035174734c188f735d8bc`.
- **0087 built and packaged; stage-4 attempt made (2026-10-19 ~23:05 local):** patch
  `0087-sweet-adreno-gpucc-power.patch` adds `sweetgpu=4` (CX GDSC power,
  then a GPU-aperture register read only if the AHB CBCR reports a running
  clock) and `sweetgpu=5` (raise-only `gfx.lvl`, then CX, then GX BCR pulse,
  clamp release and GDSC poll). It mirrors the downstream `qcom,gdsc`
  enables; there are no parent-rail, PLL, RCG, branch-clock, GMU or SMMU
  writes. Editable sources: `research/gpu/0087-work/bsd/dev/sweet_gpu.c`.
  The patch chain (0086 then 0087) was verified with `git apply` against
  the pre-0086 sources. The old `RBBM_IDENTIFIER` name was wrong: the public
  A6XX register database has no chip-ID register, so the aperture read logs
  `RBBM_*`, `CP_RB_*`, `CP_HW_FAULT` and `VBIF_VERSION` and the live-DT
  chip ID `0x06010800` stays the identity reference. CI run `39289968547`
  is green on commit `f6f8624` (an earlier run `39286495743` compiled but
  failed only because the CI stage-5 marker string was wrong; fixed in
  `f6f8624`). Kernel `xnu/out-0087/kernel.development.sweet` is 17,239,704
  bytes, SHA-256
  `84e2df9780567378af72a955de421c05abb124df3c8d42c07ed02a860f7e8ae7`.
  Images: `shim/out/shim-boot-0087-cx-power-120hz.img` (stage 4, SHA-256
  `e492aada0c1c495fca92ca599b09c24427a3a3c04d6a38b40435fac2cbbcee59`) and
  `shim/out/shim-boot-0087-cx-gx-power-120hz.img` (stage 5, SHA-256
  `691a95865b237cff388b340097d2c08a1cfb38da9dbfcfc40b976f5b6244f728`).
  **Stage 4 was approved and attempted once (see the top bullet); the
  result is inconclusive because the CX write never ran. Stage 5 is not
  approved and remains blocked until the CX gate passes.**
- **0085 hardware result (2026-10-19 20:12):** booted
  `shim/out/shim-boot-0085-arc-readback-120hz.img` with
  `sweetfb_hz=120 sweetgpu=2`. Read-only pull:
  `logs/gpu-arc-probe-agent.txt` (486,461 bytes, flush #432); the user's own
  `logs/gpu-arc-probe.txt` may be a duplicate. iOS reached SpringBoard with
  eight CPUs, touch and framebuffer updates; no panic. The 0084 ARC READ
  path completed for all three rails: `mx.lvl` raw 0x3 (index 3 -> 321),
  `cx.lvl` raw 0x5 (index 5 -> 257), `gfx.lvl` raw 0 (index 0 -> 1). The
  reply encoding is not proven; raw 0 is consistent with the off selector or
  no APPS vote. GPUCC remained collapsed, every GPU CBCR was off, and
  `backboardd` still logged `IOAcceleratorES -> 0 found`; no GPU command,
  firmware load or rendering occurred.
- **120 Hz presentation result:** patch 0085 ran the software timer at
  120 Hz (1200 callbacks/10 s), but the eight active windows showed only
  4-86 `present_calls`/10 s (45-86 after the first partial window) and up
  to 374 missed deadlines. Physical panel mode is inherited from ABL and
  unchanged. This does not establish 120 fps or GPU acceleration;
  `sweetfb_hz=30` remains the lower-rate fallback.

- **Latest continuation / 0084:** the OpenCode/DeepSeek 0083 build booted
  but remained slow; phone was returned to OrangeFox.
  `logs/gpu-opencode-0083.txt` (220,473 bytes, flush 9) confirms `stage=1`,
  three ARC tables,
  CX/GX still collapsed, and no IOAcceleratorES. The read-only table gate
  passed; no ARC write was exercised.
- **Power-code correction:** 0083's fixed LOW_SVS votes could replace a
  higher inherited APPS vote. DT regulator minimum/init constraints do not
  establish ABL's current vote. Patch 0084 removes that write API and makes
  `sweetgpu=2` deferred ARC READ transactions (MSGID WRITE clear, response
  register 0x40), using the same lock/TCS ownership as touch. It waits 60 s
  and requires successful L11C setup before issuing the first read. Timeout
  quarantines the controller without retry. Replies are raw evidence, not
  yet established local-vote indices. No GPUCC/core/GMU writes are added.
- ARC aux parsing now requires even alignment and observed version 0x1000,
  checks all trailing padding, and keeps consumer levels in u32 (65535+1
  must not wrap). CI must contain the `ARC readback v1 starting` marker.
  0084/0085's hardware readback passed (above). Full acceleration, firmware
  execution, command submission, Metal compatibility and an iPhone 8
  performance comparison remain unimplemented/unmeasured.

- **Prior GPU hardware result (0082):** audit source d37d1ca boots to iOS
  (user confirmed). `logs/gpu-audit-v2.txt` contains the complete v2 audit:
  CX_GDSCR=0x222001, CX_HW_CTRL=0x5ff4, GX_GDSCR=0x222001,
  GX_DOMAIN=1, GX_BCR=0; every sampled CBCR has CLK_OFF set. Both domains
  collapsed; GX clamped. No GPU/core/GMU writes or acceleration implemented.
- **Historical 0083 milestone (superseded by 0084 below):**
  `xnu/patches/0083-sweet-adreno-arc-vote.patch` adds read-only ARC level
  tables for `mx.lvl`/`cx.lvl`/`gfx.lvl` with `sweetgpu=1`, and LOW_SVS (65)
  votes in order mx, cx, gfx with `sweetgpu=2`. It refuses selector 0 (off),
  validates aux length/alignment/monotonic levels, and bounds each RPMH
  transaction at 200 ms. It performs no GPUCC power/clock/reset writes and no
  GPU/GMU access. Design, evidence and pass/fail gates are in
  `docs/GPU_PROGRESS.md`. The read-only tables and the 0084 READ path were
  booted on hardware; the 0083 fixed-vote mode remains unapproved and
  unbooted.
- GPU work now on branch `codex/adreno-power-audit`. The 0081 artifact was
  incorrectly labeled as containing the GPU probe: CI run 39205190400 skipped
  patch 0081 (bsd/conf/files and sweet_ufs.c context failures). Its kernel and
  boot image contain no GPU strings. Treat earlier probe-build claims below
  as superseded. The saved `logs/gpu-baseline.txt` confirms eight cores came
  alive, framebuffer swaps and working touch, but has no GPU output.
- Fixed 0081 against patches through 0080; new 0082 adds GPUCC-only CX/GX
  power/reset and ten clock snapshots with `GPUCC audit v2` marker. No MMIO
  writes, new GPU aperture accesses, firmware or rendering acceleration.
  Workflow now rejects skipped project patches and requires marker in kernel.
  Commit `d37d1ca` pushed to myfork; CI run `39228692751` succeeded,
  including the GPU marker check. Local ARM64 and real XNU compiles passed.
  Packaged `shim/out/shim-boot-0082-gpu-audit.img` and temporarily booted it
  successfully via host fastboot. iOS boot and hardware audit were confirmed.
- User clarifies lag: quick taps, animations below 20 fps, blur absent.
  Do not rebuild/reflash the iOS root for this GPU diagnostic.
- This user now reports iOS userland boots using
  `shim/out/shim-boot-0081-gpucc.img` and the new root image, but is extremely
  laggy. No boot log or timing measurements have been captured yet. The
  historical v15/on-phone reports below belong to the original project.
- The new iOS root is built and checked at
  `research/ipsw-x/ios16-root-20H392.img` and `.simg`. Sparse size is
  12,145,090,872 bytes; SHA-256
  `42f6ac7c362651144bf138f3175604c2074aa76f633fe9a45dbecb377649e4f0`.
- User authorized the userdata write ("write, phone is in fastboot mode") and
  performed installation from their host terminal after sandbox USB access
  failed. Their subsequent iOS boot report confirms progress; the exact flash
  transcript is not available. Do not reflash or rebuild the root for lag.
- Next performance comparison: temporary boot of
  `shim/out/shim-boot-0081-gpucc-8cpu.img`. It changes only `cpus=6` to `cpus=8`
  (enables the A76 pair supported by patch 0078). The saved baseline log
  confirms all eight CPUs started. Rendering is still software; backboardd
  repeatedly finds no IOAcceleratorES service. Keep the six-core image as fallback.

## Agent handoff / active work (2026-10-15, latest)

### >>> UPDATE 2026-10-18 (read first, then the 10-17 handoff below) <<<

### >>> UPDATE 2026-10-19 (GPU probe and device-tree capture) <<<

- Gesture navigation remains hardware-verified by patches 0070/0071.
- The Adreno 618 work is still diagnostic phase 1, not a rendering driver.
  XNU patch 0081 adds an opt-in, read-only GPUCC probe (`sweetgpu=1`); the
  GitHub Actions `xnu #2` run on commit `8cfc234` succeeded. The downloaded
  arm64 kernel is in `xnu/out/kernel.development.sweet`, SHA-256
  `e2be0a0c4151207a82a1d77441c3e793acc0c2261deb1416480bca22be8dd580`.
  The older kernel was preserved at `/tmp/kernel.development.sweet.pre-0081`.
- OrangeFox read-only dumps are in ignored `backup/`: `boot.img` (128 MiB),
  `dtbo.img` (32 MiB), and `live-fdt.dtb` (555,248 bytes). The boot image's
  base DTB is `dtb/boot_unpacked/dtb`; extracted overlay is
  `dtb/boot_unpacked/overlay-0.dtb`. The live FDT identifies GPUCC as
  `qcom,gpucc-sdmmagpie` at `0x05090000`/`0x9000`, Adreno chip ID
  `0x06010800`, GPU regs at `0x05000000`, GMU at `0x0506a000`, and KGSL SMMU
  at `0x05040000`. The five probe offsets match Qualcomm's SDM Magpie source.
- Extracted the D22AP DeviceTree from the iPhone10,3 iOS 16.7.16 IPSW into
  `research/ipsw-x/d22ap.adt`; generated `shim/out/sweet.adt` with its `/product`
  node. The raw Apple artifact remains under ignored `research/`.
- The AArch64 cross-toolchain is installed; the `sweetgpu=1` shim image is now
  built. See the later 2026-10-19 update for its checksum and device result.
  Preserve Android partitions; use `fastboot boot`, never flash boot.

**STATE at end of 2026-10-18 (evening):**
- On the phone: userdata = **image v15** (`/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v15.img/.simg`; = v14 + `/private/var/preferences/SystemConfiguration/preferences.plist` with one network service "USB Ethernet": Interface DeviceName ue0 Type Ethernet, IPv4 Manual 192.168.7.2/24 router 192.168.7.1, DNS 1.1.1.1 8.8.8.8, set "Automatic" as CurrentSet). Fallback image v14 (v12/v13 deleted).
- Kernels: **0078** = all 8 cores (`cpus=8`, cpu6/7 = A76, homogeneous SMP; mkadt now emits 8 CPU nodes; ADT `shim/out/sweet-0078hi.adt`); **0079** = display off/on via IOMFB selector 12 kern_RequestPowerChange(state) (fade over 6 vsyncs, then black; verified: power button gives "SWEET FB: display off/on"); **0080** = `SWEET NET: <proc> route add/delete/change ...` and `... add/delete address ...` trace (built, `shim/out/shim-boot-0080s.img`, NOT booted yet).
- **Internet over USB: not working yet.** Host side is ready (temporary iptables: FORWARD accept for 192.168.7.0/24 + MASQUERADE, comment "ios-sweet"; lost on host reboot). With v15 the phone sent no DNS and no outbound traffic (only mDNS/ARP) -> configd probably did not apply the service (or did, without default route/DNS). Next: boot 0080s on v15, watch `SWEET NET` lines (does configd add 192.168.7.2 / default route on ue0?). If configd ignores ue0 (not an IOKit IONetworkInterface; configd 1241.140.6 source in `research/configd-1241`, SCNetworkInterface createInterface needs IONetworkInterface -> controller (IOMACAddress, SelfNamed) -> bus), add IOKit stand-ins and/or rename the ifnet to en*.
- Live log listener (`nc -ukl 5140`) may still be running, writing `logs/live-0078s.txt`.

**LIVE LOG over USB (0077, boot 0077s): no more recovery round-trips.** Boot an image with `sweetusb=2`, run `tools/sweet-usbnet.sh logs/live-NAME.txt` on the PC: it waits for the phone's CDC-ECM interface, sets 192.168.7.1/24 (sudo) and prints/saves the kernel console the phone streams every 100 ms as UDP to 192.168.7.1:5140 (starts with the last 64 KiB). `ping 192.168.7.2` checks the link. Current image: `shim/out/shim-boot-0077s.img` (kernel 0077, ADT sweet-0069hi, cpus=6, sweetfb_noconsole=1, sweetfb_hz=30, sweetusb=2). The recovery pull (`tools/sweet-log-pull.sh`) is still the way after a panic/hang before USB comes up.

**🎉 USB networking works (boot 0076s, 18:24 local): `ping 192.168.7.2` from the PC answers in ~1 ms, 0 % loss (200-packet burst, 1500-byte frames, 512/1024-byte frames).** 0075: CDC-ECM function (bulk 0x81/0x01, interrupt 0x82 NETWORK_CONNECTION; host MAC 02:00:5e:77:ee:01) + kernel ethernet ifnet `ue0` (legacy output, header strip like if_fake) with 192.168.7.2/24 set from the kernel (proto_plumb + SIOCAIFADDR on an in-kernel socket). 0076: received frames into one cluster mbuf (mbuf_allocpacket gave a first mbuf of length 0 -> panic "Failed mbuf validity check: len -14"). Host side (Linux cdc_ether, interface `enp0s20f0u3u1`): `nmcli dev set IF managed no; sudo ip addr add 192.168.7.1/24 dev IF; sudo ip link set IF up`. Boot image `shim/out/shim-boot-0076s.img` (`sweetusb=2`). iOS itself does not see ue0 yet (configd needs an IONetworkInterface in IOKit).

**USB device mode works (boot 0074s, 17:45 local): the host enumerates the phone at high speed as `1209:0001 ios-sweet "Sweet iOS (USB bring-up)"`.** 0072 read-only probe: ABL leaves the USB30_PRIM GDSC and all 7 USB clocks on, DWC3 v3.20a (GSNPSID 0x5533320a) in device mode (GCTL 0x00112000), halted (DCTL RUN_STOP 0, DSTS 0x00d3de04), event buffer still at ABL's 0xfdfc8000, QUSB2 PHY alive. 0073 (`sweetusb=2`): DCTL soft reset, own event buffer (polled, interrupt masked), ep0 DEPSTARTCFG/SETEPCONFIG/SETTRANSFRESOURCE, setup TRB, RUN_STOP; control-transfer state machine as Linux ep0.c; descriptors: vendor class, no data endpoints. It ran but no event came: **0074** forces the VBUS session in the Qualcomm wrapper (QSCRATCH 0xa6f8800: HS_PHY_CTRL |= UTMI_OTG_VBUS_VALID(20)|SW_SESSVLD_SEL(28), SS_PHY_CTRL |= LANE0_PWR_PRESENT(24), as Linux dwc3_qcom_vbus_override_enable) -> connect + enumeration. Image `shim/out/shim-boot-0074s.img`. Black Phone app: main thread waits in -[VMVoicemailManager fetchAccounts] (sync XPC; voicemail needs the modem).

**Navigation works (boot 0071s on v14, log `logs/boot-0071s.txt`):** swipe up from the bottom edge returns to the home screen; app switcher works. Two fixes: **0070** pid_hibernate (syscall 435 was nosys on the macOS config -> SIGSYS killed SpringBoard in -[SBToAppsWorkspaceTransaction _didComplete] when an app left the foreground; the only CONFIG_EMBEDDED-only entry in syscalls.master); **0071** touches starting within 48 px of an edge carry kIOHIDDigitizerEventFromEdgeTip (UIKit `_UIEventHIDTouchEdgeTypeForHIDEvent` reads the digitizer EventMask 0xb0007 bits 0x800/0x1000/0x2000/0x40000). Diagnosed, not fixed: Settings (Preferences) SIGTRAP in PodcastsUI func_1cdc47c58: force-unwrap of `URL.volumeTotalCapacity` (nil on our HFS+ root). Phone/Photos/Messages show black (MobilePhone launches, no crash) - next: stack samples via boot-arg `sweetsample=MobilePhone,MobileSlideShow,MobileSMS,SpringBoard` (image `shim/out/shim-boot-0071s.img`).

**🎉🎉 MILESTONE 2026-10-18 ~15:30 local: iOS 16.7 SPRINGBOARD HOME SCREEN ON THE PHONE** (boot `shim/out/shim-boot-0069s.img` = kernel 0069 + ADT `shim/out/sweet-0069hi.adt`, image **v14**; log `logs/boot-0069s-v14-HOMESCREEN.txt`). Home screen with a few apps, Settings opens, Control Center / lock screen / notification center work, touch + buttons work, 6 A55 cores (`cpus=6`), `sweetfb_noconsole=1 sweetfb_hz=30`. Known issue: cannot leave Settings (home gesture / AssistiveTouch home do nothing) - to investigate.
What it took today (in order): 0058 HID services (Apple IOHIDFamily-1915 user client), 0059 iPhone X identity (hw.product etc.), 0060 SPMI keys + GENI touch, 0061 1125x2436 presented display, 0062 RPMh L11C touch power + IOPMPowerSource battery, 0063 GENI FIFO packing (Goodix GT9896), 0064 PRIV_WORK_INTERVAL grant (SpringBoard "Failed to create work interval"), 0065 SMP via PSCI (6 A55), 0066 crash map regions, 0067 console mute without cons_ops write, 0068 IOSurface lookup of an already-mapped surface returns base -1 (backboardd snapshot crash), 0069 UFS log flush 10 s. Image v13: CommCenter removed from launchd.plist, BKNoWatchdogs. **Image v14** = v13 + Setup skipped (`/var/mobile/Library/Preferences/com.apple.purplebuddy.plist` SetupDone/SetupFinishedAllSteps true, SetupVersion 11, daemonizeNeedsToRun false; purplebuddy.notbackedup mini-buddy flags false; managedconfiguration.notbackedup LockdownActivationIndicatesCloudConfigurationAvailable false; springboard MCForceShowCloudConfigurationUI false) + mobileactivationd data ark seeded (`/private/var/containers/Data/System/D90C94C4-F94E-4FCF-8A28-A93BE3846D8E/Library/internal/data_ark.plist`, -ActivationState Activated etc.) + MediaToolbox D22/D221/Default SubServerConfiguration StartRecorderServer=false (mediaserverd crash loop: portrait-filter prewarm called NULL GL). **Activation:** ADT /chosen certificate-production-status 0, effective-production-status-ap 0, certificate-security-mode 1, effective-security-mode-sep 1 -> mobileactivationd's native Hactivation branch (Codex `codex/activation/report.md`; alternative 4-byte daemon patch documented there). Fallback image v13; v12 deleted.

**🎉 TOUCH WORKS (boot 0063u on image v13, 2026-10-18 ~13:15 local):** tapping (i) on Setup's Hello opens the device-info sheet; swipe-up is recognised (spinner, then back to Hello - under investigation). Chain: RPMh L11C vote (0062, `bsd/dev/sweet_rpmh.c`) -> GT9896 @0x5d on GENI SE7 (0063 fixed FIFO packing: register address went out byte-swapped) -> sweet_hid_touch -> SweetHIDTouch -> backboardd. Battery icon OK (0062 SweetBattery IOPMPowerSource, static 100 %). **Image v13** (= v12 + CommCenter jobs removed from launchd.plist + `BKNoWatchdogs`=true and `FBLaunchWatchdogExceptions{com.apple.purplebuddy:100}` in /var/mobile/Library/Preferences/com.apple.springboard.plist): Setup no longer killed by the scene-create watchdog (it blocked in CoreTelephony getSubscriptionInfo -> CommCenter, Codex `codex/setup/report.md`; that report also lists how to skip Setup: SetupDone/SetupFinishedAllSteps/SetupVersion=11 in com.apple.purplebuddy.plist + activation check). v9 deleted, fallback = v12. Input latency ~1 min with 1 CPU -> Codex task 11 = SMP via PSCI (`codex/smp/`).

**INPUT WORKS (boot 0060u on a freshly flashed v12, log `logs/boot-0060u.txt`):** power, volume up, volume down reach iOS (PMIC -> sweet_keys.c -> SweetHIDButtons -> backboardd). Power locks/unlocks (screen "off" shows the kernel console because the panel never really turns off). **Device identity fixed: iPhone X** (home indicator, Face-ID-style Setup). Earlier test (0059u, `sweethidtest=15` fake Home presses) advanced Setup from Hello to the language list.
- **0058** HID: `iokit/Kernel/arm/hid/`: IOHIDEventServiceUserClient + IOHIDEventServiceQueue + IOHIDEvent compiled UNCHANGED from Apple IOHIDFamily-1915.140.3 (= exact version of the iOS 16.7 IOHIDLib, clone in `research/iohid-1915`) over our minimal IOHIDEventService (SweetHID.cpp: SweetHIDButtons consumer page 0x0C power 0x30 / vol 0xE9/0xEA / menu 0x40; SweetHIDTouch digitizer collections). C hooks `sweet_hid_key(key, down)` (0 power, 1 vol+, 2 vol-, 3 home) and `sweet_hid_touch(finger, down, x, y)`. HID logs go to IOLog. Boot-arg `sweethidtest=N` = fake Home press every N s.
- **0059** identity: hw.machine/hw.model/hw.target/hw.product from the ADT (macOS-config kernel used to answer "iPad8,6"/"Redmi Note 10 Pro"). mkadt: root compatible `D22AP|iPhone10,3|xiaomi,sweet|...`, model iPhone10,3, target-type D22, `/product` copied from the real D22AP device tree (`research/ipsw-x/d22ap.adt`, extracted from the IPSW, NOT in git), `/buttons home-button-type 2`. Codex report `codex/mg/report.md`: MG ProductType = sysctl **hw.product** (unrecognized -> iPod), HWModelStr = hw.target, HomeButtonType = `/buttons/home-button-type`; disk cache `/private/var/containers/Shared/SystemGroup/systemgroup.com.apple.mobilegestaltcache/Library/Caches/com.apple.MobileGestalt.plist` (keyed by build + UUID only -> stale iPod answers survived; reflashed clean v12 to clear it).
- **0060** drivers by Codex (`codex/input/hw-report.md`): `bsd/dev/sweet_keys.c` (SPMI arbiter v5 observer reads, PON 0x810 bits KPDPWR/RESIN SID0, PM6150L GPIO2 SID4 0xc110; works), `bsd/dev/sweet_touch.c` (GENI SE7 I2C @0xa84000, starts 45 s after boot). Touch result: SE7 fw ok (proto 3) but **0x5d and 0x38 both time out -> touch IC unpowered** (L11C LDO needs an RPMh vote). Codex task 8 = RPMh client (`codex/rpmh/`), task 7 = battery (`codex/battery/`).
- **0061** (CI 39117078444): red status bar "rdar:45025538" = UIKit "No status bar visual provider for screen" (360x800 pt is no real iPhone). AppleCLCD now presents **1125x2436** (iPhone X) and shows the centered 1080x2400; touch mapped the same; ADT canvas 1125x2436. `sweetfb_native=1` reverts.
- Images: `shim/out/shim-boot-0060b.img` (0060 + /buttons ADT `sweet-0060hi.adt`). Next: 0061 image with ADT from current mkadt.

### >>> HANDOFF 2026-10-17 late (read this first) <<<

**STATE: iOS 16.7 Setup.app "Hello" screen is ON THE PHONE** (boot `shim/out/shim-boot-0057u.img` on image v12). Photo `/run/media/dipa/android/ios-sweet-work/MILESTONE-setup-hello-2026-10-17.jpg`. SpringBoard + backboardd + Setup run; nothing crashes in the GUI path. Remaining crash/relaunch loops: installcoordinationd (should be fixed by 0057, unverified), mediaserverd (VirtualAudio removed in v12, unverified), mobileactivationd (~100/boot, not analysed), bluetoothd + fairplayd.H2 relaunch constantly (no hardware).

**What is on the phone now:** userdata = image **v12** (`/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v12.img` + `.simg` + `.sha256`; v9 kept as fallback, v8/v10/v11 deleted or superseded). Kernel **0057** (`xnu/out-0057`, CI `39067532732`). ADT **`shim/out/sweet-0057hi.adt`** (QEMU twin `research/q0057-hi.adt`). Boot image **`shim/out/shim-boot-0057u.img`**. Boot args: `debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress rd=ufsroot0 unentitled_ios_sim_launch=1 ignition_level=1 wdt_test=no_user_checkin ufsrw=1 rootrw=1 launchd_unsecure_cache=1 wdt=-1`.

**Build a phone image** (kernel K, ADT A): `python3 tools/mkpayload.py $K $A shim/out/payload-X.bin --cmdline "$B" && (cd shim && ./build.sh ../shim/out/payload-X.bin && cp out/shim-boot.img out/shim-boot-X.img)` then check `python3 tools/unpack_bootv2.py shim/out/shim-boot-X.img shim/out/check-X && cmp shim/out/check-X/ramdisk shim/out/payload-X.bin`. QEMU smoke test: `python3 tools/mkpayload.py $K research/q0057-hi.adt $S/csq/payloadX.bin --ramdisk $S/csq/root-fork.img --cmdline "debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress rd=md0"` + `PATH=~/swift/usr/bin:$PATH python3 tools/qemu-sweet.py ... --secs 40 --kernel $K --fail-on-panic` (S = scratchpad; root-fork.img lives in the session scratchpad csq/ dir and may be gone in a new session).

**Kernel workflow:** edit `research/bt28/xnu/...` (keep the pre-edit copy in `research/preNNNN/`), syntax check `python3 tools/xnu-cc.py research/run28.log research/bt28 <file>`, make `xnu/patches/NNNN-*.patch` with `diff -u pre bt28 | sed` path rewrite, add new/changed files to `research/check-patch-order.py` list, run it (must say applied + MATCH), commit + push -> GitHub Actions `xnu` builds (~8 min) -> `gh run download <id> -R dipa-studio/ios-sweet -n xnu-kernel -D xnu/out-NNNN`. **Repo is PUBLIC since 2026-10-17** (private macOS runner minutes ran out: "recent account payments have failed"); public = free runners. Commit trailer: only `Co-Authored-By: dipa-studio <183933817+dipa-studio@users.noreply.github.com>`.

**Phone workflow (user does buttons, Claude runs commands):**
- Boot test kernel: phone in fastboot -> `fastboot boot shim/out/shim-boot-X.img`. NEVER `fastboot getvar`.
- **Read logs WITHOUT filming:** after a test the user holds Power+VolDown -> fastboot -> `fastboot boot recovery/recovery.img` (OrangeFox, temp) -> wait for `adb shell true` -> `tools/sweet-log-pull.sh /run/media/dipa/android/ios-sweet-work/logs/<name>.txt` (reads the 4 MiB console ring the kernel keeps in the last 1025 blocks of userdata; patch 0047). Then `adb reboot bootloader` brings the phone back to fastboot without the user.
- **Flash a root image:** `fastboot flash -S 512M userdata <img>.simg` (7 min, 25 chunks). After a big flash the Xiaomi ABL fastboot hangs: next `fastboot boot` stalls at "Sending" -> the USER must re-enter fastboot by hand (Power+VolDown). Do not retry commands against the hung ABL. A flash right after `adb reboot bootloader` once failed on chunk 1 (USB protocol error): just retry. User must NOT unplug during flash (once interrupted at 25/25 -> reflash).
- **Edit the phone root without reflashing (same-length only):** find the file's raw offset in the local image (it is one extent; search the image for its first 4 KiB), compare with `adb exec-out dd if=/dev/block/sda34 ...`, then `adb push` changed 4 KiB blocks and `dd of=/dev/block/sda34 seek=... conv=notrunc,fsync`, read back. Recovery kernel has no hfsplus.
- **Pull the whole phone root** (12 min): `adb exec-out "dd if=/dev/block/sda34 bs=1048576 count=16384" > img`; mount ro with udisksctl or `sudo mount -t hfsplus -o loop,ro`. To write into a pulled image: `sudo fsck.hfsplus -fy` first (volume is never cleanly unmounted -> Linux mounts ro even with -o force), then `sudo mount -t hfsplus -o loop,force`.
- Making images: copy latest img with `cp --sparse=always`, mount rw with force, change, `sync; umount`, `sha256sum`, `img2simg`. Keep only the latest image + one fallback on the SSD (/home is ~98% full; big work files live in `/run/media/dipa/android/ios-sweet-work/`, old kernels/images archived in `.../ios-sweet-work/archive/`).

**Diagnostics built into the kernel (all SWEET-only):** persistent console log (0047, boot-arg sweetlog=0 off; flush every ~1 s); `|name| text` copies of /dev/null writes for processes in `sweetnull` (default backboardd,SpringBoard; 0048); `IOK name:` IOKit lookup/open trace for `sweetiolog` (default backboardd,SpringBoard; 0049/0054, 400 lines max); `SWEET Sandbox: proc op N` (0050); `STR ...` heap text dump on SIGABRT/SIGTRAP for `sweetscan` (default backboardd,SpringBoard; 0051/0053) - this is how NSException/BSAssert reasons were found; `SMP name tN pc .. bt ..` user stack sampler for `sweetsample` (default SpringBoard,backboardd; first at 90 s then every 120 s, 40 samples, 16 frames; 0054/0055); `SWEET FB: N swaps` every 1000 swaps; `CRASH name ...` crash reports (0043/0044). Symbolicate cache addresses with `ipsw dyld a2s research/dsc/dyld_shared_cache_arm64 <addr - slide>`; main-binary addresses: addr - mh + 0x100000000.

**Patches added today (0046-0057, all CI-built):** 0046 IOSurface (IOCoreSurfaceRoot) + IOMobileFramebuffer (AppleCLCD on /arm-io/disp0) + arm-io nubs; 0047 persistent log; 0048 /dev/null tee; 0049 IOKit trace; 0050 permissive "Sandbox" MAC policy; 0051/0053 heap text on crash; 0052 RENAME_SWAP via exchangedata on HFS; 0054/0055 sampler, swap counter, AKS selectors 35 (extended device state = 17) and 101; 0056 panel copy from the vsync timer at 10 Hz (sweetfb_hz) instead of every swap; 0057 no config_ext_resolver (ACL group entries resolve without memberd). ADT (tools/mkadt.py): /arm-io/disp0, /chosen/display-scale 3, /chosen/unique-chip-id (synthetic 0x5357454554000001) + chip-id 0x8015 -> IOPlatformUUID A9683613-3704-53F2-95D0-98A8FB574E78. Image v9-v12: /private/var/containers/{Bundle,Data/System,Shared/SystemGroup,Temp}; launchd.plist StandardOut/ErrorPath renamed to ...PatX (pointing jobs at /dev/console stops them launching: EACCES); v12: LastSystemVersion=20H392 in plain /var/mobile/Library/Preferences/com.apple.migration.plist (stops the 1-hour data migration that ran every boot), VirtualAudio.plugin moved to /Library/Audio/Plug-Ins/HAL.disabled/.

**Codex helper** (`codex exec --skip-git-repo-check -m gpt-6.1-sol -c model_reasoning_effort=high -s workspace-write -C /run/media/dipa/android/ios-sweet-work/codex -o OUT "$(cat task.md)" < /dev/null`, run detached with setsid nohup; config already defaults to gpt-6.1-sol high). Reports in `/run/media/dipa/android/ios-sweet-work/codex/`: mediaserverd-report.md, installcoordinationd-report.md (-> 0057), ufs-performance.md (ranked UFS speed plan), **ufs/0058-sweet-ufs-faster.patch** (READY, not yet applied: ufslogsecs default 10 s log flush batching, optional ufscache=1 directional cache ops, optional ufswait=1 spin-then-sleep; syntax-checked, host-stub verified; see ufs/README.md). Apply it as patch 0058 after review.

**Claude safety-classifier note:** several messages about QuartzCore renderer selection / writing an HFS reader tool were cut off by an automatic classifier (false positives, user agrees). Keep those topics short or let them be; they are not blockers now (rendering already works).

**NEXT STEPS (in order):**
1. **Input** (Setup stuck on Hello): touch + power + volume as IOHIDEventService/IOHIDDevice services backboardd matches (log shows backboardd matching IOHIDDevice with DeviceUsagePairs and IOService with HIDServiceSupport=true). Phone hardware: Goodix/Focaltech touch over I2C/SPI (check live DT `dtb/`), buttons via PMIC (pon) and GPIO keys. Start with a fake HID service (e.g. power button via a timer or boot-arg) to learn the ABI, then real drivers.
2. **Device identity**: MobileGestalt reports iPod + Home button. Add /product node / product-type (iPhone10,3, D22AP), device class iPhone, home-button-less (Face ID) properties in mkadt; find exact MG keys (MGIOMFBSupport.c, `IODeviceTree:/product` lookups seen in backboardd trace).
3. **Battery**: AppleSmartBattery/IOPMPowerSource style service (status bar shows empty red battery).
4. Apply Codex 0058 (UFS speed), consider cpus>1, check whether installcoordinationd/mediaserverd loops are gone, analyse mobileactivationd.
5. Hide kernel console over the GUI when not debugging: boot-arg sweetfb_noconsole=1 (but then panics are not on screen).

- **🎉 MILESTONE: iOS GUI ON THE PHONE - Setup.app "Hello" screen (2026-10-17 ~21:30 UTC, boot 0057u on image v12).** Photo: `/run/media/dipa/android/ios-sweet-work/MILESTONE-setup-hello-2026-10-17.jpg` (animated "dobar d(an)" hello lettering, status bar "iPod" + battery icon, bottom "Pritisnite Home za otvaranje" = "Press Home to open" (locale hr), (i) button). Rendered by iOS 16.7 SpringBoard/Setup through CoreAnimation -> backboardd -> kernel IOSurface (IOCoreSurfaceRoot) -> kernel AppleCLCD swaps -> ABL panel memory. What made it: data migration finally skipped (v12: `LastSystemVersion` 20H392 in the plain `/var/mobile/Library/Preferences/com.apple.migration.plist`; ADT `/chosen/unique-chip-id` + `chip-id` for IOPlatformUUID), 0057 (no ext resolver -> ACL containers work), plus everything before (0046 display/IOSurface, 0050 Sandbox policy, display-scale 3, containers dirs, RENAME_SWAP, AKS 35/101, 10 Hz present).
  **Missing (next work, in rough order):**
  - **Input: no touch, no power button, no volume buttons** (no HID drivers; backboardd looks for IOHIDDevice / HIDServiceSupport services). Setup is stuck on "Hello".
  - **Device identity: it thinks it is an iPod with a Home button** (status bar "iPod", "Press Home to open"): MobileGestalt has no product/model identity (no /product node, product-type, etc.) - give it iPhone10,3 / home-button-less (Face ID class) identity.
  - **No battery detection** (no battery/charger/PMIC service; status bar battery shows empty/red).
  - Locale came up Croatian (hr) - harmless.
  - Performance: cpus=1, polled UFS (Codex UFS patch in progress: `codex/ufs/`), backboardd presents at 60 Hz.
- **README refreshed (2026-10-17):** read the full notes and updated the public overview through patch 0045, v7 hardware observations and prepared v8 diagnostics. Documents real Apple userspace, the missing display/IOSurface GUI work, limited security services, highmem, persistent userdata root, current tooling and build examples. Documentation only; no kernel build, tests, flashing or phone boot performed.

- **v11 still migrated (boot 0056u, 20:32 UTC, `logs/boot-0056u-v11.txt`).** On iOS kCFPreferencesCurrentHost lands in the plain plist: all DMEnvironment setters (setLastBuildVersionPref, setUserDataDispositionPref, ...) use CurrentHost and the phone's `/var/mobile/Library/Preferences/com.apple.migration.plist` holds DMUserDataDisposition; datamigrator's XPC service runs with `_RoleAccount: mobile`. **Image v12** = v11 + `LastSystemVersion` = 20H392 added to that plain `com.apple.migration.plist` (keeps DMUserDataDisposition, 501:501 0600) + `/Library/Audio/Plug-Ins/HAL/VirtualAudio.plugin` moved to `/Library/Audio/Plug-Ins/HAL.disabled/` (Codex: stops the mediaserverd SIGSEGV loop; untested). **installcoordinationd loop explained (Codex `codex/installcoordinationd-report.md`):** os_crash "Failed to create data directory" EACCES for `/private/var/containers/Shared/SystemGroup/systemgroup.com.apple.installcoordinationd/Library/InstallCoordination`: `Library`/`Library/Caches` in the container are ACL-only (uid/gid 0xfffffffe, mode 0000, ACE for group GUID ...012B = gid 299 systemusers); with `config_ext_resolver` the kernel asks an identity resolver (memberd/opendirectoryd) that iOS does not run -> EWOULDBLOCK -> ACE skipped -> EACCES. **Patch 0057**: drop config_ext_resolver from FILESYS_BASE (compatibility UID/GID<->GUID mapping is used instead), likely fixes other ACL-protected containers too. Codex task 3: UFS driver performance design (`codex/ufs-performance.md`).
- **mediaserverd SIGSEGV loop explained (Codex report `/run/media/dipa/android/ios-sweet-work/codex/mediaserverd-report.md`):** the fault (FAR 0x390) is in the HAL plug-in `/Library/Audio/Plug-Ins/HAL/VirtualAudio.plugin/VirtualAudio` (ApplyBeamFormerPreference <- VirtualAudio_PlugIn::SetPropertyData <- HALS_AHPPlugIn::ObjectSetPropertyData <- AudioObjectSetPropertyData <- MediaExperience vaemVADSerializationListener): the default virtual audio device ('vdef', VirtualAudioDevice_Default) does not exist (no audio hardware/driver, no /product/audio). Proposed workaround for GUI bring-up: move VirtualAudio.plugin out of /Library/Audio/Plug-Ins/HAL/ in the next root image (untested). v11 reflashed OK after an interrupted flash (USB unplugged at chunk 25/25). Codex task 2: installcoordinationd crash loop.
- **ROOT CAUSE of the endless migration: no IOPlatformUUID (2026-10-17).** datamigrator's `-[DMMigrator isMigrationNeededWithBuildVersion:lastBuildVersion:clientPID:]` gets lastBuildVersion from `-[DMEnvironment lastBuildVersionPref]` = `CFPreferencesCopyValue("LastSystemVersion", "com.apple.migration", kCFPreferencesCurrentUser, kCFPreferencesCurrentHost)` -> a ByHost file. The phone had `/var/mobile/Library/Preferences/com.apple.migration.plist` (only DMUserDataDisposition) and NO `Preferences/ByHost` directory at all. On arm XNU publishes IOPlatformUUID (gethostuuid) only from `/chosen/unique-chip-id` (8 bytes) + `/chosen/chip-id` (4 bytes) (IOPlatformExpertDevice::generatePlatformUUID, called from IOPlatformExpert::start); the SWEET ADT had neither, so every CurrentHost preference write failed. Fix: mkadt adds `unique-chip-id` = 0x5357454554000001 (synthetic) and `chip-id` = 0x8015 -> UUID `A9683613-3704-53F2-95D0-98A8FB574E78` (same SHA1 namespace computation as the kernel). ADT `shim/out/sweet-0057hi.adt`, boot image `shim/out/shim-boot-0056u.img` (0056 kernel + 0057hi ADT, QEMU OK). **Image v11** = v10 + `Preferences/ByHost/com.apple.migration.<UUID upper and lower>.plist` {LastSystemVersion: 20H392} (501:501, dir 0755 files 0600). Flashing v11 2026-10-17 ~20:10 UTC. Codex (gpt-6.1-sol high) analysing the mediaserverd SIGSEGV loop in `/run/media/dipa/android/ios-sweet-work/codex/` (report `mediaserverd-report.md`).
- **v10 did NOT stop the migration (boot 0056n on v10, 19:18 UTC, `logs/boot-0056n-v10.txt`):** datamigrator + migrationpluginwrapper ran again, backboardd drew the progress UI (13000+ swaps in ~5 min). So `com.apple.DataMigration.plist`/LastSystemVersion is not the deciding input. What is known about the decision (datamigrator XPC service `DataMigration.framework/XPCServices/com.apple.datamigrator.xpc/com.apple.datamigrator`, strings): `Migration sentinel present. Migration necessary.` (sentinel `/private/var/datamigrator/migrate`, created and removed by datamigrator itself; absent in v10), `System build version changed from %@ to %@. Migration necessary.` / `...unchanged... not necessary`, `Disposition dictionary with build version %@ is out of date relative to current build version %@`, `SyntheticDidUpgrade`, DMUserDataDispositionManager; previous build source not found yet (not in the binary's plain strings; likely in DataMigration.framework's DMUserDataDispositionManager / preferences domain `com.apple.migration`). backboardd calls `DMPerformMigrationIfNeeded` and holds SpringBoard in `checkInAndWaitForDataMigration` until it returns. NEXT: find where the previous build version / disposition dictionary is stored (disassemble DataMigration.framework DMUserDataDispositionManager + datamigrator's isMigrationNeeded path), write it into the image; or get datamigrator's os_log out (OS_ACTIVITY_DT_MODE needs a launchd env change).
- **Data migration never records completion -> skip it (image v10, 2026-10-17).** Boot 0056n (18:16 UTC, `logs/boot-0056n.txt`, 30 min): 10 Hz panel copies, backboardd still presents at ~60 Hz (107k swaps), SpringBoard still in `BKSSystemShellService _checkIn` at sample 14. Every migration run takes 1+ hour on cpus=1 and repeats each boot. DataMigration decides from `/var/mobile/Library/Preferences/com.apple.DataMigration.plist` key `LastSystemVersion` (vs ProductBuildVersion 20H392); the pulled phone root (21:01 local) had NO such file. **v10** = that pulled phone state + fsck.hfsplus -fy (volume was marked not cleanly unmounted -> Linux mounts it read-only even with -o force; fsck found only hard-link counts + minor volume header, repaired) + `com.apple.DataMigration.plist` {LastSystemVersion: 20H392} (501:501 0600). `/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v10.img` (+ .sha256, .simg); v8 files deleted, v9 kept as fallback.
- **Repo made PUBLIC (2026-10-17, user request) so GitHub-hosted macOS runners are free:** the 0056 CI run was refused ("recent account payments have failed or your spending limit needs to be increased"). Checked before: no device dumps, serials, IMEI or Apple binaries in the history (.gitignore keeps backup/, dtb/, recovery/, research/ out); commit author emails are public now. 0056 rerun `39056660104` passed -> `xnu/out-0056`, QEMU OK, image `shim/out/shim-boot-0056n.img` booted 18:16 UTC.
- **Boot 0055n (17:16 UTC, ~65 min, `logs/boot-0055n.txt`): migration bar reached 99% in ~30 min (was ~60), then crawled.** Root cause of the slowness: backboardd presents the progress UI at ~58 Hz (**167,000 swaps in ~48 min**) and AppleCLCD copied the full 10 MB frame into the panel on every swap -> ~600 MB/s memcpy on the single CPU (cpus=1), starving everything (polled UFS I/O too). Sampler (16 frames): **SpringBoard waits in `-[BKSSystemShellService _checkIn]`** (synchronous BSXPC call to backboardd, from FBSystemShellInitialize <- SBSystemAppMain) - backboardd holds the system shell check-in until the data migration ends, so the migration is the gate to SpringBoard UI. **Patch 0056**: swap only records the latest swap record; the vsync timer copies it to the panel every 60/sweetfb_hz ticks (default 10 Hz); `SWEET FB: N swaps` every 1000.
- **Boot 0054n (16:53 UTC, ~12 min, `logs/boot-0054n.txt`, patch 0054 sampler):** the data migration runs AGAIN on every boot (it completed once in the 0053s run) - completion is not being recorded. backboardd keeps swapping the progress UI (3700+ swaps in ~10 min, `SWEET FB: N swaps`). Sample during migration: SpringBoard main thread blocked in a synchronous XPC call (`mach_msg2_trap <- dispatch_mach_send_and_wait_for_reply <- xpc_connection_send_message_with_reply_sync <- -[BSXPCServiceConnectionMessage _sendSynchronously:] <- BSXPCServiceConnectionProxy ...`). Unsupported AKS selectors seen: 101 `aks_assert_consume`, 93 `aks_get_internal_info`, 34 `_aks_stash_load` (datamigrator), **35 `aks_get_extended_device_state`** (SpringBoard, chronod, remindd, searchd, symptomsd...). **Patch 0055**: AKS 35 answered like 17 (same DER device state), 101 succeeds; sampler backtraces 16 frames, 40 samples (~80 min).
- **MILESTONE: FIRST iOS PIXELS ON THE PANEL (boot 0053s long run, 15:33 UTC, ~60 min; photos IMG_20261017_183029/183222/183419; log `/run/media/dipa/android/ios-sweet-work/logs/boot-0053s-long.txt`, 407 KB, 1428 flushes).** The iOS data-migration boot screen (Apple logo + progress bar) was shown and the bar advanced to full in ~60 minutes. It is drawn by **backboardd through our kernel display path**: `SWEET SURF: backboardd create id 2/3 1080x2400 fmt 0x42475241 (BGRA) bpe 4 bpr 4352 size 10452992 keys 11`, IOSurface sels 9, 27, 5, 34 and traps 2/3 (lock/unlock), IOMFB SwapBegin (4) / SwapEnd (5, 1132-byte struct) / GetLayerDefaultSurface (3) / 68 / 55, then `SWEET FB: swap N layer 0 surface 2|3 1080x2400 ... a 0,0 1080x2400 b 0,0 1080x2400` alternating (double buffering; only the first 16 swaps are logged). Triggered by `com.apple.datamigrator` (AKS sels 101/93/34 return unsupported) and `com.apple.migrationpluginwrapper`. Probably ProgressUI-style CoreGraphics drawing into IOSurfaces (no GPU used). After the migration: SpringBoard and backboardd stay up (exec once each, no crash), nothing new drawn; crash loops only for mediaserverd, installcoordinationd, mobileactivationd (~108 each in the hour); bluetoothd and fairplayd.H2 relaunch constantly (no hardware). Kernel console text overlays the image (sweetfb_noconsole=1 would hide it).
- **MILESTONE: SpringBoard and backboardd both stay up (boot 0053s on v9, 15:29 UTC, `logs/boot-0053s.txt`).** With /chosen/display-scale = 3: `exec SpringBoard` once, `exec backboardd` once, no CRASH for either; MobileGestaltHelper reads IOMFB size/dot pitch; backboardd opens AppleCLCD (sels 57,15,22,29,33,79,74 as before). No IOSurface creates and no swaps: backboardd keeps polling `IOProviderClass IOAcceleratorES` (-> 0 found) and AGXAccelerator/IOAcceleratorESLegacy - no GPU, so no CoreAnimation renderer yet. Log goes quiet after ~47 s (only 47 flushes): the system idles. NEXT WALL: CoreAnimation rendering without a GPU (CPU path) so the window server produces frames.
- **SpringBoard's invalid mode = scale 0 (boot 0053n, 15:19 UTC, `logs/boot-0053n.txt`, heap scan):** `FBSDisplayMode cannot be configured as requested : <FBSDisplayMode: ...; infxinf@0x (1080x2400/0) 60Hz sRGB SDR>` - pixel size and 60 Hz are fine, scale is 0. libMobileGestalt (MGIOMFBSupport.c, sub_19c560a54) reads `display-scale` (u32, default 0) and `display-rotation` from **IODeviceTree:/chosen**, pixel size via IOMobileFramebufferGetDisplaySize (MobileGestaltHelper sel 8/28 in the log). mkadt now adds `/chosen/display-scale` = 3 (1080x2400 @3x = 360x800 pt). ADT `shim/out/sweet-0054hi.adt` (QEMU `research/q0054-hi.adt`), image `shim/out/shim-boot-0053s.img` = 0053 kernel + 0054hi ADT. Possible catch: MobileGestalt may have cached scale 0 in `/private/var/containers/Shared/SystemGroup/systemgroup.com.apple.mobilegestaltcache/...`.
- **MILESTONE: backboardd survives and opens the display (boot 0052n on v9, 15:07 UTC, `logs/boot-0052n.txt`).** No `CRASH backboardd`. It walks `IODeviceTree:/arm-io` children, matches IOMobileFramebuffer, `open AppleCLCD type 0 -> 0x0`, calls IOMFB selectors 57, 15, 22, 29, 33, 79 (GetBlock, out 1580/40), 74 (SupportedFrameInfo), 12, 6, 27, 17, 84, 50; opens IOCoreSurfaceRoot (sel 13); looks for AGXAccelerator / IOAcceleratorES / IOAcceleratorESLegacy (none, repeatedly), `IODeviceTree:/product` (none, repeatedly), `/buttons`, aop-smart-cover, IOPMrootDomain. No swaps yet. **SpringBoard now gets past display discovery** and dies (sig 5) in `-[FBSDisplayMode _initWithWidth:height:preferredScale:scaleOverride:refreshRate:gamut:hdr:rotation:virtual:validityCheck:]+580` <- `_initWithCADisplayMode:scale:rotation:` <- `FBSDisplayConfiguration _initWithImmutableDisplay:` <- `FBSDisplaySource initWithDisplay:` <- `FBSDisplayMonitor` (slide 0x14504000): "FBSDisplayMode cannot be configured as requested". Checks: width/height >= 2, preferredScale > 0, scaleOverride >= 0, refreshRate non-zero, valid gamut/HDR/rotation. Suspects: refresh rate 0 (IOMFB timing blocks answered with zeros) and/or scale 0 (MobileGestalt main-screen-scale; no /product node in the ADT). **Patch 0053**: heap text scan also on SIGTRAP and for SpringBoard (default sweetscan list backboardd,SpringBoard) to read the exact mode description.
- **Image v9 (2026-10-17) = v8 + `/private/var/containers/{Bundle,Data/System,Shared/SystemGroup,Temp}` (root:wheel 0755; on iOS these live on the data volume, absent from the rootfs) + launchd.plist with StandardOutPath/StandardErrorPath renamed to ...PatX (the in-place phone fix; daemons' stdout reaches the log via the 0048 /dev/null tee).** `/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v9.img` SHA256 aa9f9139...6e67, `.simg` 12,145,303,864 bytes; written through `sudo mount -t hfsplus -o loop,force`. **Patch 0052** (`xnu/patches/0052-sweet-hfs-rename-swap.patch`, CI `39042761826` -> `xnu/out-0052`, QEMU OK, image `shim/out/shim-boot-0052n.img`): vn_rename falls back to VNOP_EXCHANGE when VNOP_RENAMEX(RENAME_SWAP) returns ENOTSUP for two regular files on one volume (HFS exchange swaps data and cnode descriptors = swap semantics). Flashing v9 started 2026-10-17 ~15:00 UTC.
- **backboardd EXCEPTION FOUND (boot 0051n, 14:41 UTC, `logs/boot-0051n.txt`, patch 0051 heap scan):** `*** Terminating app due to uncaught exception 'NSInvalidArgumentException', reason: '*** -[NSURL initFileURLWithPath:]: nil string parameter'`. First-throw stack (slide 0x337f0000): `+[NSURL fileURLWithPath:]` <- `-[BSSystemContainerForCurrentProcessPathProvider libraryPath]` <- `cachesPath` <- backboardd main+0x1d858 (StartWindowServer, shader cache path). BaseBoard: libraryPath = `BSSystemSharedResourcesDirectoryForIdentifier` = `BSSystemSharedDirectoryForIdentifier(id)` + "/Library"; that calls `container_system_path_for_identifier` (libsystem_containermanager -> `_container_legacy_lookup` -> containermanagerd XPC), which returned NULL (BaseBoard logs "Error getting shared system container ..."). With 0050 the sandbox denials are gone. containermanagerd.log.0 in the pulled (pre-0050) image: "Did not receive any persona attributes", "Expected to find at least one persona", notify_register_dispatch failed 9; `/private/var/containers` does NOT exist in the root image (on iOS it lives on the data volume). usermanagerd does create kernel personas 99/199/1000, but fails to save `/private/var/keybags/usersession.kb` and `persona.kb`: "Failed to atomically swap writing file: Operation not supported (45)" (HFS has no RENAME_SWAP). NEXT: create `/private/var/containers` (and check what containermanagerd needs for system group containers), fix or emulate RENAME_SWAP on HFS, then re-check backboardd.
- **Disk housekeeping (2026-10-17):** /home was at 4 GiB free. Moved (not deleted) `xnu/out-0010..0044` and old `shim/out/*.img`/`payload-*.bin` (everything before 0046) to `/run/media/dipa/android/ios-sweet-work/archive/{xnu,shim-out}` (6.3 GiB); /home now 8.7 GiB free. Large work files (phone-root.img 16 GiB, logs, tracev3 dumps) live in `/run/media/dipa/android/ios-sweet-work/`. **Patch 0051** (`xnu/patches/0051-sweet-crash-heap-strings.patch`, CI `39041440935`): on SIGABRT of a process in boot-arg `sweetscan` (default backboardd) the crash log prints heap text runs as `STR ...` (to find the NSException reason). Boot 0051n 14:41 UTC.
- **Boot 0049n (14:11 UTC, `logs/boot-0049n.txt`, patch 0049 trace): backboardd's IOKit lookups before it dies** (each run): `IODeviceTree:/arm-io/pearl-sep` -> none, matching ApplePearlSEPDriver -> none, `IOService:/IOResources/AppleKeyStore` open OK, matching IOHIDDevice (DeviceUsagePairs) / `HIDServiceSupport` notifications, `usb-drd,t8027` notification; **never any IOMobileFramebuffer / display lookup** - the exception happens before display detection. launchd (tracev3) shows backboardd's mach lookups denied with `error = 159: Sandbox restriction`: com.apple.mobilegestalt.xpc, com.apple.iohideventsystem, com.apple.system.notification_center, com.apple.logd, containermanagerd, ACCHWComponentAuthService, oscard. libsystem_sandbox uses `__mac_syscall("Sandbox", op, arg)` (ops: 1 init, 2 check, 3 note, 4 container path, 5/6/7/8 extensions, 0x15 check_bulk, ...); with no policy named "Sandbox" every call gets ENOPOLICY. **Patch 0050** (`xnu/patches/0050-sweet-sandbox-policy.patch`): built-in permissive MAC policy "Sandbox" in sweet_amfi.c (mpo_policy_syscall returns 0, container/extension ops ENOENT; logs first use of each op as `SWEET Sandbox: proc op N`).
- **Phone root pulled (16 GiB, 12 min, `adb exec-out dd if=/dev/block/sda34`) to `/run/media/dipa/android/ios-sweet-work/phone-root.img` (loop-mounted ro at /run/media/dipa/iOSRoot).** No .ips crash reports. logd tracev3 (Persist, LZ4 `bv41` blocks, decompressed strings in `ios-sweet-work/tracev3/all.txt`): /private/var/db/uuidtext is EMPTY so only launchd's own messages are readable. Findings: v8 launch failure = `Service could not initialize: posix_spawn(/usr/libexec/backboardd), error 0xd - Permission denied` (xpcproxy exit 78) - /dev/console not openable for the mobile user; backboardd[98] spawned `com.apple.MTLCompilerService` before SIGABRT; **launchd denies many mach lookups with `error = 159: Sandbox restriction`** (mediaserverd -> com.apple.backboard.display.services, remoted, mobiletimerd -> sleepd, identityservicesd -> apsd) and **tccd exits OS_REASON_SANDBOX "profile not found"** - no Sandbox kext/profiles on SWEET; likely cause of many daemon crash loops (next wall after the display). CF's uncaught-exception text never reaches stderr for daemons (no CF handler; libc++abi prints only the type). **Patch 0049** (`xnu/patches/0049-sweet-iokit-trace.patch`): `IOK name: ...` console lines for registry path lookups, child iterators, matching (with result count/class), matching notifications and IOServiceOpen of the processes in boot-arg `sweetiolog` (default backboardd), max 400 lines.
- **CONFIRMED: launchd StandardOut/ErrorPath=/dev/console stops backboardd from launching.** After the in-place rename (boot 0047n-b, log `logs/boot-0047n-b.txt`) backboardd launches again and dies as on v7: sig 6, `___pthread_kill <- abort <- demangling_terminate_handler <- _objc_terminate <- std::terminate <- _dispatch_client_callout <- _dispatch_once_callout <- main+0x9334` (window-server dispatch_once). It never opened AppleCLCD or IOCoreSurfaceRoot (no `SWEET FB/SURF: backboardd` lines) -> throws before/at display detection. **Patch 0048** (`xnu/patches/0048-sweet-devnull-tee.patch`, CI `39037749668` -> `xnu/out-0048`, image `shim/out/shim-boot-0048n.img`): writes to /dev/null by processes named in boot-arg `sweetnull` (default `backboardd,SpringBoard`) are printed as `|name| text`. Boot 0048n (13:48 UTC, log `logs/boot-0048n.txt`): only `|backboardd| libc++abi: terminating due to uncaught exception of type NSException` (CF's reason line goes to os_log, not stderr, despite CFLOG_FORCE_STDERR/OS_ACTIVITY_DT_MODE). Next: pull the whole 16 GiB root from recovery (`adb exec-out dd if=/dev/block/sda34`) to `/run/media/dipa/android/ios-sweet-work/phone-root.img` and read ReportCrash .ips files / logd tracev3 for the exception reason.
- **Persistent log WORKS (boot 0047n on v8, 13:31 UTC; pulled from OrangeFox: 70 KB, 111 flushes, `/run/media/dipa/android/ios-sweet-work/logs/boot-0047n.txt`).** In 3 min **backboardd was never exec'd** (v7 ran it), ~70 bare `exec xpcproxy`, SpringBoard loops on FBSDisplayMonitor (no display), mediaserverd sig 11 loop, locationd, lockdownd sig 5. Suspect the v8 plist (StandardOut/ErrorPath=/dev/console on backboardd). **In-place plist edit from recovery (no reflash):** launchd.plist is one extent at image/partition byte offset 0x1e025f000 (195 blocks; verified bytes identical on phone vs local v8 mount); renamed StandardOutPath->StandardOutPatX and StandardErrorPath->StandardErrorPatX (same length, both jobs) in blocks 68 and 105 via `adb push` + `dd of=/dev/block/sda34 seek=... conv=notrunc,fsync`, read back VERIFIED. Phone now differs from local v8 image in those 2 blocks (files in `/run/media/dipa/android/ios-sweet-work/`). Test boot 0047n 13:38:42 UTC. Recovery kernel has no hfsplus (raw block edits only).
- **Patch 0047: persistent console log (CI `39036478663` -> `xnu/out-0047`, QEMU no panic; phone image `shim/out/shim-boot-0047n.img` = 0047 + sweet-0046hi.adt + 0045n cmdline).** `_cnputs` (all console output: kernel printf + daemons' /dev/console) also appends to a 4 MiB ring in sweet_ufs.c; after the UFS partition is ready and writable (ufsrw=1) a thread writes new ring blocks + header to the LAST 1025 blocks (4 KiB) of userdata every ~1 s and SYNCHRONIZE CACHE (prints `SWEET LOG: console log to the last 4100 KiB ...`). Read: phone to fastboot -> `fastboot boot recovery/recovery.img` (OrangeFox) -> `tools/sweet-log-pull.sh OUT.txt` (adb dd of that area + `tools/sweet-log.py`). Not yet verified on the phone. Panics after the last flush are not in it (screen still shows them). 2x font attempt reverted: XNU's v_scale only scales the progress icon.
- **Boot 0046n on v8 (13:05 UTC, VID_20261017_150522_HSR_120):** phone prints `SWEET SURF: IOCoreSurfaceRoot registered` and `SWEET FB: AppleCLCD 1080x2400 stride 4320 at 0x9c000000, default surface 1` (nubs + match work on hardware). Visible userspace: early boot tasks, launchd_cache_loader, `exec SpringBoard`; no `exec backboardd` / `SWEET FB: backboardd` line seen in the readable frames (video blurry). Frame 00:47: lower half of the screen shows OLDER console text (16:01:12) below newer lines -> unconfirmed hint that a swap copied an old snapshot of the panel (CA reads the default/displayed surface = panel memory). SpringBoard still dies in `-[FBSDisplayMonitor _initWithBookendObserver:transformer:]+1160` (sig 5, slide 0x30960000). **Shim: 2x console font** (`shim/loader.c` Video.v_depth scale field = 1) so phone videos are readable; image `shim/out/shim-boot-0046b.img` = 0046n + 2x font.
- **Patch 0046: built-in IOSurface + IOMobileFramebuffer (2026-10-17, offline, NOT phone-tested).** `xnu/patches/0046-sweet-iosurface-iomfb.patch`: new `iokit/Kernel/arm/SweetSurface.{h,cpp}` (IOCoreSurfaceRoot + IOSurfaceRootUserClient: sel 13/0/4/34/35, traps 0-5, wired buffers mapped into the caller, per-client refs) and `SweetFramebuffer.cpp` (AppleCLCD : IOMobileFramebuffer on /arm-io/disp0: sizes, swap = CPU copy of enabled layers into the ABL panel memory, 60 Hz vsync via sendAsyncResult64WithOptions(CanDrop), panel memory published as the default IOSurface, kernel text console paused on the first swap only with `sweetfb_noconsole=1` (default keeps the console: pausing switches it to serial and would hide panics)); SweetPlatformExpert creates nubs for /arm-io children (not gic) attached to the platform expert (IOPlatformDevice::compareName casts its provider to IOPlatformExpert, so nubs under the arm-io nub never matched - first CI build showed that in QEMU). mkadt: /arm-io/disp0 {device_type display-subsystem, compatible sweet,display, canvas-width 1080, canvas-height 2400}. CI `39021572169` -> `xnu/out-0046`. **QEMU** (`research/q0046-hi.adt`, RAM disk init, msgbuf): `SWEET SURF: IOCoreSurfaceRoot registered`, `SWEET FB: AppleCLCD 1080x2400 stride 4320 at 0x9c000000, default surface 1`, init alive, no panic. Phone image **`shim/out/shim-boot-0046n.img`** (SHA256 9c4d2fe9..., CI `39024108588` with the review fixes: swap bounds check, zeroed 64-bit async ref, port release, 1.5 GiB wired budget, ref leak/race fixes; QEMU re-checked) = 0046 kernel + `shim/out/sweet-0046hi.adt` + the 0045n cmdline; boot it on image v8 (backboardd/SpringBoard log to console) and film. Expected new console lines: `SWEET SURF: <proc> create id ...`, `SWEET FB: backboardd sel N ...`, `SWEET FB: swap ...`. Open question for the next wall: QuartzCore's IOMFB server builds its renderer from Metal first (`CAMetalContextCreate`), GLES only when enabled; how the CPU renderer gets selected on a device without a GPU is not yet established.
- **IOSurface values ABI (offline RE, `/run/media/dipa/android/ios-sweet-work/iosurface-values.md`) - TODO in SweetSurface (0046 only accepts and drops values, so IOSurfaceCopyValue always returns NULL):** sel 9 SetValue in = {u32 id, u32 pad, IOCFSerialize([value, key]) or ([dict] = merge)}, sel 0xb RemoveValue in = {u32 id, u32 pad, key\0} (empty key = remove all); both out 4 bytes = new generation, client accepts only (gen+2)|1. Kernel after each change: gen = (gen+2)|1 (odd, never 0) -> state+0x20, serialized size (>=1) -> state+0x28. sel 0xa in {u32 id, u32 0, u8 0}, out {u32 ?, u32 gen, serialized non-bulk dict}; too small -> 0xe00002db + needed size (client retries x16). sel 0x1b in 0x88: bulk struct 0x70 @0, u64 valid size @0x70, u64 changed mask @0x78, u32 id @0x80 -> copy masked fields into state+0x30 (bit0 32B @0, bit1 16B @0x20, bit2 8B @0x30, bits3-10 1B @0x38..0x3f, bits11-14 24B @0x40/4B @0x58/8B @0x5c/1B @0x64).
- **IOMFB display matching confirmed (offline RE, `/run/media/dipa/android/ios-sweet-work/iomfb-match.md`):** the framework never matches on "AppleCLCD"; any registered IOMobileFramebuffer subclass works. `iomfb_match_callback` reads only `dfr`/`AppleTV`/`external` (CFBoolean, service itself, no parents); internal main display = all absent/false (type 0, name "primary"). Display count = /arm-io children whose device_type starts with "display-subsystem" (count 0 -> CreateDisplayList returns NULL; more nodes than services -> 30 s wait). IOMobileFramebufferOpen fails only on IOServiceOpen failure; canvas-width/height read as 4-byte CFData from the IOService-plane parent (optional). CopyProperty = IORegistryEntrySearchCFProperty on the service only. Patch 0046 (disp0 node, `external`=false, AppleCLCD : IOMobileFramebuffer) fits all of this.
- **IOMobileFramebuffer swap + notification ABI (2026-10-17, offline).** Swap record = fb+0x18, **0x46c bytes**, zeroed by SwapBegin and sent whole by `kern_SwapEnd` = IOConnectCallStructMethod(sel 5, rec, 0x46c, no output). **SwapBegin** = sel 4, no input, 1 scalar out = swap id (stored at rec+0x50). `kern_SwapSetLayer(fb, layer<4, IOSurfaceRef, CGRect a, CGRect b, flags)`: rec+0x54+4*layer = **IOSurface id**; rec+0x104 bit layer = layer enabled (rec+0x108 bit cleared); rect a (int x,y,w,h, scaled) at rec+0x64+16*layer, rect b at rec+0xc4+16*layer; rec+0x114+4*layer = transform from flags&0xf; flag bits 5/31/6/7/8/9 set bytes rec+0x2ca/0x2cc/0x2e3/0x2e5/0x2e6/0x467+layer. A NULL surface only sets the enable bit with zero id. SwapWait = sel 6 scalars [swap id, timeout, 0].
  - **Notifications**: `IOMobileFramebufferScheduleWithDispatchQueue`/`GetRunLoopSource` create an IONotificationPort and `IOConnectSetNotificationPort(conn, type, port, 0)` (kernel `registerNotificationPort`). `kern_EnableNotifications(fb, type, cb, ctx)` = **sel 72 scalars [NotifyCallbackFuncs[type], fb, type, 0]** -> the kernel keeps an async reference {port, func, refcon=fb} per type and fires it with `sendAsyncResult64`. Types: 0 HotPlug, 1 Power, 3 FrameInfo (args[0] = mask of 31-bit keys, args[1] u32, then values), 4 CRC, **5 Vsync**: 6 args, each a 32-bit half in a u64 slot: (ts lo, ts hi), (t2 lo, hi) passed through the time-ingest helper, (t3 lo, hi); the framework then calls the client's vsync callback(fb, ts, t2', t3, ctx). Vsync callbacks are dropped while the notification port has queued messages.

- **IOSurface.framework user-client ABI reversed (2026-10-17, offline, step 2 of the GUI plan).** `ipsw dyld disass research/dsc/dyld_shared_cache_arm64 --image IOSurface` (selector scan script: scratchpad `selscan.py`).
  - Connect (`_iosConnectInitalize`, pthread_once): `IOServiceGetMatchingService(IOServiceNameMatching("IOCoreSurfaceRoot"))`, retried with sleep(1) up to 241 times (a missing service stalls every client ~4 min, no crash); `IOServiceOpen(type 0)`; then **sel 13** (no inputs, struct out 0x18 bytes = `_ioSurfaceLimits`) and requires u16 at out+0x16 == 1 (ABI version); out+0 = offset/plane-base alignment mask, out+4 = bytes-per-row alignment mask (used as (x+m)&~m), +0..+0x14 also copied into IOSurface remote-method init, otherwise logs and closes the connection.
  - **Create = sel 0**: scalar in [parent id or 0], struct in = `IOCFSerialize(props, kIOCFSerializeToBinary)` (OSUnserializeBinary format; keys IOSurfaceWidth/Height/BytesPerRow/BytesPerElement/PixelFormat/AllocSize/PlaneInfo/CacheMode/IsGlobal/...), struct out **0xa68 bytes** ("lock result"). **Lookup = sel 4**: scalar in [id], same struct out. LookupFromMachPort = sel 0x22 (scalar in [port name]); CreateMachPort = sel 0x23 (scalar in [id, options], scalar out [port name]).
  - Lock result (copied to client+0x68, size 0x68 + planes*0x80, min 0xe8): 0x00 u64 base address **mapped in the caller's task**; 0x08 u64 task VA of a per-surface shared state page (+0 u64 registry id, +8/+0xa u16 bound accel ids, +0xc u32 seed, +0x14 u8 tiled, +0x20 u32 value generation, +0x24 YCbCr matrix, +0x28 u32 non-bulk values size, +0x2d u8 displayable, +0x2e u8 sysmem-only, +0x30 0x70 bytes bulk attachments); 0x10 u64 task VA of timestamp page (4 u64 indexed timestamps, +0x20 detach code); 0x18 u32 surface id; 0x1c u32 alloc size; 0x20 width; 0x24 height; 0x28 bytes per row; 0x2c buffer offset; 0x30 pixel format; 0x34 u32 (dcache flush length); 0x38 plane count; 0x40 u16 bytes/element; 0x42 u8 element width; 0x43 u8 element height; 0x44 cache mode; 0x4c u8 subsampling; 0x4d u8 prohibit-use count; 0x50 parent id; 0x54 slice count; 0x58 u64 trace id; 0x60 u64 protection options; planes[n] at 0x68+n*0x80 (+0 width, +4 height, +8 bytes/row, +0xc offset, +0x14 size, +0x18 u16 bpe, +0x1a/+0x1b u8 element w/h, +0x1c u8 component count, +0x1d address format, +0x54 u8 compression type (non-zero -> no CPU base address)).
  - **Traps** (IOConnectTrapN -> `IOUserClient::getTargetAndTrapForIndex`): 0 IncrementUseCount(id, category), 1 DecrementUseCount(id, category), 2 **Lock**(id, options, &seed_out), 3 **Unlock**(id, options, &seed_out), 4 Release(id), 5 Release after a failed create/lookup setup (id).
  - Other selectors: 5 SetYCbCrMatrix, 6 WrapClientImage, 7 WrapClientMemory, 9 SetValue (serialized), 0xa UpdateNonBulkValueCache (struct in 9), 0xb RemoveValue, 0xc BindAccel, 0x11 SetSurfaceNotify (async), 0x12 RemoveSurfaceNotify, 0x13 Log, 0x14 SetPurgeable, 0x15 SetOwnership, 0x16 SetTiled, 0x18 SetTimestamp, 0x19 GetTileFormat, 0x1a CopyDataValueBytes, 0x1b SetBulkAttachments (struct 0x88), 0x1e/0x2e-0x31 (via outlined helpers), 0x1f SetCompressedTileDataRegionMemoryUsed, 0x20 GetGraphicsCommPageAddress, 0x21 SetIndexedTimestamp, 0x24-0x26/0x2d IOSurfaceAccelerator, 0x27-0x29 shared events, 0x2a SignalEvent, 0x2b Set/InitDetachModeCode, 0x2c SetOwnershipIdentity, 0x32-0x34 data properties.
  - => SWEET kernel service: class/name **IOCoreSurfaceRoot** + IOSurfaceRootUserClient; minimum = sel 13, 0, 4, 9/0xa/0xb (values), traps 0-5, 0x22/0x23 later for cross-process surfaces (CA sends surfaces to backboardd by mach port). Memory: IOBufferMemoryDescriptor per surface, mapped into each task with createMappingInTask; ids from a global table.

- **Image v8: daemon logs on the console.** `tools/patch-launchd-plist.py IN OUT LABEL...` rewrites launchd.plist (binary, plistlib): renames every `_PanicOnCrash` to `_PanicOnCrasX` and gives the listed jobs `StandardOutPath`/`StandardErrorPath` `/dev/console` (devfs console is 0622, writable by mobile) plus `OS_ACTIVITY_DT_MODE=YES` (os_log mirrored to stderr) and `CFLOG_FORCE_STDERR=1`. v8 = v7 + that plist for com.apple.backboardd and com.apple.SpringBoard, written through the mount (796310 bytes, root:80 0644), SHA256 in `ios16-root-20H392-v8.img.sha256`, `.simg` ready; boot with `shim/out/shim-boot-0045n.img`. Goal: see backboardd's uncaught exception text and the window server's display probing. ODDITY (unresolved): the v5 raw-image rename at offset 0x1e025f000 is present in the v5-v7 image bytes, but the plist read through a Linux mount of v7 still had the original `_PanicOnCrash` keys; filefrag shows the file as one extent. Never patch file contents through raw offsets again; write through the mount.
- **IOMobileFramebuffer.framework reversed (step 1 of the GUI plan).** `ipsw dyld disass research/dsc/dyld_shared_cache_arm64 --image IOMobileFramebuffer`.
  - Display discovery (`IOMobileFramebufferCreateDisplayList` -> `getDisplayListNumber`): iterates children of `IODeviceTree:/arm-io`, counts nodes whose `device_type` is "display-subsystem" or "ext-display-subsystem" (or `compatible` "paravirtualizedgraphics,gpu"); then `iomfb_populate_display_infos` adds an IOServiceAddMatchingNotification(IOServiceFirstMatch, **IOServiceMatching("IOMobileFramebuffer")**) and runs the run loop up to 30 s until that many services matched. `iomfb_match_callback` reads service properties `dfr`, `AppleTV`, `external` (CFBoolean) and names displays (external ones "%s-%d").
  - `IOMobileFramebufferOpen(service, task, type, &fb)`: `__ioMobileFramebufferAlloc` (CFPreferences com.apple.iokit.IOMobileGraphicsFamily: canvas_width/height, override_display_width/height, override_panel_width/height, benchmark, enable_ktrace), `IOServiceOpen(service, task, type)` (GetMainDisplay passes type 0), `getEDTProperty` reads `canvas-width`/`canvas-height` from the service's IODeviceTree parent, then fills a table of `kern_*` function pointers.
  - User client selectors (`kern_*` -> IOConnectCall*): 3 _kern_GetLayerDefaultSurface[ScalarMethod_stub]; 4 _kern_SwapBegin[ScalarMethod_stub]; 5 _kern_SwapEnd[StructMethod_stub]; 6 _kern_SwapWaitWithTimeout[ScalarMethod_stub] _kern_SwapWait[ScalarMethod_stub]; 7 _kern_GetID[ScalarMethod_stub]; 8 _kern_GetDisplaySize[ScalarMethod_stub]; 10 _kern_SetTVOutMode[ScalarMethod_stub]; 11 _kern_SetWSSInfo[ScalarMethod_stub]; 12 _kern_RequestPowerChange[ScalarMethod_stub]; 13 _kern_EnableStatistics[ScalarMethod_stub]; 14 _kern_SetFlags[ScalarMethod_stub]; 15 _kern_SetDebugFlags[ScalarMethod_stub]; 16 _kern_SetTVOutSignalType[ScalarMethod_stub]; 17 _kern_SetGammaTable[StructMethod_stub]; 18 _IOMobileFramebufferIsMainDisplay[ScalarMethod_stub]; 19 _kern_SetWhiteOnBlackMode[ScalarMethod_stub]; 20 _kern_SwapSignal[ScalarMethod_stub]; 22 _kern_SetDisplayDevice[ScalarMethod_stub]; 23 _kern_SetDigitalOutMode[ScalarMethod_stub]; 24 _kern_GetHDCPDownstreamState[StructMethod_stub]; 25 _kern_GetDigitalOutState[ScalarMethod_stub]; 26 _kern_printDisplayRegs[ScalarMethod_stub]; 27 _kern_GetGammaTable[StructMethod_stub]; 28 _kern_GetDotPitch[ScalarMethod_stub]; 29 _kern_GetDisplayArea[StructMethod_stub]; 30 _kern_EnableDisableDithering[ScalarMethod_stub]; 31 _kern_SetUnderrunColor[ScalarMethod_stub]; 33 _kern_EnableDisableVideoPowerSavings[ScalarMethod_stub]; 34 _kern_SetVideoDACGain[ScalarMethod_stub]; 35 _kern_SetLine21Data[ScalarMethod_stub]; 41 _IOMobileFramebufferEnableMirroring[ScalarMethod_stub]; 42 _IOMobileFrameBufferGetMirroringCapability[StructMethod_stub]; 43 _IOMobileFramebufferSetRenderingAngle[StructMethod_stub]; 44 _IOMobileFramebufferSetMirrorContentRegion[StructMethod_stub]; 47 _kern_HDCPSendRequest[ScalarMethod_stub]; 48 _kern_HDCPGetReply[ScalarMethod_stub]; 49 _kern_SurfaceIsReplaceable[ScalarMethod_stub]; 50 _kern_SetBrightnessCorrection[ScalarMethod_stub]; 51 _kern_SetColorRemapMode[ScalarMethod_stub]; 52 _kern_SwapCancel[ScalarMethod_stub]; 53 _IOMobileFramebufferSetCanvasSize[ScalarMethod_stub]; 54 _IOMobileFramebufferGetMatrix[Method_stub]; 55 _IOMobileFramebufferSetMatrix[Method_stub]; 56 _IOMobileFramebufferKernelTests[StructMethod_stub]; 57 _kern_GetColorRemapMode[ScalarMethod_stub]; 66 _kern_SPLCSetBrightness[ScalarMethod_stub]; 67 _kern_SPLCGetBrightness[ScalarMethod_stub]; 68 _kern_SetParameter[Method_stub]; 69 _kern_SwapSetColorMatrix[StructMethod_stub]; 72 _kern_DisableNotifications[ScalarMethod_stub] _kern_EnableNotifications[ScalarMethod_stub]; 73 _kern_ChangeFrameInfo[ScalarMethod_stub]; 74 _kern_SupportedFrameInfo[Method_stub]; 75 _kern_FactoryPortal[StructMethod_stub]; 77 _kern_GetCurrentAbsoluteTime[ScalarMethod_stub]; 78 _kern_SetBlock[Method_stub]; 79 _kern_GetBlock[Method_stub]; 80 _kern_GetLinkQuality[ScalarMethod_stub]; 81 _kern_SwapCancelAll[ScalarMethod_stub]; 82 _kern_GetProtectionOptions[ScalarMethod_stub]; 83 _kern_CopyLayerDisplayedSurface[ScalarMethod_stub]; 84 _kern_SetContrast[StructMethod_stub]; 85 _kern_GetBufBlock[Method_stub]; 86 _IOMFBGainEncoderFinishEncoding[Method_stub]; UNKNOWN {}
  - => Kernel needs: an `/arm-io` child node with device_type "display-subsystem" (+ canvas-width/height) and an IOService of class **IOMobileFramebuffer** (subclass name match) attached under it, opened with type 0, implementing at least GetDisplaySize(8), IsMainDisplay(18), GetID(7), GetLayerDefaultSurface(3), SwapBegin(4)/SwapEnd(5 struct)/SwapWait(6), Enable/DisableNotifications(72), frame info(73/74), CopyLayerDisplayedSurface(83), plus IOSurface for the swapped buffers. NEXT: CoreAnimation's AppleDisplay/IOMFBDisplay usage of these (constructor, first swap, vsync), then IOSurface.framework's IOSurfaceRoot selectors.
- **GUI PLAN (decided 2026-10-16): CPU rendering + kernel IOMobileFramebuffer.** User: CPU rendering is fine for now (goal = prove the GUI runs), may later research an Apple-style GPU driver for the Adreno 618 -> keep display output and renderer separable. QuartzCore findings (symbols from the cache's .symbols subcache: `ipsw dyld symaddr research/dsc/dyld_shared_cache_arm64 --image QuartzCore`; strings: `ipsw dyld macho ... QuartzCore --strings`):
  - Renderers: `sw_new_context` (CA::OGL::SWContext, full CPU renderer incl. blur, images, IOSurface-backed surfaces, make_buffer_current(WindowServer::Surface)), `gles_new_context`, `metal_new_context`, `new_null_context`. Env knobs include CA_ENABLE_OGL, CA_DISPLAY_TYPE, CA_FORCE_LOCAL_SERVER, CA_FORCE_TIMER, CA_CANVAS_WIDTH/HEIGHT, CA_MAIN_DISPLAY_SCALE/ORIENTATION, CA_DISABLE_RENDER, CA_ALWAYS_RENDER (selection logic not traced yet).
  - `-[CAWindowServer _detectDisplays]` (0x1884e3150) iterates static tables `open_funcs`/`count_funcs` = AppleCLCDDisplay, AppleRGBOutDisplay, AppleWirelessDisplay only; each `count()` = `display_count_of_type(IOMFBDisplayType)` over **`IOMobileFramebufferCreateDisplayList()`**, `open(i)` = `create_display_of_type` -> `CA::WindowServer::AppleDisplay(CFString name, ..., IOMFBDisplayType)` (an IOMFBDisplay driven by IOMFBServer: vsync_callback, frame_info_callback, swaps). `CA::WindowServer::VirtualDisplay` exists but is never auto-detected (explicit API only). => the main display MUST come from an IOMobileFramebuffer kernel service.
  - NEXT: reverse IOMobileFramebuffer.framework: IOMobileFramebufferCreateDisplayList / IOMobileFramebufferOpen (service matching, display type property), the user-client selectors used by IOMFBServer (display size/modes, swap begin/set-layer/end with IOSurface ids, vsync notifications), then the IOSurface kernel side (IOSurfaceRoot user client: create/lookup/lock, memory mapping) that both IOMFB swaps and CA surfaces need. Kernel: SWEET IOMobileFramebuffer service that copies the swapped IOSurface to the 1080x2400 framebuffer at 0x9c000000.
- **logd fixed; backboardd = DISPLAY WALL (boot 0045n on v7, 21:08:57 UTC, frames VID_20261016_230850 00:55-01:28, slide 0x4c3ec000, `research/crash-0045n-v7.txt`).** No `CRASH logd` any more; new daemons run (runningboardd, WirelessRadioManagerd, networkserviceproxy, dasd, cloudpaird, magicswitchd, atc, askpermissiond, ManagedSettingsAgent, containermanagerd uses AKS 0/17). New: locationd sig 11 at CFRunLoopAddSource+132 (NULL source) from its main+0x2af138 in a dispatch_once. **backboardd** sig 6: abort <- std::terminate <- _objc_terminate <- dispatch_once <- main+0x9334 = the "start %{public}@..." wrapper (0x100009264) for **"window server"** (block 0x10000a770 -> dispatch_once body 0x10001d7f0 = StartWindowServer): MTLSetShaderCachePath, `[CAWindowServer serverWithOptions:@{kCAWindowServerDisableUpdatesOnMainDisplay: @YES}]`, `setRendererFlags:`, **`mainDisplay`** -> `displayId` -> `[CADisplay displayWithDisplayId:]`, setWindowServerDisplay:, bounds, pointScale, currentMode, preferredScale, nativeOrientation, touchScales... With no display, mainDisplay is nil and it throws. (HID system and GS start fine before it.) SpringBoard dies on the same missing display (FBSDisplayMonitor). QuartzCore display backends (from the cache's .symbols subcache, `ipsw dyld str`): `CA::WindowServer::AppleCLCDDisplay` (IOMobileFramebuffer hardware) and **`CA::WindowServer::VirtualDisplay`** (open(), new_server, clone_update). `CA_DISPLAY_TYPE` and `CA_FORCE_LOCAL_SERVER` strings exist in QuartzCore but have no direct xref (likely read via a table of CA_* env names). NEXT: find how CA::WindowServer::Server enumerates displays (IOMobileFramebuffer services vs VirtualDisplay / CA_DISPLAY_TYPE) and choose: (a) kernel IOMobileFramebuffer service + IOSurface, or (b) a VirtualDisplay rendered by CA's CPU path (CA::OGL::SW) and copied to the framebuffer.
- **watchdogd + CommCenter + identityservicesd fixed (boot 0045n on v6, 20:41:30 UTC, frames VID_20261016_224133 00:39-01:13, slide 0x20fec000).** `SWEET WDT: watchdogd sel 0 in 0/0 out 1/0 -> ok`, no watchdogd/CommCenter/identityservicesd crashes (CommCenter/identityservicesd now call AKS 0/17/7). usermanagerd boot task runs (UMD_INIT creates primary session uid 501 "mobile", personas 99 System and 199 Default registered with the kernel); `Failed to atomically swap writing file /private/var//keybags/usersession.kb: Operation not supported (45)` (HFS lacks the atomic swap rename; non-fatal so far). Remaining: SpringBoard (FBSDisplayMonitor, no display), backboardd (uncaught exception), mediaserverd, lockdownd (crash inside obfuscated code, main+0x4c1b4?), logd. **logd**: main+0x18160 = `BUG IN LIBTRACE: Failed to migrate directory` from 0x10001801c(dir 0): 0x100016538 requires the dir to exist with uid/gid/mode from the table at 0x10002df14 (0: /private/var/db/diagnostics 272:272 0751, 1: /private/var/db/uuidtext 272:272 0755, 2: /private/var/db/diagnostics/timesync 272:272 0755) plus a `.migrated` file, else XPC "operation 6" to a helper and crash. **Image v7** = v6 + those dirs/markers (also in mkiosroot.sh), SHA256 in `ios16-root-20H392-v7.img.sha256`, `.simg` ready. Boot with `shim/out/shim-boot-0045n.img`.
- **Exit-reason text works (boot 0044n, frames VID_20261016_221726 00:32-00:53, slide 0x22d5c000).** CommCenter/identityservicesd: `couldn't dlopen libobjc-trampolines.dylib: dlopen(/usr/lib/libobjc-trampolines.dylib, 0x0106): tried: '/usr/lib/libobjc-trampolines.dylib' (no such file, not in dyld cache)`: the file only exists in the OS cryptex and dyld does not search cryptexes at ignition_level=1. 19 cryptex files have no root counterpart (OS: DriverKit dyld cache + .symbols, VisualTestKit, libffi-trampolines, libobjc-trampolines, libstdc++ 6.0.9 + 2 symlinks; App: SafariSupport.bundle, AuthenticationServicesAgent, com.apple.Safari.History, passwordbreachd, safarifetcherd, webbookmarksd, webinspectord, webpushd) -> hard-linked (symlinks copied: Linux hfsplus cannot hard-link symlinks); `tools/mkiosroot.sh` does it generically. watchdogd still crashed: the `-wdt_no_pmu` / `/arm-io/wdt no-pmu` check (0x10000a01c) only runs when os_variant_has_internal_diagnostics; otherwise 0x100009b18 looks up IOServiceMatching("IOService") + IOPropertyMatch {IOPMUPrimary: 1} (NSConstantDictionary) and crashes if absent, then IORegistryEntrySetCFProperty(boot stage 0xff) (failure only logged). Patch **0045**: IOWatchdog sets IOPMUPrimary = 1 and accepts setProperties; CI `38961814194` -> `xnu/out-0045`. **Image v6** = v5 + the 19 cryptex links, SHA256 7c610c6a...297a, `ios16-root-20H392-v6.simg` (full flash needed). Boot image `shim/out/shim-boot-0045n.img` (0045 + sweet-0044hi.adt). Still open: SpringBoard (no display), backboardd (uncaught exception, reason none), mediaserverd (Fig NULL dict), logd (main+0x18160), lockdownd (main+0x4c1b4).
- **System survives daemon crashes (boot 0043p, video frames VID_20261016_215006 00:25/00:42/00:54).** With the v5 plist patch no panic: launchd keeps launching (runningboardd, powerd, tccd, healthd, peopled, cryptexd, routined, aksd, mediaremoted, familynotificationd, nanoprefsyncd, navd, cloudpaird, passd, amsaccountsd, accessoryupdaterd, wcd ...). Shared cache slide that boot 0x406b4000 (`research/crash-0043p.txt`). watchdogd still crashed at main+0xb82c from the PMU step: `-wdt_no_pmu` is only honored when os_variant_has_internal_diagnostics; customer builds read `no-pmu` from IODeviceTree:/arm-io/wdt (IORegistryEntrySearchCFProperty) -> mkadt adds /arm-io/wdt {no-pmu}. **cryptexd** sig 6: abort_with_payload <- os_crash_fmt <- `__darwin_arm_secure_boot_chip.cold.7` ("panic: failed to get crypto algorithm") <- `_img4_chip_select_personalized_ap`: reads /chosen use-ddi-secure-boot, allow-ecid-mismatch, crypto-hash-method ("sha1" | "sha2-384"), /chosen/asmb lp-smb0/1 -> mkadt adds crypto-hash-method "sha2-384". **identityservicesd** sig 6 = same `_objc_fatal` in imp_implementationWithBlock as CommCenter (block trampolines; message needed). lockdownd sig 5 at main+0x4c1b4. Patch **0044**: crash report adds `CRASH name reason ns N code X: text` (exit reason description = abort_with_payload text). CI `38959096811` -> `xnu/out-0044`, QEMU OK. Image `shim/out/shim-boot-0044n.img` = 0044 + `shim/out/sweet-0044hi.adt` (wdt no-pmu, crypto-hash-method) + cmdline (wdt=-1, no -wdt_no_pmu).
- **First symbolicated daemon crashes (boot 0043u on journaled v4, 19:38:53 UTC; video frame chat image 36; transcribed in `research/crash-0043u.txt`; shared cache slide 0x40d60000 for every process).** (Gemini's video transcription invented values, use frames only.) `tools/sweet-crash.py research/dsc/dyld_shared_cache_arm64 x research/crash-0043u.txt`:
  - **watchdogd** pc main+0xb82c (`_os_crash` trap helper) from 0x100009bb0 = set PMU boot stage via IORegistry (no PMU) -> fix: boot-args `wdt=-1 -wdt_no_pmu` (Apple's own test mode writes "%s wdt=-1 -wdt_no_pmu"; watchdogd logs "skipping setting PMU boot stage because -wdt_no_pmu found in boot-args or no-pmu found in EDT"). Kernel effect of wdt=-1: sched hygiene debug off, zone_exhausted_timeout -1 (harmless).
  - **SpringBoard** sig 5 in `-[FBSDisplayMonitor _initWithBookendObserver:transformer:]+1160` <- `+[FBDisplayManager sharedInstance]` <- `FBSystemShellInitialize` <- `SBSystemAppMain`: no main display. GUI WALL: display service (IOMobileFramebuffer / CoreAnimation display).
  - **backboardd** sig 6: abort <- std::terminate <- _objc_terminate (uncaught exception; message not shown).
  - **CommCenter** sig 6: `_objc_fatal` in `imp_implementationWithBlock` (from `+[_PFRoutines initialize]`, CoreData): objc block trampolines cannot be allocated -> likely kernel gap (trampoline page remap); will hit many processes.
  - **mediaserverd** sig 11: CFDictionaryGetValue(NULL) in FigNotificationListenerStartNotifications (routing session manager).
  - **logd** sig 5 at its main+0x18160 (bt main+0x9dfc); lockdownd sig 5 (not yet symbolicated).
  - Image v5 = v4 + launchd.plist (`/System/Library/xpc/launchd.plist`, contiguous at image offset 0x1e025f000, 1456980 bytes) with the 7 `_PanicOnCrash` keys renamed `_PanicOnCrasX` in place (raw image patch, no metadata change) -> `diff-v4-v5.simg` = 7 blocks; v5 SHA256 78f96974...185a. Boot image `shim/out/shim-boot-0043p.img` = 0043 + `wdt=-1 -wdt_no_pmu`.
- **0043 console crash reports (QEMU verified); image `shim/out/shim-boot-0043u.img`; NEXT: flash v4 + boot 0043u.** Boot 0042u (video photo IMG_20261016_211646): short exec lines work; watchdogd crashes with NO `SWEET WDT:` line (fails before its first IOWatchdog call: lookup or IOServiceOpen, or elsewhere); lockdownd, SpringBoard, mediaserverd, CommCenter, identityservicesd, backboardd, watchdogd all crash repeatedly ("Too many corpses being created" = corpse limit, no crash reports). AKS callers: lockdownd (0, 7), imagent/sleepd (0, 17, 7), backboardd (0, 17), UserEventAgent (0, 7, 102, 103 -> unsupported). Patch **0043**: `sweet_crash_log` (osfmk/kern/exception.c, called from proc_prepareexit for crash exits on the current proc) prints `CRASH name[pid] sig S code (exc<<32|code) sub X slide SR mh MH`, `CRASH name pc lr sp fp far`, `CRASH name bt` (10 fp frames); AKS sel 103 notify_state_is_set -> 0. CI `38954520419` -> `xnu/out-0043`. QEMU test: init forks, child `brk #1`: `CRASH launchd[2] sig 5 code 0x600000001 sub 0x1000004f0 slide 0x0`, pc correct, parent keeps running (pid 1 crashes panic before proc_prepareexit, so test with a child). `tools/sweet-crash.py CACHE ROOT [report]` symbolicates typed-in CRASH lines (`ipsw dyld a2s` with addr - slide; main binary offsets = addr - mh + 0x100000000).
- **v3 reflashed 18:50-18:55 UTC (`research/flash-userdata-v3b.log`, 24/24 OKAY); 0042u booted 19:01:17 UTC on the clean disk (`research/boot0042u-2.log`), SLOW MO VID pending (watchdogd failure reason).** ABL hangs on the first `fastboot boot` after every big flash; user re-enters fastboot. User gave standing approval to reflash/boot without asking (memory standing-flash-approval). Commits now end with `Co-Authored-By: dipa-studio <183933817+dipa-studio@users.noreply.github.com>` (user request; never a Claude trailer).
  - watchdogd flow: connect_to_kext (0x100005408): IOServiceGetMatchingService("IOWatchdog") fails -> "failed to discover" + _os_crash; IOServiceOpen(type 1) fails -> "IOServiceOpen failed with error : 0x%x" + _os_crash. Then sel 0 (out 1 scalar): 0 -> "userspace monitoring disabled", 0x100009bb0, sleep 2, exit(0); nonzero -> "userspace monitoring enabled", normal operation.
  - **Image v4 (JOURNALED):** `tools/mkiosroot.sh` now does `mkfs.hfsplus -J`, mounts with `-o loop,force`, adds the /System/Library/Caches/com.apple.dyld hard links (44) and /private/var/keybags/systembag.kb (0700/0600 root). Built from the IPSW dmgs (OS 087-86698-044, SystemOS 087-86593-047, AppOS 087-86715-047) -> `/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v4.img` SHA256 fba4aeae...ec0f9 + `.simg`. Header: attr 0x80002100 (unmounted, journaled), journal info block 129, journal 8 MiB at 532480 flagged need-init. Linux `fsck.hfsplus -n` reports "Volume header needs minor repair" on any fresh journaled volume even after -y (quirk). QEMU: 64 MB journaled test root (`rd=md0 rootrw=1`, 0042 kernel): `hfs: Initializing the journal (joffset 0x3000 sz 0x80000)` -> mounted -> init runs, no panic. NEXT: flash v4 (full), boot 0042u.
- **SPRINGBOARD STARTS (boot 0041u 18:39:53 UTC, photo IMG_20261016_204142).** backboardd, bluetoothd, fairplayd.H2, SpringBoard (pid 111, crashed: "Too many corpses"), xpcproxy run; `SWEET AKS: backboardd sel 0 -> 0`, `sel 17 in 1/0 out 0/4096 -> 0`. Panic: watchdogd[113] exited, namespace 2 subcode 5 (SIGTRAP = its _os_crash): watchdogd crashes on purpose when IOServiceGetMatchingService("IOWatchdog") fails ("connect_to_kext() unexpectedly failed") or IOServiceOpen(type 1) fails ("IOServiceOpen failed with error"); IOWatchdog DOES register (QEMU msgbuf: `SWEET WDT: IOWatchdog registered`), cause not on screen yet -> need video. Note `wdt_test=no_user_checkin` makes watchdogd log "detected testing boot-args to not run, exiting" (path not reached here?). **HFS CORRUPTION:** `hfs_swap_BTNode: offset #0 invalid (0xBC1D) (blockSize 0x2000 numRecords 81)`, `node=19677 fileID=4` (catalog), `hfs_mark_inconsistent` repeated. Likely cause: non-journaled HFS + unclean panics while writing (0038e wrote a 2.7 GB core then panicked; every boot since ends in panic). Plan: reflash v3 (clean), longer term HFS journaling. Patch **0042**: one short `exec NAME` console line instead of the 3-line "Allowing spawn of iOS binary" notice; CI `38949451276` -> `xnu/out-0042`; image `shim/out/shim-boot-0042u.img` (same cmdline as 0041u).
- **LAUNCHD DAEMONS RUN (boot 0039u, 18:29:13 UTC, photo IMG_20261016_203052).** With `launchd_unsecure_cache=1` all boot tasks pass and launchd starts LaunchDaemons (pids > 100: identityservicesd 114 and lockdownd 106 crash, "Too many corpses being created"). Fatal: `watchdogd: ERROR: failed to discover watchdog KEXT service` -> exit (namespace 2 = signal, subcode 5) -> `_PanicOnCrash` panic. watchdogd: IOServiceMatching("IOWatchdog"), IOServiceOpen type 1, IOConnectCallScalarMethod sel 0/1 (check in)/3/4 (disable/enable userspace monitoring)/5, IOConnectCallStructMethod sel 2 (1 KiB in) and 9 (small out); VM path ("detected virtual machine environment and no watchdog KEXT found, exiting...") only when it detects a VM. Console also flooded with `hfs: set hardlink bit on vol=iOSRoot cnid=321318` (Linux-made hard links lack kHFSHasLinkChainBit; repaired only in memory on each lookup). Patches **0040** (`iokit/Kernel/arm/SweetWatchdog.cpp`: IOWatchdog + IOWatchdogUserClient, all selectors succeed with zeroed outputs, each logged once as `SWEET WDT: ...`) and **0041** (sweet_hfs hfs_catalog.c: the three link-repair printfs limited to 8 per boot); CI `38948245372` PASSED -> `xnu/out-0041` (QEMU regression OK). Image `shim/out/shim-boot-0041u.img` (0041 + sweet-0038hi.adt + ufsrw rootrw + launchd_unsecure_cache=1) booted 18:39:53 UTC (`research/boot0041u.log`), photo pending.
- **v3 FLASHED + keybag passes; next wall AMFI (launchd_cache_loader).** User said "flash, nothing to lose": `fastboot -S 512M flash userdata ios16-root-20H392-v3.simg` 17:51:52-17:57:01 UTC, 24/24 chunks OKAY (`research/flash-userdata-v3.log`). userdata = v3 (SHA256 fa84274c...22df) until the next rw boot. ABL fastboot then hung ("Sending" forever, `reboot bootloader` timed out) until the user re-entered fastboot by hand (common Xiaomi ABL crash). Boot 0038e 18:06:15 UTC (`research/boot0038e-3.log`), photos IMG_20261016_200812/200838: keybag OK, then boot tasks tzinit (`Bad zoneinfo link`, resets to system partition, OK), finish-restore, finish-demo-restore, sysstatuscheck, prng_seedctl (`failed to load kernel prng seed`, `failed to load virtual random (-147)`, AllowCrash), auearlyboot, then **launchd_cache_loader: `abort: Unable to find AMFI`** -> 2.7 GB core written to /cores in 91 s -> launchd panic `launchd_cache_loader[22] exited -- exit reason namespace 7 subcode 0x1`. launchd_cache_loader: SecStaticCodeCheckValidity of /System/Library/xpc/launchd.plist with detached .sig, then IOServiceMatching("AppleMobileFileIntegrity") + IOServiceOpen type 0 + IOConnectCallMethod sel 6 (1 scalar = 1, struct in = cdhash) = trust the cdhash, then sends the cache to launchd. Boot-arg `launchd_unsecure_cache=1` skips signature + AMFI ("Using unsecure cache"); the value is parsed with strtoul base 10 (helper 0x100002da8), so a path = 0 = off: boot 0039w (18:17:13 UTC, `=/System/Library/xpc/launchd.plist`, photo IMG_20261016_202008) still aborted `Unable to find AMFI` (core dump now skipped: `Failed to generate core file ... error 14`, so 0039 works). Image `shim/out/shim-boot-0039u.img` uses `launchd_unsecure_cache=1`. Patch **0039**: do_coredump = 0 on SWEET (kern.coredump sysctl still works); CI `38945267341` -> `xnu/out-0039` (_do_coredump now in __common). Image `shim/out/shim-boot-0039w.img` = 0039 + sweet-0038hi.adt + ufsrw rootrw + `launchd_unsecure_cache=/System/Library/xpc/launchd.plist`. TODO later: real AppleMobileFileIntegrity IOService (sel 6 trust cdhash etc.).
- **Image v3 ready, awaiting user "flash".** Boot 0038w 17:38:10 UTC, photo (chat image 30): `SWEET AKS: keybagd sel 0 -> 0x0`, `sel 14 out 1/0 -> 0x0` (x2), `MKB_INIT: mount is with NO CPROTECT ... does not support Data Protection`, `System Keybag loaded`; then keybagd still opens /private/var/keybags/systembag.kb (0x100012d3c, for BackupKeyBagKeys) -> ENOENT, `no-effaceable-storage` not set -> `Can't load the keybag. tears in rain... Time to die` -> `FATAL KEYBAG ERROR: kb_load` -> reboot -> Halt/Restart Timed Out panic. Fix: ADT `/defaults/no-effaceable-storage` (mkadt.py; `shim/out/sweet-0038hi.adt`, `research/q0038-hi.adt`, QEMU OK) + file `/private/var/keybags/systembag.kb` (0600 root, dir 0700; XML plist {KeyBagKeys: data}). **v3 image** `/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392-v3.img` = v2 + that file, fsck OK, SHA256 fa84274c...22df; `ios16-root-20H392-v3.simg` (img2simg: zero blocks become FILL chunks, so a full flash overwrites every block the rw boots changed). Flash: `fastboot -S 512M flash userdata ios16-root-20H392-v3.simg`, then boot `shim/out/shim-boot-0038e.img` (0038 + sweet-0038hi.adt + ufsrw=1 rootrw=1).
- **0038 AppleKeyStore (CI 38938744130 PASSED -> `xnu/out-0038`; QEMU msgbuf: `SWEET AKS: AppleKeyStore registered (no passcode, unlocked)`; phone image `shim/out/shim-boot-0038w.img` booted 17:38:10 UTC, `research/boot0038w.log`, photo pending).** QEMU full kernel log trick (fb.txt is only the last screen): `llvm-nm KERNEL | grep _msgbufp` -> `--hmp "x /1gx ADDR"` gives struct msgbuf address (static, 0xfffffe0007b56678 on 0038) -> `--hmp "x /3gx STRUCT"` gives bufc (3rd quad) -> `--hmp "memsave BUFC 0x20000 \"FILE\""` (quote the file name), `tr -d "\0" < FILE`. Same inputs give the same addresses. Boot 0037w 17:08:35 UTC, photo IMG_20261016_191052: bindfs fixed mount; mount-phase-1/2 pass; `cc_fips_test` boot task: all corecrypto FIPS POST tests PASSED in userspace; then keybag boot task `keybagd --init`: `MKB_INIT: FATAL KEYBAG ERROR: failed to load system bag`, `Could not get options entry from the device tree` (x2, IODeviceTree:/options = NVRAM, non-fatal), `REBOOTING INTO RECOVERY MODE` -> reboot -> panic `Halt/Restart Timed Out @IOPlatformExpert.cpp:884` (no restart path on SWEET).
  - keybagd (static libaks): load_devicebag (0x100012c30) calls aks_get_system (sel 14, 0-1 scalar in, 1 out) and loads /private/var/keybags/systembag.kb only if the result is 0xe00002f0 (NotFound); any other error -> fatal; SUCCESS -> skip the file (userspace-reboot path). systembag.kb is a plist {KeyBagKeys}; decrypted with effaceable storage unless IODeviceTree:/defaults has `no-effaceable-storage`. Image has no /private/var/keybags. has_data_protection: statfs of /private/var; no cprotect on our HFS -> device-bag volume mapping skipped.
  - libaks connection: IORegistryEntryFromPath / IOServiceMatching("AppleKeyStore"), IOServiceOpen type 0, then selector 0 (no args) must succeed.
  - **AppleKeyStore selector map (iOS 16.7 AppleKeyStore.framework, via `ipsw dyld disass`):** 0 open, 2 create_bag, 3 save_bag, 4 unload_bag, 5 set_system_with_passcode, 6 load_bag, 7 get_lock_state (1 in, 1 out; bit 2 = unlocked since boot), 9 unlock_device, 10 wrap_key, 11 unwrap_key, 12 unlock_bag/test_stashed_kek, 13 lock_bag, 14 get_system, 15 change_secret_se, 16 internal_state, 17 get_device_state (1 in, struct out <=4K: DER SET of SEQUENCE{UTF8 key, INTEGER}: ss state, sls lock_state (MKBGetDeviceLockState returns it), sb backoff, sbo, sfa failed attempts, sfao, sgs generation, sas assertions, sgpe grace, srcd countdown, sr reserved), 18 recover_with_escrow_bag, 19 obliterate_class_d, 20 drain_backup_keys, 21/22 set/clear_backup_bag, 23 get_bag_uuid, 24 rewrap_key_for_backup, 26/27 assert_hold/drop, 30/31 set/get_configuration, 33-37 stash, 38 auth_token_create, 40 generation, 41 fdr_hmac_data, 42 verify_password, 43 _aks_operation, 44 remote_session, 49/50 signing keys, 51/53/54 stash enable/persist/escrow, 55 unload_session_bags, 57/58 remote peer, 59-62 smartcard, 63 migrate_s_key, 65 set_keybag_for_volume_with_cookie_persona, 66/67 sync bags, 68 copy_volume_cookie_persona, 69-73 se_*, 74-79/84/85/99 fv_* (FileVault/VEK), 80/81 absinthe, 82 make_public_backup_bag, 83 drop_auxiliary_auth, 86 kext get/set options, 87 invalidate_bag, 88-91/94/95/106/107 se_*, 92 lower_iteration_count, 93 get_internal_info, 96-98/117 keybag_persona create/list/delete, 100/101 assert promote/consume, 102/103 notify_state set/is_set, 104 fairplay, 105 change_secret_epilogue, 108 delete_xart_leak, 110-113 backup volume, 114 sealed_hashes_set, 115 prewarm_sps, 118-128 AKSIdentity*/AKSGetLastUser.
  - Patch **0038**: `iokit/Kernel/arm/SweetKeyStore.cpp` (class AppleKeyStore on IOResources, IOUserClientClass AppleKeyStoreUserClient; personality in KernelConfigTables.cpp): 0/4/5/9/12/13/26/27/55/102 success, 14 -> handle 0, 7 -> 0x4, 17 -> DER state (ss 4, sls 0), others kIOReturnUnsupported; logs `SWEET AKS: <proc> sel N arg0 in a/b out c/d -> ret` for every call.
  - Tools: shared cache copied to `research/dsc/` (git-ignored; ipsw needs a writable dir for its .a2s symbol cache); `ipsw dyld disass research/dsc/dyld_shared_cache_arm64 --image AppleKeyStore|MobileKeyBag`; scratchpad `annot2.py BIN DIS FIXUPS` annotates llvm-objdump output with strings and import names (`ipsw macho info --fixups`).
- **0037 bindfs (phone boot 0037w pending photo).** Boot 0036mw 16:57:47 UTC, photo IMG_20261016_190102: WRITABLE ROOT WORKS: `/dev/ufsroot0 on / (hfs, local, noatime)` after mount_hfs update (GetMasterBlock EBUSY harmless), coredump of mount written to /cores (rw). Then `mount: failed to mount /private/preboot/5CBE3601.../usr/standalone/firmware -> ...: Operation not supported by device (19)` -> exit 74 -> launchd panic `mount[2] exited`. Cause: the macOS-derived config had nullfs but no bindfs (ENODEV = unknown fs type). Caller passes env 0 -> bind not required, ENOENT (missing source dir) tolerated. Patch **0037** adds `bindfs` to FILESYS_BASE in MASTER.arm64.sweet; CI `38936550483` -> `xnu/out-0037` (bindfs_vfsops present; QEMU probe regression OK). Image `shim/out/shim-boot-0037w.img` (0037 + sweet-0036hi.adt + ufsrw=1 rootrw=1) booted 17:08:35 UTC (`research/boot0037w.log`). NOTE: userdata has now been modified by rw boots; it no longer equals the v2 image, so sparsediff with v2 as OLD is not safe any more (full reflash or a new baseline needed).
- **0036 phone: launchd boot tasks run; mount -P 1 needs /chosen/boot-manifest-hash.** Boot 0036hr 16:15:32 UTC, photo IMG_20261016_185236: platform check passes; fsck boot task runs (`fsck: not booting main or diagnostic OS. Skipping fsck on OS container`); mount-phase-1: `failed to get filesystem info` x2 (no IODeviceTree:/filesystems/fstab, non-fatal), `mount: data volume missing, but not required in env: 0`, mount_hfs update of /: `GetMasterBlock: Error 16 opening /dev/ufsroot0` (EBUSY, mounted block dev, non-fatal) then mount() EROFS (read-only boot) -> `mount: / failed with 1`; then `failed to get boot-manifest-hash` -> errx exit 66 -> launchd panic `mount[3] exited`. /sbin/mount: fn 0x100005f90 reads IODeviceTree:/chosen boot-manifest-hash (hex), bindfs-mounts /private/preboot/<HEX>/usr/standalone/firmware (ENOENT tolerated unless required). `tools/mkadt.py` now adds `boot-manifest-hash` = SHA-384("sweet boot manifest") (5CBE3601...5205). ADTs `shim/out/sweet-0036hi.adt`, `research/q0036-hi.adt` (QEMU probe OK, csops 0x26000001). Image `shim/out/shim-boot-0036mw.img` (0036 + new ADT + ufsrw=1 rootrw=1), booted 16:57:47 UTC (`research/boot0036mw.log`): FIRST WRITABLE BOOT of userdata (restore source: v2 image on SSD).
- **0036 CS_SIGNED (phone boot 0036hr pending photo).** Phone boot 0035hr 2026-10-16 15:57:02 UTC (`research/boot0035hr.log`, photo IMG_20261016_180249): highmem works on hardware; shared cache maps (trust-cache warnings only), Apple launchd runs (`Darwin Bootstrapper Version 7.0.0`, `entering ondemand mode`), starts boot task fsck, then userspace panic `boot task is not a platform binary (fsck, /sbin/fsck)`. launchd+0x3fea0: `csops(pid, CS_OPS_STATUS)` must have bit 0x04000000. Cause: nothing set CS_SIGNED (AMFI does it in vnode_check_signature), so csproc_get_blob() returns NULL for every process and csops never reports CS_PLATFORM_BINARY (also breaks per-process cdhash/entitlement queries). QEMU probe init (`csops` status printer, scratchpad csq/) on 0035: `0x02000001`; patch **0036** (`*cs_flags |= CS_SIGNED` in the SWEET AMFI hook), CI `38929635791` -> `xnu/out-0036`: `0x26000001`. Images `shim/out/shim-boot-0036hr.img` / `-0036hw.img` (same cmdlines as 0035). 0036hr booted 16:15:32 UTC (`research/boot0036hr.log`). Next launchd boot tasks (embedded __config plist): fsck -qL and mount -P 1/2 (RequireSuccess); no /etc/fstab on the image.
- **0035 ENTITLEMENTS + PLATFORM BINARIES (QEMU verified).** `xnu/patches/0035-sweet-amfi-entitlements-platform-binaries.patch`: new `iokit/Kernel/arm/SweetEntitlements.cpp` (OSObject subclass: OSDictionary from OSUnserializeXML of the signature's XML entitlements + original XML for CS_OPS_ENTITLEMENTS_BLOB); `bsd/kern/sweet_amfi.c` answers queryEntitlementBoolean/String(+WithProc, string also matches array members), copyEntitlementAsOSObject(+WithProc, retained), asdict (retained), get_xml (kalloc_data copy); built-in MAC policy "SWEET AMFI" (registered from bsd_init before vfs/exec) with mpo_vnode_check_signature: every signed binary on the MNT_ROOTFS mount gets CS_PLATFORM_BINARY (stand-in for AMFI's static trust cache: iOS system binaries are ad-hoc, cd_flags 0x2, platform byte 0), and attaches entitlements ONLY for the writable copy (cs_blob != csb_ro_addr; revalidation passes the RO blob). LESSON: first version attached in adjustContextWithoutMonitor -> QEMU `unable to adjust OSEntitlements context without monitor: 53` because ubc_cs_convert_to_multilevel_hash (4K-page signatures on 16K kernel) sets csb_entitlements_blob = NULL; ALL iOS 16 binaries (launchd, backboardd, SpringBoard) use 4K code pages, so this would have broken launchd on the phone. Boot-arg `sweet_amfi_debug=1` logs `SWEET AMFI: <ident>: N entitlements (test: ...)`. `tools/mkinit.py OBJ OUT [ENTS.plist]` adds an XML entitlements blob (special slot -5; default output byte-identical). QEMU `research/q0035-ent` (RAM disk `research/root-hfs-ent.img`, init with 3 test entitlements, highmem): `org.sweet.init: 3 entitlements (test: bool 1 string 1 array 1)` then init PID 1. MAC policies loaded: 0034 = 1, 0035 = 2 (QMP `mac_policy_list`). CI `38909807230` -> `xnu/out-0035`. Phone images: `shim/out/shim-boot-0035hr.img` (highmem, ro), `-0035hw.img` (highmem, +ufsrw=1 rootrw=1), `-0035r.img` (low mem fallback).
- **PHONE TEST ORDER (updated):** 0035hr -> if instant reset, 0035r -> then 0035hw.

- **HIGHMEM: XNU in the upper DDR bank, ~3.95 GiB (was 1.2 GiB).** Live FDT memory = <0x80000000 2G> + <0x1_00000000 0xfcc00000>; all fixed reserved-memory carve-outs are in the low bank (highest: ramdump_fb 0xaf000000, rmtfs ~0xfde01000), dynamic pools have alloc-ranges <= 0xffffffff, UFS node has no `iommus` (Linux DMAs anywhere). `tools/mkadt.py --highmem` sets /chosen dram-base 0x100000000 dram-size 0xfcc00000; `tools/mkpayload.py` now takes phys_base/mem_size from the ADT (default payloads byte-identical, verified); `tools/qemu-sweet.py` sizes -m from the payload (8G for highmem). Patch **0034** (`0034-sweet-ufs-dma-above-4g.patch`): UFS refuses DMA memory above 4G without UFSHCI CAP.64AS (bit 24), logs `DMA at 0x...`. CI `38905889883` success -> `xnu/out-0034`. **QEMU verified** (`research/q0034-hi`, ADT `research/q0034-hi.adt` --qemu --highmem, HFS RAM disk): phys_base 0x100000000, init PID 1 heartbeat, no panic; `max_mem` = `mem_size` = 0xfcc00000 read via QMP (`research/q0034-hi-mem`). Phone images: `shim/out/shim-boot-0034hr.img` (read-only) / `-0034hw.img` (+ufsrw=1 rootrw=1), ADT `shim/out/sweet-0034hi.adt` (asmb SIP + sweet-ufs + highmem). PHONE RISK: unknown runtime TZ/XPU protection in the upper bank would show as an instant reset; fall back to 0033r/w. Watchdog ruled out for the overnight reset: msm-watchdog bark is 20 s (qcom,bark-time 0x4e20) yet panic screens stay up for many minutes.

- **Offline research 2026-10-16 (user away, phone unavailable). Images rebuilt** `shim/out/shim-boot-0033r.img` / `-0033w.img` now also carry `wdt_test=no_user_checkin`. NEXT ON PHONE: boot 0033r (expect past in6_nigroup), then 0033w (first writes: rw root fixes HFS link-chain bits itself and gives /var for launchd sockets /var/run/syslog, /var/run/lockdown.sock).
- **launchd.plist** (`/System/Library/xpc/launchd.plist`, compiled; 532 daemons, 66 RunAtLoad/KeepAlive, list in scratchpad `early-daemons.txt`). `_PanicOnCrash` daemons: containermanagerd/usermanagerd (InternalOnly), lockdownd/remoted/trustd/remotepairingdeviced (AutomationOnly), **watchdogd unconditional** (KeepAlive SuccessfulExit=false). watchdogd needs IOKit `IOWatchdog` kext service; if absent it exits only when `kern.hv_vmm_present` (ADT `/defaults/vmm-present`, hvg_sysctl.c) or with boot-arg `wdt_test=no_user_checkin` ("detected testing boot-args to not run, exiting"; reads kern.bootargs). Using the boot-arg (vmm-present would change many daemons; keep as an option).
- **GUI path research (QuartzCore in the shared cache, `ipsw dyld str CACHE STR`, ~10 s/search):** CA::WindowServer has IOMFBServer/IOMFBDisplay (Apple DCP/IOMobileFramebuffer), AppleCLCDDisplay, AppleRGBOutDisplay, **VirtualServer/VirtualDisplay** and **MemorySurface**, EAGLServer. Renderers: CA::OGL::MetalContext, GLContext and a **CPU software rasterizer CA::OGL::SW** (samplers/blend modes; "image is too large for software renderer"). Env knobs: `CA_ENABLE_OGL`, `CA_DISPLAY_TYPE`, `CA_FORCE_LOCAL_SERVER` (249 CA_* vars). Hypothesis (unverified): backboardd's render server could run a VirtualServer/MemorySurface with the SW renderer and we copy the surface to the ABL framebuffer -> needs disassembly of CA::WindowServer server creation. Expected other GUI walls: IOSurface (closed-source kext; needs a reimplementation), IOMobileFramebuffer user client, IOHID touch events, AppleKeyStore/keybag (SEP) for lock state, MobileGestalt answers (AppleCLCD etc.).

- **0032r RESULT (boot 05:07:50 UTC, `research/boot0032r.log`, photo IMG_20261016_071200): SHARED CACHE MAPPED, LAUNCHD RUNS.** With SIP relaxed (ADT asmb) the `not in trust cache` lines are only warnings for every subcache (.31 ... .42.dyldlinkedit); no SIP rejection; libSystem loads; launchd configures networking (`skip attaching fsw to lo0 using legacy TX model`) -> kernel data abort far 0x28 in `SHA1Init` (symbolized as sha1_init) from `in6_nigroup` (IPv6 on lo0): g_crypto_funcs NULL. Kernel addresses are unslid: `~/swift/usr/bin/llvm-symbolizer --obj=KERNEL ADDR` works directly.
- **0032** BearSSL SHA-1 (`0032-sweet-bearssl-sha1.patch`), **0033** BearSSL SHA-384/512 (`0033-sweet-bearssl-sha384-sha512.patch`, BearSSL ctx stored in SHA512_CTX, static-asserted to fit; ubc_subr.c uses SHA-384 for code directories). `0020-stage-bearssl.sh` now also stages hash/sha1.c, hash/sha2big.c, codec/{dec,enc}{32,64}be.c. CI `38854909053` success (0032-only run `38854802073` superseded). Kernel `xnu/out-0033` (SHA1Init -> _br_sha1_init verified by disassembly; nm symbols have a leading underscore). Images (ADT sweet-0032): `shim/out/shim-boot-0033r.img` (read-only), `shim-boot-0033w.img` (+ufsrw=1 rootrw=1). Still NULL g_crypto_funcs users: des, chacha20poly1305, rsa, aesxts, rand, flow_divert.
- `research/check-patch-order.py` now skips patches touching CI-staged sweet_hfs sources (0031) instead of stopping.

- **0031r RESULT (boot 05:03:52 UTC 2026-10-16, `research/boot0031r.log`, photo IMG_20261016_070508): SHARED CACHE FOUND.** dyld now opens `dyld_shared_cache_arm64` (v2 image hard links), kernel refuses to map it: `shared_region: ... map(...'dyld_shared_cache_arm64'): not in trust cache` -> `vnode is not SIP-protected` (vm_unix.c: macOS arm64 path needs trust cache, else root-owned + SF_RESTRICTED unless CSR_ALLOW_UNRESTRICTED_FS) -> `dyld cache '(null)' not loaded: syscall to map cache into shared region failed` -> libSystem not loaded -> coredump to /cores fails error 30 (EROFS) -> panic initproc failed -> "MACH Reboot" (spins; screen kept). Lots of `hfs: set hardlink bit on vol=iOSRoot cnid=... linkid=...` spam (Linux-made hard links lack the link-chain bit; read-only mount cannot persist the fix). FIX: ADT `/chosen/asmb` with `lp-sip0`=0x6f (u64; incl. CSR_ALLOW_UNRESTRICTED_FS) and `lp-sip1`=1 (unauthenticated root), `tools/mkadt.py` -> `shim/out/sweet-0032.adt`. Images with kernel 0031: `shim/out/shim-boot-0032r.img` (read-only) and `shim-boot-0032w.img` (+ufsrw=1 rootrw=1).

- **2026-10-16 morning:** userdata now = **v2** image (`diff-v1-v2.simg` flashed 21:06:50 UTC 2026-10-15, OKAY, `research/flash-userdata-diff-v2.log`); future diffs use `ios16-root-20H392-v2.img` as OLD. Booted `shim-boot-0029k.img` (ignition_level=1) right after (`research/boot0029k-v2.log`); **no photo** (user asleep). Next morning the phone was in PixelOS recovery = some hardware reset (ABL then booted Android, which cannot use userdata). OBSERVATION: our kernel cannot reset the SoC itself (`PE_halt_restart` is never set; reboot/halt print "MACH Reboot"/"CPU halted" and spin), so the reset came from outside XNU: Qualcomm watchdog or a TZ/XPU violation. Unknown which; re-boot 0029k and watch/video until the reset. Do NOT tap factory reset/format data in recovery (would wipe the iOS image).
- **0030** (`xnu/patches/0030-sweet-ufs-writes.patch`, CI `38852655542`): UFS WRITE(10) + read-modify-write for partial 4 KiB blocks + SYNCHRONIZE CACHE (DKIOCSYNCHRONIZE), only with boot-arg `ufsrw=1`; `ufs_write_blocks` re-checks partition bounds before every write (only place data goes out).
- **0031** (`xnu/patches/0031-sweet-read-write-root.patch`, CI `38852837176`): boot-arg `rootrw=1` clears MNT_RDONLY before the root mount (no data volume on sweet: /private/var is on the root; image has no /etc/fstab); Apple HFS accepts a dirty non-journaled ROOT read-write on SWEET (logs "mounting dirty non-journaled root read-write"). Later: journaled HFS for crash safety. Kernel `xnu/out-0031` (CI `38852837176` success, 5m31s; 0030 run cancelled by the newer push). Boot images (phone ADT `sweet-0028.adt`, ramdisk byte-matched): `shim/out/shim-boot-0031r.img` = 0029k args (read-only, ignition_level=1) and `shim/out/shim-boot-0031w.img` = same + `ufsrw=1 rootrw=1`. PLAN: boot 0031r first and film until the reset (diagnose overnight reset), then 0031w (first ever writes to userdata; v2 image on the SSD is the restore source). `research/check-patch-order.py` cannot check 0031 (staged sweet_hfs files are not in its index); CI stage scripts run first, so CI is the check.

- **ignition_level=1 result** (boot 21:01:39 UTC, `research/boot0029k.log`, photo IMG_20261015_230308): preboot/cryptex stages skipped (need >= 2/3), `dylib_cache: finding shared cache on system volume: ignition level = 1, search root fd = 4, subpath = System/Library/Caches/com.apple.dyld/` -> not found -> `ignition boot failed: 19` -> no shared cache -> libSystem not loaded. FIX IN PROGRESS: image v2 = copy of the flashed image + `/System/Library/Caches/com.apple.dyld/` with HARD LINKS to the 44 cryptex cache files (no extra space; avoids symlink-following doubts), then `tools/sparsediff.py ios16-root-20H392.img ios16-root-20H392-v2.img diff.simg` and flash only the diff. Boot with `ignition_level=1` (shim-boot-0029k.img).
- **ignition_level=0 result** (boot 20:58:07 UTC, `research/boot0029j.log`, photo IMG_20261015_230037): all stages skipped as "ignition level insufficient" (hello/preboot/cryptex1 sniff >= 2, select os cryptex path/graft >= 3, **dyld_cache >= 1**), `ignition boot failed: 89`, "ignition partially disabled", then same `Library not loaded: /usr/lib/libSystem.B.dylib (no such file, no dyld cache)`. => dyld gets the shared cache directory from ignition's dyld_cache stage. Next: `ignition_level=1` (`shim/out/shim-boot-0029k.img`) to skip preboot/cryptex stages but run dyld_cache; its log "finding shared cache on %s ... subpath = %s" will show where it searches. If it searches rootfs /System/Library/Caches/com.apple.dyld, add a symlink there (image rebuild; consider a diff-only sparse flash tool to avoid rewriting 12 GB).
- **APPLE dyld/launchd RUNNING (libignition) with `unentitled_ios_sim_launch=1`** (boot 20:53:24 UTC, `research/boot0029i.log`, photo IMG_20261015_225617): kernel prints "Allowing spawn of iOS binary ... boot-arg set to true"; `shared_region: ... check_np ... vm_shared_region_start_address() failed` (no region yet, normal); dyld's built-in **libignition** (Darwin Ignition Sequence 1.0.0, libignition-51~1974; prints rd=ufsroot0, hardware target sweet, ignition level 0x5, platform 0x2, boot spec local, 7 stages): `hello from launchd.1` -> `preboot: mounting preboot` -> `failed to lookup preboot: 25` (ENOTTY: looks for a separate Preboot volume by role) -> ignition fails -> dyld `Library not loaded: /usr/lib/libSystem.B.dylib ... (no such file, no dyld cache)` -> launchd exit -> panic initproc failed. dyld boot-args found via strings: `ignition_level`, `ignition_force_dylib_root` (forced rootfs/livefs/cryptex fallback), `ignition_halt_after`, `ignition_live_app_graft`; dyld cache search paths include /System/Cryptexes/OS, /System/Volumes/Preboot/Cryptexes/OS/, /private/preboot/Cryptexes/OS/ (present in our image). Next try `ignition_level=0` (`shim/out/shim-boot-0029j.img`).
- **APPLE launchd EXECUTED FROM UFS (2026-10-15).** Flashed iOS image: `img2simg` -> `ios16-root-20H392.simg` (12.1 GB) then `fastboot -S 512M flash userdata ...simg` (24 chunks, 307 s, `research/flash-userdata-ios16.log`; a raw-image flash was killed by earlyoom at 2.6 GB RSS - always flash the sparse image). ABL fastboot hung on getvar after the flash; power+vol-down fixed it. Boot 0029u 20:49:04 UTC (`research/boot0029u-ios16.log`), photo IMG_20261015_225134: `hfs: mounted iOSRoot` -> load_init_program /sbin/launchd -> panic `unexpected SIGKILL of launchd (CS_KILLED) with reason -- namespace 9 code 0xe` = OS_REASON_EXEC / EXEC_EXIT_REASON_WRONG_PLATFORM: our kernel is XNU_TARGET_OS_OSX, iOS-platform binaries are "alien" and refused unless spawn attr platform set or DEVELOPMENT TUNABLE `unentitled_ios_sim_launch`. Next try: same kernel, cmdline + `unentitled_ios_sim_launch=1` (`shim/out/shim-boot-0029i.img`). LONGER TERM: macOS-config kernel marks iOS maps alien (vm_map_mark_alien) and differs from iOS in many policies; an iPhoneOS-config kernel build may eventually be needed.
- **iOS 16 ROOT IMAGE BUILT:** `/run/media/dipa/android/ios-sweet-ipsw/ios16-root-20H392.img` (16 GiB case-sensitive HFS+, label iOSRoot, 12 GB used, fsck OK after `fsck.hfsplus -y` rewrote 32 non-ASCII localized WorkflowKit Shortcut names Linux decomposes differently). Built by `tools/mkiosroot.sh OS.dmg SYSTEMOS.dmg APPOS.dmg OUT.img [SIZE]` (needs sudo; apfs-fuse as root; OS dmg is UDIF -> link must end .dmg). Contents: iPhone X 20H392 OS root + SystemOS cryptex at /private/preboot/Cryptexes/OS (so /System/Cryptexes/OS -> cache, 44 files, dyld_v1 arm64) + AppOS at .../App. /sbin/launchd and /usr/lib/dyld root:wheel 0755. NEXT: (confirm) `fastboot flash userdata ios16-root-20H392.img` (auto-sparse), boot 0029u (rd=ufsroot0) -> Apple launchd attempt.
- **MILESTONE ON HARDWARE: BOOT FROM INTERNAL FLASH.** 0029 CI `38797924912` PASSED, `xnu/out-0029` SHA256 `a68bc847...2a37`. Image `shim/out/shim-boot-0029u.img` (payload-0029u = 0029 kernel + `shim/out/sweet-0028.adt` + `rd=ufsroot0`, NO RAM disk). Boots 20:01:34 and 20:12:37 UTC (`research/boot0029u*.log`), photo IMG_20261015_221403: `SWEET UFS: "userdata" on LUN 0: first block 0x2b8c00, 109643 MiB, read-only` -> `BSD root: ufsroot0, major 1, minor 0` -> `hfs: mounted SweetRoot on device b(1, 0)` -> `SWEET: root volume is not authenticated (error 25); continuing unsealed` -> `load_init_program ... /sbin/launchd` -> `*** SWEET userspace: init running as PID 1 ***` -> heartbeat (ran for minutes unattended). userdata currently holds the 8 MB `research/root-hfs.img`. NEXT: build a multi-GB HFS+ image with the iOS 16 (iPhone X 20H392) userland (OS rootfs + SystemOS cryptex contents incl. dyld shared cache at /System/Library/Caches/com.apple.dyld; preserve modes/owners/symlinks; decompress LZBITMAP via apfs-fuse), flash it to userdata (sparse, needs user OK), then try Apple's dyld/launchd. Driver is read-only (root mounts RDONLY), fine for first launchd attempts.
- **userdata FLASHED with the 8 MB HFS test volume** (user said "flash it", 2026-10-15 19:53:17 UTC, `research/flash-userdata-hfs-test.log`: `fastboot flash userdata research/root-hfs.img` OKAY; pre-checks: one sweet device, partition-size:userdata 0x1AC4BFB000, partition-type f2fs, image fsck OK, SHA256 3626ea69...). Android userdata is now destroyed (expected/approved). Boot 0028u at 19:53:20 (`research/boot0028u-hfs.log`), photo IMG_20261015_215541: `BSD root: ufsroot0` -> **`hfs: mounted SweetRoot on device b(1, 0)` from internal UFS** -> panic `rootvp not authenticated after mounting @bsd_init.c:931` (arm64 macOS-config requires FSIOC_KERNEL_ROOTAUTH sealed root for non-ramdisk roots). Patch **0029** (`dd1c284`): SWEET logs `SWEET: root volume is not authenticated (error N); continuing unsealed` instead of panicking (Apple configs keep the panic; pivot_root still requires auth). CI **`38797924912`** running. Next: download out-0029, phone image with sweet-0028.adt + rd=ufsroot0 (no RAM disk), boot -> expect init from UFS.
- **UFS ROOT DRIVER VERIFIED ON HARDWARE (read-only).** 0028 CI `38796139570` PASSED, `xnu/out-0028` SHA256 `8b981640...9da6`; QEMU HFS RAM-disk regression `research/q0028-hfs` still runs init. Phone boot 2026-10-15 19:50:24 UTC (`research/boot0028u.log`, image `shim/out/shim-boot-0028u.img`, phone ADT `shim/out/sweet-0028.adt` with /sweet-ufs, NO RAM disk, `rd=ufsroot0`), photos IMG_20261015_215151/215220: `SWEET UFS: "userdata" on LUN 0: first block 0x2b8c00, 109643 MiB, read-only` -> `BSD root: ufsroot0, major 1, minor 0` -> `hfs_ValidateHFSPlusVolumeHeader: unknown Volume Signature : f188` (Android-encrypted data, as expected) -> `cannot mount root, errno = 19`, kernel retries mountroot (no panic). Nothing written to flash. NEXT (destructive, needs user confirmation right before): `fastboot flash userdata research/root-hfs.img` (8 MB HFS with test init) then boot rd=ufsroot0 -> expect init from internal flash.
- **PATCH 0028 (`33aceac`): XNU read-only UFS root device**, CI **`38796139570`** running (next artifacts xnu/out-0028, research/bt29, run29.log). `bsd/dev/sweet_ufs.c`: reuses ABL's UFSHCI state (like the shim probe), takes over UTRL at a kmem_alloc_contig region (DMA layout: UTRL 0x0, UCD 0x1000, 1 MiB bounce 0x4000), verifies UTRLBA readback before any doorbell, IE=0 polled, one SCSI cmd at a time on LUN0 (TUR x3, READ(10) via bounce, PRDT 256 KiB chunks), flush_dcache64 by PA around each command (host not dma-coherent per DT), finds GPT partition by name (`ufsroot=` boot-arg, default `userdata`), exposes `/dev/ufsroot0`/`rufsroot0` (512-byte logical sectors, physical 4 KiB, DKIOC* ioctls), refuses writes (EROFS). IOKitBSDInit: `rd=ufsroot0` -> sweet_ufs_rootdev() (lazy init) else panic. ADT: `tools/mkadt.py` adds `/sweet-ufs` reg <0x1d84000 0x3000> for the PHONE only (not --qemu). Local compile OK, imports resolve. Sandbox research/bt28 (pre0028 originals). FIRST PHONE TEST PLAN (non-destructive): boot 0028 with phone ADT + `rd=ufsroot0` and NO RAM disk: expect `SWEET UFS: "userdata" on LUN 0: first block 0x2b8c00, 109643 MiB` + `BSD root: ufsroot0`, then root mount failure (userdata is still Android-encrypted) -> panic. That validates the driver before flashing an HFS image into userdata (needs explicit user confirmation at that time).
- **SUPERSEDES the 64 GiB iosroot GPT plan: user said "u can delete android" (2026-10-15).** Use the existing `userdata` partition (LUN0 sda34, 4K LBA 0x2B8C00, 0x1AC4B MiB ~107 GiB) directly as the iOS root: NO GPT changes. Keep bootloaders, recovery (OrangeFox), modem/persist/fsg/modemst untouched. Write the HFS image later with `fastboot flash userdata` (sparse, auto-split under the 768 MiB limit). Android will no longer boot normally (expected; reinstallable). Order: XNU UFS driver first, read-only against the current GPT; flash only once it can locate userdata. Nothing erased yet.
- **DECIDED (user, 2026-10-15): 64 GiB `iosroot` on LUN0 (sda), taken from userdata (wipes Android user data; Android/super kept).** Planned GPT (4K LBAs): userdata 0x2B8C00..0xD7D6FF (~43 GiB, keep type/unique GUIDs and attributes); iosroot 0xD7D700..0x1D7D6FF (0x1000000 blocks = 64 GiB, 1 MiB aligned); backup GPT area unchanged. Procedure: user backs up phone data -> boot OrangeFox recovery (adb) -> back up GPT (sgdisk --backup + raw dd of LBA 0-5 and last 6 blocks) to research/ -> sgdisk edit -> Format Data -> verify Android boots to setup. NOT EXECUTED YET.
- **INTERNAL UFS WORKS FROM THE SHIM (read-only), user authorized internal-UFS approach incl. flashing (2026-10-15, "i allow everything").** Shim probe `shim/ufs.c` (commit `12ed3ed` + filter): ufshc@1d84000 is UFSHCI 2.1 (ver 0x210, cap 0x1587031F, 32 slots), ABL leaves HCE=1, HCS=0x10F (device present, lists ready, UIC ready); probe takes over UTRL at its own buffer (verifies UTRLBA readback before any doorbell), TEST UNIT READY/READ CAPACITY/READ(10) all succeed. LUN0 = 0x1D7D800 x 4 KiB (~126.6 GB), valid GPT (64 entries at LBA 2). Photos IMG_20261015_212916/213325/213429. Big LUN0 partitions (start in 4K LBAs / MiB): super 0xCA00/0x2200 (8.5 GiB), logdump 0x22CA00/64, minidump 0x230A00/128, rawdump 0x238A00/128, recovery 0x240C00/128, cache 0x248C00/384 ext4, exaid 0x260C00/384, cust 0x278C00/1024 ext4, **userdata 0x2B8C00/0x1AC4B MiB (~107 GiB), fs unreadable = encrypted (metadata encryption)**. cache..cust contiguous = only 1.75 GiB (too small for iOS ~10+ GiB). Plan needing user consent: shrink userdata in GPT (wipes Android data) and add an `iosroot` partition at the end of LUN0. LUNs 1-5 not listed yet (sdb..sdf backed up in backup/lun).
- **fastboot limit measured (2026-10-15, read-only getvar):** `max-download-size: 805306368` (768 MiB); `max-fetch-size` unsupported; `is-userspace: no` (ABL fastboot). A `fastboot boot` image (shim+kernel+RAM disk) must be < 768 MiB, so the 2.5 GiB iOS shared cache cannot ride in the boot RAM disk even if XNU gets all RAM. Real userland needs storage access (UFS driver, or USB host) or a much smaller cache. Unlocking full RAM is still needed later (GUI).
- **iOS 16 USERLAND SOURCE (Claude, 2026-10-15).** Chose **iPhone X (iPhone10,3/10,6, D22/D221AP)** iOS **16.7.16 (20H392)** over iPhone 8: same A11 (no PAC -> plain arm64, runs on Kryo 470/ARMv8.2), last iOS for A11; X userland is home-button-less and tall-screen (1125x2436 vs sweet 1080x2400), better fit for later GUI. Note the X has its OWN SystemOS cryptex (different digest from iPhone 8's). IPSW URL in `research/ipsw-x/URL.txt` (+ BuildManifest). Images on the user's external SSD (NOT in git, do not format that disk: it holds the user's photos): `/run/media/dipa/android/ios-sweet-ipsw/iPhone10,3_16.7.16_20H392/` — `087-86593-047.dmg` = Cryptex1,SystemOS (2,743,074,816 B), `087-86698-044.dmg` = OS rootfs (4,730,814,044 B), both zip CRC-32 verified, plus Firmware/*.root_hash/.trustcache/.mtree. AppOS cryptex is 087-86715-047.dmg (10 MB, same as iPhone 8).
  - Images are raw APFS; files use LZBITMAP (decmpfs type 14). 7z cannot decode. Tools: `ipsw` 3.1.738 (AUR ipsw-bin, user-approved) needs apfs-fuse on Linux; **apfs-fuse** built in user space (system cmake broken: cmake 4.4.4 wants libjsoncpp.so.27, jsoncpp 1.9.6 installed = partial upgrade, NOT fixed) with pip cmake + `-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_CXX_FLAGS="-include cstdint"`, installed to `~/.local/bin/apfs-fuse` (+apfs-dump-quick). Mount via a comma-free symlink (apfs-fuse splits options on ',' in `iPhone10,3`; exFAT has no symlinks -> put link elsewhere): `apfs-fuse LINK/087-86698-044.dmg MNT` -> `MNT/root/...`; unmount `fusermount3 -u MNT`.
  - Verified from headers: `dyld_shared_cache_arm64` magic `dyld_v1   arm64`; `/sbin/launchd`, `/usr/lib/dyld` = cputype 0x100000c subtype 0 (plain arm64). libSystem exists only inside the cache.
  - **BLOCKER for real userland via RAM disk:** shared cache = 3.10 GiB in 44 files (~2.5 GiB without the 592 MB `.symbols`), but the kernel manages only 1.20 GiB (0xb0000000..0xfd000000). Options: (A) give XNU the phone's full ~6 GB (banks 0x80000000+2G and 0x100000000+0xfcc00000, carve-out holes need pmap support) AND check fastboot max-download-size for a multi-GB boot image; (B) storage driver (Qualcomm UFS) to read a root from flash (no flashing allowed: would need a file/partition strategy); (C) run against individual dylibs extracted from the cache (uncertain).
- **MILESTONE ON HARDWARE: HFS+ ROOT FILESYSTEM -> PID 1.** Boot 2026-10-15 18:22:12 UTC (`research/boot0027b.log`, image `shim/out/shim-boot-0027b.img`), photo `IMG_20261015_202334.jpg`: `BSD root: md0` -> `hfs: mounted SweetRoot on device b(1, 0)` -> `load_init_program: attempting to load /sbin/launchd` -> `*** SWEET userspace: init running as PID 1 ***` -> repeated `SWEET init: still alive`, no panic. Kernel 0027 (built-in Apple OSS HFS + BearSSL MD5), init with SP alignment fix. NEXT: real iOS 16 userland on the HFS image (dyld, libSystem/dyld shared cache, launchd). Known gaps: entitlements (none granted), SHA-1/SHA-384/512 providers, no storage driver (RAM disk only), root mounted read-only.
- **PHONE HFS ROOT (0027) + SIGBUS ROOT-CAUSED.** Phone boot 2026-10-15 17:49:06 UTC (`research/boot0027.log`, photo `IMG_20261015_200031.jpg`): `hfs: mounted SweetRoot on device b(1, 0)` (HFS works on hardware), init printed its first line, then `pid 1 exited -- exit reason namespace 2 subcode 0xa` = SIGNAL/SIGBUS -> `initproc failed to start` panic. Diagnostic boot 18:08:13 UTC (`research/boot0027d.log`, image `shim/out/shim-boot-0027d.img`, diag init `research/diag-init` from scratch diag-init.S with [a][b][c] markers, `cs_debug=1`; photo `IMG_20261015_201547.jpg`): output `PID [a][b]` then SIGBUS -> fault at first `strb w21,[sp]`; no code-signing messages. Cause: kernel enters a static binary with sp at argc, not necessarily 16-aligned (depends on arg/apple string lengths, differs mockfs vs HFS); EL0 SP alignment check -> EXC_ARM_SP_ALIGN -> SIGBUS (sleh handle_sp_align). QEMU does not enforce SP alignment. NOT a kernel bug: real binaries start in dyld which aligns. Fix `1a8fc15`: sweet-init aligns sp at entry. Rebuilt `research/root-hfs.img`, QEMU `research/q0027-hfs2` OK. Phone image `shim/out/shim-boot-0027b.img` SHA256 `098ba46c...c5c` (payload-0027b, ramdisk match check-0027b) ready, awaiting FASTBOOT. Lesson: Assembly `;` is a comment in Apple arm64 asm (use .macro, not ;-joined #define).
- **HFS ROOT WORKS (QEMU).** 0027 CI `38779441895` PASSED, `xnu/out-0027` SHA256 `a8643886...5f6a`. QEMU `research/q0027-hfs` (RAM disk = `research/root-hfs.img`, rd=md0): `BSD root: md0` -> `hfs: mounted SweetRoot on device b(1, 0)` -> `load_init_program: attempting to load /sbin/launchd` -> `*** SWEET userspace: init running as PID 1 ***` + heartbeat, no panic. Phone image `shim/out/shim-boot-0027.img` (payload-0027 = 0027 kernel + sweet-0021.adt + root-hfs.img + rd=md0, ramdisk byte-match check-0027) ready; awaiting FASTBOOT for phone test.
- **HFS ROOT WORK (Claude), patches 0026-0027.**
  - 0026 (`f0e990c` + link fix `64c96f4`): Apple OSS **hfs-627.100.6** (commit `31d5ec238dfab485e78a9a30b8b92d13e8914f03`, paired with xnu-8796.141.3 in macos-135 release.json) staged by `xnu/patches/0026-stage-hfs.sh` into `bsd/sweet_hfs` (core + hfs_encodings; AppleKeyStore include/key prototypes guarded by CONFIG_PROTECT; SDK TargetConditionals copied). New config option `sweet_hfs` (MASTER + MASTER.arm64.sweet); 40 `optional sweet_hfs` files; Makefile.template foreach adds `_CFLAGS_ADD = -include sweet_hfs_compat.h -I...` (MUST come after kernel -D flags; compat undefs BSD/XNU_KERNEL_PRIVATE and the CONFIG_* HFS tests so kext-config.h decides: macOS profile, no content protection). `sweet_hfs_iokit.c` replaces hfs_iokit.cpp/hfs_encodings_kext.cpp: `sweet_hfs_register()` called in bsd_init after vfsinit (encodings init, vfs_fsadd VFS_TBLCANMOUNTROOT, zones, sysctls); is_ejectable 0, iterate_media none, serial KERN_NOT_SUPPORTED, OSKextGetCurrentLoadTag -> kOSKextInvalidLoadTag (renamed via compat macro). hfs_encodings' own `hfs_allocated` renamed per object (separate kexts on Apple; first CI `38777325964` failed on that duplicate at link). mockfs now validates Mach-O magic (its TODO) so non-Mach-O roots fall through to HFS (new fs entries come AFTER mockfs in vfsconf). CI `38778133720` PASSED, `xnu/out-0026` SHA256 `0b49170c...701a`, prints `SWEET HFS: hfs-627.100.6 registered`.
  - HFS+ image: `research/root-hfs.img` (8 MB, mkfs.hfsplus from AUR hfsprogs installed with user approval; loop-mounted with sudo; /sbin/launchd = research/sweet-init, /dev; fsck.hfsplus OK). QEMU `research/q0026-hfs`: `BSD root: md0` then data abort in MD5Init from hfs_getvoluuid (mount computes volume UUID).
  - 0027 (`14bf2b7`): 0020 stager also copies BearSSL hash/md5.c + codec/dec32le.c/enc32le.c; `corecrypto_md5.c` SWEET branch converts MD5_CTX <-> br_md5_context around br_md5_* calls; wrappers sweet_md5.c/sweet_dec32le.c/sweet_enc32le.c. Host test 40/40 match md5sum. CI **`38779441895`** running.
  - Local tooling: `tools/xnu-cc.py` has `XNU_CC_POST` (flags after logged ones). Sandbox `research/bt28/xnu` = 0025 CI tree + 0026/0027 edits; originals `research/pre0026`, `research/pre0027`. HFS local objects `research/hfs-objs`. Checker entries updated to bt28.
  - SHA-1 / SHA-384/512 still have no provider.
- **MILESTONE ON HARDWARE: USERSPACE PID 1 RUNS ON THE PHONE.** Boot 2026-10-15 17:16:00 UTC (`research/boot0025.log`: send OKAY0.410s, boot OKAY0.135s), image `shim/out/shim-boot-0025.img` SHA256 `9dbfa0b1...89db0`. Photo `IMG_20261015_191733.jpg`: `BSD root: md0` -> `load_init_program: attempting to load /sbin/launchd` -> `*** SWEET userspace: init running as PID 1 ***` -> repeated `SWEET init: still alive` (5 s heartbeat), no panic. Kernel 0025 + mockfs RAM disk + built-in no-entitlements AMFI. No flashing performed.
- **NEXT (toward real iOS userland):** real root filesystem with real binaries. Options: (a) build Apple OSS `hfs` (macOS 13.5 tag `hfs-627.100.6`) into SWEET like libpthread, then an HFS RAM disk image; (b) iOS 16 dyld + libSystem/dyld shared cache from the IPSW (needs APFS or repacking onto HFS). Expected hurdles: entitlements (sweet_amfi grants none), SHA-1/MD5/SHA-384/512 providers (BearSSL), missing kexts/daemons. Current init is a static test program, not launchd.
- **MILESTONE: USERSPACE RUNS (QEMU).** 0025 CI `38774788048` PASSED, `xnu/out-0025` SHA256 `5496069b...4877`. QEMU `research/q0025-rd` (150 s, no panic): `BSD root: md0` -> `load_init_program: attempting to load /sbin/launchd` -> `*** SWEET userspace: init running as PID 1 ***` -> `SWEET init: still alive` every 5 s. Exec, ad-hoc SHA-256 code signature, syscalls, devfs /dev/console, select timeouts all work. Phone image prepared: `shim/out/shim-boot-0025.img` 16,515,072 bytes SHA256 `9dbfa0b15140c0bd733a16c4f727f6a7df44a8741f98d8e266d5f2fa4f689db0` (payload-0025 = 0025 kernel + sweet-0021.adt + research/sweet-init + rd=md0; ramdisk byte-match check-0025). Waiting for user to put phone in FASTBOOT.
- **0024 CI `38773277811` PASSED**, `xnu/out-0024` SHA256 `a0ba964b...4348`. QEMU `research/q0024-rd` and **PHONE (boot 2026-10-15 17:05:55 UTC, `research/boot0024.log`, image `shim/out/shim-boot-0024.img` SHA256 `cf3dac86...6964`, 16,510,976 bytes, payload `shim/out/payload-0024.bin` = 0024 kernel + `shim/out/sweet-0021.adt` + RAM disk `research/sweet-init` + rd=md0; ramdisk byte-match `check-0024`)** agree: `Added memory device md0`, `BSD root: md0`, `load_init_program: attempting to load /sbin/launchd`, then data abort in accelerate_entitlement_queries (PC 0x...77d2458, FAR 0x1a0) = NULL AMFI interface. Photo `IMG_20261015_190715.jpg`. RAM disk root is physically verified.
- **0025 (`2518c79`)**: new `bsd/kern/sweet_amfi.c` registers a built-in AMFI interface at EARLY_BOOT/MIDDLE: no entitlements (adjustContextWithoutMonitor succeeds without creating a context, queries KERN_DENIED, copies NULL/KERN_NOT_FOUND, get_xml/transmuted false); WithMonitor KERN_NOT_SUPPORTED; OSEntitlements_query and query_context_to_object panic explicitly (CoreEntitlements closed; no kernel caller); TrustCache version 0 (0015 keys on trust_cache_rt==NULL, img4 absent -> still unavailable); CoreEntitlements table zero (PMAP_CS only). Compiles locally; checker updated (bt27 entries). CI **`38774788048`** running. Next artifacts `xnu/out-0025`, `research/bt29`, `research/run29.log`. Real iOS binaries will need real entitlements later (no OSS CoreEntitlements exists).
- **0021-0024 RAM-disk-root iteration (Claude), QEMU only so far, no new phone boot since 0020.** Each kernel tested with `tools/mkpayload.py KERNEL research/q0021.adt OUT --ramdisk research/sweet-init --cmdline "debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress rd=md0"` then `tools/qemu-sweet.py OUT DIR --secs 150 --kernel KERNEL --fail-on-panic` (PATH needs ~/swift/usr/bin for llvm-nm). Fresh ADTs: `research/q0021.adt` (QEMU), `shim/out/sweet-0021.adt` (phone).
  - 0021 CI `38769532000` PASSED, `xnu/out-0021` SHA256 `2b7500a1...962e`, `research/bt25`/`run25.log`. QEMU `research/q0021-rd`: md0 root, then assert `iomediaClass` in IOService::publishHiddenMedia.
  - 0022 (`20ece9c`, publishHiddenMedia without IOMedia class) CI `38770666158` PASSED, `xnu/out-0022` SHA256 `a4d0e787...30c5`, `research/bt26`/`run26.log`. QEMU `research/q0022-rd`: data abort in SHA256_Init (NULL g_crypto_funcs) from vm_kernel_addrhash_internal.
  - 0023 (`c8089d1`, libkern SHA256_Init/Update/Final use built-in ccsha256_ltc_di + ccdigest_init/update on SWEET; also serves code-signing SHA-256; SHA-1/MD5/SHA-384/512 still have NO provider - BearSSL from 0020 could supply them later) CI `38771876423` PASSED, `xnu/out-0023` SHA256 `d763fed7...1003`, `research/bt27`/`run27.log`. QEMU `research/q0023-rd`: **mockfs mounted md0, `load_init_program: attempting to load /sbin/launchd`**, then panic `mockfs_fsnode_vnode failed to create fictitious pages ... rvalue = 5` (mockfs.c:351).
  - 0024 (`3293c80`): mockfs used memdev mi_base (4K page number of a VIRTUAL kernel address for IOKit RAMDisk, truncated to 32 bits) << PAGE_SHIFT as a physical address. Now zero-copy pager mapping only when mi_phys (with 4K units); virtual md uses pagein/blockmap/strategy -> memdev bcopy. memdev's own mdPhys path assumes 4K ppnums (unsafe on 16K arm64), so do NOT switch the RAMDisk to physical. CI **`38773277811`** running. Next artifacts `xnu/out-0024`, `research/bt28`, `research/run28.log`.
  - Sandboxes: bt25/bt26/bt27 contain the next patch's edited files; pristine copies in `research/pre0022`..`pre0024`.
- **PATCH 0021 (Claude): RAM disk root via mockfs -> first userspace program.** Commits `9c37771` (initial; CI `38768500280` FAILED: mockfs_fsnode.c kfree_type(mockfs_fsnode_t) typed-alloc static assert; local xnu-cc stubs `__builtin_xnu_types_compatible` so it can't catch this) and `193772f` (fix), CI **`38769532000`** running: https://github.com/dipa-studio/ios-sweet/actions/runs/38769532000. Next artifacts: `xnu/out-0021`, `research/bt25`, `research/run25.log`.
  - Kernel: `config/MASTER.arm64.sweet` FILESYS_BASE += `mockfs` (XNU's built-in FS: root device is itself a Mach-O, served as /sbin/launchd, plus /dev for devfs). Upstream mockfs bit-rot fixed: kalloc_type/kfree_type used the POINTER typedef mockfs_fsnode_t (nodes were 8 bytes) -> `struct mockfs_fsnode`. `IOService::setRootMedia` asserted IOBlockStorageDriver class exists (IOStorageFamily absent): SWEET-only, a missing class means root=NULL (same as IOSetImageBoot); Apple path keeps assert. All 21 patches apply (`research/check-patch-order.py`); 5 TUs compile locally with -DMOCKFS (`research/mockfs-objs`), imports resolve vs 0020 kernel. Sandbox `research/bt24/xnu` now holds 0021 versions of those 3 files; pristine 0020 copies in `research/pre0021`.
  - Tools: `tools/mkpayload.py --ramdisk IMAGE` appends IMAGE as an extra payload segment right after the kernel, moves kernel_end past it (shim unchanged: ADT+boot_args follow it, image stays below topOfKernelData), and adds ADT `/chosen/memory-map` `RAMDisk`=<phys,size>. Boot with `rd=md0`. Without --ramdisk output is byte-identical to existing 0020 QEMU and phone payloads (verified). `tools/sweet-init.S` + `tools/mkinit.py`: PIC static init (open /dev/console, print PID, heartbeat every 5 s via select, never exits) wrapped into a static arm64 Mach-O (__PAGEZERO, __TEXT@0x100000000, LC_UNIXTHREAD, ad-hoc SHA-256 CodeDirectory v0x20400, 4K pages). Build: `~/swift/usr/bin/clang -target arm64-apple-ios16.0 -c tools/sweet-init.S -o X.o && tools/mkinit.py X.o research/sweet-init` (ld64.lld cannot link static iOS). Signature not verified by a real codesign; the kernel will tell.
  - QEMU with 0020 kernel + RAM disk (`research/q0020-rd.bin`, rd=md0, 150 s `research/q0020-rd150`): `Added memory device md0`, **`BSD root: md0, major 1, minor 0`** (plumbing works), then the setRootMedia assert (now fixed in 0021). NOTE: QEMU needs >60 s to reach IOFindBSDRoot (60 s run showed nothing after stf0; likely RTC/inittodr wait), so use --secs 150.
  - NEXT: download 0021, QEMU 150 s with ramdisk; expect mockfs mount then init exec -> `*** SWEET userspace: init running as PID 1 ***`. Then phone boot with same payload (phone ADT shim/out/sweet-0020.adt pattern + --ramdisk + rd=md0).
- **0020 PHONE RESULT (user transcription of screen, 2026-10-15): KERNEL INIT COMPLETE, WAITING FOR ROOT.** After Skywalk self-tests PASSED, kminit, ifnet_attach lo0/gif0/stf0, screen prints `BSD`, then `Waiting on <dict><key>IOProviderClass</key><string>IOMedia</string><key>Content</key><string>Apple_HFS</string></dict>` and `Failed to write boot-picker-bringup-reason to NVRAM.` No panic. This is IOFindBSDRoot waiting for a root device (none exists); NVRAM warning is expected (no NVRAM driver). QEMU `cpu_idle` snapshot is presumably the same wait (not GDB-confirmed; milestone GDB run was cancelled by user since phone answered it). Draft `research/check-milestones20.gdb` (hbreaks inittodr/vfs_mountroot/IOFindBSDRoot/bsd_utaskbootstrap) left uncommitted/ignored.
- **NEXT MILESTONE: root filesystem -> launchd.** Plan: (1) RAM disk: shim places an image in RAM, ADT /chosen/memory-map `RAMDisk` entry, boot-arg `rd=md0` (XNU md driver). (2) HFS is a separate kext on Apple; build Apple OSS `hfs` (tag paired with macOS 13.5 in distribution-macOS macos-135 release.json) into SWEET like libpthread 0019. APFS is closed. (3) Minimal HFS image with iOS 16 launchd (later: dyld/shared cache; AMFI/code-signing absent will be a later hurdle). User's end goal is full iOS GUI.
- **0020 (AES) VERIFIED + PHONE BOOTED (Claude took over after Codex usage limit).** Actions `38765422261` (`ab9e23f`) PASSED; kernel `xnu/out-0020/kernel.development.sweet` checksum OK; buildtree `research/bt24`, log `research/run24.log`. Actual-kernel AES checks PASS (`research/check-aes20.gdb` / `.log`, 12 extra CBC cases match OpenSSL). 45s QEMU `research/q0020`: no panic, zero unknown glyphs, past old TCP/AES panic -> Skywalk self-tests PASSED -> bsd_autoconf kminit -> ifnet_attach lo0/gif0/stf0; final snapshot PC in `cpu_idle` (idle or waiting; NOT yet diagnosed, could be root-device wait). Image `shim/out/shim-boot-0020.img` 16,461,824 bytes SHA256 `775200cb02d95fc750981516b79d91dcdc96184bbe67a38f9d57c59b4a391828`, ramdisk byte-matches `payload-0020.bin` (`check-0020`). **Phone temporarily booted 2026-10-15 16:10:00 UTC**: one FASTBOOT product sweet, send OKAY0.407s, boot OKAY0.134s, total0.549s, exit0 (`research/boot0020.log`). Awaiting user photo; do not reboot the phone before it. NEXT: read photo; diagnose what QEMU waits on after stf0 (GDB backtrace of threads / longer run). Codex's STOP request below is complete.
- User requests STOP after completing the current0020 AES iteration (Actions/QEMU/phone), then leave a precise committed handoff for another AI. User says FASTBOOT ready again after0019 photo. Do not start a new blocker fix after this iteration.
- Goal: iOS16-generation XNU on Redmi Note10Pro sweet/SM7150. Always commit, use gh/Actions for kernel builds, keep this handoff current. Temporary fastboot boot only; preserve Android/backups. No userland/root filesystem boot yet.
- **Patch0020 update:** initial `e71098d` / Actions `38765012466` superseded by scalar-only compile flags. Added -mgeneral-regs-only to all six provider/BearSSL TUs and corecrypto_aes.c; object disassembly now confirms no SIMD. Updated implementation `ab9e23f`, Actions `38765422261` queued/running: https://github.com/dipa-studio/ios-sweet/actions/runs/38765422261. Do not launch another run. portable BearSSL AES CBC and legacy one-block ECB wired directly into SWEET corecrypto_aes.c. Genuine bitslice implementation, no CPU AES/SIMD requirement, heap or fake global crypto table. Source fetched by `0020-stage-bearssl.sh` from official BearSSL git at immutable `7bea48e5e850ab4cafbe68d3765cdaba13a86d6f`; MIT license retained, downloaded source stays out of git. Kernel limits include adapter only. All128/192/256-bit keys, CBC encrypt/decrypt, copied contexts, in-place/out-of-place; zero block count and size overflow handled, invalid keys clear context. Seven TUs compile to arm64 objects (`research/aes-objs`), imports resolve, all20 patches apply and sources match (`research/patch0020-check.log`). Sandbox `research/bt23/xnu`, originals `research/pre0020`. NEXT: Actions then actual-kernel NIST/random CBC/ECB checks and >=45s QEMU before another phone boot. Next artifacts out-0020 / bt24 / run24.log. 0020 updated CI pending. Local sources/checks match the scalar-only patch.
- **Latest built kernel0019**, implementation `256d3f0`, patches0001-0019. Actions `38763072907` PASSED (4m5s): https://github.com/dipa-studio/ios-sweet/actions/runs/38763072907. Kernel `xnu/out-0019/kernel.development.sweet`, SHA256 `a27df7568f03fb541cf0e8024c0a594fa178a85054fcf9eba29158cbd4c5ecc0`; checksum verified. Log/source `research/run23.log` / `research/bt23` (no nested .git). Seven pthread source/build/header files and staged adaptations byte-match local compile inputs.
- **0019 hardware photo received** `IMG_20261015_175306.jpg`: pthread registration visible, passes mbinit and DLIL checksum/scheduling, same AES fault as QEMU: PC`0xfffffe00079174e0` (`aes_encrypt_key128`), LR`0xfffffe000767c3a4` (`tcp_init`), FAR0x78, x8=0. Disassembly proves NULL g_crypto_funcs->ccaes_cbc_encrypt. Photo processed; no more0019 observation pending. General crypto table remains absent, other hash/mode operations may still fault after0020. DO NOT bypass TCP or install fake crypto. XNU already contains SHA256/HMAC implementations; AES source there is a dispatch wrapper. Full future provider interface libkern/libkern/crypto/register_crypto.h. Apple/corecrypto default README describes evaluation-only licensing, so no external corecrypto was fetched or staged; BearSSL selected for this portable AES integration.
- **0019 pthread FIXED in QEMU and observed beyond its init on phone**: 45s QEMU `research/q0019`, zero unknown glyphs, then AES panic. Actual-kernel API checks PASS `research/check-pthread19.gdb` / `.log`, guest `research/q0019-api`: function/callback table identity, actual provider pthread_init returns, locks/global hash/threadcall/typed zones initialized, thread_selfid via syscall shim + buffer bounds, process hash init/delete via shims restores NULL. Monitor quit prints expected remote-disconnect after PASS. All fixture guests stopped.
- **0019 provider**: genuine Apple libpthread-514.100.2 commit `a6c70ca0e544347a0377babf748f831577ec24f9`, exact paired release with xnu8796.141.3 in distribution-macOS/macos-135/release.json (`research/macos-135-release.json`). 0019 shell stage runs before patches (workflow copies *.sh); three SWEET wrapper TUs use kext BSD header view, vm_fault_external, renamed provider-side pthread_kern/current_uthread; register real function table at EARLY_BOOT SECOND, real pthread_init later. Stager applies bounded address casts, zero user addresses, proper panic format. SWEET opaque ksyn storage enlarged48->56 for trailing kwe_uth, checked with provider static assertion. Local pre-CI TargetConditionals fixture was syntax-only; artifact bt23 carries actual SDK header and CI is authoritative.
- **Phone0019 temporarily booted** 2026-10-15 15:51:36 UTC: exactly one FASTBOOT/product sweet verified; send OKAY0.406s, boot OKAY0.134s, total0.546s, exit0 (`research/boot0019.log`). Image `shim/out/shim-boot-0019.img`, 16,457,728 bytes, SHA256 `19429a57e486b856880df9e27327ed488e0139345311fdb0350974e72103a9a1`. Phone ADT/payload `shim/out/sweet-0019.adt` / `payload-0019.bin` timer19, unpacked ramdisk byte-match (`check-0019`). EntryPA0xb146b488, kernelendPA0xb2000000, boot_argsPA0xb2004000/top0xb2008000. Last user FASTBOOT+USB reply preceded this boot; do not assume phone is still in FASTBOOT.
- **CPU0018 physically verified** by `IMG_20261015_173951 (2).jpg`: boot CPU0 registered, real SGI1 verified, initialization complete -> BSD/MAC/mbinit -> old pthread-table panic. SweetCPU uses legacy IOCPU, ml_processor_register/processor_start/initCPU, real SGI1 clears SIGPdisabled before max CPUs completion; no forced flags/skipped waits. CPU APIs and two-CPU rejection passed (`research/check-cpu18.gdb`, `q0018-api`, `q0018-smp`). Single core; SMP/suspend unsupported. CURRENT SWEET has NO USE_APPLEARMSMP.
- QEMU next run: generate fresh `tools/mkadt.py research/q0020.adt --qemu` (timer27), mkpayload with `debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress`, then `tools/qemu-sweet.py PAYLOAD OUT --secs 45 --kernel KERNEL --fail-on-panic`. 12s can miss later panics because IORTC waits30s. Runner snapshot only: panic exit2, blank/incomplete exit3; absence of panic does not mean complete boot. Qualcomm XPU/ABL/panel and phone SMC PSCI require hardware checks.
- Local checker `tools/xnu-cc.py research/run23.log research/bt23 FILTER --per-file`, XNU_CC_OBJDIR for objects. For new BearSSL TUs add XNU_CC_EXTRA with -I bt23/xnu/libkern/crypto/sweet_bearssl/{inc,src}. Patch checker `research/check-patch-order.py` uses temporary upstream index; source fixtures/artifacts ignored under research. GDB mask registers to unsigned64, use function-entry manual-call return traps, discard manipulated guests. No QEMU guest intentionally left running.

## Session log and earlier handoffs (superseded by the active summary above)

### Prior 0019 transition notes (superseded)

- Goal: iOS 16 generation XNU on sweet. Always commit completed work, use gh/Actions for kernel builds, keep this handoff current. Temporary fastboot boot only; preserve Android/backups.
- **0019 build PASSED** Actions `38763072907` (4m5s), implementation `256d3f0`. Kernel `xnu/out-0019/kernel.development.sweet`, SHA256 `a27df7568f03fb541cf0e8024c0a594fa178a85054fcf9eba29158cbd4c5ecc0`; checksum verified. Log/source `research/run23.log` / `research/bt23`; all seven patched files and staged provider adaptations match local compile inputs. Prefer bt23 as the next baseline.
- **0019 pthread FIXED in QEMU**: 45s run `research/q0019` registers provider, passes mbinit, DLIL SUM16 checks and DLIL thread scheduling. Actual-kernel GDB checks `research/check-pthread19.gdb` / `.log` PASS: function/callback table identity, real _pthread_init returns, locks/global hash/cleanup threadcall/typed zones non-NULL, thread_selfid via actual syscall shim with output bounds, process hash init/delete through actual shims restores NULL. Guest stopped after tests; no fixture left running. GDB monitor quit prints expected remote disconnect after PASS.
- **Current next blocker:** QEMU0019 panic data abort FAR0x78 at `aes_encrypt_key128` PC`0xfffffe00079174e0`, called by `tcp_init` LR`0xfffffe000767c3a4`. Disassembly confirms `g_crypto_funcs == NULL`, loading ccaes_cbc_encrypt at+0x78. General crypto registration is still absent; DO NOT bypass TCP or supply fake crypto. Next integrate a genuine compatible crypto implementation/provider (AES first needed here, other digest/mode APIs also used). XNU contains SHA256/HMAC but no AES implementation; its corecrypto_aes.c is a dispatch wrapper. Full crypto table is in libkern/libkern/crypto/register_crypto.h.
- **0019 phone image prepared and verified**, user reports FASTBOOT+USB again; exactly one device/product sweet verified. `shim/out/shim-boot-0019.img`, 16,457,728 bytes, SHA256 `19429a57e486b856880df9e27327ed488e0139345311fdb0350974e72103a9a1`; unpacked ramdisk matches phone payload (`shim/out/check-0019`, `payload-0019.bin`, `sweet-0019.adt`, timer19). EntryPA0xb146b488; kernelendPA0xb2000000; boot_argsPA0xb2004000/top0xb2008000. Usual debug cmdline cpus=1/-noprogress. **Temporarily booted 2026-10-15 15:51:36 UTC**: send OKAY0.406s, boot OKAY0.134s, total0.546s, exit0 (`research/boot0019.log`). User asked for photo after45s. Await the photo; DO NOT reboot again before observation. Bootloader acceptance only; no0019 physical progress observed yet.
- **Hardware photo confirms 0018 progress** (`IMG_20261015_173951 (2).jpg`): real phone prints `SWEET CPU: boot CPU 0 registered, SGI1 verified, initialization complete`, enters BSD/MAC, reaches `mbinit: done`, then the same missing-pthread-function-table panic as QEMU at pthread_shims.c:318. CPU completion/interrupt delivery is now physically observed. The photo was received; no further phone boot performed yet.
- **Patch0019 committed/pushed `256d3f0`; Actions `38763072907` queued/running:** https://github.com/dipa-studio/ios-sweet/actions/runs/38763072907. Do not launch a duplicate. genuine Apple OSS libpthread-514.100.2 provider (immutable commit `a6c70ca0e544347a0377babf748f831577ec24f9`), paired with xnu-8796.141.3 by Apple's `distribution-macOS/macos-135/release.json` (`research/macos-135-release.json`). `0019-stage-libpthread.sh` fetches/stages it before patches; workflow now copies shell scripts too. SWEET-only wrappers compile all three real kernel sources, rename provider-side symbols, use kext header view and vm_fault_external, register the real function table at EARLY_BOOT SECOND. Existing pthread_init then runs the actual provider initialization. No pthread wait/syscall is bypassed. Source staging includes bounded explicit address casts/zero user addresses and panic-format fixes. SWEET kernel opaque ksyn storage enlarged from48 to56 bytes to contain this provider's trailing kwe_uth, with a provider size assertion. Local compile of three provider TUs plus pthread_shims/kern_fork passed; imports resolve and provider exports do not collide with existing kernel symbols. All19 patches apply and changed sources match (`research/patch0019-check.log`). Source sandbox `research/bt22/xnu`, originals `research/pre0019`, objects `research/pthread-objs`. Local TargetConditionals fixture only has target macros for syntax; CI stages the real macOS SDK header and is authoritative. Next Actions artifacts: out-0019 / bt23 / run23.log; run QEMU >=45 seconds. 0019 CI is pending; no 0019 QEMU run yet.
- **Latest kernel implementation `e7d9550`, patches0001-0018; CI `38760004398` PASSED (3m56s)**: https://github.com/dipa-studio/ios-sweet/actions/runs/38760004398. No build is pending. Kernel `xnu/out-0018/kernel.development.sweet`, SHA256 `b3c8d59acc88f93a2c836eb2527a05ed4fbaac7c580a2ea8c3b795137f4aadf1`; checksum verified. Source/log `research/bt22` / `research/run22.log` (baseline for patch0019; no nested .git). New driver/config sources match CI exactly.
- **CPU wait FIXED in QEMU**: SweetCPU uses legacy IOCPU, validates single boot CPU, ml_processor_register -> processor_start -> initCPU real SGI1 -> scheduler/IOKit completion -> ml_set_max_cpus. It does NOT force a flag or skip a wait. CPU APIs passed: Running CPU0, processor_array[0] association, SGI signalling enabled, name, unsupported secondary-start error (`research/check-cpu18.gdb` / `.log`, `research/q0018-api`). Single-core only; SMP and suspend unsupported. Two-CPU topology fails explicitly (`research/q0018-smp`).
- **Next blocker reproduced in 45s QEMU** (`research/q0018-long`, zero unknown glyphs): BSD initialization reaches MAC framework, network mcache and `mbinit: done`, then **`pthread kernel extension not loaded (function table is NULL). @pthread_shims.c:318`**. Do not bypass pthread_init: find/integrate the matching open-source libpthread kernel implementation and register its real function table. A 12s run `research/q0018` looked panic-free before the 30s IORTC wait elapsed; use >=45s or GDB checkpoints for the next iteration. No userland/root filesystem boot yet.
- **Phone 0018 TEMPORARILY BOOTED** 2026-10-15 **15:31:15 UTC**: exactly one FASTBOOT device verified product sweet; send OKAY0.400s, boot OKAY0.134s, total0.539s, exit0. Local log `research/boot0018.log`. This confirms bootloader acceptance only. User was asked for a photo; await it, DO NOT reboot the phone again before their observation. No flashing performed.
- Current hardware image `shim/out/shim-boot-0018.img`, **16,437,248 bytes**, SHA256 **`b7a23595d143a76c72c9fe2cfcc5d77a8f16987b5af1a2703bbffdfa6922908c`**. Phone ADT/payload `shim/out/sweet-0018.adt` / `shim/out/payload-0018.bin` (timer19), unpacked ramdisk exactly matches payload (`shim/out/check-0018`). EntryPA0xb146b488; kernelendPA0xb1ffc000; boot_argsPA0xb2000000/top0xb2004000. Command line `debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress`.
- QEMU payload/ADT `research/q0018.bin` / `research/q0018.adt` (timer27). Run `python3 tools/qemu-sweet.py research/q0018.bin research/NEW-OUT --secs 45 --kernel xnu/out-0018/kernel.development.sweet --fail-on-panic`; fresh ADT seed for a new payload. Output fb.png/fb.txt/regs.txt/qemu.log/result.json. Visible panic exit2; blank/incomplete decode exit3; no_panic_observed is snapshot-only, not successful boot. -noprogress prevents the graphical indicator corrupting console text. Qualcomm XPU/bootloader/panel and SMC PSCI still need physical validation.
- Patches0014-0017 fixed PRNG registration/direct SHA256, explicit unavailable trust-cache service with rejected loads/no matches, built-in SweetPlatformExpert, and timer PPI from ADT (phone19/default19, QEMU27). All their checks/artifacts are preserved in the history below. CURRENT SWEET has NO USE_APPLEARMSMP; do not assume gCPUIC/gAIC/gPMGR AppleARMSMP globals exist.
- Local single-file checker: tools/xnu-cc.py research/run22.log research/bt22 FILTER --per-file, output via XNU_CC_OBJDIR. Patch checker `research/check-patch-order.py` applies all18 via temporary index, matches sandbox sources. Sandbox for0018 `research/bt21/xnu`, originals `research/pre0018`, objects `research/cpu-objs`. First compiler issues were missing machine/atomic.h and nonexistent getNameSymbol; fixed and both TUs compiled successfully before CI. Fixtures/artifacts under research are local-only/ignored.
- GDB: mask int(register)&((1<<64)-1) for unsigned kernel pointers; use function-entry manual-call return traps, discard manipulated guests. No QEMU fixture is intentionally left running.

### Earlier active handoff snapshot (superseded)


- **0018 milestone:** Actions `38760004398` PASSED (3m56s), checksums passed, new CPU/build files byte-match CI tree. Kernel `xnu/out-0018/kernel.development.sweet`, SHA256 `b3c8d59acc88f93a2c836eb2527a05ed4fbaac7c580a2ea8c3b795137f4aadf1`; log/tree `research/run22.log` / `research/bt22`. QEMU `research/q0018` 12s: boot CPU registered, SGI1 verified, CPU initialization complete, **BSD initialization entered** (copyright, MAC framework, buffer headers), no panic/zero unknown glyphs. Direct CPU API checks PASS (`research/check-cpu18.gdb`, `.log`, `research/q0018-api`): CPU0 Running, processor matches processor_array[0], real SGI enabled, CPU name correct, secondary-start returns KERN_NOT_SUPPORTED. Two-CPU ADT correctly panics before registration (`research/q0018-smp`). Longer 45s run `research/q0018-long` still running; next BSD wait/fault not yet identified.
- **Prepared 0018 phone image:** `shim/out/shim-boot-0018.img`, 16,437,248 bytes, SHA256 `b7a23595d143a76c72c9fe2cfcc5d77a8f16987b5af1a2703bbffdfa6922908c`. Phone ADT/payload `shim/out/sweet-0018.adt` / `shim/out/payload-0018.bin`, usual debug args with cpus=1 and -noprogress. Unpacked ramdisk byte-compared (`shim/out/check-0018`). Entry PA remains 0xb146b488; kernel end PA 0xb1ffc000, boot_args 0xb2000000/top0xb2004000. User reports FASTBOOT+USB ready; read-only check found exactly one product sweet. NEXT: temporary fastboot boot of this verified image, record send/boot results, then await hardware photo without rebooting again.
- Patch 0018 committed/pushed **`e7d9550`**; Actions **`38760004398`** queued/running: https://github.com/dipa-studio/ios-sweet/actions/runs/38760004398. Do not launch a duplicate run.
- Active continuation: patch **0018** implemented in `iokit/Kernel/arm/SweetCPU.cpp` + arm64 source list + exact qcom,kryo470 personality. Keeps legacy IOCPU, accepts ONLY the single boot CPU, fills ml_processor_info from parsed topology with CPU object as cpu_id, calls ml_processor_register and processor_start. initCPU sends SGI1 via ICC_SGI1R_EL1 and waits (1-second deadline) for real cpu_signal_handler to clear SIGPdisabled. Only after successful startup calls ml_cpu_init_completed/cpusRunning and publishes completion/max CPUs. Secondary startup/suspend are explicitly unsupported. Local C++ compile passed for both TUs (`research/cpu-objs`), all imports resolve, object contains dsb ishst / ICC_SGI1R / isb; all18 patches apply in order and match sources. Sandbox `research/bt21/xnu`, originals `research/pre0018`; next CI artifacts should use `xnu/out-0018`, `research/bt22`, `research/run22.log`. User has the phone available; first run QEMU, then prepare/temporarily boot useful hardware progress.
- Goal: iOS 16 generation XNU on Redmi Note 10 Pro / sweet. User wants persistent progress, commits, kernel builds via `gh` Actions, and this handoff kept current across agent switches. Temporary `fastboot boot` only; preserve Android and backups.
- Latest kernel implementation: **`aa3f991`**, patches **0001-0017**. All four new Actions builds PASSED: 0014 `38748543058` (4m0s), 0015 `38749549779` (4m3s), 0016 `38750733996` (4m3s), 0017 **`38751507437` (4m7s)**. Latest: https://github.com/dipa-studio/ios-sweet/actions/runs/38751507437. No build is pending.
- **Current blocker: CPU initialization has no SWEET CPU driver.** QEMU registers SweetPlatformExpert, then kernel_bootstrap_thread waits in **kperf_init_early -> ml_wait_max_cpus**. Confirmed via actual-kernel GDB call stack (`research/locate-cpu-wait2.log` / `.symbols`) and normal-run `max_cpus_initialized == 2` (MAX_CPUS_WAIT). Do not force the flag or remove the wait. Next implement real CPU registration and completion using XNU's CPU/processor interfaces. CURRENT SWEET uses legacy IOCPU (`USE_APPLEARMSMP` is absent); enabling AppleARMSMP is a separate architectural change requiring GIC/PMGR services. No userland/root filesystem boot yet.
- Fixed this session: PC=0 unregistered PRNG callback (0014, built-in HMAC-SHA256 provider + direct entropy/write_random hashing); absent Image4/AMFI trust-cache service (0015, unavailable runtime, loads rejected and queries no-match); missing platform driver (0016, SweetPlatformExpert exact personality); incorrect QEMU timer IRQ (0017, validated ADT PPI property, phone default19/QEMU27).
- **Latest kernel** `xnu/out-0017/kernel.development.sweet`, SHA256 `055a80190ab998b4df9203f1102072dcc78faad1779a537a514e522b682b582f`. Buildtree/log **`research/bt21` / `research/run21.log`**; all nine changed C/C++/header/config files byte-match local checked sources. Checksums passed. Entry PA `0xb146b488`, kernel end PA `0xb1ff4000`. Preferred baseline for patch 0018: `research/bt21/xnu` (no nested .git).
- **QEMU verified**: `research/q0017-noprogress` ran **30 seconds**, no visible panic, **zero unknown glyphs**, IRQ exceptions returned normally (4 IRQs, tickless idle). It still waits for CPU completion, so this is NOT a completed boot. Earlier 15s run `research/q0017` captured 122 IRQs, timer value27, GICR_ISENABLER0 `0x08000002`, and CPU WAIT; graphical progress overlay corrupted 13 glyphs. Add **`-noprogress`** to keep the console readable. New runner classifies incomplete/blank decoding explicitly (exit3 with --fail-on-panic), visible panic exits2. Snapshot-only; use regs/GDB to diagnose stalls.
- Current QEMU payload/ADT: **`research/q0017-noprogress.bin` / `research/q0017.adt`**. Run: `python3 tools/qemu-sweet.py research/q0017-noprogress.bin research/NEW-OUT --secs 30 --kernel xnu/out-0017/kernel.development.sweet --fail-on-panic`. Generate fresh seed with mkadt for a new payload. QEMU virt models generic ARM execution, not Qualcomm XPU/bootloader/panel or the phone's SMC PSCI conduit.
- **Prepared phone image**: **`shim/out/shim-boot-0017.img`**, 16,416,768 bytes, SHA256 `51ff7fe30b4a6668c3452ffe1da922c5ac30cb8de60625c2a652587ecbd0ec92`. Phone ADT/payload `shim/out/sweet-0017.adt` / `shim/out/payload-0017.bin` (timer19). Unpacked ramdisk exactly matches payload (`shim/out/check-0017`). Command line **`debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam -noprogress`**. NOT sent to phone this session: known CPU-driver wait should be addressed in QEMU first. Last actual phone boot remains 0013; user's latest photo `IMG_20261015_160033.jpg` confirmed the old PRNG PC=0 panic.
- Direct QEMU/GDB API checks: PRNG 600-byte chunking, buffer bounds, changing output, zero-length reads/writes, user reseed, contended refresh, generator1 init/output PASS again on latest0017 (`research/check-prng17.gdb` / `.log`, guest `research/q0017-api`). Trust-cache unavailable-service/argument/output cases PASS on0015 (`research/check-trustcache.gdb` / `.log`). Platform registration + target/product normal/truncated/zero/null/bounds PASS on0016 (`research/check-platform.gdb`, `research/check-platform3.log`). Different platform ID correctly retains IOPanicPlatform fallback (`research/q0016-negative`). Timer PPI32 and size2 correctly panic (`research/q0017-badppi`, `research/q0017-badsize`); omitted property selects19 (`research/q0017-default`). These are API/bring-up checks, not crypto certification or SMP scheduling validation.
- Local checker **`research/check-patch-order.py`** verifies all17 patches and changed source equality using a temporary upstream git index. Fixtures/logs/artifacts under research are local-only and ignored. For GDB unsigned kernel pointers, mask `int(register_value) & ((1<<64)-1)`; x30/x29 may be rendered signed. Stop at a function entry for manual-call return traps, and discard each manipulated guest after API tests. All fixture QEMU guests have been stopped.


- User wants persistent progress toward iOS 16, commits for completed changes, builds through `gh` / GitHub Actions.
- Keep this file current at every milestone so another agent can resume immediately, including when usage limits interrupt a session.
- Patch 0017 committed/pushed `aa3f991`, Actions `38751507437` running: https://github.com/dipa-studio/ios-sweet/actions/runs/38751507437. Next artifacts: xnu/out-0017, research/bt21, research/run21.log. Do not duplicate.
- Patch 0016 kernel downloaded/checksum verified: SHA256 `d06336c54baa06e0df6561a8bd7df98a9b9f7502c9381c62d1a44c045cd1ce46`, `xnu/out-0016/kernel.development.sweet`; buildtree/log `research/bt20` / `research/run20.log`. QEMU `research/q0016` (10s) prints platform expert registered, zero unknown glyphs, no panic observed BUT stalls (timer mapping still wrong until 0017). Direct actual-kernel GDB tests passed for registered instance and target/product names, including maxLength=1/0 and NULL, sentinels intact (`research/check-platform.gdb`, `research/check-platform3.log`, `research/q0016-api3`). First two fixture attempts used premature/wrong return breakpoints; discarded, corrected fixture uses start-return observation then a function-entry return trap. No phone boot yet.
- Patch 0016 CI `38750733996` PASSED (4m3s); artifacts not yet downloaded/booted. Patch 0017 prepared to fix QEMU timer fidelity BEFORE interpreting future no-panic/stall runs: actual `qemu-system-aarch64 -M virt,gic-version=3,dumpdtb=research/qemu-virt.dtb ...` timer property has virtual PPI 11 => INTID 27, while phone uses INTID 19. Kernel formerly hardcoded 19. New optional /arm-io/gic virtual-timer-intid property selects the PPI (validate exact u32 and 16..31); missing property retains phone default 19. One RO-late global drives GIC enable and sleh_irq dispatch. tools/mkadt.py emits phone19/QEMU27. Both C TUs compile (`research/timer-objs`), ADT serialize/parse checks confirm values/addresses, all 17 patches apply and match. Sandbox `research/bt19/xnu`, originals `research/pre0017`; initial local sleh compile failed due read-only exported header not updated, fixed export and BOTH recompiled successfully. CI exports normally.
- Architecture correction for next agent: CURRENT SWEET board_config does NOT define USE_APPLEARMSMP; it compiles the legacy IOCPU path, not cpu_boot_thread/IOPMGR AppleARMSMP. Existing roadmap mentions AppleARMSMP as a future approach. Do not look for gCPUIC/gAIC/gPMGR globals in the current kernel: they are absent. Determine the actual CPU-init wait after platform registration before choosing the next driver architecture.
- Patch 0016 committed/pushed as `0dcdf5c`; Actions `38750733996` running: https://github.com/dipa-studio/ios-sweet/actions/runs/38750733996. Next artifacts: `xnu/out-0016`, `research/bt20`, `research/run20.log`.
- Patch 0016 implemented: new `iokit/Kernel/arm/SweetPlatformExpert.cpp`, arm64 source list, built-in personality in KernelConfigTables.cpp guarded by ARM64_BOARD_CONFIG_SWEET. Matches only xiaomi,sweet at score 10000, keeps IOPanicPlatform fallback for other hardware. Uses IODTPlatformExpert base initialization/power domain and createNubs CPU logical-ID annotation; overrides top-level publishing because base requires nonexistent Apple NVRAM /options. Publishes real CPU/top-level nodes, provides bounded target/product names, and registers the platform service. GIC/PMGR and arm-io child-bus drivers remain separate work. Both C++ TUs compile (`research/platform-objs`), all imports resolve against 0015/own objects, all 16 patches apply and sources match. Sandbox `research/bt19/xnu`; originals `research/pre0016`. NEXT: full Actions/QEMU; inspect CPU boot waits after the platform registers, do not equate absence of panic with successful boot.
- Patch 0015 CI `38749549779` PASSED (4m3s), checksum verified. Kernel `xnu/out-0015/kernel.development.sweet`, SHA256 `ca1b424a596c7058550ad1659080a333e7643270f019656401d80bf2b876645f`; buildtree/log `research/bt19` / `research/run19.log`. QEMU `research/q0015` (10s, zero unknown glyphs) passes trust-cache handling and machine lockdown, then panics `Unable to find driver for this platform: "xiaomi,sweet" @IOPlatformExpert.cpp:2319` (IOPanicPlatform fallback). Next concrete work: add a built-in SWEET IODTPlatformExpert personality/class; keep the fallback for mismatched platforms. No phone boot yet.
- Direct trust-cache API checks PASSED in the actual 0015 kernel under QEMU/GDB (`research/check-trustcache.gdb`, `research/check-trustcache.log`, `research/q0015-api`): runtime pointers NULL, hash/UUID queries return NOT_FOUND, typed/untyped/legacy loads return NOT_SUPPORTED, capabilities outputs zero and bounds preserved, invalid-argument cases still reject. Second PRNG check also verifies contended refresh returns immediately (`research/check-prng2.log`, `research/q0014-api2`).
- Patch 0015 committed/pushed `8f851d4`, Actions run `38749549779` running: https://github.com/dipa-studio/ios-sweet/actions/runs/38749549779. Do not duplicate.
- Direct PRNG API checks passed in the REAL 0014 kernel under QEMU/GDB, stopped before trust-cache init (`research/check-prng.gdb`, log `research/check-prng.log`, guest `research/q0014-api`). Invoked read_random for 600 bytes (chunking and buffer sentinel intact), repeated output differed, zero-length read/write succeeded, write_random(64-byte input) succeeded, secondary generator 1 initialized and generated distinct output. QEMU log has no exceptions during these checks; guest was discarded after the tests. This tests API behavior, not a cryptographic certification or SMP scheduling.
- Patch 0015 implemented in `bsd/kern/kern_trustcache.c`: SWEET with missing Image4/AMFI marks both runtime pointers NULL and logs service unavailability. Existing static loader handles NULL. Public load APIs return KERN_NOT_SUPPORTED; hash/UUID queries return KERN_NOT_FOUND; capabilities outputs are zero with KERN_NOT_SUPPORTED. Invalid-input checks remain, and Apple paths/available-provider initialization retain existing behavior. No trust is granted. Local arm64 compile passed (`research/trustcache-objs`), all 15 patches apply and sources match. Sandbox `research/bt18/xnu`, baseline `research/pre0015`. Next: Actions/QEMU and direct unavailable-service API checks using GDB.
- Patch 0014 CI `38748543058` PASSED (4m0s), artifact checksum passed. Kernel `xnu/out-0014/kernel.development.sweet`, SHA256 `741de20c851755d333759d7e668346f3edb4f325efac4cc7c142d80d1f5ff75c`; buildtree/log `research/bt18` / `research/run18.log`. QEMU run `research/q0014` (10s, zero unknown glyphs) prints `SWEET PRNG: HMAC-SHA256 registered`, passes commpage initialization, loads vnguard policy, THEN panics `image4 interface not available @kern_trustcache.c:253`. PC=0 fault FIXED IN QEMU. No new phone boot performed.
- Next work: SWEET currently has neither AppleImage4 nor AMFI. `trust_cache_runtime_init` unconditionally requires both; unlike BCM2837, SWEET enters this startup path. Investigate an explicitly unavailable trust-cache runtime with rejected loads/no query matches while providers are absent; preserve all Apple-platform checks. Merely skipping the startup call would leave later AMFI/IMG4 dereferences unsafe. Existing load_static_trust_cache already handles trust_cache_rt == NULL.
- QEMU runner now supports `--kernel MATCHING_KERNEL --fail-on-panic`: runtime font extraction from the payload produces exact `fb.txt`, plus `result.json`; a visible panic returns 2. Baseline 0013 re-run `research/q0013-text` detects the same NULL callback panic with zero unknown glyphs. Synthetic glyph roundtrip (including literal `?`) and invalid-VA rejection passed. Screen rows remain circular, not a full log; `no_panic_observed` does not prove successful boot. README has the invocation.
- Patch 0014 committed/pushed as `be61f99`; Actions run `38748543058` is running: https://github.com/dipa-studio/ios-sweet/actions/runs/38748543058. Do not start a duplicate build.
- 2026-10-15 active: hardware photo `IMG_20261015_160033.jpg` confirms the same PC=0 instruction-fetch abort as QEMU (LR `0xfffffe0007443fe4` = read_random). Independently reproduced with kernel 0013 in `research/q0013` (8-second run); prior allocator fix works. Latest tooling commit reviewed: `8d66d2d`.
- Patch 0014 implemented: registers a built-in HMAC-SHA256 DRBG provider at EARLY_BOOT/FIRST, before commpage creation. A shared mutex serializes output/reseed across CPUs (correctness first; no per-CPU scaling yet), initgen validates CPU identities, refresh uses try-lock and the existing entropy filter/health tests. SWEET entropy accumulation and write_random now use the built-in SHA256 directly; no incomplete g_crypto_funcs table is registered. The existing bootloader-seed requirement remains enforced.
- Local validation: both PRNG translation units compile to arm64 objects in `research/prng-objs`; no SHA256 wrapper imports remain. All 14 patches apply in CI order and resulting PRNG sources match local inputs. Sandbox `research/bt17/xnu`, originals `research/pre0014`, checker `research/check-patch-order.py`. NEXT: Actions kernel build, download to `xnu/out-0014`, run the QEMU payload before requesting another phone boot. Baseline QEMU ADT/payload are `research/q0013.adt` / `research/q0013.bin`.
- 2026-10-15 08:15 QEMU EMULATION WORKS (`tools/qemu-sweet.py`, see "QEMU emulation" below). Patch 0013 VERIFIED in QEMU: boot passes kmem_crypto_init and ulock_initialize. NEXT BLOCKER (reproduced in QEMU, expected on hardware too): `pmap_create_commpages -> random() -> RandomULong -> read_random -> prng_funcs.refresh == NULL` (instruction fetch abort at pc 0, lr read_random+0x34). Nobody calls `register_and_init_prng` (closed corecrypto kext does it on Apple). Fix = patch 0014: SWEET-registered cckprng_funcs provider (osfmk/prng/prng_random.c); note random_cpu_init also panics while !prng_ready. The 07:26 hardware boot of 0013 will most likely show this same panic.
- Previous hardware boot: patch 0013 (`a3a46e2`) CI `38686345689` PASSED (3m44s), image `shim/out/shim-boot-0013.img` was temporarily booted on 2026-10-15 at 05:26 UTC: send OKAY (0.407s), boot OKAY (0.134s), total 0.546s. Superseded by the hardware/QEMU PC=0 observation and patch 0014 work above.
- Latest kernel implementation commit: `8f851d4`. Latest successful kernel CI: `38749549779` (patch 0015), https://github.com/dipa-studio/ios-sweet/actions/runs/38749549779.
- CURRENT image: `shim/out/shim-boot-0013.img`, 16,412,672 bytes, SHA256 `110d1c46aee4b60253db24b6535e9781c4cf0fd25ca2a70e9016cf81378cea25`. Kernel `xnu/out-0013/kernel.development.sweet`, SHA256 `f0b1d375995316c550ce7014ff234d8271e9b9ac2857795ceff41b76ba2cd740`, plain arm64. CI log confirms 0013 applied; checksums passed. Entry PA `0xb146b488`, kernel end PA `0xb1ff4000`.
- Current log/buildtree: `research/run17.log`, `research/bt17` (extracted, no nested .git; preferred baseline for patch 0014). Payload/ADT: `shim/out/payload-0013.bin`, `shim/out/sweet-0013.adt` (includes /defaults). Unpacked ramdisk byte-compared with payload in `shim/out/check-0013`.
- Patch-0012 original image: `shim/out/shim-boot-0012.img`, 16,412,672 bytes, SHA256 `cf4b8ec4a65a51888d71c17d09cd0831d6b761b90e2531fdb8e575c32f4fd24a`. Kernel `xnu/out-0012/kernel.development.sweet`, SHA256 `69b78f49f6459cc63a0af44e7cf73c1779ef72130ef72b12ae86c88183ffc742`; checksums passed and sweet_early_relocate symbol present. Entry PA remains `0xb146b488`, kernel end PA `0xb1ff4000`.
- Patch-0012 log/tree: `research/run16.log`, `research/bt16` (extracted, no nested .git; preferred baseline for next patch). CI log confirms 0012 applied. Payload `shim/out/payload-0012.bin`, ADT `shim/out/sweet-0012.adt`; packed ramdisk byte-compared with payload in `shim/out/check-0012`.
- Active payload command line: `debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam`.
- Latest observed photo: `/home/dipa/Downloads/IMG_20261015_071346.jpg` (local only). Stages 9 (kernel framebuffer VA), 10 (TTBR0 off/TLB flush), 11 (physmap table pointers) and 12 (calling pmap_bootstrap) all visible. Persistent diagnostics are VERIFIED ON HARDWARE.
- pmap.c:1987 is the assert after SecureDTLookupEntry(NULL, "/defaults", &entry). The pmap-max-asids property itself is optional; an empty /defaults uses XNU's MAX_ASIDS. Do not bypass the assertion or guess an ASID limit.
- Previous payload-only fix image: `shim/out/shim-boot-0012-defaults.img`, SHA256 `3ff61a268853c6ea7cbf284833f1346a1d791980ca366b6e1a215a86e040512b` (16,412,672 bytes). Same kernel/CI as 0012; ADT 2516 bytes (`shim/out/sweet-0012-defaults.adt`), root /defaults present with no ASID override. Payload `shim/out/payload-0012-defaults.bin`; unpacked ramdisk exactly matches payload (`shim/out/check-0012-defaults`). This boot reached the allocator crypto NULL-pointer fault below.
- Latest hardware photo: `/home/dipa/Downloads/IMG_20261015_071608.jpg` (local only). Normal serial framebuffer console works. VM bootstrap/zone initialization, logging, scheduler, IPC, KDP and ulock_initialize print before the kmem_crypto_init data abort. gPanicBase warnings are expected because PLATFORM_PANIC_LOG_DISABLED is configured, and are not the faulting call.
- Faulting instruction: `ldr x0, [x8, #0x150]` in crypto_random_kmem_ctx_size; x8 = NULL g_crypto_funcs. Source: libkern/crypto/corecrypto_rand.c and osfmk/vm/vm_kern.c:2448. rand.h explicitly permits NON-cryptographic kmem RNGs, but requires no locking in generation and context size <=256. Do not substitute read_random here: the global PRNG provider is also unregistered and kmem generation must remain lock-free. early_random may seed contexts during single-CPU bootstrap; it uses the built-in HMAC-SHA256 DRBG with the host-generated ADT seed.
- Patch 0013 keeps the replacement entirely private to vm_kern.c: SplitMix64 state is 8 bytes per CPU/sizeclass, initialized using early_random. Uniform selection uses rejection sampling to avoid modulo bias. Other platforms retain the original corecrypto calls; SWEET's generic crypto APIs/registration remain untouched. Reference: https://prng.di.unimi.it/splitmix64.c (Sebastiano Vigna, public domain).
- Validation: vm_kern.c compiles to arm64 (`research/kmem-rng-objs`, using `research/run16.log` + `research/bt16`). Undefined symbols include early_random and NO crypto_random_* references. All 13 patches apply in CI order and match the checked source. Original saved as `research/vm_kern.pre0013.c`.
- RNG host checks: extracted actual new helper in `research/kmem-rng-check.c`; 16,384 outputs match the author's downloaded reference (`research/splitmix64-reference.c`), 147,456 bounded outputs satisfy their limits (including 1 and UINT64_MAX), and a forced rejection advances correctly. Full kernel crypto services and the registered kernel PRNG provider remain future work; this patch only supplies the allocator RNG contract.
- Patch-0011 artifacts: kernel `xnu/out-0011/kernel.development.sweet`, log `research/run15.log`, extracted buildtree `research/bt15` (no nested .git). CI log confirms patch 0011 applied; artifact checksums passed. Kernel is plain arm64, entry VA `0xfffffe000746b488` -> PA `0xb146b488`, kernel end PA still `0xb1ff4000`.
- Patch-0011 image: 16,412,672 bytes, SHA256 `29ad5cbfc752473de4af2c471a24a8b34f92da4bff5afec65c614b90cd3ad53a`. Kernel SHA256 `8ac3a956894b09f7bd0d890de8c5de38c65513287feb1e1bcd12db8ca772ee5c`. Payload/ADT in `shim/out/payload-0011.bin`, `shim/out/sweet-0011.adt`. Header unpacked/checked in `shim/out/check-0011`.
- First patch-0011 attempt timed out waiting for FASTBOOT; superseded by the successful send/boot recorded above. Do not repeat the boot before the user reports the current screen.
- Patch-0011 photo: `/home/dipa/Downloads/IMG_20261015_070218.jpg` (local only). Visible stages 1-8 and old red shim text; entry PA B146B488, boot args PA B1FF8000 / top B1FFC000. No evidence yet locating execution after stage 8.
- Patch-0012 sandbox: `research/bt15/xnu` (NO nested .git). Proposed permanent debug VA: VM_MIN_KERNEL_ADDRESS + ARM_TT_L2_SIZE, in the reserved gap below gVirtBase and above low globals; map the same PA framebuffer as Normal-NC in the final TTBR1 before arm_vm_physmap_init changes phystokv(). Keep the early panic sink until normal kprintf startup.
- Patch 0012 validation: all 4 changed C translation units compiled to arm64 objects in `research/post-ttbr0-objs`; internal header export added to osfmk/arm64/Makefile and recreated locally in EXPORT_HDRS for pe_serial.c. All 12 patches apply in CI order and match all 6 edited files. Baselines are saved under `research/pre0012/`.
- Actual mapping-helper host model (`research/early-map-check.c`, barrier stubbed only for host compilation) confirms: VA `0xfffffe0002000000`, L1 index 2016, L2 index 1; descriptor `0x006000009c000605` = framebuffer PA, Normal-NC, outer-shareable, EL1 RW, PXN+UXN. All other L2 entries preserved. The gap is excluded by ARM_LARGE_MEMORY's pmap_virtual_region(0).
- Next boot command line MUST add `-enable_kprintf_spam`: DEVELOPMENT disables kprintf_output by default even when DB_KPRT is set (`pexpert/arm/pe_kprintf.c`). Planned command line: `debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam`.

### Patch 0012 screen legend (after original stages 1-8)
- 9: kernel framebuffer VA live; 10: TTBR0 off / TLB flushed; 11: physmap pointers rebound.
- 12: calling pmap_bootstrap; 13: pmap_bootstrap returned; 14: early I/O page tables ready; 15: arm_vm_init returned.
- 16: calling serial_init; 17: fbcon config read / mapping; 18: fbcon mapping returned; 19: serial_init returned; 20: kprintf startup complete.
- Normal serial initialization clears the screen, so earlier stages may disappear before 19. If serial exists, early console disables after 20; otherwise early panic output stays enabled.
- Local source sandbox: `research/bt13/xnu` (git baseline `623b060`, patches 0001-0008 already in baseline; uncommitted export edits correspond to patch 0009). Preserve those edits when producing patch 0010.
- Previous compile log/tree: `research/run13.log`, `research/bt13`; previous kernel: `xnu/out/kernel.development.sweet`. New successful log/tree: `research/run14.log`, `research/bt14`; new kernel: `xnu/out-0010/kernel.development.sweet`.
- Current device access: patch-0012 image was temporarily started from FASTBOOT; no adb connection is expected. Never flash partitions for bring-up; use `fastboot boot` with the shim image.
- Patch `0010` lives in `xnu/patches/0010-sweet-early-framebuffer-debug.patch`. Its exact compiled sources are under `research/bt13/xnu`; generated objects are in `research/early-objs`.
- Local checks passed: all 5 changed/new translation units compiled to arm64 Mach-O objects; new console only imports `_doprnt` and `iso_font` (both exist in the previous kernel). All 10 patches applied to a temporary upstream git index in CI order, and all 7 resulting changed source files matched the local compile inputs.
- Reproduce changed-file compilation: `XNU_CC_OBJDIR="$PWD/research/early-objs" python3 tools/xnu-cc.py research/run13.log research/bt13 arm_init.c arm_vm_init.c kern/debug.c arm64/start.s osfmk/arm64/sweet_early.c --per-file` (create the output directory first). The helper now accepts explicit new source paths absent from the old artifact's Makefiles and rejects empty selections.
- Device command result: `adb reboot bootloader`, then `timeout 30s fastboot boot shim/out/shim-boot-0010.img`: send OKAY (16028 KB), boot OKAY, total 0.541s. This confirms bootloader acceptance only, not XNU progress.
- The successful boot above was on 2026-10-14. On 2026-10-15, `timeout 45s fastboot boot ...` timed out waiting for a device; no image was sent. Boot image SHA256 rechecked and unchanged. No new build is needed to repeat the trial.
- Earlier retry: `timeout 90s fastboot boot shim/out/shim-boot-0010.img` timed out (exit 124). This is superseded by the successful 2026-10-15 repeat boot recorded above; do not repeat it until the user reports the screen.
- User observation confirms MMU-on C entry, platform expert, CPU topology/bootstrap, startup bootstrap and cpu_init all complete. Failure is within arm_vm_physmap_init (non-XNU_MONITOR branch), before stage 8 / TTBR0 teardown. Old red shim text remains behind the early console; this is expected because only written rows are cleared.
- New edit/compile sandbox is `research/bt14/xnu` (extracted artifact, NO nested .git; `git diff` there would act on the parent project). Use a saved baseline file / generated unified diff, or explicitly initialize a local baseline before editing. `research/bt13/xnu` does have its own baseline git repo.
- Exact user-reported panic: `arm_vm_init.c:1606 Assertion failed: (temp_ptov_table[ptov_index - 1].va + temp_ptov_table[ptov_index - 1].len) <= physmap_end`.
- Root cause: MacOSX build sets `ARM_LARGE_MEMORY=1`; `XNU_MONITOR` is off. Block alignment in arm_vm_physmap_slide can add gaps, but the non-PPL large-memory physmap_end omitted the padding already present in the small-memory branch. Patch 0011 reserves PTOV_TABLE_SIZE*ARM_TT_TWIG_SIZE (8*32MB) in both the aperture bound and its L1 allocation; monitor/PPL builds retain zero padding. The assertion is preserved.
- Validation: `arm_vm_init.c` compiled to an arm64 object using `research/run14.log` + `research/bt14` (object in `research/physmap-objs`). All 11 patches apply in CI order and produce exactly that source. Host harness `research/physmap-check.c` extracts the real XNU slide function with page-table-write stubs: all 65,536 page-aligned slides within 1GB checked against the current kernel memory layout; old bound fails 65,504 cases, padded bound fails zero; worst overrun 0x1ffc000 bytes. Baseline source saved as `research/arm_vm_init.pre0011.c`.
- CI: https://github.com/dipa-studio/ios-sweet/actions/runs/38638702083 (success; artifacts and full log downloaded).
- New image: `shim/out/shim-boot-0010.img` (16,412,672 bytes, SHA256 `ed2c270e0081bad5ff517be415f9bfe8805463aea7866732203fa97ac737e64e`). Previous image preserved as `shim/out/shim-boot-pre0010.img`.
- New kernel SHA256: `3ee05270cc291c49589a101678f904b1b69cb2f2575e05398bce8a4e052f409a`. Artifact checksums passed; arm64 (not arm64e), early-console symbols present. Payload entry VA `0xfffffe000746a488` -> PA `0xb146a488`; kernel end PA `0xb1ff4000`. Command line remains `debug=0xc serial=3 cpus=1 keepsyms=1`.

### Patch 0010 screen legend
- Bottom-left white 64x32 tile: assembly entered `start_first_cpu`, before page tables/MMU.
- Text `SWEET EARLY 1`: entered `arm_init` with MMU on; 2: platform expert done; 3: CPU topology done; 4: CPU bootstrap done; 5: startup bootstrap done; 6: CPU init done; 7: entering `arm_vm_init`; 8: leaving bootstrap TTBR0.
- `SWEET EARLY PANIC:` prints a copy of panic arguments before normal panic handling needs per-CPU/thread state.
- In patches 0010/0011 the console disabled before TTBR0 invalidation; patch 0012 replaces that with a persistent kernel VA alias and diagnostics through normal serial/kprintf startup (see its legend above).
- Screen contents require a physical observation from the user: adb cannot capture a framebuffer after Android has been replaced by a bare-metal kernel.

## QEMU emulation (2026-10-15)
- `tools/mkadt.py OUT.adt --qemu` = same ADT with QEMU virt GICD 0x08000000 / GICR 0x080a0000 / CNTFRQ 62.5 MHz.
- `tools/qemu-sweet.py PAYLOAD OUTDIR [--secs N] [--hmp "x /8gx ADDR"] [--gdb]`: host-side copy of shim/loader.c
  (segments + ADT + boot_args at their PAs, 6-instruction stub sets x0 and jumps), `-M virt,gic-version=3 -cpu cortex-a76`
  (same ID_AA64MMFR0 as Kryo 470), 4G RAM so 0x9c000000 fb and 0xb0000000 kernel are plain RAM.
  Outputs `fb.png` (the screen), `regs.txt`, `qemu.log` (exceptions with ESR/FAR/ELR). `--gdb` halts with a stub on :1234
  (system `gdb` + `set architecture aarch64`, `hbreak *VA` works with the MMU on).
- Not emulated: Qualcomm XPUs, ABL, real panel, PSCI conduit (QEMU virt uses hvc). Hardware still decides those.
- Example: `python3 tools/mkadt.py /tmp/q.adt --qemu && python3 tools/mkpayload.py xnu/out-0013/kernel.development.sweet /tmp/q.adt /tmp/pq.bin --cmdline 'debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam' && python3 tools/qemu-sweet.py /tmp/pq.bin /tmp/q`

## Layout
- `backup/`  partition dumps + full sdb..sdf LUNs, `SHA256SUMS` (all verified on-device == local)
- `dtb/`     live FDT (from OrangeFox), PixelOS boot.img dtb, dtbo, dts versions
- `shim/`    bare-metal boot shim (`./build.sh` -> `out/shim-boot.img`)
- `research/` mainline sm7150 dts, XNU pe_fiq.c etc, PocketDarwin patches
- `tools/unpack_bootv2.py`

## Device access
- No root in PixelOS 17 (user build, adb root disabled). Root = `fastboot boot recovery/recovery.img` (OrangeFox R12.0_2, temp boot, not flashed).
- `fastboot boot` works for stock boot.img, OrangeFox, and our shim.
- Shim resets itself via PSCI SYSTEM_RESET; IMEM restart_reason 0x77665500 is seen
  (bootreason=bootloader) but ABL still boots Android, so it does not stop in fastboot.

## Hardware facts (live DT + shim measurements)
- Framebuffer (ABL cont splash): 0x9c000000, 1080x2400, stride 4320, a8r8g8b8, panel is DSC *video* mode (no kick needed).
- RAM: 0x80000000+2G, 0x100000000+0xfcc00000. Live reserved-memory = PixelOS base dtb + sweet dtbo fixups.
- GICv3: GICD 0x17a00000, GICR 0x17a60000 (stride 0x20000, 8 PEs). GICD_TYPER 0x007B041D: SecurityExtn=1 (DS=0).
  ABL leaves GICD_CTLR = 0x10 (ARE_NS on, groups off). Writing 0x13 reads back 0x12: EnableGrp1A (bit1) is the one that matters.
- GIC Group 1 VERIFIED (shim 2c): vtimer INTID 19 = 30 IRQs/300ms, ptimer INTID 18 = 30, SGI 1 to self = 10/10.
  ICC_SRE=1 already, ICC_CTLR 0x8440, priority/enable writes stick. GICR_IGROUPR0 and GICD_IGROUPRn read 0 from NS
  even though Group 1 delivery works (RAZ in NS view) - do not use them as a health check.
  INTID 28 shows pending constantly (unknown PPI 12).
- Timer 19.2 MHz. Qualcomm PPI numbering: secure-phys/phys/virt/hyp = PPI 1/2/3/0 -> vtimer INTID 19, ptimer 18 (NOT 27/30).
- PSCI 1.1 via SMC, SMCCC 1.1. CPU_ON ok; SYSTEM_SUSPEND and SYSTEM_RESET2 unsupported.
- SMP: CPU_ON works for all 7 secondaries (MPIDR 0x81000n00).
- Entry EL1, MMU off, x0 = DTB (magic ok). ID_AA64MMFR0 = ...101122: 16K granule YES, 4K YES.
- No serial console: debug UART SE is in SPI mode on sweet. Screen is the only debug channel.

## XNU implications
- XNU GICv3 path (pexpert/arm/pe_fiq.c, VMAPPLE) puts the timer (PPI 27) in Group 0 = FIQ. With SecurityExtn=1,
  Group 0 is secure -> impossible from NS EL1. Need: Group 1 IRQ, ICC_IGRPEN1, INTID 19, and the timer handled in the IRQ path.
- XNU VMAPPLE PSCI uses `hvc`; sweet needs `smc`.
- PocketDarwin NonAppleGenericCpu.patch only swaps -mcpu=apple-h7 for -march=armv8-a.

## Milestone 3 (2026-10-14): 16K MMU verified
- ABL loads the Image at 0x80080000 (base 0x80000000 + 0x80000) regardless of text_offset=0.
- 16K granule, T0SZ=28 (L2 start, 32MB blocks), MAIR 0x44ff00, TCR 0x1409CB51C, PARange=2 (40-bit PA).
- Only own image pages WB (Qualcomm XPUs: never map carve-outs cacheable); fb Normal-NC; rest Device.
- AT S1E1R checks all as expected; unmapped -> translation fault L2.
- Fill fb: 110 ms (MMU off) -> 3.5 ms (NC). Memory loop 70 ms -> 0.7 ms. vtimer + SGI IRQs still fine with MMU on.

## XNU loader design notes (xnu-8796)
- Entry: `_start` -> `start_first_cpu`, x0 = boot_args (PA), MMU off, EL1. boot_args: Revision/Version 2,
  virtBase/physBase/memSize/topOfKernelData, Boot_Video (fb 0x9c000000, rowBytes 4320, 1080x2400, depth 32),
  deviceTreeP/Length (ADT), CommandLine[608], bootFlags, memSizeActual.
- start.s maps [kernel mach header .. physBase+memSize) with cacheable BLOCKS. On Qualcomm every XPU carve-out
  must therefore lie below the kernel: use physBase = 0xb0000000 (above ramdump_fb 0xaf000000+16M,
  cdsp_sec, qseecom), end before rmtfs 0xfde01000 -> memSize ~0x4d000000 for bring-up.
- Base ADT: iPhone 8 d20ap DeviceTree from iOS 16.7.16 IPSW (tools/remotezip.py + tools/adt.py; LZFSE payload).
  Template values are empty: the loader must fill chosen/*, memory-map, cpu*/reg etc. like iBoot.
- XNU build: GitHub Actions (dipa-studio/ios-sweet, xnu.yml) via blacktop/darwin-xnu-build, MACOS_VERSION=13.5.
  New-clang fixes in xnu/patches; Xcode 16 iig is incompatible (IORPC.kernelContent) -> macos-14 runner, oldest Xcode.

## Kernel architecture plan (SWEET machine config)
- First stock build (VMAPPLE) is arm64e (PAC). Kryo 470 = ARMv8.2, no PAC -> SWEET must build plain arm64
  (HAS_APPLE_PAC is only defined for __arm64e__, so arm64 drops PAC automatically).
- Template: VMAPPLE (16K pages, GICv3 code in pe_fiq.c) minus APPLEVIRTUALPLATFORM hypercalls/paravirt PAC,
  plus BCM2837 (generic ARM: no APPLE_ARM64_ARCH_FAMILY Apple sysregs). Do not link the KDK lib<cfg>.os.*.a.
- Timer: XNU expects it on FIQ (sleh_fiq -> rtclock_intr). SWEET: GIC Group 1, so sleh_irq must ack ICC_IAR1,
  route INTID 19 to the same timer path, SGIs to cpu_signal_handler, EOI via ICC_EOIR1.
- SMP/interrupts: open-source AppleARMSMP (USE_APPLEARMSMP) needs an IOInterruptController (AIC role) and an
  IOPMGR (CPU power -> PSCI CPU_ON via SMC). Plan: built-in IOKit classes (SweetGIC, SweetPMGR, platform
  expert) compiled into the kernel and matched via iokit/KernelConfigTables.cpp -> no closed kexts needed.
- Debug: xnu patch 0003 sweet-fbcon = kprintf drawn on the framebuffer (ADT node "sweet-fbcon").
- ADT /arm-io ranges must not map to parent 0 (pe_arm_get_soc_base_phys()==0 disables serial): use parent 0x1000000.

## Status 2026-10-14 late: first XNU boot attempt
- SWEET kernel builds (CI run 38635343732, patches 0001-0009). Verified offline: arm64, no PAC, no >v8.2 code,
  70300 classic relocs all in range (slide 0 = no loader work), entry = __start.
- Boot: shim loader worked (initrd 0x846F0000-0x855FE3E0, BOOTARG 0xB1FF8000, JUMP 0xB146A488) but screen stayed
  red -> xnu died/hung before serial_init (fbcon). kprintf is a no-op before STARTUP_SUB_KPRINTF.
- Historical next-step plan (patch 0010, now implemented and built; see active handoff above): early fb debug channel
  1. start.s start_first_cpu (after mov x20,x0 / mov x21,#0): PA marker band (MMU off, direct store to 0x9c000000).
  2. start.s after both create_bootstrap_mapping calls: add ONE block entry to the existing V=P L2 table
     (x25 + 2*PGBYTES; do NOT call create_bootstrap_mapping again: fb and RAM share L1 entry 0, it would replace
     the L2 table). Entry = TYPE_BLOCK|VALID|SH(OUTER)|ATTRINDX(CACHE_ATTRINDX_WRITECOMB=1)|AF|PNX|NX|0x9c000000,
     index = (va & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT.
  3. new osfmk/arm64/sweet_early.c (+files.arm64 entry): sweet_early_mark(n) colored bands at screen bottom and
     sweet_early_putc using iso_font via VA=PA while the bootstrap TTBR0 is live; disable before
     arm_vm_init's set_mmu_ttb(invalid_ttep) (arm_vm_init.c:2148).
  4. markers in arm_init after PE_init_platform, ml_parse_cpu_topology, cpu_bootstrap, kernel_startup_bootstrap,
     cpu_init, before arm_vm_init.
  5. panic hook at top of panic_trap_to_debugger: va_copy + _doprnt(fmt, &copy, sweet_early_putc, 16).
  Verify with tools/xnu-cc.py (compile+symbols) and patch-order simulation before the single CI run.

### >>> UPDATE 2026-10-19 (GPU probe boot image packaged) <<<

- Cross tools are installed and the shim has been built around the Actions kernel
  from XNU patch 0081. Payload `shim/out/payload-0081-gpucc.bin` embeds
  `sweetgpu=1`; image `shim/out/shim-boot-0081-gpucc.img` is Android boot header
  v2 and contains the captured base DTB. SHA-256:
  `2115e09372cdaeaeaec8cfd6a97b54bc15e0ae37a0f313a0217ad0724139d010`.
- User reports this image booted and touch/button input responds. This confirms
  kernel/input progress only; iOS userland and a valid HFS+ root on this phone
  have NOT been verified. The older v15 notes describe the original project's
  device and must not be treated as this user's device state. The user has not
  downloaded the iOS firmware. Only the small D22AP device tree was fetched.
- Root-image preparation: Apple iPhone X 16.7.16 (20H392) BuildManifest is in
  ignored `research/ipsw-x/BuildManifest.plist`. Required members are OS
  `087-86698-044.dmg`, SystemOS `087-86593-047.dmg`, and AppOS
  `087-86715-047.dmg` (5,830,922,492 compressed bytes in total). Initial 64 MiB
  HTTP range transfers stalled; retrying with smaller ranges. `apfs-fuse` is
  missing, and sudo cannot elevate in this sandbox (`no new privileges`), so
  mounting/build steps will need the user's host terminal. No userdata flash
  has been authorized or performed on this user's phone in this session.
- Root-image host handoff: user installed `apfs-fuse`. Ignored helper
  `research/ipsw-x/download-root-components.py` uses six parallel 8 MiB ranges,
  checks Content-Range and ZIP CRC-32, and preserves completed ranges to resume.
  AppOS is complete/CRC verified; SystemOS reached at least 448/1046 MiB
  compressed. Download process stopped for the host-terminal handoff; cached
  ranges remain in `research/ipsw-x/firmware/.chunks/`.
- Run `bash research/ipsw-x/build-root.sh` in the host terminal. This resumes
  downloads, builds a 16 GiB HFS+ root using `tools/mkiosroot.sh`, applies
  `tools/sweet-root-tweaks.py`, checks expected launchd/dyld/cache/build,
  runs fsck, and creates `.simg` plus SHA-256 checksums under
  `research/ipsw-x/`. Script passed `bash -n`; build has NOT run yet.
  Log: `research/ipsw-x/build-root.log`. It performs no phone writes.
- User ran the host script: all three DMGs completed and passed ZIP CRC-32.
  The space guard stopped the build before image creation (23.7 GiB reported).
  Later inspection shows 27.7 GiB free, consistent with Btrfs reclaiming the
  deleted download chunks after the check. Added `sync -f "$work"` immediately
  after downloads and before measuring space. Current space passes the 26 GiB
  guard; rerun the same build command. No root image has been created yet.
- Next host attempt failed during the OS rsync with thousands of source EIOs;
  user interrupted it. Installed Fedora reader is
  `apfs-fuse-0-30.20200928gitee71aa5.fc43`, predating upstream LZBITMAP support.
  The incomplete `research/ipsw-x/ios16-root-20H392.img` is 16 GiB logical /
  about 2.7 GiB allocated. Old APFS mounts remain at
  `/tmp/tmp.4YfH05ONll/{os,sysos,appos}`; HFS output is no longer attached.
- Fetched upstream APFS source with submodule into ignored
  `research/apfs-fuse-current`, pinned at
  `66b86bd525e8cb90f9012543be89b1f092b75cf3` (includes LZBITMAP decoder).
  New `tools/build-apfs-reader.sh` builds it locally with current CMake/GCC
  compatibility flags. User was asked to install cmake, gcc-c++, fuse3-devel,
  zlib-ng-compat-devel, bzip2-devel; compilation is pending those dependencies.
- `tools/mkiosroot.sh` now accepts APFS_FUSE and verifies a known compressed
  plist before output creation. Local host wrapper selects the new binary;
  `bash research/ipsw-x/build-root.sh --restart-partial` unmounts the old
  session mounts and removes only the incomplete HFS+ image (after checking it
  is detached and no completed outputs exist), then rebuilds. Run
  `bash tools/build-apfs-reader.sh` first. Shell syntax checks pass; actual
  decompression and full rebuild remain unverified. No phone writes occurred.
- Current reader built successfully; its compressed-file check passed. The
  13:29 host build copied OS, SystemOS, and AppOS and created the cryptex links.
  Root now uses about 12 GiB within a 16 GiB image. It stopped at the final
  read-only fsck, before running `sweet-root-tweaks.py` or generating `.simg`.
- Investigated fsck locally on the detached image: `fsck.hfsplus -f -y` and
  `-d -f -y` both finish with exit 0, volume OK. `-d -f -n` reports only
  primary-header result 35 (Linux EDEADLK), VIStat 0x8000; catalog/extents/
  bitmap statuses are zero. The upstream checker cache returns EDEADLK for
  conflicting active buffers. Logs: `research/ipsw-x/fsck-*.log`.
- Removed the failing `-n` pass from `mkiosroot.sh` and stopped swallowing
  the repair pass's exit status. Host wrapper now supports `--resume-root`:
  unmount leftover `/tmp/tmp.osCkEDttNv/{appos,sysos,os}`, confirm the image is
  detached, run full write-enabled fsck, apply tweaks, verify contents, unmount,
  check again, generate sparse image and checksums. Next host command:
  `bash research/ipsw-x/build-root.sh --resume-root`. Keep the completed copy;
  do not restart from scratch. Both modified scripts pass `bash -n`.
- Host resume applied all root tweaks, then stopped at my incorrect assertion
  that some individual dyld cache file must exceed 1 GiB. No copy was missing.
  Read the SystemOS DMG directory with the current APFS library: 44 components,
  3,330,359,296 bytes total, largest file 592,101,376 bytes; main cache is only
  393,216 bytes. Expected layout is in `research/ipsw-x/dyld-source-layout.txt`.
  The host wrapper now checks every component size against that layout and
  checks the main cache's arm64 magic instead of using the size threshold.
- Verified the unmounted HFS+ image directly with ignored diagnostic
  `research/ipsw-x/hfs-read.py` (including hardlink resolution): all 44 cache
  sizes match, main magic is `dyld_v1   arm64`, launchd/dyld are executable
  Mach-O, system build is 20H392, Setup/Watchdog/Migration preferences are
  applied, CommCenter jobs removed, keybag present, MediaToolbox recorder
  flags off. `fsck.hfsplus -f -y` then passed. No host rerun needed.
- Packaging completed locally: raw 17,179,869,184 bytes, SHA-256
  `a94f5295fd88d0fa337fb25d05fab2ffd8b5edb1f10366630aa1186d6f4b5fb5`;
  sparse 12,145,090,872 bytes, SHA-256
  `42f6ac7c362651144bf138f3175604c2074aa76f633fe9a45dbecb377649e4f0`.
  Checksums: `research/ipsw-x/root-image.sha256`. Sparse structure checked:
  27,812 valid chunks, expands to the raw image's exact length. Existing
  shim boot image SHA-256 remains `2115e09372cdaeaeaec8cfd6a97b54bc15e0ae37a0f313a0217ad0724139d010`.
  `fastboot devices` currently returned no devices. Next action needs the
  user's destructive-write confirmation and phone in bootloader fastboot.
- New milestone: user reports iOS boots, with severe lag. Existing image uses
  six A55 CPUs and software rendering; GPU patch 0081 is only a read-only
  GPUCC probe. Prepared eight-core comparison image (17,149,952 bytes),
  SHA-256 `cee57cd5b80fb77741b2d7571f13d5b1f4857a76de4d2d355e92da8db5970732`.
  Verified eight CPU nodes in the original embedded ADT, identical shim code,
  Android boot header v2, and exactly one changed byte in the embedded payload
  (`cpus=6` -> `cpus=8`). `sweetfb_hz=30` and all other args are unchanged.
  User was asked whether lag is choppy animation, delayed input, or settles
  after a few minutes; response pending. Actual CPU count and crash loops
  still need a device log. Eight-core image is packaged but NOT boot-tested.

### GPU audit implementation (2026-10-19)

- Reviewed live FDT and pinned Qualcomm/Lineage SM6150 sources. Exact CX/GX
  GDSC and reset locations are recorded in docs/GPU_BRINGUP.md. Firmware
  selection is a630_sqe.fw / a618_gmu.bin / a615_zap; no firmware copied yet.
- Reproduced 0081's patch rejection against the relevant XNU 8796.141.3 files;
  corrected 0081 and applied 0082 successfully, resulting source matches the
  intended GPUCC audit. Collected primary sources/logs stay in research/gpu.
  logs/ is now ignored to keep device-specific logs out of git.

- ADB became accessible during this turn (OrangeFox). Mounted the existing
  vendor logical block device dm-4 as EROFS **read-only**, copied matching
  a618_gmu.bin (32,768 bytes), a630_sqe.fw (32,136 bytes), all a615_zap
  components and vendor build.prop into ignored research/gpu/device-firmware,
  then unmounted and removed the RAM-backed mountpoint. Manifest has hashes.
  Vendor reports V14.0.8.0.TKFMIXM / RKQ1.210614.002, sweet / M2101K6G;
  recovery product is sweetin. No userdata or boot partition writes occurred.
  Firmware authenticity/redistribution permission not established; keep local.

- OrangeFox KGSL reports `Adreno618v1`, recovery kernel
  `4.14.348-openela-SML/b0507b61`, available GPU frequencies
  800/650/565/430/355/267/180 MHz. This describes recovery's Linux driver,
  not the state inherited by XNU after the next ABL boot.
- CI 39228692751 completed successfully on source d37d1ca. Both the full
  kernel compile and the required GPU marker check passed. Downloading the
  xnu-kernel artifact to xnu/out-0082-gpu-audit; no new boot yet.

- GPU audit kernel downloaded with ZIP ranges (the full artifact download was
  slow). Local path xnu/out-0082-range/kernel.development.sweet, SHA-256
  3eaf43cb6839bcf16cf3bf629e3e998c22f54c38b7e0f32a036b3096821c7e8a.
  ZIP CRC and artifact SHA256SUMS match; GPUCC audit v2 marker is present.
- Packaged shim/out/shim-boot-0082-gpu-audit.img (17,149,952 bytes), SHA-256
  545174a46ed0d83716691cd2978d91ad2eac62cdd3b9a454de66065513c33205.
  Embedded ramdisk matches intended payload; boot v2 header verified. ADT
  and boot args match tested eight-core image, including sweetgpu=1 and
  sweetfb_hz=30; no sweetusb. Original known-good images preserved.
- Rebooted OrangeFox to fastboot. Sandboxed fastboot timed out because it
  could not access the device; host escalation succeeded: Sending boot.img
  OKAY (0.522 s), Booting OKAY (0.130 s). Temporary RAM boot only, no flash.
  User said "it flashed"; clarified temporary boot and asked for home screen
  result, then Power+VolDown back to FASTBOOT after 30 seconds. Next: host
  fastboot boot recovery/recovery.img, then read persistent log into
  logs/gpu-audit-v2.txt. Need actual SWEET GPU lines before any power writes.

- User says the audit image is still booting and requested at least five
  minutes before judging. Waiting for the result. No local recovery/recovery.img
  exists on this host (historical notes refer to another machine). If needed,
  user can enter installed OrangeFox with the hardware recovery key sequence;
  once ADB returns, first pull the current recovery partition read-only for
  future temporary boots. Do not substitute stock boot.img or flash recovery.

- User confirmed audit image boots to iOS; missing acceleration is expected
  because the image only reads GPUCC. Returned to OrangeFox, recovered full
  persistent log into logs/gpu-audit-v2.txt (flush 12, 215,412 console bytes).
  GPUCC audit completes, all GPU clocks off and CX/GX collapsed. See current
  handoff and docs/GPU_BRINGUP.md for exact values and ARC implementation gap.
- Read-only recovery partition backup now exists at recovery/recovery.img
  (128 MiB), SHA-256
  4efe904506a021f2853d756d19b97f113f991b64f5531bd4228b3293131f7e54.
  Source was /dev/block/bootdevice/by-name/recovery -> /dev/block/sda30.
  It is an Android boot image, not a partition write. Earlier "missing"
  recovery-image note is superseded.
- Also prepared an audit-disabled comparison image:
  shim/out/shim-boot-0082-gpu-disabled.img, SHA-256
  81bd8e1670f3d49c5b157aa6c65a632307baa00f5e9863404aaaf6ef9060483c.
  Same kernel/ADT and boot args except sweetgpu=0. Payload differs in exactly
  one byte; boot image also differs in its recomputed header SHA-1. Not booted.

- End of audit cycle: phone remains in OrangeFox with ADB accessible; no
  further reboot/flash performed after log/recovery backup. Changes pushed
  to myfork/codex/adreno-power-audit (source d37d1ca, hardware report 64439b0).
  Next code requires shared ARC level-vote handling before GPUCC power writes;
  acceleration remains unimplemented. Full kernel artifact is also saved as
  research/gpu/xnu-kernel-0082.zip (60,010,491 bytes); the independent ZIP-range
  kernel copy was used for the successful boot. Original gh download session
  39357 may still be fetching the same artifact into its separate ignored
  output directory; do not confuse it with the verified xnu/out-0082-range.

- **2026-10-19 ARC milestone build:** commit 9b446e6 (kernel/patches/docs)
  pushed to myfork/codex/adreno-power-audit. GitHub Actions `xnu` run
  **39239651278** succeeded, including both marker checks (`GPUCC audit v2`
  and `ARC tables read-only complete`). Docs follow-up f189a5d adds the
  guest IOGPU/AGXMetalA11 symbol evidence; it does not change the kernel.
  The kernel artifact is downloading to `xnu/out-0083` (full artifact was
  slow on 2026-10-19; use ZIP-range retrieval if needed). No device boot yet.
  Next: package `sweetgpu=1` stage-2 image (read-only ARC tables) from this
  kernel, ask the user for approval, boot it temporarily, and recover the
  log before any `sweetgpu=2` vote. Exact gate criteria and expected markers
  are in `docs/GPU_PROGRESS.md`. **User approved the stage-2-only temporary
  boot on 2026-10-19; stage 3 (ARC votes) is not approved yet.**
- Stage-2 image is built and verified: kernel
  `xnu/out-0083/kernel.development.sweet` (17,222,944 bytes, SHA-256
  `ed03acc1ca6d9388969cf42b15e152b8d26eb633b1405bfc2225dcb525f3c3b6`,
  matches the CI artifact SHA256SUMS and the ZIP-range copy), ADT
  `shim/out/sweet.adt` (unchanged eight-core ADT), cmdline identical to the
  tested 0082 image except it keeps `sweetgpu=1`; payload
  `shim/out/payload-0083-arc-probe.bin`; boot image
  `shim/out/shim-boot-0083-arc-probe.img` (17,149,952 bytes, SHA-256
  `1e549e65b4aa0e2d4113825b05bd8b496f8c1631edc38fff937e78c19107f9ef`).
  Boot v2 unpack, embedded payload match and ARC marker all verified. Phone
  is in fastboot; sandbox fastboot has no USB access, so the user must run
  the temporary boot from the host.

### 2026-10-19 continuation after external-agent build

- Read-only pull from OrangeFox saved `logs/gpu-opencode-0083.txt`. The
  source 9b446e6 image ran stage 1, printed all ARC tables, and kept touch and
  iOS working. No ARC vote, GPU power-on or GPU rendering occurred.
- Committed/pushed 0084 correction as `352601e`; CI `39244904492` is running
  on that exact source. Only 0084 `sweetgpu=2` is suitable for the next
  readback experiment; do not use 0083 mode 2, which writes fixed votes.
- The prepared image still uses `sweetfb_hz=30`. The framebuffer timer also
  rearms after presentation work, so actual presentation can be slower than
  that cap. No current frame-time benchmark establishes the cost breakdown.
  Do not promise iPhone 8 performance from GPU power or a framebuffer copy.
- Qualcomm's 2025-11-21 rpmh_read discussion says resource reads return the
  current setting, without separate active/sleep/wake values. This does not
  yet establish this board's ARC response encoding/local vote semantics.
  Reference: https://lists.openwall.net/linux-kernel/2025/11/21/534
  Hardware raw response is the next prerequisite; readback was not claimed
  to be an inherited vote in the new code.

- 120 Hz scheduler change committed/pushed as `c056996`; combined CI run
  `39245587501` builds patches through 0085 and requires both ARC-readback
  and scheduler-v2 markers. It supersedes the readback-only run 39244904492.
  No phone reboot or partition write performed in this continuation yet.

- Live-FDT panel capability checked read-only: Xiaomi K6 38/0c/0a node has
  supported-dfps-list [120, 60], timing@0 1080x2400 at 120. This does not
  prove the active ABL scanout rate. Recovery DRM modes file was empty.
- Image packaging will reuse the exact shim kernel/DTB unpacked from the
  successful 0083 boot image, replacing only the XNU payload. Prepared local
  research/gpu/package-0085.py for 120 Hz and 30 Hz comparison images.

- **0085 artifact pulled and both images built (2026-10-19 ~18:15 local).**
  `gh run download 39245587501 -R DC28088/ios-sweet -n xnu-kernel` into
  `xnu/out-0085`; source `c056996`. Kernel `kernel.development.sweet`
  (17,223,168 bytes, SHA-256
  `c5111aaefb5d37c1c783f4c5f4d5e7913587db73be4aed77e562af216f26406f`)
  matches the artifact SHA256SUMS and contains both required markers
  (`SWEET GPU: ARC readback v1 starting`, `SWEET FB: scheduler v2
  requested=`). `research/gpu/package-0085.py` then built the comparison
  pair from the 0083 boot image's unpacked shim kernel/DTB and
  `shim/out/sweet.adt`:
  - `shim/out/shim-boot-0085-arc-readback-120hz.img` (17,149,952 bytes,
    SHA-256 `65ce7fcfba318657577b0aaaac7579b445117c935d7c43cf6dcecade95366dda`),
    cmdline keeps everything from 0083 plus `sweetfb_hz=120 sweetgpu=2`.
  - `shim/out/shim-boot-0085-arc-readback-30hz.img` (17,149,952 bytes,
    SHA-256 `e111d6469bf195912f5fa0fc464ca482ced37c1d88f30fbd1952d37f4b3e53ae`),
    same cmdline with `sweetfb_hz=30`.
  Both v2 unpacks verified: shim kernel and DTB byte-identical to
  `shim/out/check-0083-readback-base`, embedded payload equal to a fresh
  deterministic `mkpayload.py` run. Manifest with full cmdlines:
  `research/gpu/0085-boot-manifest.json`. **The 120 Hz image was
  temporarily booted (2026-10-19 20:12) and the persistent log was pulled
  read-only to `logs/gpu-arc-probe-agent.txt`.** It passed the 60 s ARC
  readback, reached SpringBoard, and kept touch/display working. The 30 Hz
  comparison image was not booted. The raw replies and 120 Hz timing
  limits are recorded at the top of this file and in
  `docs/GPU_PROGRESS.md`. The raise-only `gfx.lvl` vote (0086) was then
  approved and passed on 2026-10-19 (see the top section). Patch 0087 is
  built (CI `39289968547` green) and both images are packaged (CX power =
  `sweetgpu=4`, CX+GX power = `sweetgpu=5`) but not booted; both boots need
  explicit approval, 4 before 5. No GPUCC/core/GMU write has been performed
  on hardware yet.
