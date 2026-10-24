#include "kern_display13_boot.hpp"
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
struct IO {
    std::map<uint32_t,uint32_t> regs;
    std::vector<AlderLakeN::Register> writes;
    uint32_t pteReads {},badPTE {UINT32_MAX};
    XELPMemory::Layout memory=XELPMemory::decode(0x03C1,0x7A800000,0);
    uint64_t readPTE(uint32_t reg) {
        assert(reg==0x800000+8*pteReads);++pteReads;
        return memory.pte(uint64_t(reg-0x800000)/8*4096)^(reg==badPTE?4096:0);
    }
    bool noFrames {},noStop {},noIdle {},changePeer {};
    uint32_t changeRegister {},failWrite {};
    IO() {
        regs[0x45404]=0xFC0F;regs[0x4438C]=0x1C000700;
        regs[0x64000]=regs[0x64100]=0x80000000;
        regs[0x46140]=0x10000000;regs[0x46144]=0x20000000;regs[0x61200]=0x1000;
        for(unsigned p=0;p<2;++p) {
            const auto b=p*0x1000;
            regs[0x60400+b]=p ? 0x90030500 : 0x88030100;
            regs[0x70008+b]=0xC0000000;regs[0x70180+b]=0x84000008;
            regs[0x70188+b]=120;regs[0x70190+b]=0x0437077F;
            regs[0x60000+b]=0x0897077F;regs[0x6000C+b]=0x04640437;
            regs[0x7027C+b]=0x03FF0000;
        }
    }
    bool readable(uint32_t reg) {return reg<0x1000000;}
    uint32_t read(uint32_t reg) {return regs[reg];}
    void write(uint32_t reg,uint32_t value) {
        writes.push_back({reg,value});if(reg==failWrite)return;regs[reg]=value;
        if((reg==0x71008 || reg==0x72008) && !(value&0x80000000) && !noStop)regs[reg]&=~0x40000000;
        if((reg==0x64000 || reg==0x64100 || reg==0x64200) && !(value&0x80000000) && !noIdle)regs[reg]|=0x80;
        if(writes.size()==1)regs[0x4500C]=0;
        if(changePeer && writes.size()==1)regs[0x70180]^=8;
        if(changeRegister && writes.size()==1)regs[changeRegister]^=1;
    }
    void delay() {if(!noFrames) {++regs[0x70040];if(regs[0x71008]&0xC0000000)++regs[0x71040];if(regs[0x72008]&0xC0000000)++regs[0x72040];}}
};
const uint32_t prop[] {1,7,1,2,3};
const auto wiring=TGLHDMIWiring::fromProperty(prop,sizeof(prop));
const auto memory=XELPMemory::decode(0x03C1,0x7A800000,0);
static void dp(IO &io,unsigned pipe,unsigned port,unsigned lanes,bool enhanced) {
    const auto bank=pipe*0x1000;
    io.regs[0x60400+bank]=0x82030000|((port+1)<<27)|((lanes-1)<<1);
    io.regs[0x64000+port*0x100]=0x80000000|((lanes-1)<<1);
    io.regs[0x60540+bank]=0x80000300|(enhanced?1U<<18:0);
    io.regs[0x60030+bank]=0x7E100000;io.regs[0x60034+bank]=0x800000;
    io.regs[0x60040+bank]=0x100000;io.regs[0x60044+bank]=0x800000;
}
static Display13OutputWiring protocols(unsigned dpMask) {
    Display13OutputWiring w=wiring;w.hdmi=3&~dpMask;w.dp=dpMask;
    for(unsigned d=0;d<3;++d)if(!(w.hdmi&(1U<<d)))w.ddc[d]=0;
    assert(w.described());return w;
}
static IO singleGOP(unsigned ddi,unsigned pipe=0) {
    IO io;
    for(unsigned p=0;p<3;++p) {
        const auto b=0x1000*p;
        io.regs[0x70008+b]=p==pipe?0xC0000000:0;
        io.regs[0x70180+b]=p==pipe?0x84000008:0;
        io.regs[0x60400+b]=p==pipe?0x80030000|((ddi+1)<<27):0;
        if(p==pipe) {
            io.regs[0x70188+b]=120;io.regs[0x70190+b]=0x0437077F;
            io.regs[0x60000+b]=0x0897077F;io.regs[0x6000C+b]=0x04640437;
        }
    }
    return io;
}
static void prefixCertificates() {
    unsigned cases=0;
    for(unsigned ddi=0;ddi<3;++ddi)for(unsigned gms:{1U,2U,3U}) {
        auto io=singleGOP(ddi);io.memory=XELPMemory::decode((gms<<8)|0xC1,0x7A800000,0);
        const auto display=AlderLakeN::readDisplay(io,true);
        assert(Display13Boot::reservedPrefix(io,display,true,ddi,io.memory));
        assert(io.pteReads==io.memory.bytes/4096 && io.writes.empty());
        for(unsigned i:{0U,unsigned(io.memory.bytes/8192),unsigned(io.memory.bytes/4096-1)}) {
            auto bad=io;bad.pteReads=0;bad.badPTE=0x800000+8*i;
            assert(!Display13Boot::reservedPrefix(bad,display,true,ddi,bad.memory));
            assert(bad.pteReads==i+1 && bad.writes.empty());++cases;
        }
        for(unsigned fail=0;fail<7;++fail) {
            auto bad=singleGOP(ddi);auto mem=io.memory;
            if(fail==1)bad=singleGOP(ddi,1);
            if(fail==2)dp(bad,0,ddi,4,true);
            if(fail==3)mem.valid=false;
            if(fail==4)mem.allocationEnd=AlderLakeN::ScanoutBytes-4096;
            if(fail==5)bad.regs[0x701AC]=4096;
            assert(!Display13Boot::reservedPrefix(bad,AlderLakeN::readDisplay(bad,true),fail!=0,
                                                  fail==6?3:ddi,mem));
            assert(!bad.pteReads && bad.writes.empty());++cases;
        }
        ++cases;
    }
    printf("PASS %u Display boot certificates: physical layouts, PTE corruption, no-read rejection\n",cases);
}
int main() {
    prefixCertificates();
    IO io;const auto before=io.regs;
    const auto r=Display13Boot::normalize(io,AlderLakeN::readDisplay(io,true),true,wiring,memory);
    assert(r.status==Display13Boot::Status::Complete && r.after==1 && r.stage==7 && r.writes==10);
    assert(!io.regs[0x7127C] && !io.regs[0x7137C] && !io.regs[0x7117C]);
    for(auto w:io.writes)assert(w.offset==0x71180 || w.offset==0x7119C || w.offset==0x71008 ||
        w.offset==0x61404 || w.offset==0x61400 || w.offset==0x64100 || w.offset==0x46144 ||
        w.offset==0x7127C || w.offset==0x7137C || w.offset==0x7117C);
    for(auto pair:before)if(pair.first!=0x70040 && pair.first!=0x46144 && pair.first<0x61000 && pair.first!=0x60404)
        assert(io.regs[pair.first]==pair.second);
    assert(io.regs[0x46140]==0x10000000 && !io.regs[0x46144] && io.regs[0x61200]==0x1000);
    IO frames;frames.noFrames=true;
    assert(Display13Boot::normalize(frames,AlderLakeN::readDisplay(frames,true),true,wiring,memory).status==Display13Boot::Status::Timeout);
    assert(frames.writes.size()==2 && frames.regs[0x7127C]);
    IO stop;stop.noStop=true;
    assert(Display13Boot::normalize(stop,AlderLakeN::readDisplay(stop,true),true,wiring,memory).status==Display13Boot::Status::Timeout);
    assert(stop.writes.size()==3 && stop.regs[0x7127C]);
    IO idle;idle.noIdle=true;
    assert(Display13Boot::normalize(idle,AlderLakeN::readDisplay(idle,true),true,wiring,memory).status==Display13Boot::Status::Timeout);
    assert(idle.writes.size()==6 && idle.regs[0x7127C]);
    IO peer;peer.changePeer=true;
    assert(Display13Boot::normalize(peer,AlderLakeN::readDisplay(peer,true),true,wiring,memory).status==Display13Boot::Status::PeerChanged);
    assert(peer.writes.size()==2 && peer.regs[0x7127C]);
    for(auto reg:{0x71080U,0x71280U,Display13FBIRQ::DSBControl(1,0),0x60404U}) {
        IO foreign;foreign.regs[reg]=0x80000001;
        assert(Display13Boot::normalize(foreign,AlderLakeN::readDisplay(foreign,true),true,wiring,memory).status==Display13Boot::Status::Topology);
        assert(foreign.writes.empty());
    }
    IO port;port.regs[0x61400]=0x88030500;
    assert(Display13Boot::normalize(port,AlderLakeN::readDisplay(port,true),true,wiring,memory).status==Display13Boot::Status::Topology && port.writes.empty());
    IO denied;assert(Display13Boot::normalize(denied,AlderLakeN::readDisplay(denied,true),false,wiring,memory).status==Display13Boot::Status::Unavailable && denied.writes.empty());
    // Three firmware GOPs, all six physical-port/pipe permutations.
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b)for(unsigned c=0;c<3;++c) {
        if(a==b || a==c || b==c)continue;
        IO t;unsigned ports[] {a,b,c};
        for(unsigned p=0;p<3;++p) {
            const auto bank=p*0x1000;
            for(auto reg:{0x70008U,0x70180U,0x70188U,0x70190U,0x60000U,0x6000CU,0x7027CU})t.regs[reg+bank]=t.regs[reg];
            t.regs[0x60400+bank]=0x80030100|((ports[p]+1)<<27);
            t.regs[0x46140+4*p]=(ports[p]+1)<<28;
            t.regs[0x64000+0x100*ports[p]]=0x80000000;
        }
        auto n=Display13Boot::normalize(t,AlderLakeN::readDisplay(t,true),true,wiring,memory);
        assert(n.status==Display13Boot::Status::Complete && n.before==7 && n.after==1 && n.writes==20);
        assert(t.regs[0x64000+0x100*a]&0x80000000);
        assert(!t.regs[0x7127C] && !t.regs[0x7227C] && t.regs[0x7027C]);
    }
    for(auto width:{1280U,1920U,2560U,3840U})for(auto height:{720U,1080U,1440U,2160U}) {
        IO t;
        for(unsigned p=0;p<2;++p) {
            const auto bank=p*0x1000;
            t.regs[0x70188+bank]=(width*4+63)/64;
            t.regs[0x70190+bank]=((height-1)<<16)|(width-1);
            t.regs[0x60000+bank]=((width+280-1)<<16)|(width-1);
            t.regs[0x6000C+bank]=((height+45-1)<<16)|(height-1);
        }
        assert(Display13Boot::normalize(t,AlderLakeN::readDisplay(t,true),true,wiring,memory).status==Display13Boot::Status::Complete);
    }
    unsigned combinations=0;
    for(unsigned a=0;a<2;++a)for(unsigned parked:{1U,2U})for(unsigned dpMask=0;dpMask<4;++dpMask)
    for(unsigned lanes:{1U,2U,4U})for(bool enhanced:{false,true}) {
        IO t;auto mixed=protocols(dpMask);
        if(parked==2) {
            for(auto reg:{0x70008U,0x70180U,0x70188U,0x70190U,0x60000U,0x6000CU,0x7027CU})t.regs[reg+0x2000]=t.regs[reg];
            t.regs[0x61400]=t.regs[0x71008]=t.regs[0x71180]=0;
        }
        for(unsigned p:{0U,parked}) {
            const auto port=p?1-a:a;
            t.regs[0x60400+p*0x1000]=0x80030100|((port+1)<<27);
            t.regs[0x46140+4*p]=(port+1)<<28;
            if(dpMask&(1U<<port))dp(t,p,port,lanes,enhanced);
        }
        const auto boot=AlderLakeN::readDisplay(t,true);const auto before=t.regs;
        assert(boot.version==2 && boot.count==139);
        const auto result=Display13Boot::normalize(t,boot,true,mixed,memory);
        const bool parkDP=dpMask&(1U<<(1-a));
        assert(result.status==Display13Boot::Status::Complete && result.after==1 && result.writes==(parkDP?11U:10U));
        for(auto reg:{0x60400U,0x60540U,0x60030U,0x60034U,0x60040U,0x60044U,0x46140U,0x78000U,0x78004U})
            assert(t.regs[reg]==before.at(reg));
        assert(t.regs[0x64000+0x100*a]==before.at(0x64000+0x100*a));
        if(parkDP) {
            assert(t.writes[5].offset==0x64000+0x100*(1-a));
            assert(t.writes[6].offset==0x60540+parked*0x1000 && !(t.writes[6].value&0x80000000));
            assert(t.writes[7].offset==0x46140+4*parked);
        }
        for(auto write:t.writes)for(auto mn:{0x60030U,0x60034U,0x60040U,0x60044U})
            assert(write.offset!=mn && write.offset!=mn+parked*0x1000);
        ++combinations;
    }
    auto mixed=protocols(3);
    // Both the retained GOP and the parked GOP must have a valid source.
    const AlderLakeN::Register invalid[] {{0x60540,0x80000000},{0x60540,0xC0000300},
        {0x60540,0x88000300},{0x60540,0x80000380},{0x60404,0x10},{0x78000,1U<<29},
        {0x78004,1U<<31},{0x60030,0x7E000000},{0x60034,0},{0x60040,0},{0x60044,0},
        {0x60030,0xFE100000},{0x60034,0x100},{0x60040,0x1000000},{0x60044,0x1000000},
        {0x60400,0x8A030004},{0x60400,0x8B030006},{0x60400,0x8A030206}};
    for(unsigned p:{0U,1U})for(auto bad:invalid) {
        IO t;dp(t,0,0,4,true);dp(t,1,1,4,true);
        auto reg=bad.offset+(bad.offset>=0x78000?p*0x200:p*0x1000);
        auto val=bad.value;if(bad.offset==0x60400)val+=p<<27;
        t.regs[reg]=val;
        assert(Display13Boot::normalize(t,AlderLakeN::readDisplay(t,true),true,mixed,memory).status==Display13Boot::Status::Topology);
        assert(t.writes.empty());
    }
    for(auto reg:{0x64000U,0x64100U,0x46140U,0x46144U}) {
        IO t;dp(t,0,0,4,true);dp(t,1,1,4,true);t.regs[reg]=0;
        assert(Display13Boot::normalize(t,AlderLakeN::readDisplay(t,true),true,mixed,memory).status==Display13Boot::Status::Topology && t.writes.empty());
    }
    // Snapshot freshness and peer preservation cover DP metadata as well.
    for(auto reg:{0x60540U,0x60030U,0x60034U,0x60040U,0x60044U,0x46140U,0x78004U}) {
        IO t;dp(t,0,0,4,true);dp(t,1,1,4,true);const auto boot=AlderLakeN::readDisplay(t,true);
        t.regs[reg]^=1;
        assert(Display13Boot::normalize(t,boot,true,mixed,memory).status==Display13Boot::Status::Readback && t.writes.empty());
        t.regs[reg]^=1;t.changeRegister=reg;
        assert(Display13Boot::normalize(t,boot,true,mixed,memory).status==Display13Boot::Status::PeerChanged && t.writes.size()==2);
    }
    IO tp;dp(tp,0,0,4,true);dp(tp,1,1,4,true);tp.failWrite=0x61540;
    auto failure=Display13Boot::normalize(tp,AlderLakeN::readDisplay(tp,true),true,mixed,memory);
    assert(failure.status==Display13Boot::Status::Timeout && failure.stage==4 && tp.writes.size()==7 && tp.regs[0x7127C]);
    assert(Display13FBTopology::bootDDI(mixed,tp.regs[0x60400])==UINT32_MAX); // Runtime stays closed.
    printf("PASS GOP bootstrap: %u HDMI/DP combinations, STOP/transport/idle ordering, peer/M-N preservation and bounded failures\n",combinations);
}
