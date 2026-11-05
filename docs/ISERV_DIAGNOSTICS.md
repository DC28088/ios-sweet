# Native account and Store diagnostics

Patch 0209 is diagnostic only. It changes no authentication, signing,
keychain, activation, cryptographic validation, or Store result.

With `sweetservtrace=1`, it records BSD syscall number and errno for `adid`
and `fairplayd.H2`. It omits transient EINTR/EAGAIN and internal restart codes.
Each daemon has a boot-wide cap of 256 errors and 32 exit-status records,
including respawns. No file paths, buffers, account data, return pointers,
device identifiers or cryptographic data are printed. Default is disabled.
The exit event is logged before dispatch because exit does not return.
Nonlocal syscall return paths and Mach traps are outside this first probe.

The native reproduction reached HTTP 200 at both anisette provisioning
endpoints, ADIProvisioningStart succeeded, and ADIProvisioningEnd returned
-42410 (AKAnisetteError -8004 / AKUnderlyingADIErrors -42410). The meaning of
-42410 has not been established. In the same test, appstored's bag did not
finish loading (ASDErrorDomain 515) and fairplayd.H2 repeatedly started.
These observations justify checking local OS failures; they do not establish
an OS failure rather than cryptographic or activation requirements.

Next test: use the same 0207 hardware configuration, add sweetservtrace=1,
retain selective daemon logs, correct wall clock and connect Wi-Fi before
one account reproduction. Keep raw captures local and publish aggregates only.
Do not infer that a lack of syscall failures proves Apple's credentials or
attestation requirements are satisfied.

Local validation: patch apply check against the prepared 0207 source passed;
whitespace check passed. A full kernel compile and hardware test remain pending.

## Observed ADI storage repair

On the native 0209 reproduction, the server's exact text/code was captured:
`MID is invalid`, `ec=-80009`. ADI finalization separately returned -42410.

Read-only inspection found the `com.apple.adid` system container's Documents,
Library, Library/Caches, Library/Preferences and tmp directories had mode 000
and UID/GID 4294967294. A temporary entitlement-free native check, running
as mobile UID/GID 501, returned EACCES for write/search access to every one.

Saved their original metadata privately and changed only these five existing
standard directories to UID/GID 501, mode 0700. The mobile access check then
passed for all five. Container root and metadata file, account contents,
audio code/processes and boot state were not changed. No service success is
claimed until native login is retested.

`tools/sweet-adid-storage.py` can inspect a separately mounted root and repair
this exact invalid-template signature. Default is inspect-only. Applying
requires a new private backup outside the mounted root; unexpected ownership
or modes and symlinks are refused. It does not enumerate account files.

Validation: Python compilation and CLI help passed. A disposable root fixture
confirmed inspection, refusal of unexpected metadata without mutation, and
refusal of symlink candidates. The actual phone repair was performed separately
with scoped SSH chmod/chown and verified by a native mobile-UID access check;
the mounted-root Python tool has not been applied to a mounted phone image.
