#!/usr/bin/env python3
"""Exercise the runtime clock admission boundary against impostor objects."""
from pathlib import Path
import re,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
function=re.search(r'bool XE::adlsClockTarget\(.*?\n\}',s,re.S).group()
code='''#include "kern_alderlake_s.hpp"
#include <cassert>
#include <cstdio>
struct XE {
 bool adlsClockReady=true;void *tglObservationController;uint8_t tglPlatformCopy[0x1c0] {};
 AlderLakeS::HDMI adlsHDMIWiring;static bool adlsClockTarget(void*,void*,void*);
};
XE state;XE *callbackXE=&state;
FUNCTION
template<class T>T &at(void *p,unsigned off){return *reinterpret_cast<T *>(static_cast<uint8_t *>(p)+off);}
int main(){
 alignas(8) uint8_t ctl[0xF00] {},port[0x700] {},access[0x70] {};
 state.tglObservationController=ctl;state.adlsHDMIWiring.valid=true;state.adlsHDMIWiring.selected=8;state.adlsHDMIWiring.pin[3]=9;
 uint32_t config[]={3,9,2,0,0x800,0x202};auto c=reinterpret_cast<uint32_t *>(state.tglPlatformCopy+0x68);
 memcpy(c,config,sizeof(config));at<void *>(port,0x548)=c;at<void *>(port,0x578)=access;
 at<void *>(ctl,0xC40)=access;at<uint32_t>(port,0x5B8)=0x6C10C;at<uint32_t>(access,0x60)=0x1000000;
 assert(XE::adlsClockTarget(ctl,port,nullptr));assert(XE::adlsClockTarget(ctl,port,port));
 assert(!XE::adlsClockTarget(nullptr,port,nullptr));assert(!XE::adlsClockTarget(ctl,nullptr,nullptr));
 assert(!XE::adlsClockTarget(ctl,port,access));assert(!XE::adlsClockTarget(access,port,nullptr));
 at<void *>(port,0x548)=config;assert(!XE::adlsClockTarget(ctl,port,nullptr));at<void *>(port,0x548)=c;
 at<void *>(port,0x578)=nullptr;assert(!XE::adlsClockTarget(ctl,port,nullptr));at<void *>(port,0x578)=access;
 at<uint32_t>(port,0x5B8)=0;assert(!XE::adlsClockTarget(ctl,port,nullptr));at<uint32_t>(port,0x5B8)=0x6C10C;
 at<uint32_t>(access,0x60)=0x100000;assert(!XE::adlsClockTarget(ctl,port,nullptr));at<uint32_t>(access,0x60)=0x1000000;
 for(unsigned i=0;i<6;++i){c[i]^=1;assert(!XE::adlsClockTarget(ctl,port,nullptr));c[i]^=1;}
 state.adlsHDMIWiring.selected|=1;assert(!XE::adlsClockTarget(ctl,port,nullptr));state.adlsHDMIWiring.selected=8;
 state.adlsClockReady=false;assert(!XE::adlsClockTarget(ctl,port,nullptr));
 puts("PASS ADL-S runtime clock guard: exact owner, immutable TC1 configuration, mapped Combo B and no shared port");
}'''.replace('FUNCTION',function)
with tempfile.TemporaryDirectory(prefix='adls-clock-guard-') as d:
 d=Path(d);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
