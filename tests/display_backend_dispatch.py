#!/usr/bin/env python3
"""Replay the production kext dispatcher with independent Render/Display inputs."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_xe.cpp').read_text()
a=s.index('\t// Display dispatch is independent')
b=s.index('\nstatic bool adlnWriterMappingBound();',a)
body=s[a:b].rstrip()[:-1]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
using mach_vm_address_t=uintptr_t;
struct KernelPatcher {};
struct Kext {size_t loadIndex;};
bool loadOnly=false;
bool checkKernelArgument(const char *arg) {assert(!strcmp(arg,"-wex-adln-load-only"));return loadOnly;}
struct XE {
 bool display13FramebufferRequested=false,adlnRequested=false,adlnDetected=false;
 bool adlnNativeRequested=false,adlnNativeReady=false,tglActive=false;
 Kext *currentFramebuffer=nullptr,*currentGraphics=nullptr;
 enum Call {None,Display13,Render,Rejected,LoadOnly,Display12};Call call=None;
 bool processDisplay13Framebuffer(KernelPatcher&,size_t,mach_vm_address_t,size_t){call=Display13;return true;}
 bool processTGLAccelerator(KernelPatcher&,size_t,mach_vm_address_t,size_t){call=Render;return true;}
 bool processAlderLakeNRejectedAccelerator(KernelPatcher&,size_t,mach_vm_address_t,size_t){call=Rejected;return true;}
 bool processAlderLakeNLoadOnly(KernelPatcher&,size_t,mach_vm_address_t,size_t){call=LoadOnly;return true;}
 bool processKextFull(KernelPatcher&,size_t,mach_vm_address_t,size_t){call=Display12;return true;}
 bool dispatch(KernelPatcher&patcher,size_t index,mach_vm_address_t address,size_t size) {BODY}
};
int main() {
 Kext fb{1},graphics{2};KernelPatcher patcher;unsigned cases=0;
 // Removing/rejecting Render must not redirect a selected Display13 kext
 // through the Display12 frame, nor make a graphics kext run Display code.
 for(unsigned render=0;render<4;++render)for(bool display:{false,true})
 for(bool framebuffer:{false,true})for(unsigned index=1;index<=3;++index) {
  XE x;x.currentFramebuffer=framebuffer?&fb:nullptr;x.currentGraphics=&graphics;
  x.display13FramebufferRequested=display;x.adlnRequested=x.adlnDetected=x.adlnNativeRequested=render!=0;
  x.adlnNativeReady=x.tglActive=render>=2;loadOnly=render==3;
  const bool result=x.dispatch(patcher,index,0x1000,0x2000);
  if(display && framebuffer && index==1)assert(result && x.call==XE::Display13);
  else if(!render)assert(result && x.call==XE::Display12);
  else if(index!=2)assert(!result && x.call==XE::None);
  else assert(result && x.call==(render==1?XE::Rejected:render==2?XE::Render:XE::LoadOnly));
  ++cases;
 }
 // Missing graphics object and missing native request never bypass rejection.
 for(bool detected:{false,true}) {
  XE x;x.adlnRequested=true;x.adlnDetected=detected;x.adlnNativeRequested=true;
  assert(!x.dispatch(patcher,2,0x1000,0x2000) && x.call==XE::None);
  x.currentGraphics=&graphics;x.adlnNativeRequested=false;
  assert(!x.dispatch(patcher,2,0x1000,0x2000) && x.call==XE::None);
 }
 printf("PASS %u independent Display/Render dispatch cases, native preflight rejection and load-only routing\n",cases);
}
'''.replace('BODY',body).replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='wex-display-dispatch-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
