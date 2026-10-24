#!/usr/bin/env python3
"""Replay actual AUX/dispatch wrappers, including denial and opaque buffers."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'WhateverXe/kern_xe.cpp').read_text()
names = ['ReadAUX', 'WriteAUX', 'RunAUX', 'ClientAttribute']
bodies = '\n'.join(re.search(r'uint32_t XE::wrapTGL' + name + r'\(.*?\n\}',
                            source, re.S).group() for name in names)
fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <sys/mman.h>
unsigned scopes,pending,calls;
bool deny;
uint32_t result,address,length;
void *opaque;
std::vector<unsigned> events;
struct TGLDisplayTraceScope {TGLDisplayTraceScope(){++scopes;}~TGLDisplayTraceScope(){--scopes;}};
struct TGLDisplayActivity {
 bool open=true;
 template<class... A>TGLDisplayActivity(A...){assert(scopes);++pending;}
 void finish(uint32_t r){assert(open);open=false;--pending;assert(r==(deny?0xe00002d8:result));}
 ~TGLDisplayActivity(){assert(!open);}
};
void tglTransitionRecord(unsigned stage,bool done,void *object,uint64_t a,uint64_t b,uint64_t r){
 assert(object==opaque && scopes && pending);
 assert(r==(done?(deny?0xe00002d8:result):0));
 if(stage==100 || stage==103){assert(a==address && uint32_t(b)==length && (b>>32)<=1);}
 events.push_back(stage*2+done);
}
template<class... A>void tglTrace(A...){assert(scopes && pending);}
template<class... A>void tglDPCDRecord(A...){assert(scopes && pending);}
template<class... A>void tglAUXRecord(A...){assert(scopes && pending);}
template<class F>uint32_t tglInvokeBootstrapAUX(bool ready,unsigned kind,void *port,F f){
 assert(ready && kind<2 && port==opaque && scopes && pending);
 return deny?0xe00002d8:f();
}
struct XE {
 bool tglBootstrapAUXReady=true;
 uintptr_t orgTGLReadAUX,orgTGLWriteAUX,orgTGLRunAUX,orgTGLClientAttribute;
 static uint32_t wrapTGLReadAUX(void*,uint32_t,void*,uint32_t);
 static uint32_t wrapTGLWriteAUX(void*,uint32_t,void*,uint32_t);
 static uint32_t wrapTGLRunAUX(void*,uint32_t,uint32_t*,uint32_t,bool);
 static uint32_t wrapTGLClientAttribute(void*,uint32_t,unsigned long*,unsigned long,unsigned long*,unsigned long*,void*);
};
XE instance,*callbackXE=&instance;
#define FunctionCast(f,p) reinterpret_cast<decltype(&f)>(p)
uint32_t nativeAUX(void *port,uint32_t a,void *data,uint32_t n){
 assert(port==opaque && data==opaque && a==address && n==length && scopes && pending);
 assert(events.back()==206);++calls;return result;
}
uint32_t nativeRun(void *port,uint32_t command,uint32_t *packet,uint32_t n,bool flag){
 assert(port==opaque && packet==opaque && command==8 && n==0 && flag && scopes && pending);
 assert(events.back()==208);++calls;return result;
}
uint32_t nativeClient(void *client,uint32_t attribute,unsigned long *input,unsigned long size,
 unsigned long *output,unsigned long *outputSize,void *arguments){
 assert(client==opaque && input==opaque && output==opaque && outputSize==opaque && arguments==opaque);
 assert(attribute==0x985 && size==0x50 && scopes && pending && events.back()==204);
 ++calls;return result;
}
// BODIES
int main(){
 opaque=mmap(nullptr,4096,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(opaque!=MAP_FAILED);
 instance.orgTGLReadAUX=(uintptr_t)nativeAUX;instance.orgTGLWriteAUX=(uintptr_t)nativeAUX;
 instance.orgTGLRunAUX=(uintptr_t)nativeRun;instance.orgTGLClientAttribute=(uintptr_t)nativeClient;
 for(auto a:{0U,0x102U,0xE000CU,0xFFFFFFFFU})for(auto n:{0U,1U,16U,255U})
 for(auto error:{0U,0xe00002d6U})for(unsigned blocked=0;blocked<2;++blocked)for(unsigned write=0;write<2;++write){
  address=a;length=n;result=error;deny=blocked;calls=0;events.clear();
  auto value=write?XE::wrapTGLWriteAUX(opaque,a,opaque,n):XE::wrapTGLReadAUX(opaque,a,opaque,n);
  assert(value==(deny?0xe00002d8:result) && calls==(deny?0U:1U) && !scopes && !pending);
  assert(events==(deny?std::vector<unsigned>{200,201}:std::vector<unsigned>{200,206,207,201}));
 }
 deny=false;
 for(auto error:{0U,0xe00002d6U}){
  result=error;calls=0;events.clear();
  assert(XE::wrapTGLRunAUX(opaque,8,(uint32_t*)opaque,0,true)==result);
  assert(calls==1 && !scopes && !pending && events==std::vector<unsigned>({202,208,209,203}));
  calls=0;events.clear();
  assert(XE::wrapTGLClientAttribute(opaque,0x985,(unsigned long*)opaque,0x50,
   (unsigned long*)opaque,(unsigned long*)opaque,opaque)==result);
  assert(calls==1 && !scopes && !pending && events==std::vector<unsigned>({204,205}));
 }
 munmap(opaque,4096);
 puts("PASS: 128 AUX paths plus transport/dispatch; native args/results, denied calls, opaque buffers and publication scopes");
}
'''.replace('// BODIES', bodies)
with tempfile.TemporaryDirectory(prefix='tgl-aux-boundaries-') as directory:
    path = Path(directory)
    (path / 'check.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++14', '-O1', '-g', '-fsanitize=address,undefined',
                    str(path / 'check.cpp'), '-o', str(path / 'check')], check=True)
    subprocess.run([str(path / 'check')], check=True)
