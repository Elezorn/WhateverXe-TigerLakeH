#!/usr/bin/env python3
"""Run production profile selection: ADL-P Render must preserve Display13."""
from pathlib import Path
import re, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_xe.cpp').read_text()
f=re.search(r'void XE::selectProfile\(.*?\n\}',s,re.S).group()
code=r'''
#include "kern_xelp_platform.hpp"
#include "kern_xelp_os.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
const char *version="Darwin Kernel Version 20.6.0: Thu Jul  6 22:12:47 PDT 2023; root:xnu-7195.141.49.702.12~1/RELEASE_X86_64";
bool noaccel=false;
bool checkKernelArgument(const char *s){return noaccel && !strcmp(s,"-wex-noaccel");}
int kextIntelTGL,kextIntelTGLFb;
bool adlnNativeRCSProfileRequested=false,display13TraceEnabled=false,display13Offline=false;
struct XE {
 XELP::Profile xeProfile;
 bool display12SmallGTDetected=false,adlsDisplaySelected=false;
 bool adlnRequested=false,adlnDetected=false,adlnKernelReady=false,adlnNativeRequested=false;
 bool adlpDisplaySelected=false,adlpRenderRequested=false,display13FramebufferRequested=false;
 bool tglRequested=false,tglAcceleratorRequested=false,tglInactivePorts=false,tglPG1Poll=false;
 bool tglClockOrderRequested=false,tglTrainingLeaseRequested=false,tglProgramEnabled=false;
 int *currentFramebuffer=nullptr,*currentGraphics=nullptr;
 struct {bool available=false;}modDVMTCalcFix;
 void selectProfile(const XELP::Profile &);
};
FUNCTION
int main(){
 for(unsigned which=0;which<3;++which)for(unsigned rev=0;rev<256;++rev)for(unsigned off=0;off<2;++off) {
  noaccel=off;XE x;
  const auto p=which==0?XELP::identify(0x46A68086,rev,0x9A498086):
      which==1?XELP::identify(0x46D18086,rev):XELP::identify(0x9A498086,rev);
  x.selectProfile(p);
  assert(XELP::same(x.xeProfile,p));
  assert(x.currentFramebuffer==&kextIntelTGLFb && x.currentGraphics==&kextIntelTGL);
  assert(x.display13FramebufferRequested==(which!=2));
  assert(x.adlpDisplaySelected==(which==0));
  assert(x.adlpRenderRequested==(which==0&&!off));
  assert(x.adlnDetected==(which==1 || (which==0&&!off)));
  assert(x.tglAcceleratorRequested==!off);
  assert(adlnNativeRCSProfileRequested==(which==1 || (which==0&&!off)));
 }
 noaccel=false;XE unknown;unknown.selectProfile(XELP::identify(0x46A68086,12));
 assert(!unknown.adlpRenderRequested && !unknown.adlnDetected && !unknown.tglAcceleratorRequested);
 puts("PASS 1536 production profile selections: ADL-P Render/noaccel, ADL-N and TGL; physical/display identities preserved");
}
'''.replace('FUNCTION',f)
with tempfile.TemporaryDirectory(prefix='wex-adlp-render-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
