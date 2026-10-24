#include "kern_display13_tc_clock.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace Display13TC;
struct IO {
    Port p;std::map<uint32_t,uint32_t> regs,bank;
    std::vector<std::pair<uint32_t,uint32_t>> writes;
    bool held {},power {true},owner {true},idle {true},stuckPower {},stuckLock {},fault {};
    unsigned time {},locks {},unlocks {};uint32_t reject {},absent {};
    IO(unsigned ddi) {
        assert(Port::resolve(ddi,p));
        regs={{p.pll,0x20000000},{p.clock,0x1234},{p.ready,0xc},{p.buffer,0xc0},
              {Clock::Gate,0xdeadffff|Clock::gateBit(p.tc)},{p.hip,0x01010101}};
        for(auto w:TMDSClock::calculate(148500,38400).words)bank[w.reg]=0xa5a5a5a5;
    }
    bool powerHeld(){return power;}
    bool owned(unsigned d){return owner && d==p.ddi;}
    bool quiescent(unsigned d){return idle && d==p.ddi;}
    bool readable(uint32_t r){return r!=absent && (regs.count(r) || (r>=p.phy && r<p.phy+0x1000));}
    uint32_t key(uint32_t r){return (((regs[p.hip]>>p.shift)&15)<<12)|(r-p.phy);}
    uint32_t read(uint32_t r){assert(readable(r));return regs.count(r)?regs[r]:bank[key(r)];}
    void write(uint32_t r,uint32_t v) {
        assert(held && readable(r));writes.emplace_back(r,v);if(r==reject)return;
        if(r==p.pll) {
            auto status=regs[r]&(Clock::Locked|Clock::Powered);
            if(!stuckPower)status=(status&~Clock::Powered)|((v&Clock::Power)?Clock::Powered:0);
            if(!stuckLock)status=(status&~Clock::Locked)|((v&Clock::Enable)?Clock::Locked:0);
            regs[r]=(v&~(Clock::Locked|Clock::Powered))|status;
        } else if(regs.count(r))regs[r]=v;else bank[key(r)]=v;
    }
    unsigned lock(){assert(!held);held=true;++locks;return 7;}
    void unlock(unsigned irq){assert(held && irq==7);held=false;++unlocks;}
    void delayUs(unsigned n){assert(!held);time+=n;}
    void bankFault(){fault=true;}
};
int main() {
    const auto full=TMDSClock::calculate(148500,38400),hd=TMDSClock::calculate(74250,38400);
    // Independent live i915 shared-PLL captures: central FHD / outer 720p.
    const uint32_t f[]={0x100,0x500,0x15400,0x84274,0x1c0027,0x40002000,0x41000000,0x4a};
    const uint32_t h[]={0x100,0x500,0x16500,0xa5278,0x1c0027,0x40002000,0x76600000,0x47};
    assert(full.valid() && hd.valid());
    for(unsigned i=0;i<8;++i){assert(full.words[i].value==f[i]);assert(hd.words[i].value==h[i]);}
    for(unsigned ref:{19200U,24000U,38400U})for(unsigned rate=25000;rate<=600000;rate+=125) {
        auto plan=TMDSClock::calculate(rate,ref);
        // Finite PLL divider range has real clock holes; do not round those
        // into a different display timing. Independently enumerate solutions.
        unsigned oscillator=0;for(unsigned h:{7U,5U,3U,2U})for(unsigned d=10;d;--d)
            if(!oscillator && 5*rate*h*d>=7992000 && 5*rate*h*d<=10000000)oscillator=5*rate*h*d;
        assert(plan.valid()==(oscillator && oscillator/(2*ref)<=255));
        if(!plan.valid())continue;
        const auto actual=TMDSClock::frequency(ref,plan.words[3].value,plan.words[6].value,plan.words[2].value);
        assert(actual==rate);
        for(auto w:plan.words)assert(!(w.value&~w.mask));
    }
    for(unsigned rate:{0U,24999U,600001U,UINT32_MAX})assert(!TMDSClock::calculate(rate,38400).valid());
    for(unsigned ref:{0U,1U,25000U,UINT32_MAX})assert(!TMDSClock::calculate(148500,ref).valid());
    for(unsigned ddi=3;ddi<=6;++ddi) {
        IO io(ddi);const auto before=io.regs;auto previous=io.bank;
        assert(Clock::enable(io,io.p,full));assert(io.regs[io.p.hip]==before.at(io.p.hip));
        assert(io.regs[io.p.pll]==0xec000000 && io.regs[io.p.clock]==0x80001234);
        assert(io.regs[Clock::Gate]==(before.at(Clock::Gate)&~Clock::gateBit(ddi-3)));
        for(auto w:full.words)assert(io.bank[w.reg]==((previous[w.reg]&~w.mask)|w.value));
        const auto count=io.writes.size();assert(!Clock::enable(io,io.p,hd) && io.writes.size()==count);
        assert(Clock::disable(io,io.p));assert(io.regs[io.p.pll]==0x20000000 && io.regs[io.p.clock]==0x1234);
        assert(io.regs[Clock::Gate]==before.at(Clock::Gate) && io.locks==io.unlocks);
        assert(Clock::enable(io,io.p,hd));assert(Clock::disable(io,io.p));
    }
    const uint32_t native[]={464,2048,3,2,2};
    assert(TMDSClock::nativeRate(native,2)==148500);
    assert(!TMDSClock::nativeRate(native,3) && !TMDSClock::nativeRate(nullptr,2));
    for(unsigned d=3;d<=6;++d)for(unsigned preset=0;preset<10;++preset) {
        IO io(d);auto before=io.regs[io.p.hip];
        for(unsigned lane=0;lane<2;++lane)for(unsigned reg:{0xa0U,0xd00U,0x2c0U,0x2c4U,0x2c8U})io.bank[lane*0x1000+reg]=0xa5a5a5a5;
        assert(TMDSSignal::program(io,io.p,preset));
        const unsigned swing[]={7,6,4,2,0,0,0,0,0,0},emphasis[]={0,0,0,0,0,5,6,7,8,10};
        for(unsigned lane=0;lane<2;++lane) {
            assert(io.bank[lane*0x1000+0xa0]==((0xa5a5a5a5&~0xc0U)|0x80));
            assert(io.bank[lane*0x1000+0xd00]==0);
            for(unsigned tx=0;tx<2;++tx)assert(io.bank[lane*0x1000+0x2c0+4*tx]==((0xa5a5a5a5&~0x3ff07U)|swing[preset]|(emphasis[preset]<<8)));
            assert(io.bank[lane*0x1000+0x2c8]==((0xa5a5a5a5&~0x7cU)|(lane?0x78U:0x40U)));
        }
        assert(io.regs[io.p.hip]==before);
        const auto count=io.writes.size();assert(!TMDSSignal::program(io,io.p,10) && io.writes.size()==count);
        io.owner=false;assert(!TMDSSignal::program(io,io.p,7) && io.writes.size()==count);
    }
    for(unsigned failure=0;failure<8;++failure) {
        IO io(3);
        if(failure==0)io.power=false;if(failure==1)io.owner=false;if(failure==2)io.idle=false;
        if(failure==3)io.regs[io.p.ready]=5;if(failure==4)io.regs[io.p.buffer]|=1U<<31;
        if(failure==5)io.regs[io.p.buffer]&=~64U;if(failure==6)io.absent=io.p.pll;if(failure==7)io.regs[io.p.pll]=~0U;
        assert(!Clock::enable(io,io.p,full) && io.writes.empty());
        assert(!Clock::disable(io,io.p) && io.writes.empty());
    }
    {IO io(3);io.stuckPower=true;assert(!Clock::enable(io,io.p,full) && io.time==1000 && io.writes.size()==1);}
    {IO io(3);io.stuckLock=true;assert(!Clock::enable(io,io.p,full) && io.time==1000);assert(!(io.regs[io.p.clock]&0xf0000000));}
    {IO io(3);io.reject=io.p.phyRegister(0x2200);auto bank=io.regs[io.p.hip];assert(!Clock::enable(io,io.p,full));assert(io.regs[io.p.hip]==bank && !(io.regs[io.p.pll]&Clock::Enable));}
    puts("PASS Display13 TC HDMI PLL live fixtures, clock sweep, reserved bits, bounded failures and lifecycle");
}
