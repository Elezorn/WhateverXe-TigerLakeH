#!/usr/bin/env python3
"""Actual ADL-S wrappers: parent-before-IO and IO-before-release, no ref edits."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
a=s.index('uint8_t *XE::adlsPowerTarget');b=s.index('// Software-only transmitter boundary.',a)
code=r'''
#include "kern_alderlake_s.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#define FunctionCast(fn,p) reinterpret_cast<decltype(&fn)>(p)
struct XE {
 bool adlsDetected=true,adlsPowerReady=true;void *tglObservationController;
 AlderLakeS::HDMI adlsHDMIWiring;uintptr_t orgADLSPowerPG,orgTGLPowerDDIState;
 static uint8_t *adlsPowerTarget(void*);
 static void wrapADLSPowerPG(void*,bool,uint32_t);
 static void wrapTGLPowerDDIState(void*,bool,uint32_t);
};
XE state;XE *callbackXE=&state;
struct Call{unsigned kind,target;bool enable;};std::vector<Call> calls;
void pg(void*,bool e,uint32_t t){calls.push_back({0,t,e});}
void ddi(void*,bool e,uint32_t t){calls.push_back({1,t,e});}
template<class...T>void tglTransitionRecord(T...){}
template<class...T>void tglObserveActivation(T...){}
template<class T>T &at(void*p,unsigned o){return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+o);}
FUNCTIONS
int main(){
 alignas(8) uint8_t ctl[0xF00]{},power[0x80]{},access[0x70]{};
 state.tglObservationController=ctl;state.adlsHDMIWiring.valid=true;state.adlsHDMIWiring.selected=8;
 state.orgADLSPowerPG=reinterpret_cast<uintptr_t>(pg);state.orgTGLPowerDDIState=reinterpret_cast<uintptr_t>(ddi);
 at<void*>(power,0x10)=ctl;at<void*>(ctl,0xC8)=power;at<void*>(power,0x78)=access;at<void*>(ctl,0xC40)=access;
 assert(XE::adlsPowerTarget(power)==power);assert(!XE::adlsPowerTarget(nullptr));unsigned count=0;
 for(unsigned active:{0U,1U,7U})for(unsigned mask=0;mask<256;++mask)for(bool on:{false,true}){
  at<uint32_t>(power,0x3C)=active;calls.clear();XE::wrapADLSPowerPG(power,on,mask);
  assert(calls.size()==1&&calls[0].target==(mask|(active?0x2A:0))&&calls[0].enable==on);
  assert(at<uint32_t>(power,0x3C)==active);++count;
 }
 for(unsigned bits=0;bits<16;++bits)for(unsigned tc:{0U,0x80U}){
  unsigned expected=0;for(unsigned i=0;i<4;++i){at<uint32_t>(power,0x20+4*i)=(bits>>i)&1;if((bits>>i)&1)expected|=2U<<(2*i);}
  uint8_t before[sizeof(power)];memcpy(before,power,sizeof(power));calls.clear();XE::wrapTGLPowerDDIState(power,false,tc);
  assert(calls.size()==2);assert(calls[tc?0:1].kind==0&&calls[tc?0:1].target==(expected|(tc?0x2A:0)));
  assert(calls[tc?0:1].enable==bool(tc));assert(calls[tc?1:0].kind==1&&calls[tc?1:0].target==tc);
  assert(!memcmp(before,power,sizeof(power)));++count;
 }
 for(unsigned bad=0;bad<6;++bad){
  state.adlsDetected=bad!=0;state.adlsPowerReady=bad!=1;state.adlsHDMIWiring.valid=bad!=2;
  state.adlsHDMIWiring.selected=bad==3?9:8;at<void*>(ctl,0xC8)=bad==4?nullptr:power;at<void*>(power,0x78)=bad==5?nullptr:access;
  assert(!XE::adlsPowerTarget(power));calls.clear();XE::wrapADLSPowerPG(power,false,2);assert(calls.size()==1&&calls[0].target==2);
  calls.clear();XE::wrapTGLPowerDDIState(power,false,0x80);assert(calls.size()==1&&calls[0].kind==1);
 }
 printf("PASS %u lane/pipe conditions; acquire/release order; unchanged refs; 6 owner/profile rejections\n",count);
}
'''.replace('FUNCTIONS',s[a:b])
with tempfile.TemporaryDirectory(prefix='adls-lane-power-') as d:
 d=Path(d);(d/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
