#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>
#include <sys/mman.h>
#include "kern_alderlake_s.hpp"
#include "kern_tgl_port_map.hpp"
template<class T>T &at(void *p,unsigned off){return *reinterpret_cast<T *>(static_cast<uint8_t *>(p)+off);}
void jump(uint8_t *p,uintptr_t t){p[0]=0x48;p[1]=0xb8;memcpy(p+2,&t,8);p[10]=0xff;p[11]=0xe0;}
struct Event {uint32_t kind,reg,value;};
std::vector<Event> events;std::map<unsigned long,uint32_t> mmio;
alignas(8) uint8_t ctl[0xF00] {},port[0x700] {},access[0x70] {};
uintptr_t vtable[64] {};uint32_t config[]={3,9,2,0,0x800,0x202};
uint8_t *image;unsigned fault,delays,errors;
uint32_t readReg(void *p,unsigned long reg){assert(p==access);assert(mmio.count(reg));return mmio.at(reg);}
void writeReg(void *p,unsigned long reg,uint32_t v){
 assert(p==access && mmio.count(reg));
 if(reinterpret_cast<uintptr_t>(__builtin_return_address(0))==reinterpret_cast<uintptr_t>(image+0x6d973))
  TGLComboPLLFraction::adjust(at<uint32_t>(ctl,0xEA8),reg,v);
 events.push_back({1,uint32_t(reg),v});
 if(reg==0x46014) {
  // Model hardware ACK/LOCK independently from request bits. Failure cases
  // exercise the donor's bounded waits, not successful hardware operation.
  v&=~0x44000000U;
  if((v&0x08000000U) && fault!=1)v|=0x04000000U;
  if((v&0x80000000U) && fault!=2)v|=0x40000000U;
 }
 mmio[reg]=v;
}
void mailbox(void *p,uint32_t command,uint32_t value,bool wait){assert(p==ctl && command==7 && !wait);events.push_back({2,command,value});}
uint32_t voltage(void *p){assert(p==ctl);return 1;}
void getDDI(void *p){assert(p==port && at<uint32_t *>(p,0x548)[0]==3);events.push_back({3,3,1});}
void putDDI(void *p){assert(p==port && at<uint32_t *>(p,0x548)[0]==3);events.push_back({3,3,0});}
void delay(unsigned n){assert(n==1);++delays;}
void error(const char *,...){++errors;}
void redirectCall(unsigned site,uintptr_t target,unsigned stub){
 assert(image[site]==0xe8);jump(image+stub,target);int32_t d=stub-site-5;memcpy(image+site+1,&d,4);
}
int main(int argc,char **argv){
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
 image=static_cast<uint8_t *>(mmap(nullptr,0x400000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANON,-1,0));assert(image!=MAP_FAILED);
 memcpy(image,bytes.data(),bytes.size());
 // Rebase only the four native profiling-pointer tables used on these paths.
 for(auto table: {std::pair<unsigned,unsigned>{0xcd330,3},{0xcd348,6},{0xcd378,3},{0xcd3c0,3}})
  for(unsigned i=0;i<table.second;++i){auto &v=at<uint64_t>(image,table.first+8*i);assert(v>=0xd0000 && v<0x160000);v+=reinterpret_cast<uintptr_t>(image);}
 jump(image+0x5e344,(uintptr_t)readReg);jump(image+0x4691e,(uintptr_t)mailbox);
 jump(image+0x6bf6e,(uintptr_t)voltage);jump(image+0x1fefe,(uintptr_t)getDDI);jump(image+0x1ff54,(uintptr_t)putDDI);
 for(unsigned site:{0x6d081u,0x6d9ecu,0x6dd6du,0x6de16u})redirectCall(site,(uintptr_t)delay,0x300000);
 for(unsigned site:{0x6d54cu,0x6dda6u})redirectCall(site,(uintptr_t)error,0x300020);
 vtable[0x130/8]=(uintptr_t)writeReg;at<void *>(access,0)=vtable;at<void *>(ctl,0xc40)=access;
 at<uint32_t *>(port,0x548)=config;
 auto enable=(void(*)(void*,void*,void*,uint32_t*))(image+0x6ce10);
 auto disable=(void(*)(void*,void*,void*))(image+0x6dae6);
 unsigned cases=0;
 for(unsigned ref=0;ref<=2;++ref)for(unsigned p:{2u,3u,5u,7u})for(unsigned k:{1u,2u,3u})for(unsigned q:{1u,3u}) {
  fault=0;events.clear();delays=errors=0;at<uint32_t>(ctl,0xea8)=ref;
  mmio={{0x46014,0},{0x16428c,0},{0x164290,0},{0x164280,0x01e07400}};
  uint32_t params[]={464,2049,p,q,k};AlderLakeS::HDMIClock expected;
  assert(AlderLakeS::HDMIClock::encode(params,ref,expected));
  enable(ctl,port,nullptr,params);
  assert(!errors&&!delays && mmio[0x16428c]==expected.cfg0 && mmio[0x164290]==expected.cfg1);
  assert(mmio[0x46014]==0xcc000000 && mmio[0x164280]==((0x01e07400&~0x80cU)|4));
  assert(events.front().kind==2 && events.front().value==3 && events.back().kind==3 && events.back().value==1);
  assert(config[0]==3);events.clear();disable(ctl,port,nullptr);
  assert(!errors&&!delays && mmio[0x46014]==0 && (mmio[0x164280]&0x800));
  assert(events.front().kind==3 && events.front().value==0 && events.back().kind==2 && events.back().value==1);++cases;
 }
 for(fault=1;fault<=2;++fault){
  events.clear();delays=errors=0;mmio={{0x46014,0},{0x16428c,0},{0x164290,0},{0x164280,0x01e07400}};
  uint32_t params[]={464,2048,2,3,2};enable(ctl,port,nullptr,params);
  assert(delays==2000 && errors==1 && mmio[0x164280]==0x01e07400);
 }
 munmap(image,0x400000);
 printf("PASS %u real donor Combo PLL enable/disable cases: physical TC1 preserved, PHY B selection, reference fraction, voltage/power order; 2 bounded timeout paths. MMIO and power hardware mocked.\n",cases);
}
