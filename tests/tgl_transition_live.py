#!/usr/bin/env python3
"""Actual lifecycle recorder: concurrent snapshots, IRQ state and opaque objects."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'WhateverXe/kern_xe.cpp').read_text()
# Tracing rewrites PG entry bytes. Qualify the native ABI before routing those
# entries, or enabling diagnostics alone blocks framebuffer startup.
trace_install = source.index('KernelPatcher::RouteRequest traceRoutes[]')
assert source.index('!memcmp(reinterpret_cast<void *>(tglDisablePG),tcPGPutEntry') < trace_install
assert source.index('tglNativeLegacyHPDCaller=hotplug+0x185') < trace_install
assert source.rfind('if (tglDisplayTraceEnabled)', 0, trace_install) >= 0
body = source[source.index('struct TGLTransitionRecord {'):
              source.index('// Independent and nested training calls')]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <sys/mman.h>
template<class T,size_t N>constexpr size_t arrsize(const T(&)[N]){return N;}
using IOSimpleLock=std::mutex;
IOSimpleLock *IOSimpleLockAlloc(){return new IOSimpleLock;}
void IOSimpleLockFree(IOSimpleLock *p){delete p;}
thread_local bool irqEnabled=true,held=false;
unsigned IOSimpleLockLockDisableInterrupt(IOSimpleLock*l){
 bool prior=irqEnabled;irqEnabled=false;l->lock();assert(!held);held=true;return prior;
}
void IOSimpleLockUnlockEnableInterrupt(IOSimpleLock*l,unsigned prior){
 assert(held&&!irqEnabled);held=false;l->unlock();irqEnabled=prior;
}
std::atomic<uint64_t> ticks{0},threadCounter{0};
void clock_get_uptime(uint64_t*t){*t=++ticks;}
void absolutetime_to_nanoseconds(uint64_t t,uint64_t*n){*n=t;}
void *current_thread(){thread_local uintptr_t id=++threadCounter;return reinterpret_cast<void*>(id);}
bool failAlloc=false,failData=false;
void *IOMalloc(size_t n){assert(!held);return failAlloc?nullptr:malloc(n);}
void IOFree(void*p,size_t){assert(!held);free(p);}
struct OSSerialize{std::vector<uint8_t> data;};
struct OSData{
 std::vector<uint8_t> data;
 static OSData *withBytes(const void*p,size_t n){
  assert(!held);if(failData)return nullptr;auto d=new OSData;
  d->data.assign((const uint8_t*)p,(const uint8_t*)p+n);return d;
 }
 bool serialize(OSSerialize*s){assert(!held);s->data=data;return true;}
 void release(){assert(!held);delete this;}
};
// BODY
void validate(OSSerialize&s){
 assert(s.data.size()==16+1024*128);uint32_t h[4];memcpy(h,s.data.data(),16);
 assert(h[0]==1&&h[2]==1024&&h[3]==128);
 for(unsigned i=0;i<1024;++i){
  TGLTransitionRecord r;memcpy(&r,s.data.data()+16+128*i,128);
  if(!r.q[2])continue;
  assert(r.q[2]<=h[1]&&(r.q[2]-1)%1024==i);
  assert(r.q[1]&&r.q[3]==90&&r.q[4]==1);
  assert(r.q[6]==r.q[7]&&r.q[7]==r.q[8]);
  for(unsigned j=9;j<16;++j)assert(r.q[j]==r.q[6]);
 }
}
int main(){
 OSSerialize snapshot;IOSimpleLock lock;
 assert(!tglTransitionSerialize(nullptr,nullptr,&snapshot));
 tglInitTransitionTrace(false);assert(!tglTransitionLock&&!tglTransitions&&!tglTransitionFirst);
 tglTransitionRecord(1,false,nullptr,0,0,0);assert(!tglTransitionCount);
 failAlloc=true;tglInitTransitionTrace(true);assert(!tglTransitionLock&&!tglTransitions&&!tglTransitionFirst);
 failAlloc=false;tglDisplayTraceEnabled=true;tglInitTransitionTrace(true);assert(tglTransitionLock);
 auto inaccessible=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
 assert(inaccessible!=MAP_FAILED);
 tglTransitionRecord(1,false,inaccessible,2,3,4);assert(tglTransitionCount==1);
 // Invalid length/payload rejected before any copy; object is never followed.
 tglTransitionRecord(1,false,inaccessible,2,3,4,inaccessible,57);
 tglTransitionRecord(1,false,inaccessible,2,3,4,nullptr,1);
 assert(tglTransitionCount==1);
 for(bool prior:{false,true}){
  irqEnabled=prior;tglTransitionRecord(1,true,inaccessible,0,0,0);
  assert(irqEnabled==prior&&!held);
  assert(tglTransitionSerialize(nullptr,nullptr,&snapshot));assert(irqEnabled==prior);
 }
 irqEnabled=true;failAlloc=true;assert(!tglTransitionSerialize(nullptr,nullptr,&snapshot));
 failAlloc=false;failData=true;assert(!tglTransitionSerialize(nullptr,nullptr,&snapshot));failData=false;
 // Wrap all slots, then race multiple producers against memory-only snapshots.
 auto write=[&](unsigned count){for(unsigned i=0;i<count;++i){
  uint64_t payload[7];for(auto &v:payload)v=i;
  tglTransitionRecord(90,true,inaccessible,i,i,i,payload,sizeof(payload));
  assert(irqEnabled&&!held);
 }};
 write(2048);assert(tglTransitionSerialize(nullptr,nullptr,&snapshot));validate(snapshot);
 std::thread a([&]{write(4000);}),b([&]{write(4000);});
 for(unsigned i=0;i<100;++i){assert(tglTransitionSerialize(nullptr,nullptr,&snapshot));validate(snapshot);}
 a.join();b.join();assert(tglTransitionCount==10051);
 assert(tglTransitionSerialize(nullptr,nullptr,&snapshot));validate(snapshot);
 assert(tglTransitionSerialize(nullptr,reinterpret_cast<void*>(1),&snapshot));
 assert(snapshot.data.size()==16+4096*128);
 for(unsigned i=0;i<4096;++i){
  TGLTransitionRecord r;memcpy(&r,snapshot.data.data()+16+128*i,128);assert(r.q[2]==i+1);
 }
 for(auto reg:{0x45400ul,0x45404ul,0x45408ul,0x4540Cul,0x45450ul,0x45454ul})assert(tglTransitionRegister(reg));
 for(auto reg:{0x162000ul,0x162FFCuL,0x6C000ul,0x6CFFCuL,0x164284ul,0x164290ul,0xC7200ul,0xC7210ul,
               0x45440ul,0x45444ul,0x164280ul,0x46010ul,0x46014ul,
               0x64000ul,0x64100ul,0x46140ul,0x46144ul,0x60400ul,0x61400ul,0x60540ul,0x61540ul})
  assert(tglTransitionRegister(reg));
 for(auto reg:{0ul,0x45410ul,0x45451ul,0x138124ul,0x162001ul,0x163000ul,0x6D000ul,
               0x164294ul,0xC7214ul,0x64010ul,0x64900ul,0x4614Cul,0x63400ul,0x63540ul})
  assert(!tglTransitionRegister(reg));
 for(unsigned pipe=0;pipe<3;++pipe){
  assert(tglTransitionRegister(0x60400+0x1000*pipe));
  assert(tglTransitionRegister(0x60540+0x1000*pipe));
  assert(tglTransitionRegister(0x46140+4*pipe));
  assert(!tglTransitionRegister(0x60404+0x1000*pipe));
 }
 for(unsigned ddi=0;ddi<9;++ddi){
  assert(tglTransitionRegister(0x64000+0x100*ddi));
  assert(!tglTransitionRegister(0x64004+0x100*ddi));
 }
 tglDisplayTraceEnabled=false;
 for(auto reg:{0x62400ul,0x62540ul,0x64500ul,0x46148ul})assert(!tglTransitionRegister(reg));
 // Concurrent progress is independent of transition ring wrap. An inactive
 // scope costs no record and opaque objects must never be dereferenced.
 assert(!tglDisplayActivityBegin(0,inaccessible,1));
 assert(!tglDisplayActivityCount && !tglDisplayTraceScopes);
 {
  TGLDisplayTraceScope scope;assert(tglDisplayTraceScopes==1);
  {TGLDisplayTraceScope nested;assert(tglDisplayTraceScopes==2);}
  assert(tglDisplayTraceScopes==1);
  uint64_t pending[64];
  for(unsigned i=0;i<64;++i)pending[i]=tglDisplayActivityBegin(i,inaccessible,i+1);
  assert(tglDisplayActivityCount==64);
  assert(!tglDisplayActivityBegin(64,inaccessible,0));assert(tglDisplayActivityDropped==1);
  assert(tglDisplayLiveSerialize(nullptr,nullptr,&snapshot));
  uint64_t h[8];memcpy(h,snapshot.data.data(),sizeof(h));
  assert(h[0]==1 && h[2]==10051 && h[3]==256 && h[4]==64 && h[5]==64 && h[6]==1 && h[7]==1);
  assert(snapshot.data.size()==64+(256+64)*128);
  for(unsigned i=0;i<256;++i){
   TGLTransitionRecord q;memcpy(&q,snapshot.data.data()+64+i*128,128);
   assert(q.q[2]==10051-255+i);
  }
  tglDisplayActivityEnd(pending[63],7);
  auto reused=tglDisplayActivityBegin(99,inaccessible,4);assert(reused && reused!=pending[63]);
  tglDisplayActivityEnd(pending[63],123); // stale completion cannot close a reused slot
  assert(!tglDisplayActivity[63].q[8]);
  tglDisplayActivityEnd(reused,9);assert(tglDisplayActivity[63].q[7]==9);
  for(unsigned i=0;i<63;++i)assert(!tglDisplayActivity[i].q[8]);
  for(unsigned i=0;i<63;++i)tglDisplayActivityEnd(pending[i],0);
  for(bool prior:{false,true}){
   irqEnabled=prior;
   {TGLDisplayActivity call(3,inaccessible,1);}
   assert(irqEnabled==prior&&!held);
   assert(tglDisplayLiveSerialize(nullptr,nullptr,&snapshot));assert(irqEnabled==prior);
  }
  irqEnabled=true;
  auto activityWrite=[&]{for(unsigned i=0;i<2000;++i){TGLDisplayActivity call(3,inaccessible,i);call.finish(i);}};
  std::thread c(activityWrite),d(activityWrite);
  for(unsigned i=0;i<100;++i){
   assert(tglDisplayLiveSerialize(nullptr,nullptr,&snapshot));
   for(unsigned j=0;j<64;++j){
    TGLTransitionRecord q;memcpy(&q,snapshot.data.data()+64+(256+j)*128,128);
    if(q.q[8])assert(q.q[1]>=q.q[0]);else assert(!q.q[1]);
   }
  }
  c.join();d.join();assert(tglDisplayActivityDropped==1);
 }
 assert(!tglDisplayTraceScopes && !tglDisplayActivityBegin(0,inaccessible,0));
 munmap(inaccessible,4096);
 IOFree(tglTransitions,tglTransitionSlots*sizeof(TGLTransitionRecord));
 IOFree(tglTransitionFirst,tglTransitionFirstSlots*sizeof(TGLTransitionRecord));
 IOSimpleLockFree(tglTransitionLock);
 puts("PASS: 10051 events; compact chronological snapshots; concurrent/pending activity, overflow and stale tokens; IRQ restore; no object/MMIO access");
}
'''.replace('// BODY', body)
with tempfile.TemporaryDirectory(prefix='tgl-transition-') as tmp:
    tmp = Path(tmp)
    cpp, binary = tmp / 'check.cpp', tmp / 'check'
    cpp.write_text(fixture)
    subprocess.run(['clang++', '-std=c++14', '-O1', '-g', '-pthread',
                    '-fsanitize=address,undefined', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
