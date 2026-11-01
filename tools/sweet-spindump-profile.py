#!/usr/bin/env python3
"""Self/inclusive profile of one thread in a spindump report, with the
native Metal bundle symbolized from a CI artifact's native-metal-symbols.
usage: sweet-spindump-profile.py <artifact dir> <spindump.txt> <thread name substring> [top]"""
import re,sys,bisect,collections
from pathlib import Path
art=Path(sys.argv[1]);spin=Path(sys.argv[2]);want=sys.argv[3];top=int(sys.argv[4]) if len(sys.argv)>4 else 40
tables={}
for img,kind in (('libSweetNativeBackend.dylib','backend'),('SweetMetal2','legacy')):
    rows=[]
    for line in (art/'native-metal-symbols'/(kind+'.symbols.txt')).read_text().splitlines():
        m=re.match(r'^([0-9a-fA-F]+) [tT] (.+)$',line.strip())
        if m:rows.append((int(m[1],16),m[2]))
    rows.sort();tables[img]=(rows,[r[0] for r in rows])
def sym(frame):
    m=re.search(r'\((.+?) \+ (\d+)\)',frame)
    if not m:return frame.strip()
    img,off=m[1],int(m[2])
    if img in tables:
        rows,keys=tables[img];i=bisect.bisect_right(keys,off)-1
        if i>=0:return f'{rows[i][1]} [{img.split(".")[0]}]'
    return f'{img}+{off}'
lines=spin.read_text(errors='replace').splitlines()
start=None
for i,l in enumerate(lines):
    if l.startswith('  Thread 0x') and want in l:start=i;break
if start is None:sys.exit('thread not found')
print(lines[start].strip())
node_re=re.compile(r'^( +)\*?(\d+)\s+(.*)$')
stack=[];self_c=collections.Counter();incl=collections.Counter();total=0
items=[]
for l in lines[start+1:]:
    if not l.strip() or l.startswith('  Thread 0x'):break
    m=node_re.match(l)
    if not m:continue
    items.append((len(m[1]),int(m[2]),sym(m[3])))
for idx,(ind,cnt,name) in enumerate(items):
    while stack and stack[-1][0]>=ind:stack.pop()
    child=sum(c for (i2,c,_) in items[idx+1:idx+1+0])  # placeholder
    # children: following items with greater indent until indent<=ind
    ch=0;j=idx+1;cind=None
    while j<len(items) and items[j][0]>ind:
        if cind is None:cind=items[j][0]
        if items[j][0]==cind:ch+=items[j][1]
        j+=1
    self_c[name]+=cnt-ch
    if not any(n==name for _,n in stack):incl[name]+=cnt
    if not stack:total+=cnt
    stack.append((ind,name))
print('samples',total)
print('--- self (leaf) ---')
for n,c in self_c.most_common(top):print(f'{c:6d} {100*c/total:5.1f}%  {n}')
print('--- inclusive ---')
for n,c in incl.most_common(top):print(f'{c:6d} {100*c/total:5.1f}%  {n}')
