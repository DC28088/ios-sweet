from pathlib import Path
import subprocess
s=Path('userspace/native-metal/transport/sweet_drm.cpp').read_text()
start=s.index('    for(unsigned i=0;i<s->nr_bos && !e;++i)')
end=s.index('    // Validate the complete list',start)
block=s[start:end]
code='''#include <bitset>
#include <cstdint>
#include <cerrno>
#include <cassert>
#include <random>
#include <cstdio>
constexpr unsigned MaxBO=4096, MSM_SUBMIT_BO_READ=1, MSM_SUBMIT_BO_WRITE=2, MSM_SUBMIT_BO_FLAGS=3;
struct Buffer {uint64_t kernel=1,iova=4096; bool closing=false;};
Buffer storage[MaxBO];
Buffer *buffer(unsigned h){return h && h<=MaxBO && storage[h-1].kernel?&storage[h-1]:nullptr;}
struct BO {unsigned flags,handle;uint64_t presumed;};
struct Resource {uint64_t handle;unsigned access,reserved;};
struct Request {Resource resources[1024];};
struct Submit {unsigned nr_bos;};
int candidate(BO *bos,unsigned n){Submit sub{n};auto s=&sub;Request request{};auto req=&request;int e=0;std::bitset<MaxBO> seen_handles;
'''+block+'''
return e;}
int reference(BO *bos,unsigned n){for(unsigned i=0;i<n;++i){auto b=buffer(bos[i].handle);unsigned a=bos[i].flags&3;if(!b||b->closing||!a||(bos[i].flags&~3u))return EINVAL;for(unsigned j=0;j<i;++j)if(bos[j].handle==bos[i].handle)return EINVAL;}return 0;}
int main(){BO bos[1024]{};std::mt19937 rng(42);for(unsigned t=0;t<20000;++t){unsigned n=1+rng()%1024;for(unsigned i=0;i<n;++i)bos[i]={1u+rng()%3u,1u+i*4u,0};if(t%2)bos[rng()%n].handle=rng()%4100;if(t%3==0)bos[rng()%n].flags=rng()%8;assert(candidate(bos,n)==reference(bos,n));}bos[0]={1,0,0};assert(candidate(bos,1)==EINVAL);bos[0].handle=4097;assert(candidate(bos,1)==EINVAL);bos[0].handle=4096;assert(!candidate(bos,1));storage[4095].closing=true;assert(candidate(bos,1)==EINVAL);puts("PASS: 20000 equivalence cases plus handle boundaries and closing BO");}
'''
Path('/tmp/gpu-validation.cpp').write_text(code)
subprocess.run(['g++','-std=c++17','-O2','-Wno-narrowing','/tmp/gpu-validation.cpp','-o','/tmp/gpu-validation'],check=True)
subprocess.run(['/tmp/gpu-validation'],check=True)
