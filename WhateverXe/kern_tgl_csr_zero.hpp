// Diagnostic intervention, not a qualification of context-restore correctness.
#ifndef kern_tgl_csr_zero_hpp
#define kern_tgl_csr_zero_hpp
#include <stdint.h>
#include <string.h>
namespace TGLCSRZero {
struct Receipt {
 uint64_t q[17] {}; // context, image, descriptor, tail, head, ID, EL status,
 // selector, preemption mode, ACK, CSR words before[3], words after[3], ordinal.
};
struct State {
 // schema, attempts, idle, owned layout, writes, already NOP, declined,
 // nonzero CSR base, readback failure, last native submit ordinal.
 uint64_t q[10] {1};
 Receipt first{},last{};
};
template<class Read,class MMIO,class Write>
void prepare(State &s,uint64_t owner,uint64_t streamer,uint64_t ordinal,
             Read read,MMIO mmio,Write write) {
 ++s.q[1];s.q[9]=ordinal;
 auto decline=[&] {++s.q[6];};
 auto field=[&](uint64_t p,unsigned off,uint64_t &v,unsigned n=8) {
  v=0;return (p>>47)==0x1ffff && p+off+n>=p && read(p+off,&v,n)==n;
 };
 Receipt r;auto q=r.q;q[16]=ordinal;
 q[9]=mmio(0xd84);
 if(q[9]==UINT32_MAX || !(q[9]&2))return decline();
 q[6]=mmio(0x2234);if(q[6]!=0x18001)return decline();
 ++s.q[2];q[7]=mmio(0x20e0);q[8]=mmio(0x20e4);
 // Global selector, thread-group mode: do not elide a mid-thread CSR state.
 if(q[7]==UINT32_MAX || (q[7]&0x4000) || q[8]==UINT32_MAX || (q[8]&6)!=2)return decline();
 uint64_t actual=0,bank=2,active[2]{},pending[2]{},buffer=0,size=0,engine=255,forced=0;
 if(!owner || !field(streamer,0x10,actual) || actual!=owner ||
    !field(streamer,0x50,bank,4) || bank>1 ||
    !field(streamer,0x30+unsigned(bank)*16,active[0]) || active[0] ||
    !field(streamer,0x38+unsigned(bank)*16,active[1]) || active[1] ||
    !field(streamer,0x30+unsigned(1-bank)*16,pending[0]) || !pending[0] ||
    !field(streamer,0x38+unsigned(1-bank)*16,pending[1]) || pending[1] ||
    !field(pending[0],0,q[0]) || !field(q[0],0x50,actual) || actual!=owner ||
    !field(q[0],0x6c,engine,1) || engine || !field(q[0],0x7c,q[5],4) ||
    !field(q[0],0x89,q[2]) || !(q[2]&1) ||
    !field(q[0],0xc9,forced,1) || forced ||
    !field(q[0],0x98,buffer) || !field(buffer,0x20,size) || size!=81920 ||
    !field(buffer,0x38,q[1]))return decline();
 uint64_t header=0,reg=0,control=0;
 if(!field(q[1],0x1004,header,4) || header!=0x11081019 ||
    !field(q[1],0x1008,reg,4) || reg!=0x2244 ||
    !field(q[1],0x100c,control,4) || (control&1) ||
    !field(q[1],0x1010,reg,4) || reg!=0x2034 || !field(q[1],0x1014,q[4],4) ||
    !field(q[1],0x1018,reg,4) || reg!=0x2030 || !field(q[1],0x101c,q[3],4) ||
    (q[3]&7) || q[3]>=32768 || (q[4]&0x1ffffc)==q[3] ||
    !field(q[1],0x12d8,reg,4) || reg!=0x2084 ||
    !field(q[1],0x1110,q[10],4) || !field(q[1],0x1114,q[11],4) ||
    !field(q[1],0x1118,q[12],4))return decline();
 ++s.q[3];
 if(q[11] || q[12]) {++s.q[7];return;}
 if(!q[10]) {++s.q[5];return;}
 if(q[10]!=0x61040001)return decline();
 // The native caller serializes this streamer. Recheck the observed idle and
 // pending ownership immediately before the sole 12-byte CPU image write.
 uint64_t again=0;
 if(!field(streamer,0x10,again) || again!=owner ||
    !field(streamer,0x50,again,4) || again!=bank ||
    !field(streamer,0x30+unsigned(bank)*16,again) || again ||
    !field(streamer,0x38+unsigned(bank)*16,again) || again ||
    !field(streamer,0x30+unsigned(1-bank)*16,again) || again!=pending[0] ||
    !field(streamer,0x38+unsigned(1-bank)*16,again) || again ||
    !field(pending[0],0,again) || again!=q[0] ||
    !field(q[0],0x50,again) || again!=owner ||
    !field(q[0],0x89,again) || again!=q[2] ||
    !field(q[0],0xc9,again,1) || again ||
    !field(q[0],0x98,again) || again!=buffer ||
    !field(buffer,0x38,again) || again!=q[1] ||
    !field(q[1],0x1014,again,4) || again!=q[4] ||
    !field(q[1],0x101c,again,4) || again!=q[3] ||
    mmio(0xd84)!=q[9] || mmio(0x2234)!=0x18001)return decline();
 const uint32_t nop[3] {};
 write(q[1]+0x1110,nop,sizeof(nop));
 bool ok=true;for(unsigned i=0;i<3;++i)ok=field(q[1],0x1110+i*4,q[13+i],4) && !q[13+i] && ok;
 if(!s.q[4])s.first=r;s.last=r;++s.q[4];if(!ok)++s.q[8];
}
}
#endif
