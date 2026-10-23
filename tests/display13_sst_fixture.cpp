#include <map>
#include <vector>
#include <new>
#include <fstream>
#include <iterator>
#include <sys/mman.h>
#include "kern_display13_power_bridge.hpp"
#include "kern_tgl_combo_signal.hpp"
#include "kern_xelp_sst_program.hpp"
// NATIVE_OBJECTS
struct OSObject {unsigned size;const OSObject *getMetaClass() const{return this;}unsigned getClassSize() const{return size;}};
struct IOService:OSObject {};
constexpr unsigned kIOReturnNotReady=0xE00002D8;
#define FunctionCast(fn,ptr) reinterpret_cast<decltype(&fn)>(ptr)
bool locked=false,bound=true,loseAfterNative=false;unsigned calls=0,nativeResult=0;
void IOLockLock(void*){assert(!locked);locked=true;}void IOLockUnlock(void*){assert(locked);locked=false;}
struct Map {
 std::map<uint32_t,uint32_t> regs;
 std::vector<uint32_t> reads,writes;
 uint32_t base=0,drop=0;
 Map():regs{{0x45404,0},{0x45454,0},{0x45444,0},{0x42000,0x8E380000}}{}
 uint32_t read(uint32_t reg){reads.push_back(reg);return regs.at(reg);}
 void write(uint32_t reg,uint32_t v){writes.push_back(reg);if(reg==drop)return;
  if(reg==0x45404)v=(v&~0x5405U)|((v&0xA80A)>>1);
  if(reg==0x45454||reg==0x45444)v=(v&~21U)|((v&42)>>1);
  if(reg==base+0x694)for(unsigned n=0;n<4;++n)regs.at(base+0x894+0x100*n)=v;
  regs.at(reg)=v;
 }
};
struct Context {
 Display13Power::NativeBridge bridge;void *lock=reinterpret_cast<void *>(1),*controller=nullptr,*access=nullptr;
 Map *map=nullptr;bool clockFaulted=false,memoryFaulted=false,clockStopped=false;
}display13FBPower;
struct Display13FBPowerIO {
 Map *map;bool readable(uint32_t r){return map&&map->regs.count(r);}uint32_t read(uint32_t r){return map->read(r);}
 void write(uint32_t r,uint32_t v){map->write(r,v);}void delayUs(unsigned){}
};
struct Display13FBSSTIO {
 Map *map;uint32_t read(uint32_t r){assert(locked);return map->read(r);}
 void write(uint32_t r,uint32_t v){assert(locked);map->write(r,v);}
};
bool adlnWriterMappingBound(){return bound;}
uintptr_t display13FBExpectedOwner(void*){return 0;}
struct XE {
 bool adlnDetected=true,display13FBSSTHooksReady=true;XELP::Profile xeProfile;
 Display13OutputWiring adlnOutputWiring;void *adlnPlatform=nullptr,*adlnTopologyController=nullptr;
 uintptr_t orgDisplay13TrainingPattern=0,orgDisplay13SignalLevels=0;
 static uint8_t *display13SSTPort(void*);static bool display13HDMIPort(void*);
 static uint32_t wrapDisplay13SignalLevels(void*,uint32_t,uint32_t,void*);
 static uint32_t wrapDisplay13TrainingPattern(void*,uint32_t);
}state,*callbackXE=&state;
// BINDING
// FUNCTIONS
unsigned nativeSignal(void*,unsigned,unsigned,void*){assert(!locked);++calls;return nativeResult;}
uint32_t (*realPattern)(void *,uint32_t)=nullptr;
void nativeWrite(void *access,uint64_t reg,uint32_t value){
 assert(!locked&&access==display13FBPower.access);display13FBPower.map->write(static_cast<uint32_t>(reg),value);
}
unsigned nativePattern(void *path,unsigned pattern){
 assert(!locked);++calls;
 const auto pipe=Display13FBMemory::field<unsigned>(path,0x3640);
 if(!nativeResult) {
  if(realPattern)assert(realPattern(path,pattern)==0);
  else {const uint32_t patterns[]={0x200,0,0x100,0x400,0x500,0x300};
   assert(pattern<6);display13FBPower.map->regs[0x60540+0x1000*pipe]=0x80000000|patterns[pattern];}
 }
 if(loseAfterNative)display13FBPower.map->regs[0x45454]=0;
 return nativeResult;
}
void setup(Fixture &f,Map &map,unsigned ddi,unsigned pipe,unsigned lanes,unsigned rate){
 display13FBPower.~Context();new(&display13FBPower)Context;state={};calls=nativeResult=0;bound=true;loseAfterNative=false;
 auto &c=display13FBPower;c.controller=f.ctl;c.access=f.access;c.map=&map;
 f.wiring.hdmi=1U<<(1-ddi);f.wiring.dp=1U<<ddi;for(unsigned d=0;d<3;++d)f.wiring.ddc[d]=(f.wiring.hdmi&(1U<<d))?d+1:0;
 assert(Display13FBTopology::apply(f.platform,sizeof(f.platform),f.donor,f.wiring));
 state.adlnOutputWiring=f.wiring;state.adlnPlatform=f.platform;state.adlnTopologyController=f.ctl;state.xeProfile.display=XELP::Display::XeLPD;
 state.orgDisplay13TrainingPattern=reinterpret_cast<uintptr_t>(nativePattern);state.orgDisplay13SignalLevels=reinterpret_cast<uintptr_t>(nativeSignal);
 static uintptr_t vtable[64]{};vtable[0x130/8]=reinterpret_cast<uintptr_t>(nativeWrite);put(f.access,0,static_cast<void *>(vtable));
 put(f.ctl,0,unsigned(sizeof(f.ctl)));put(f.fb,0,unsigned(sizeof(f.fb)));put(f.path,0,unsigned(sizeof(f.path)));
 put(f.plane,0,unsigned(sizeof(f.plane)));put(f.port,0,unsigned(sizeof(f.port)));
 put(f.port,0x578,static_cast<void *>(f.access));put(f.port,0x550,1U);put(f.port,0x124,lanes);put(f.port,0x128,rate);f.port[0x11A]=1;
 map.base=ddi?0x6C000:0x162000;
 put(f.port,0x620,map.base+0x804);put(f.port,0x5C8,map.base+0x28);put(f.access,0x60,0x200000U);
 for(auto reg:{0x804U,0x14U,0x28U,0x894U,0x604U,0x694U})map.regs[map.base+reg]=0;
 for(unsigned n=0;n<4;++n)for(auto reg:{0x888U,0x890U,0x894U,0x89CU})map.regs[map.base+0x100*n+reg]=0;
 map.regs[0x46140+4*pipe]=0;map.regs[0x60400+0x1000*pipe]=0;map.regs[0x64000+0x100*ddi]=0;map.regs[0x60540+0x1000*pipe]=0;
 Display13FBPowerIO io{&map};using Display13Power::Status;
 assert(c.bridge.begin(io,1-ddi,3).status==Status::Complete);
 const auto pg=pipe?pipe+2:1;
 assert(c.bridge.getPG(io,pg).status==Status::Complete&&c.bridge.getDDI(io,ddi).status==Status::Complete);
 assert(c.bridge.getAUX(io,ddi).status==Status::Complete&&c.bridge.putAUX(io,ddi).status==Status::Complete);
 // Remove boot peer first: training must depend only on its actual resources.
 assert(c.bridge.adoptBootPath(1,0,1-ddi).status==Status::Complete);
 assert(c.bridge.putDDI(io,1-ddi).status==Status::Complete&&c.bridge.putPG(io,1).status==Status::Complete);
 assert(c.bridge.outputHeld(io,ddi,pipe));map.reads.clear();map.writes.clear();
}
int main(int argc,char **argv){
 uint8_t *image=nullptr;
 if(argc>1){
  std::ifstream input(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
  assert(!bytes.empty()&&bytes.size()<0x400000);
  image=static_cast<uint8_t *>(mmap(nullptr,0x400000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANON|MAP_PRIVATE,-1,0));assert(image!=MAP_FAILED);
  memcpy(image,bytes.data(),bytes.size());realPattern=reinterpret_cast<decltype(realPattern)>(image+0x955B8);
 }
 unsigned count=0,replays=0;
 for(unsigned ddi=0;ddi<2;++ddi)for(unsigned pipe=0;pipe<3;++pipe)for(unsigned lanes:{1U,2U,4U})
 for(unsigned rate:{6U,10U,20U,30U})for(unsigned swing=0;swing<4;++swing)for(unsigned emphasis=0;emphasis<4-swing;++emphasis){
  Fixture f(2-pipe,pipe,ddi);Map map;setup(f,map,ddi,pipe,lanes,rate);
  assert(!XE::wrapDisplay13SignalLevels(f.port,swing,emphasis,nullptr)&&!calls);
  XELP::ComboPolicy policy;assert(policy.select(XELP::ComboPolicy::Platform::ADLP,rate,false,false,false,swing,emphasis));
  assert(((map.regs[map.base+0x89C]>>24)&0x7F)==policy.coefficients.scalar);
  assert(!XE::wrapDisplay13TrainingPattern(f.path,1)&&calls==1);
  assert(map.regs[0x46140+4*pipe]==((ddi+1)<<28));
  assert(map.regs[0x60400+0x1000*pipe]==(((ddi+1)<<27)|0x02000000|((lanes-1)<<1)));
  assert(map.regs[0x60540+0x1000*pipe]==0x80040000);
  assert(!XE::wrapDisplay13TrainingPattern(f.path,2)&&calls==2);
  assert(!XE::wrapDisplay13TrainingPattern(f.path,0)&&calls==3);
  // Actual native instruction replay: all six pattern values lose enhanced
  // framing. The adapter restores only the negotiated bit, preserving the
  // pattern and SST mode for both enhanced-framing settings.
  if(realPattern){
   const uint32_t patterns[]={0x200,0,0x100,0x400,0x500,0x300};
   for(unsigned enhanced=0;enhanced<2;++enhanced)for(unsigned pattern=0;pattern<6;++pattern){
    f.port[0x11A]=enhanced;
    assert(realPattern(f.path,pattern)==0&&map.regs[0x60540+0x1000*pipe]==(0x80000000|patterns[pattern]));
    assert(!XE::wrapDisplay13TrainingPattern(f.path,pattern));
    assert(map.regs[0x60540+0x1000*pipe]==(0x80000000|patterns[pattern]|(enhanced<<18)));++replays;
   }
  }
  const auto completedCalls=calls;
  Display13FBPowerIO io{&map};using Display13Power::Status;
  assert(display13FBPower.bridge.putDDI(io,ddi).status==Status::Complete);
  assert(display13FBPower.bridge.putPG(io,pipe?pipe+2:1).status==Status::Complete);
  for(auto refs:display13FBPower.bridge.state.refs)assert(!refs);
  // Deliberately leave BIOS-like enabled/request bits. No native ownership.
  map.regs[0x45404]=0xFC0F;map.regs[0x45454]=15;map.writes.clear();map.reads.clear();
  assert(XE::wrapDisplay13TrainingPattern(f.path,1)==kIOReturnNotReady&&calls==completedCalls&&map.writes.empty()&&map.reads.empty());
  ++count;
 }
 for(unsigned fault=0;fault<25;++fault){
  Fixture f(2,1,1);Map map;setup(f,map,1,1,4,20);auto &c=display13FBPower;
  if(fault==0)c.bridge.state.suspended=true;if(fault==1)c.bridge.state.faulted=true;
  if(fault==2)c.bridge.ddiRefs[1]=0;if(fault==3)c.bridge.ddiLease[1].owner=nullptr;
  if(fault==4)map.regs[0x45454]&=~8U;if(fault==5)map.regs[0x45454]&=~4U;
  if(fault==6)map.regs[0x45404]&=~0x2000U;if(fault==7)map.regs[0x45404]&=~0x1000U;
  if(fault==8)map.regs[0x42000]&=~0x100000U;if(fault==9)map.regs[0x45454]=~0U;
  if(fault==10)c.bridge.pgRefs[2]=0;if(fault==11)bound=false;
  if(fault==12)c.clockStopped=true;if(fault==13)put(f.port,0x548,reinterpret_cast<void *>(1));
  if(fault==14)f.port[0x117]=1;if(fault==15)put(f.port,0x578,static_cast<void *>(f.ctl));
  if(fault==16)put(f.path,0x3640,0U);if(fault==17)put(f.plane,0x7C,0U);
  if(fault==18)state.adlnDetected=false;if(fault==19)state.display13FBSSTHooksReady=false;
  if(fault==20)map.drop=0x46144;if(fault==21)map.drop=0x61400;
  if(fault==22)map.regs[0x46144]=0x10000000;
  if(fault==23)map.regs[0x61400]=0x88000000;
  if(fault==24)c.bridge.state.refs[0]=0;
  assert(XE::wrapDisplay13TrainingPattern(f.path,1)==kIOReturnNotReady&&!calls);
  if(fault!=20&&fault!=21)assert(map.writes.empty());
 }
 for(unsigned fault=0;fault<14;++fault){
  Fixture f(2,0,0);Map map;setup(f,map,0,0,4,30);auto &c=display13FBPower;
  if(fault==0)c.bridge.ddiRefs[0]=0;if(fault==1)c.bridge.state.suspended=true;
  if(fault==2)c.bridge.state.refs[0]=0;if(fault==3)map.regs[0x45454]&=~2U;
  if(fault==4)map.regs[0x42000]&=~0x4000000U;
  if(fault==5)put(f.port,0x620,map.base+0x804+4);if(fault==6)put(f.port,0x5C8,map.base+0x28+4);
  if(fault==7)put(f.access,0x60,map.base+0xB9C);
  if(fault==8)map.regs[map.base+0xA9C]=~0U;
  if(fault==9)put(f.port,0x124,3U);if(fault==10)put(f.port,0x128,12U);
  if(fault==11)map.drop=map.base+0x694;
  const auto result=XE::wrapDisplay13SignalLevels(f.port,fault==12?4:0,0,fault==13?f.path:nullptr);
  assert(result==kIOReturnNotReady&&!calls);if(fault!=11)assert(map.writes.empty());
 }
 {Fixture f(2,0,0);Map map;setup(f,map,0,0,2,10);nativeResult=42;
  assert(XE::wrapDisplay13TrainingPattern(f.path,1)==42&&calls==1);
  map.writes.clear();loseAfterNative=true;nativeResult=0;
  assert(XE::wrapDisplay13TrainingPattern(f.path,2)==kIOReturnNotReady&&calls==2);
  if(realPattern)assert(map.writes.size()==1&&map.writes[0]==0x60540);else assert(map.writes.empty());}
 // HDMI peer in a mixed wiring must retain native behavior and result.
 {Fixture f(2,0,0);Map map;setup(f,map,0,0,2,10);put(f.port,0x548,static_cast<void *>(f.platform+0x38));nativeResult=17;
  assert(XE::wrapDisplay13SignalLevels(f.port,0,0,nullptr)==17&&calls==1);
  assert(XE::wrapDisplay13TrainingPattern(f.path,1)==17&&calls==2&&map.writes.empty());}
 printf("PASS production Display13 signal/SST: %u compositions after peer release, 39 admission/power/MMIO faults, native error/power-loss/HDMI preservation\n",count);
 if(image){munmap(image,0x400000);printf("PASS %u exact native pattern/framing calls and unmodified donor counterexamples\n",replays);}
}
