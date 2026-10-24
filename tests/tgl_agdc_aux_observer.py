#!/usr/bin/env python3
"""The production AUX observer must never inject a port-specific transaction."""
from pathlib import Path
import re, subprocess, tempfile, sys
r=Path(__file__).resolve().parents[1]
s=(Path(sys.argv[1]) if len(sys.argv)>1 else r/'WhateverXe/kern_xe.cpp').read_text()
body=re.search(r'uint32_t XE::wrapTGLAGDCAUX\(.*?\n\}',s,re.S).group()
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
unsigned scopes=0,pending=0;
struct TGLDisplayTraceScope {TGLDisplayTraceScope(){++scopes;}~TGLDisplayTraceScope(){--scopes;}};
struct TGLDisplayActivity {
 bool open=true;
 template<class... A>TGLDisplayActivity(A...){assert(scopes);++pending;}
 void finish(uint32_t){assert(open);open=false;--pending;}
 ~TGLDisplayActivity(){assert(!open);}
};
struct Boundary {unsigned stage;bool done;void *object;uint64_t a,b,result;};
std::vector<Boundary> boundaries;
void tglTransitionRecord(unsigned stage,bool done,void *object,uint64_t a,uint64_t b,uint64_t result){
 assert(scopes && pending);boundaries.push_back({stage,done,object,a,b,result});
}
struct XE {uintptr_t orgTGLAGDCAUX;static uint32_t wrapTGLAGDCAUX(void*,void*,void*);};
XE state,*callbackXE=&state;
#define FunctionCast(f,p) reinterpret_cast<decltype(&f)>(p)
struct IORegistryEntry {
 static IORegistryEntry *getRegistryRoot(){static IORegistryEntry e;return &e;}
 template<class... A>void setProperty(A...){assert(scopes && pending);}
};
template<class... A>void tglTrace(A...){}
void clock_get_uptime(uint64_t *v){*v=60000000000ULL;}
void absolutetime_to_nanoseconds(uint64_t v,uint64_t *n){*n=v;}
unsigned calls,result;void *expectedRequest,*expectedOutput;
uint32_t native(void *c,void *request,void *output){
 assert(c==&state && request==expectedRequest && output==expectedOutput);
 assert(scopes && pending && boundaries.size()==1 && !boundaries[0].done);
 assert(++calls==1);return result;
}
BODY
int main(){
 state.orgTGLAGDCAUX=reinterpret_cast<uintptr_t>(native);
 for(unsigned ddi=0;ddi<10;++ddi)for(unsigned write=0;write<2;++write)
 for(unsigned error=0;error<2;++error)for(unsigned length=0;length<32;++length){
  alignas(8) uint8_t request[64]{},output[64]{},before[64]{};
  request[0]=ddi;request[9]=2;request[12]=length;request[13]=write;
  memcpy(before,request,sizeof(request));expectedRequest=request;expectedOutput=output;
  calls=0;boundaries.clear();result=error?0xe00002d6:0;
  assert(XE::wrapTGLAGDCAUX(&state,request,output)==result);
  assert(calls==1&&!memcmp(before,request,sizeof(request)));
  assert(!scopes && !pending && boundaries.size()==2);
  for(unsigned i=0;i<2;++i){const auto &e=boundaries[i];
   assert(e.stage==99 && e.done==bool(i) && e.object==&state && e.a==ddi);
   assert(e.b==((uint64_t(0x200)<<32)|length|(write<<8)) && e.result==(i?result:0));
  }
 }
 calls=0;boundaries.clear();expectedRequest=expectedOutput=nullptr;
 assert(XE::wrapTGLAGDCAUX(&state,nullptr,nullptr)==result&&calls==1);
 puts("PASS 1280 AUX observer cases: one native call, original arguments/result, no port-specific probe");
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='tgl-aux-observer-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
