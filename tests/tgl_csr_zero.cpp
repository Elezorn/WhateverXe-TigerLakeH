#include "kern_tgl_csr_zero.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace TGLCSRZero;
int main() {
 constexpr uint64_t owner=0xffff800000001000ULL,streamer=owner+0x2000,
  item=owner+0x4000,context=owner+0x6000,buffer=owner+0x8000,image=owner+0x10000;
 std::map<uint64_t,uint64_t> m{{streamer+0x10,owner},{streamer+0x50,0},
  {streamer+0x30,0},{streamer+0x38,0},{streamer+0x40,item},{streamer+0x48,0},
  {item,context},{context+0x50,owner},{context+0x6c,0},{context+0x7c,6},
  {context+0x89,0xc040324319},{context+0x98,buffer},{context+0xc9,0},{buffer+0x20,81920},
  {buffer+0x38,image},{image+0x1004,0x11081019},{image+0x1008,0x2244},
  {image+0x100c,0xffff0010},{image+0x1010,0x2034},{image+0x1014,0x806750},
  {image+0x1018,0x2030},{image+0x101c,0x68c0},{image+0x12d8,0x2084},
  {image+0x1110,0x61040001},{image+0x1114,0},{image+0x1118,0}};
 std::map<uint32_t,uint32_t> regs{{0xd84,2},{0x2234,0x18001},{0x20e0,0},{0x20e4,2}};
 const auto initial=m;unsigned writes=0;
 auto read=[&](uint64_t p,void *v,unsigned n)->uint64_t {
  auto it=m.find(p);if(it==m.end())return 0;memcpy(v,&it->second,n);return n;
 };
 unsigned mmioReads=0;
 auto mmio=[&](uint32_t p) {++mmioReads;assert(regs.count(p));return regs[p];};
 auto write=[&](uint64_t p,const void *v,unsigned n) {
  ++writes;assert(p==image+0x1110 && n==12);
  for(unsigned i=0;i<3;++i) {uint32_t word;memcpy(&word,static_cast<const uint8_t *>(v)+4*i,4);m[p+4*i]=word;}
 };
 State s;prepare(s,owner,streamer,2689,read,mmio,write);
 assert(s.q[4]==1 && !s.q[8] && writes==1 && s.first.q[10]==0x61040001);
 for(const auto &f:initial)assert(m[f.first]==((f.first>=image+0x1110 && f.first<=image+0x1118)?0:f.second));
 prepare(s,owner,streamer,2690,read,mmio,write);assert(s.q[5]==1 && writes==1);
 for(auto f:std::vector<std::pair<uint64_t,uint64_t>>{{streamer+0x30,item},{streamer+0x38,item},
    {streamer+0x48,item},{streamer+0x10,owner+1},{context+0x50,owner+1},
    {context+0x6c,1},{context+0xc9,1},{buffer+0x20,4096},{image+0x100c,0xffff0011},
    {image+0x101c,0x6750},{image+0x1114,0x100000},{image+0x1118,1},
    {image+0x1110,0x11000001},{image+0x12d8,0x20c4}}) {
  m=initial;m[f.first]=f.second;writes=0;s={};prepare(s,owner,streamer,3,read,mmio,write);assert(!writes && !s.q[4]);
 }
 for(auto f:std::vector<std::pair<uint32_t,uint32_t>>{{0xd84,0},{0xd84,UINT32_MAX},
    {0x2234,0x3098},{0x20e0,0x4000},{0x20e0,UINT32_MAX},{0x20e4,0},{0x20e4,4}}) {
  m=initial;const auto old=regs[f.first];regs[f.first]=f.second;writes=0;s={};
  prepare(s,owner,streamer,3,read,mmio,write);assert(!writes);regs[f.first]=old;
 }
 for(uint32_t ack:{0U,UINT32_MAX}) {
  m=initial;s={};regs[0xd84]=ack;mmioReads=0;
  prepare(s,owner,streamer,3,read,mmio,write);assert(mmioReads==1);
 }
 regs[0xd84]=2;m=initial;m[streamer+0x50]=1;m[streamer+0x30]=item;m[streamer+0x40]=0;
 s={};writes=0;prepare(s,owner,streamer,9,read,mmio,write);assert(writes==1 && s.q[4]==1);
 m=initial;s={};writes=0;unsigned bankReads=0;
 auto race=[&](uint64_t p,void *v,unsigned n)->uint64_t {
  if(p==streamer+0x50 && ++bankReads==2)m[p]=1;
  return read(p,v,n);
 };
 prepare(s,owner,streamer,3,race,mmio,write);assert(!writes && s.q[6]);
 m=initial;s={};writes=0;
 auto failRead=[&](uint64_t p,void *v,unsigned n)->uint64_t {return p==image+0x1118?0:read(p,v,n);};
 prepare(s,owner,streamer,3,failRead,mmio,write);assert(!writes && s.q[6]);
 m=initial;s={};
 auto failWrite=[&](uint64_t,const void *,unsigned) {};
 prepare(s,owner,streamer,3,read,mmio,failWrite);assert(s.q[4]==1 && s.q[8]==1);
}
