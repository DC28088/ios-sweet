#!/usr/bin/env python3
"""Summarise a freedreno u_trace 'print' file (MESA_GPU_TRACES=print).

Groups render passes by size/mode/attachments and splits GPU time into
prologue, binning, tile loads, draws and tile stores (all GPU timestamps)."""
import re,sys,collections
ts_re=re.compile(r'^(\d{16})\s+\+\d+: (\w+)(?::\s*(.*))?$')
passes=[];cur=None;mode=None;last=None
for line in open(sys.argv[1],errors='replace'):
    m=ts_re.match(line)
    if not m:continue
    t=int(m.group(1));ev=m.group(2);args=m.group(3) or ''
    if ev in('render_gmem','render_sysmem'):mode=ev[7:]
    if ev=='start_render_pass':
        a=dict(re.findall(r'(\w+)=([^,]*)',args))
        cur={'key':(a.get('width'),a.get('height'),a.get('mrts'),a.get('cbuf0_format'),a.get('zs_format'),mode,int(a.get('nbins',0))),'start':t,
             'phase':collections.Counter(),'loads':collections.Counter(),'stores':collections.Counter(),'open':{}}
        continue
    if cur is None:continue
    if ev.startswith('start_'):
        cur['open'][ev[6:]]=(t,args)
    elif ev.startswith('end_'):
        name=ev[4:]
        if name=='render_pass':
            cur['total']=t-cur['start'];passes.append(cur);cur=None;mode=None;continue
        if name in cur['open']:
            t0,a=cur['open'].pop(name);cur['phase'][name]+=t-t0
            if name=='tile_loads':cur['loads'][a]+=t-t0
            if name=='tile_stores':cur['stores'][a]+=t-t0
if not passes:sys.exit('no passes')
span=passes[-1]['start']+passes[-1]['total']-passes[0]['start']
total=sum(p['total'] for p in passes)
print(f'passes {len(passes)}  span {span/1e9:.1f} s  pass GPU time {total/1e9:.2f} s ({100*total/span:.0f}% of span)')
g=collections.defaultdict(lambda:{'n':0,'t':0,'phase':collections.Counter(),'loads':collections.Counter(),'stores':collections.Counter()})
for p in passes:
    e=g[p['key']];e['n']+=1;e['t']+=p['total'];e['phase'].update(p['phase']);e['loads'].update(p['loads']);e['stores'].update(p['stores'])
ph_all=collections.Counter()
for p in passes:ph_all.update(p['phase'])
print('phase totals (s):',{k:round(v/1e9,2) for k,v in ph_all.most_common()})
print(f"{'w':>5}x{'h':<5} mrt {'fmt':<16} {'zs':<10} {'mode':<6} bins  {'n':>5} {'GPU s':>6} {'%':>4} {'ms/pass':>7}  phases(ms/pass)")
for k,e in sorted(g.items(),key=lambda kv:-kv[1]['t'])[:int(sys.argv[2]) if len(sys.argv)>2 else 25]:
    w,h,mrts,fmt,zs,mode,nb=k
    ph=' '.join(f"{n}={v/e['n']/1e6:.2f}" for n,v in e['phase'].most_common(5))
    print(f"{w:>5}x{h:<5} {mrts:>3} {fmt:<16} {zs:<10} {str(mode):<6} {nb:>4} {e['n']:>6} {e['t']/1e9:6.2f} {100*e['t']/total:4.0f} {e['t']/e['n']/1e6:7.2f}  {ph}")
    if e['stores']:print('        stores:',{a:round(v/e['n']/1e6,2) for a,v in e['stores'].most_common(3)},' loads:',{a:round(v/e['n']/1e6,2) for a,v in e['loads'].most_common(3)})
