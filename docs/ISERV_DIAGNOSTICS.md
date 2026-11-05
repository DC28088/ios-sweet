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

## 0210: distinguish failed file probes from code-signing buffer sizing

A fresh reboot confirmed all five repaired ADI directories remain 501:501/0700
and pass mobile access checks. Wi-Fi/Safari and native HTTPS were then restored.
The user still reproduced MID invalid; ADIProvisioningEnd still returned -42410.
Therefore the directory repair does not resolve login.

Patch 0210 extends opt-in sweetservtrace=1 diagnostics only for adid. It records
selected file syscall flags/protection class/result and a path category whose
components are restricted to literal standard directory names. Unknown names,
container UUIDs and per-account ADI filenames are masked. It also records csops
operation/capacity/required blob size, and MAC policy category/operation/result.
It prints no file/entitlement contents, audit tokens, target process IDs or user
addresses. New metadata is capped at 768 records per boot; existing 0209 caps
remain. It changes no syscall, authentication, activation or signing result.

The required blob size is read only for XML/DER entitlement blob sizing errors;
other csops operations do not have their output read. The optional existing
sweetiolog tracing can be enabled for adid/fairplayd.H2 in a separate diagnostic
boot; its raw output must remain private like every device console capture.

Validation: patch applies to 0209 with exact resulting-source comparison; C
syntax check uses the target's generated syscall structures and numbers with
stub kernel declarations. Actual C path-redaction tests cover masking private
components, retaining static ADI names, rejecting oversized inputs and safe
NUL-terminated truncation. Full kernel compilation/hardware validation pending.
