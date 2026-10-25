#!/usr/bin/env python3
"""Exercise production clock transaction ownership and balanced native PW1 refs."""
from pathlib import Path
import re,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
state=re.search(r'struct ADLSClockTransaction \{.*?\n\}',s,re.S).group()
state+=' adlsClockTransaction;\n'+re.search(r'static bool adlsClockAccess\(.*?\n\}',s,re.S).group()
functions='\n'.join(re.search(r'(?:bool|void) XE::'+n+r'\(.*?\n\}',s,re.S).group() for n in ['adlsBeginClock','adlsEndClock'])
code=r'''
#include "kern_adls_clock.hpp"
#include <cassert>
#include <cstdio>
#include <map>
struct IORecursiveLock {unsigned depth=0;};
void IORecursiveLockLock(IORecursiveLock *l){++l->depth;}
void IORecursiveLockUnlock(IORecursiveLock *l){assert(l->depth);--l->depth;}
uintptr_t thread=1;
void *current_thread(){return reinterpret_cast<void *>(thread);}
unsigned refs=0,getCount=0,putCount=0,reads=0;
std::map<unsigned long,uint32_t> mmio;
alignas(8) uint8_t ctl[0xf00]{},port[0x700]{},access[0x80]{},power[0x100]{};
uint32_t config[6]{};
template<class T>T &at(void *p,unsigned off){return *reinterpret_cast<T *>(static_cast<uint8_t *>(p)+off);}
struct IORegistryEntry {
 static IORegistryEntry *getRegistryRoot(){static IORegistryEntry o;return &o;}
 void setProperty(const char *,void *,unsigned){}
};
uint32_t readNative(void *a,unsigned long reg){assert(a==access && refs && mmio.count(reg));++reads;return mmio.at(reg);}
void getPG(void *p,uint32_t pg){assert(p==power && pg==1);++refs;++getCount;}
void putPG(void *p,uint32_t pg){assert(p==power && pg==1 && refs);--refs;++putCount;}
struct XE {
 struct Native {
  bool ready=true;bool complete(){return ready;}
  void (*getPG)(void*,uint32_t)=::getPG,(*putPG)(void*,uint32_t)=::putPG;
 } tglTCNativeFunctions;
 uintptr_t orgTGLRegisterRead=reinterpret_cast<uintptr_t>(readNative);
 static uint32_t wrapTGLRegisterRead(void*,unsigned long);
 static uint8_t *adlsPowerTarget(void *p){return p==power?power:nullptr;}
 static bool adlsBeginClock(void*,void*,bool,bool&);
 static void adlsEndClock();
} xe,*callbackXE=&xe;
#define FunctionCast(f,p) reinterpret_cast<decltype(&f)>(p)
STATE
FUNCTIONS
int main(){
 IORecursiveLock lock;adlsClockTransaction.lock=&lock;
 at<void*>(ctl,0xc40)=access;at<void*>(ctl,0xc8)=power;at<void*>(port,0x548)=config;
 at<void*>(access,0x50)=ctl;
 mmio={{0x45404,3},{0x164280,0x01e07420},{0x1642bc,0x30},
       {0x46010,0xcc000000},{0x46014,0},{0x46018,0},{0x46030,0}};
 bool keep=false;
 assert(XE::adlsBeginClock(ctl,port,true,keep));
 assert(refs==1 && lock.depth==1 && adlsClockAccess(access));
 assert(!adlsClockAccess(port) && adlsClockTransaction.translation.pll==1);
 assert(adlsClockTransaction.allocation.binding[0]==2);
 assert(!XE::adlsBeginClock(ctl,port,true,keep));assert(refs==1 && lock.depth==1);
 thread=2;assert(!adlsClockAccess(access));thread=1;
 // Partial enable: retain the powered PLL's reservation until native teardown.
 mmio[0x46014]=0x0c000000;XE::adlsEndClock();
 assert(!refs && !lock.depth && !adlsClockAccess(access));
 assert(XE::adlsBeginClock(ctl,port,false,keep) && !keep);
 assert(adlsClockTransaction.translation.pll==1);
 mmio[0x46014]=0;XE::adlsEndClock();assert(!adlsClockTransaction.allocation.binding[0]);
 // A gated port without an owned attempt must not stop its stale selector0.
 assert(XE::adlsBeginClock(ctl,port,false,keep) && keep);XE::adlsEndClock();
 assert(mmio[0x46010]==0xcc000000);
 // PW1 not acknowledged: no clock-bank reads or unbalanced refs.
 mmio[0x45404]=0;auto n=reads;
 assert(!XE::adlsBeginClock(ctl,port,true,keep));assert(reads==n+1);
 mmio[0x45404]=3;mmio[0x164280]=UINT32_MAX;
 assert(!XE::adlsBeginClock(ctl,port,true,keep));
 assert(!refs && !lock.depth && getCount==putCount && !adlsClockTransaction.thread);
 puts("PASS production ADL-S clock scope: core power, nested rejection, thread/access isolation, failed-enable reservation and balanced teardown");
}
'''.replace('STATE',state).replace('FUNCTIONS',functions)
with tempfile.TemporaryDirectory(prefix='adls-clock-runtime-') as d:
 d=Path(d);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
