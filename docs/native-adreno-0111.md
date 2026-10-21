# 0111: Qualcomm SMMU fault-route startup correction

## Observed 0110 result

The paired kernel and installed Metal bundle booted iOS, with no perceived
performance improvement. Recovery console capture shows GMU/HFI success, then:

```
SWEET NATIVE VM: route preflight failed=0xa; all resources retained
SWEET ADRENO: CP SMMU errno=5 ...
SWEET ADRENO: GMU stage=9 finished errno=5; rendering unavailable
```

0xa is ArmSmmu4KBusy. The route preflight runs before table allocation,
command-processor start and IOKit provider registration. Metal cannot open
this GPU on that boot. The log shows invalid stream slots holding S2CR 0x200ff;
preflight required0x20000 after writing FAULT. Retained CBNDXff is sufficient
to trigger the observed rejection. The firmware-encoding probe in0111 will
separately establish whether ignoredFAULT writes also apply on this boot.

## Correction

Pinned Linux arm-smmu-qcom.c documents firmware ignoring FAULT writes and
rewriting BYPASS to FAULT. 0111 probes this encoding only in the leased invalid
SMR slot, confirms actual FAULT, and translates later FAULT requests through
the board adapter. No stream is published as BYPASS. No global SMMU reset or
GMU CB1 change is used. For exact sweet ID0/polling profile only, FAULT readback
ignores its irrelevant 8-bit CBNDX field. Active translated routes still require
exact matching. Quarantine uses the same encoding and readback rules.

This addresses a logged startup blocker. It does not establish successful
Metal translation or GPU-rendered frames. Keep the installed 0110 bundle:
there is no ABI change and no root reinstallation needed for this correction.
Once startup passes, check CP completion, provider publication, compositor
connection and actual rendering. The existing 60-second GPU-worker delay also
means early compositor lookups precede provider publication; confirm discovery
on late publication before making any startup-order changes.

Sources and hashes: native-adreno-0111-sources.json. Portable C compilation
and patch staging passed; see NOTES.md for final CI/packaging result.
