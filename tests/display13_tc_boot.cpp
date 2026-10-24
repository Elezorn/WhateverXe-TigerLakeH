#include "kern_display13_boot.hpp"
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
using namespace AlderLakeN;
struct IO {
    std::map<uint32_t,uint32_t> regs;
    std::vector<Register> writes;
    XELPMemory::Layout memory=XELPMemory::decode(0x04C1,0x38000000,0);
    unsigned ticks {},changeTick {},pteReads {};
    uint32_t changeReg {},badPTE {UINT32_MAX};
    bool noFrames {},failEnable {};
    bool readable(uint32_t reg) const {return reg<=0xFFFFFC;}
    uint32_t read(uint32_t reg) {return reg==0x70040 ? ticks : regs[reg];}
    uint64_t readPTE(uint32_t reg) {
        ++pteReads;
        return memory.pte(uint64_t(reg-0x800000)/8*4096)^(reg==badPTE?4096:0);
    }
    void write(uint32_t reg,uint32_t value) {
        writes.push_back({reg,value});
        if(failEnable && reg==0x70180 && value==0x94000008){failEnable=false;return;}
        regs[reg]=value;
    }
    void delay() {
        if(!noFrames)++ticks;
        if(changeTick && ticks==changeTick)regs[changeReg]^=1;
    }
};
static IO firmware(unsigned ddi) {
    IO io;const auto tc=ddi-3;
    // G660i single TC1 GOP geometry and clock/ownership, translated only
    // by the Display13 physical TC stride for the other register instances.
    io.regs={{0x45404,0xFC0F},{0x45454,3U<<(2*ddi)},
        {0x161500+4*tc,0xC},{0x64000+0x100*ddi,0x80000040},
        {0x46038+8*tc,0xCC000000},{0x46100+4*ddi,0x80000000},
        {0x46140,(ddi+3)<<28},{0x60400,0x80030100|((ddi+1)<<27)},
        {0x60000,0x0897077F},{0x6000C,0x04640437},{0x70008,0xC0000000},
        {0x70028,0xA0F},{0x70180,0x84000008},{0x70188,120},{0x70190,0x0437077F}};
    return io;
}
int main() {
    unsigned cases=0;
    for(unsigned ddi=3;ddi<=6;++ddi) {
        auto io=firmware(ddi);const auto original=io.regs;
        auto tc=Display13TC::Boot::capture(io,true,ddi,0);
        auto display=readDisplay(io,true);LinearPlan plan;
        assert(tc.valid() && tc.unchanged(io));
        assert(linearPlan(display,plan,ddi)==LinearStatus::InvalidTCSource);
        auto result=reinitializeLinear(io,display,true,ddi,GOPProtocol::TMDS,&tc);
        assert(result.status==LinearStatus::Success && result.writes==10 && tc.unchanged(io));
        for(auto w:io.writes)assert(w.offset==0x42084 || w.offset==0x70028 || w.offset==0x70180 ||
            w.offset==0x70188 || w.offset==0x7018C || w.offset==0x70190 || w.offset==0x701A4 || w.offset==0x7019C);
        for(auto r:original)if(r.first!=0x70028 && r.first!=0x70180)assert(io.regs[r.first]==r.second);
        assert(Display13Boot::reservedPrefix(io,readDisplay(io,true),true,ddi,io.memory,&tc));
        assert(io.pteReads==io.memory.bytes/4096);
        auto bad=firmware(ddi);bad.badPTE=0x800010;
        assert(!Display13Boot::reservedPrefix(bad,readDisplay(bad,true),true,ddi,bad.memory,&tc) && bad.pteReads==3);
        for(unsigned wrong:{0U,1U,2U,7U,8U,UINT32_MAX})assert(!Display13TC::Boot::capture(io,true,wrong,0).valid());
        assert(!Display13TC::Boot::capture(io,false,ddi,0).valid());
        assert(!Display13TC::Boot::capture(io,true,ddi,3).valid());
        const auto p=tc.port;
        const uint32_t link[]={0x45404,0x45454,p.ready,p.buffer,p.clock,p.pll,0x46140};
        for(auto reg:link) {
            auto changed=firmware(ddi);auto source=readDisplay(changed,true);changed.regs[reg]^=1;
            auto r=reinitializeLinear(changed,source,true,ddi,GOPProtocol::TMDS,&tc);
            assert(r.status==LinearStatus::StateChanged && changed.writes.empty());
            for(unsigned tick:{1U,3U,5U}) {
                auto drift=firmware(ddi);drift.changeReg=reg;drift.changeTick=tick;
                r=reinitializeLinear(drift,readDisplay(drift,true),true,ddi,GOPProtocol::TMDS,&tc);
                assert(r.status==LinearStatus::StateChanged && !r.restored && r.writes==(tick==1?0U:tick==3?2U:10U));
            }
            auto unavailable=firmware(ddi);unavailable.regs[reg]=UINT32_MAX;
            assert(!Display13TC::Boot::capture(unavailable,true,ddi,0).valid());
        }
        for(unsigned state:{0U,1U,2U,5U,6U,7U}) {
            auto alt=firmware(ddi);alt.regs[p.ready]=state;
            assert(!Display13TC::Boot::capture(alt,true,ddi,0).valid());
        }
        auto wrongClock=firmware(ddi);wrongClock.regs[0x46140]=(ddi+1)<<28;
        assert(!Display13TC::Boot::capture(wrongClock,true,ddi,0).valid());
        auto rollback=firmware(ddi);rollback.failEnable=true;
        result=reinitializeLinear(rollback,readDisplay(rollback,true),true,ddi,GOPProtocol::TMDS,&tc);
        assert(result.status==LinearStatus::EnableTimeout && result.restored && tc.unchanged(rollback));
        auto stopped=firmware(ddi);stopped.noFrames=true;
        result=reinitializeLinear(stopped,readDisplay(stopped,true),true,ddi,GOPProtocol::TMDS,&tc);
        assert(result.status==LinearStatus::NoVblank && stopped.writes.empty());
        ++cases;
    }
    printf("PASS %u TC GOP handoffs: power/ownership/clock preservation, plane rollback, GGTT prefix and link drift\n",cases);
}
