from pathlib import Path
import urllib.request,urllib.error,subprocess,concurrent.futures,json,hashlib,io,zipfile,socket,struct,zlib,urllib.parse
artifact=json.loads(Path('research/gpu/0185-kernel-artifact.json').read_text())
size=artifact['size_in_bytes']
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kw):return None
request=urllib.request.Request(artifact['archive_download_url'],headers={'Authorization':'Bearer '+subprocess.check_output(['gh','auth','token'],text=True).strip(),'User-Agent':'sweet-gles-artifact'})
try:urllib.request.build_opener(NoRedirect).open(request,timeout=20);raise RuntimeError('Expected redirect')
except urllib.error.HTTPError as e:
 if e.code!=302:raise RuntimeError('Artifact redirect failed') from None
 url=e.headers['Location']
cache=Path('research/gpu/kernel-0185-download-ranges')
exec(Path('research/gpu/artifact-keepalive.py').read_text())
print('Signed artifact host:',urllib.parse.urlsplit(url).netloc,flush=True)
tailstart=max(0,size-65558);tail=fetch((tailstart,size-1));eocd=tail.rfind(b'PK\x05\x06');assert eocd>=0
cdsize,cdoff=struct.unpack_from('<II',tail,eocd+12)
cd=fetch((cdoff,cdoff+cdsize-1));entries=[];at=0
while cd[at:at+4]==b'PK\x01\x02':
 method=struct.unpack_from('<H',cd,at+10)[0];crc,csize,usize=struct.unpack_from('<III',cd,at+16)
 nl,xl,cl=struct.unpack_from('<HHH',cd,at+28);off=struct.unpack_from('<I',cd,at+42)[0]
 name=cd[at+46:at+46+nl].decode();entries.append((name,method,crc,csize,usize,off));at+=46+nl+xl+cl
out=Path('xnu/out-0185');out.mkdir(exist_ok=True)
for name,method,crc,csize,usize,off in entries:
 if name not in ('kernel.development.sweet','SHA256SUMS'):continue
 hdr=fetch((off,off+29));nl,xl=struct.unpack_from('<HH',hdr,26);begin=off+30+nl+xl
 ranges=[(p,min(begin+csize,p+131072)-1) for p in range(begin,begin+csize,131072)]
 print(name,'compressed bytes',csize,'ranges',len(ranges),flush=True)
 chunks=[]
 with concurrent.futures.ThreadPoolExecutor(8) as pool:
  for chunk in pool.map(fetch,ranges):
   chunks.append(chunk)
   if len(chunks)%8==0:print('Received',len(chunks),'/',len(ranges),flush=True)
 data=b''.join(chunks);data=zlib.decompress(data,-15) if method==8 else data
 assert len(data)==usize and zlib.crc32(data)==crc
 (out/name).write_bytes(data)
 print(name,'CRC verified SHA256',hashlib.sha256(data).hexdigest(),flush=True)
