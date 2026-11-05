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
