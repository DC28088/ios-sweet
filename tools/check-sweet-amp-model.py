#!/usr/bin/env python3
"""Keep the kernel's tested amplifier model identical to the portable model."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
patch=(root/'xnu/patches/0214-sweet-speaker-amp-control.patch').read_text()
for name in ['sweet_aw2032.c','sweet_aw2032.h','sweet_amp_ctl.h']:
 part=patch.split('diff --git a/bsd/dev/'+name+' b/bsd/dev/'+name+'\n',1)[1].split('\ndiff --git',1)[0]
 actual='\n'.join(line[1:] for line in part.splitlines() if line.startswith('+') and not line.startswith('+++'))+'\n'
 expected=(root/'userspace/adsp'/name).read_text()
 if name.endswith('.c'):expected=expected.replace('#include <string.h>','#ifdef KERNEL\n#include <sys/systm.h>\n#else\n#include <string.h>\n#endif')
 elif name=='sweet_aw2032.h':expected=expected.replace('#include <stddef.h>\n#include <stdint.h>','#ifdef KERNEL\n#include <sys/types.h>\n#else\n#include <stddef.h>\n#include <stdint.h>\n#endif')
 assert actual==expected,name+' kernel/portable source drift'
print('PASS: kernel amplifier model and ABI match the portable tested sources')
