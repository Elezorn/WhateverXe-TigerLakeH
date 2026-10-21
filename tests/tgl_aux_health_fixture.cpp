#include "kern_tgl_port_map.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#define FunctionCast(name,address) reinterpret_cast<decltype(&name)>(address)
constexpr uint32_t kIOReturnNotReady=0xe00002d8;
struct {volatile uint8_t *base;} tglTCBootstrap;
static uint32_t tglDPAUXHealth[9][6]{};
struct XE {
 bool tglDPAUXHealthReady=true;void *tglDekelLock=(void*)1;
 uint32_t tglDPAltCandidates=0x78;uintptr_t orgTGLRunAUX;
 static bool tglDPAUXReady(void*);
 static bool tglDPAltFia(void*,TGLFiaPort&);
 static uint32_t wrapTGLPortReadReg(void*,uint32_t);
 static uint32_t wrapTGLRunAUX(void*,uint32_t,uint32_t*,uint32_t,bool);
} instance,*callbackXE=&instance;
std::map<uint32_t,uint32_t> regs;TGLFiaPort selected;TGLTypeCPort phy;
bool fiaReady=true,locked=false;unsigned delays=0,readyAfter=0,locks=0,nativeCalls=0;
uint32_t *health=nullptr,*hip=nullptr,savedHIP=0xa5a5a5a5;
unsigned IOSimpleLockLockDisableInterrupt(void*){assert(!locked);locked=true;++locks;return 7;}
void IOSimpleLockUnlockEnableInterrupt(void*,unsigned i){assert(i==7&&locked);assert(*hip==savedHIP);locked=false;}
void IODelay(unsigned n){assert(!locked&&n==10);++delays;if(readyAfter&&delays==readyAfter)*health=0x18028;}
bool XE::tglDPAltFia(void*,TGLFiaPort& f){f=selected;return fiaReady;}
uint32_t XE::wrapTGLPortReadReg(void*,uint32_t r){assert(regs.count(r));return regs.at(r);}
void tglAUXRecord(void*,unsigned,bool,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t){}
uint32_t native(void*,uint32_t cmd,uint32_t *packet,uint32_t len,bool flag){++nativeCalls;assert(cmd==2&&len==4&&flag&&packet[0]==42);packet[0]=43;return 0x1234;}
// PRODUCTION
int main(){
 alignas(8) uint8_t port[0x600]{},access[0x80]{};uint32_t config[8]{};
 std::vector<uint32_t> memory(0x180000/4);
 tglTCBootstrap.base=reinterpret_cast<uint8_t*>(memory.data());
 *reinterpret_cast<uint32_t**>(port+0x548)=config;
 *reinterpret_cast<uint8_t**>(port+0x578)=access;
 *reinterpret_cast<uint32_t*>(access+0x60)=0x180000;
 auto &mode=*reinterpret_cast<uint32_t*>(port+0x550);mode=3;config[3]=1;
 instance.orgTGLRunAUX=reinterpret_cast<uintptr_t>(native);
 for(unsigned ddi=3;ddi<=6;++ddi)for(bool modular:{false,true}) {
  config[0]=ddi;assert(TGLTypeCPort::resolve(ddi,phy)&&phy.resolveFia(modular?16:0,selected));
  health=memory.data()+phy.phyRegister(0x236c)/4;hip=memory.data()+phy.hipRegister/4;
  regs={{selected.owned,selected.readyMask},{selected.ready,selected.readyMask},{0x45444,3u<<(2*ddi)}};
  *hip=savedHIP;*health=0x18028;delays=0;assert(XE::tglDPAUXReady(port)&&delays==0);
  *health=0;readyAfter=3;assert(XE::tglDPAUXReady(port)&&delays==3);readyAfter=0;
  *health=0;delays=0;assert(!XE::tglDPAUXReady(port)&&delays==100);
  assert(tglDPAUXHealth[ddi][5]==1000);
  *health=~0u;delays=0;assert(!XE::tglDPAUXReady(port)&&delays==0);
  *health=0x18028;
  for(auto reg:{selected.owned,selected.ready,0x45444u})for(auto bad:{0u,~0u}) {
   auto v=regs[reg];regs[reg]=bad;auto before=locks;assert(!XE::tglDPAUXReady(port)&&locks==before);regs[reg]=v;
  }
  fiaReady=false;auto before=locks;assert(!XE::tglDPAUXReady(port)&&locks==before);fiaReady=true;
  *hip=savedHIP=~0u;assert(!XE::tglDPAUXReady(port));*hip=savedHIP=0xa5a5a5a5;
  uint32_t packet=42;assert(XE::wrapTGLRunAUX(port,2,&packet,4,true)==0x1234&&packet==43);
  *health=0;packet=42;before=nativeCalls;
  assert(XE::wrapTGLRunAUX(port,2,&packet,4,true)==kIOReturnNotReady&&packet==42&&nativeCalls==before);
 }
 for(unsigned kind=0;kind<5;++kind){
  instance.tglDPAUXHealthReady=true;instance.tglDPAltCandidates=0x78;config[0]=5;config[3]=1;mode=3;
  if(kind==0)instance.tglDPAUXHealthReady=false;
  if(kind==1)instance.tglDPAltCandidates=0;
  if(kind==2)config[0]=0;
  if(kind==3)config[3]=0;
  if(kind==4)mode=4;
  auto before=locks;uint32_t packet=42;assert(XE::wrapTGLRunAUX(port,2,&packet,4,true)==0x1234&&locks==before);
 }
 mode=3;config[0]=5;config[3]=1;instance.tglDekelLock=nullptr;assert(!XE::tglDPAUXReady(port));
 instance.tglDekelLock=(void*)1;*reinterpret_cast<uint32_t*>(access+0x60)=0;assert(!XE::tglDPAUXReady(port));
 puts("PASS: production TC1..4 AUX health, ACK/ownership guards, bounded waits, bank restoration, error propagation, unchanged non-DP-alt forwarding");
}
