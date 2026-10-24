#!/usr/bin/env python3
"""Exercise production ADL-P load-only selection and both driver boundaries."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
a=s.index('// ADLP_LOAD_ONLY_HOOKS:');b=s.index('// ADLP_LOAD_ONLY_HOOKS_END',a);body=s[a:b]
a=s.index('    // ADLP_LOAD_ONLY_KERNEL:');b=s.index('    // ADLP_LOAD_ONLY_KERNEL_END',a);selection=s[a:b]
dispatch=s[s.index('bool XE::processKext('):s.index('    if((tglRequested',s.index('bool XE::processKext('))]
assert dispatch.index('if(adlpLoadOnly)')<dispatch.index('processADLPLoadOnly')
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "kern_xelp_platform.hpp"
#include "kern_xelp_os.hpp"
using mach_vm_address_t=uintptr_t;
#define arrsize(a) (sizeof(a)/sizeof((a)[0]))
#define SYSLOG(...) do{}while(0)
struct IOService{};using SInt32=int32_t;
struct IORegistryEntry {
 static std::map<std::string,bool> props;
 static IORegistryEntry *getRegistryRoot(){static IORegistryEntry r;return &r;}
 void setProperty(const char *k,bool v){props[k]=v;}
};std::map<std::string,bool> IORegistryEntry::props;
struct Kext {size_t loadIndex;};Kext kextIntelTGLFb{1},kextIntelTGL{2};
bool arg=true;bool checkKernelArgument(const char *s){assert(!strcmp(s,"-wex-adlp-load-only"));return arg;}
const char *version="Darwin Kernel Version 20.6.0: Thu Jul  6 22:12:47 PDT 2023; root:xnu-7195.141.49.702.12~1/RELEASE_X86_64";
struct KernelPatcher {
 struct RouteRequest {const char *symbol;uintptr_t replacement;
  template<class T>RouteRequest(const char *s,T t):symbol(s),replacement(reinterpret_cast<uintptr_t>(t)){}
 };
 bool wrongSymbol=false;unsigned routed=0;
 template<class T>T solveSymbol(size_t index,const char *s,uintptr_t base,size_t) {
  bool start=strstr(s,"5start")!=nullptr;return base+(index==1?(start?0x3f1fc:0x446bc):(start?0x23ecc:0x238e2))+(wrongSymbol?1:0);
 }
 void clearError(){}
 bool routeMultipleShort(size_t,RouteRequest *routes,size_t count,uintptr_t,size_t) {
  assert(count==2 && strstr(routes[0].symbol,"5start") && strstr(routes[1].symbol,"5probe"));
  // Calling the actual replacements must reject independently of object data.
  assert(!reinterpret_cast<bool(*)(IOService*,IOService*)>(routes[0].replacement)(nullptr,nullptr));
  assert(!reinterpret_cast<IOService*(*)(IOService*,IOService*,SInt32*)>(routes[1].replacement)(nullptr,nullptr,nullptr));
  routed+=2;return true;
 }
};
struct XE {
 bool adlpLoadOnly=false,adlpFramebufferOnly=false;XELP::Profile xeProfile;unsigned initialized=0;
 void select(){SELECTION ++initialized;}
 bool processADLPLoadOnly(KernelPatcher &,size_t,mach_vm_address_t,size_t);
 static IOService *rejectADLPFramebufferProbe(IOService *,IOService *,SInt32 *);
 static IOService *rejectADLPAcceleratorProbe(IOService *,IOService *,SInt32 *);
 static bool rejectADLPStart(IOService *,IOService *);
};
BODY
int main(){
 unsigned cases=0;
 for(unsigned physical:{0x46a6U,0x46a3U,0x9a49U,0x4680U})for(bool flag:{false,true})for(bool override:{false,true}){
  XE xe;arg=flag;xe.xeProfile=XELP::identify((physical<<16)|0x8086,12,((override?0x9a49:physical)<<16)|0x8086);xe.select();
  bool expected=(physical==0x46a6 || physical==0x46a3)&&flag&&override;
  assert(xe.adlpLoadOnly==expected && xe.initialized==!expected);++cases;
 }
 for(unsigned index:{1U,2U})for(unsigned bad=0;bad<4;++bad){
  std::vector<uint8_t> image(0x50000);auto base=reinterpret_cast<uintptr_t>(image.data());
  uint8_t fb[]={0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x83,0xec,0x10,0x48,0x89,0xf3,0x49,0x89,0xfe};
  uint8_t gt[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x28};
  unsigned p=index==1?0x446bc:0x238e2,q=index==1?0x3f1fc:0x23ecc;
  memcpy(image.data()+p,index==1?fb:gt,17);memcpy(image.data()+q,gt,4);
  XE xe;xe.adlpLoadOnly=bad!=1;KernelPatcher patcher;patcher.wrongSymbol=bad==2;
  if(bad==3)image[p+8]^=1;
  IORegistryEntry::props.clear();
  assert(xe.processADLPLoadOnly(patcher,index,base,image.size())==(bad==0));
  assert(patcher.routed==(bad==0?2U:0U));
  if(!bad){assert(IORegistryEntry::props.at(index==1?"ADLPFramebufferProbeRejected":"ADLPAcceleratorProbeRejected"));assert(IORegistryEntry::props.at("ADLPLoadOnlyUnexpectedStart"));}
  ++cases;
 }
 for(unsigned module:{1U,2U}){
  std::vector<uint8_t> image(0x50000);auto base=reinterpret_cast<uintptr_t>(image.data());
  const uint8_t gt[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x28};
  memcpy(image.data()+0x238e2,gt,17);memcpy(image.data()+0x23ecc,gt,4);
  XE xe;xe.adlpFramebufferOnly=true;KernelPatcher p;
  assert(xe.processADLPLoadOnly(p,module,base,image.size())==(module==2));assert(p.routed==(module==2?2U:0U));
  if(module==2)assert(IORegistryEntry::props.at("ADLPFBOnlyAcceleratorBlocked"));++cases;
 }
 printf("PASS %u production ADLP load-only selection/probe/start checks\n",cases);
}
'''.replace('SELECTION',selection).replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='adlp-load-only-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
