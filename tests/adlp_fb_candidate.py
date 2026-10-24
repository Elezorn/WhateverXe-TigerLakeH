#!/usr/bin/env python3
"""Replay production board selection and WEX/provider binding for the first TC1 candidate."""
from pathlib import Path
import re,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
f='\n'.join(re.search(re.escape(sig)+r'.*?\n\}',s,re.S).group() for sig in ['Display13OutputWiring XE::display13BoardWiring(', 'bool XE::xelpBindProbe('])
code=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "kern_display13_firmware.hpp"
#include "kern_display13_fb_topology.hpp"
#include "kern_alderlake_s.hpp"
struct OSData {void *getBytesNoCopy(){return nullptr;}unsigned getLength(){return 0;}};
#define OSDynamicCast(T,p) static_cast<T *>(p)
struct IOService {OSData *property=nullptr;OSData *getProperty(const char *){return property;}};
struct IORegistryEntry {static IORegistryEntry *getRegistryRoot(){static IORegistryEntry r;return &r;}void setProperty(const char*,bool){}};
XELP::ProbeConfig seen;unsigned calls=0;
bool configure(void *c,const XELP::ProbeConfig *p){assert(c==(void*)1);seen=*p;++calls;return p->valid();}
struct XE {
 XELP::Profile xeProfile=XELP::identify(0x46A68086,12,0x9A498086);
 XELP::VBT::Ports firmwarePorts;
 bool adlpFramebufferOnly=true,adlsDisplaySelected=false,tglHDMIWiringApplied=false,tglTCWiringLoaded=false;
 bool display13FBTopologyHooksReady=true,display13TCProbeReady=true,display13FBTCHooksReady=true;
 bool display13DPProbeReady=false,display13FBSSTHooksReady=true,display13FBAUXHooksReady=true;
 uintptr_t xelpProviderProbe=1,xelpProviderConfigureProbe=reinterpret_cast<uintptr_t>(configure);
 void *adlnTopologyController=(void*)1;Display13OutputWiring adlnOutputWiring,tglHDMIWiring;
 struct {bool valid=false;unsigned excluded=0;unsigned selected(){return 0;}}adlsWiring;
 struct {bool valid=false;unsigned typeC=0,excluded=0;}tglTCWiring;
 unsigned tglAUXParentMask=0;unsigned tglTCProbeMask(){return 0;}
 Display13OutputWiring display13BoardWiring(IOService *);
 bool xelpBindProbe(void *);
};
FUNCTIONS
int main(){
 XE x;IOService pci;x.firmwarePorts.status=XELP::VBT::Status::Valid;x.firmwarePorts.present=0x38;
 for(unsigned d=3;d<=5;++d){auto &p=x.firmwarePorts.port[d];p.type=d==5?4:0x12;p.ddc=d;p.capabilities=3;p.hdmiPreset=7;}
 auto w=x.display13BoardWiring(&pci);assert(w.described() && w.hdmi==8 && w.ddc[3]==9 && !w.dp);
 assert(!Display13FBTopology::supportedWiring(w) && Display13FBTopology::supportedWiring(w,true));
 x.adlnOutputWiring=w;assert(x.xelpBindProbe((void*)1));
 assert(seen.valid() && !seen.combo && seen.comboBackend==3 && seen.legacy==8 && seen.legacyBackend==0x78);
 assert(seen.profile.family==XELP::Family::AlderLakeP && !seen.profile.admitted());
 for(unsigned f=0;f<5;++f){XE bad=x;
  if(f==0)bad.display13TCProbeReady=false;if(f==1)bad.display13FBTCHooksReady=false;
  if(f==2)bad.display13FBTopologyHooksReady=false;if(f==3)bad.xelpProviderConfigureProbe=0;
  if(f==4)bad.adlnTopologyController=(void*)2;
  const auto before=calls;assert(!bad.xelpBindProbe((void*)1) && calls==before);
 }
 OSData property;pci.property=&property;assert(!x.display13BoardWiring(&pci).described());pci.property=nullptr;
 x.firmwarePorts.port[3].type=4;assert(!x.display13BoardWiring(&pci).described());
 puts("PASS production ADL-P candidate: VBT TC1/pin9 -> shared board policy -> capability/hook-gated provider binding; GT closed");
}
'''.replace('FUNCTIONS',f)
with tempfile.TemporaryDirectory(prefix='adlp-fb-candidate-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
