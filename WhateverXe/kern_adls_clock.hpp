// ADL-S Combo clocks are shared resources, not a PHY-numbered array of owners.
// Linux v6.12 intel_ddi.c (rkl_ddi_get_pll) and intel_dpll_mgr.c (adls_pll_mgr).
#ifndef KERN_ADLS_CLOCK_HPP
#define KERN_ADLS_CLOCK_HPP
#include "kern_alderlake_s.hpp"

namespace AlderLakeS {
struct ClockSnapshot {
    uint32_t mux[2] {},enable[4] {};
    bool valid() const {
        for(auto v:mux)if(v==UINT32_MAX)return false;
        for(auto v:enable)if(v==UINT32_MAX)return false;
        return true;
    }
    uint32_t value(const Port &p) const {return mux[p.phy<3?0:1];}
    bool active(const Port &p) const {return !(value(p)&p.clockOff);}
    uint32_t selected(const Port &p) const {return (value(p)>>p.clockShift)&3;}
    uint32_t peers(const Port &target) const {
        uint32_t mask=0;
        for(unsigned phy=0;phy<5;++phy) {
            Port p;Port::resolve(phy?phy+2:0,p);
            if(p.phy!=target.phy && active(p))mask|=1U<<selected(p);
        }
        return mask;
    }
};
struct ClockChoice {
    uint32_t pll {UINT32_MAX};
    bool valid {},keepPLL {};
};
struct ClockAllocation {
    // PLL+1, including an incomplete enable whose output is still gated.
    // The runtime owns serialization and clears this state at controller start.
    uint8_t binding[3] {};
    ClockChoice choose(const Port &p,const ClockSnapshot &s,bool on) const {
        if(p.phy>=3 || !s.valid())return {};
        auto peers=s.peers(p);
        for(unsigned phy=0;phy<3;++phy)
            if(phy!=p.phy && binding[phy])peers|=1U<<(binding[phy]-1);
        const auto previous=s.active(p)?s.selected(p):binding[p.phy]?binding[p.phy]-1:UINT32_MAX;
        if(!on)return {previous,true,previous==UINT32_MAX || bool(peers&(1U<<previous))};
        if(previous<4 && !(peers&(1U<<previous)))return {previous,true,false};
        // Do not commandeer an enabled but unaccounted-for firmware PLL.
        auto busy=peers;
        for(unsigned pll=0;pll<4;++pll)if(s.enable[pll]&0xCC000000U)busy|=1U<<pll;
        if(!(busy&(1U<<p.phy)))return {p.phy,true,false};
        for(unsigned pll=0;pll<3;++pll)if(!(busy&(1U<<pll)))return {pll,true,false};
        return {};
    }
};
// Only the qualified donor clock body's own accesses are translated. In
// particular calculateVoltageLevel must continue to inspect physical PLLs.
struct ClockTranslation {
    uint32_t phy {},pll {};
    bool enabling {};
    bool caller(uint32_t pc) const {
        return enabling?(pc>=0x6CE10 && pc<0x6DA26):(pc>=0x6DAE6 && pc<0x6DE58);
    }
    unsigned long reg(unsigned long r,uint32_t pc) const {
        if(!caller(pc))return r;
        PLL from,to;
        if(!PLL::resolve(phy,from) || !PLL::resolve(pll,to))return r;
        if(r==from.enable)return to.enable;
        if(r==from.cfg0)return to.cfg0;
        if(r==from.cfg1)return to.cfg1;
        return r;
    }
    uint32_t mux(uint32_t value,uint32_t pc) const {
        if(!enabling || pc!=0x6D77E || phy>=3 || pll>=4)return value;
        return (value&~(3U<<(2*phy)))|(pll<<(2*phy));
    }
    template<class Read,class Write> void prepareMux(Read read,Write write,uint32_t value) const {
        const auto old=read(0x164280);
        Port route;Port::resolve(phy?phy+2:0,route);
        if((old^value)&(3U<<route.clockShift)) {
            if(!(old&route.clockOff))write(0x164280,old|route.clockOff);
            write(0x164280,value|route.clockOff);
        }
    }
};
}
#endif
