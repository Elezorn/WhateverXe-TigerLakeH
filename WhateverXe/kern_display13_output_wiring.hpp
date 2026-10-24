// Protocol selection is separate from native path/power/clock resources.
// Describing a route does not install its hardware backend or authorize boot
// handoff. Keep those admission decisions in the caller's operation contract.
#ifndef KERN_DISPLAY13_OUTPUT_WIRING_HPP
#define KERN_DISPLAY13_OUTPUT_WIRING_HPP
#include "kern_tgl_hdmi_wiring.hpp"
#include "kern_tgl_combo_dp.hpp"
struct Display13OutputWiring : private TGLHDMIWiring {
    using TGLHDMIWiring::hdmi;
    using TGLHDMIWiring::ddc;
    using TGLHDMIWiring::valid;
    uint32_t dp {};
    Display13OutputWiring()=default;
    Display13OutputWiring(const TGLHDMIWiring &w):TGLHDMIWiring(w) {}
    static Display13OutputWiring fromProperty(const void *data,uint32_t bytes) {
        return TGLHDMIWiring::fromProperty(data,bytes);
    }
    bool matchesHDMI(const uint32_t *c) const {return described() && TGLHDMIWiring::matches(c);}
    bool usesDDC(uint32_t bus) const {return described() && TGLHDMIWiring::usesDDC(bus);}
    uint32_t selected() const {return hdmi|dp;}
    bool described() const {
        if(!valid || !selected() || (selected()&~7U) || (dp&~3U) || (hdmi&dp))return false;
        uint32_t used=0;
        for(unsigned d=0;d<3;++d) {
            const auto pin=ddc[d];
            if(hdmi&(1U<<d)) {
                if(pin<1 || pin>3 || (used&(1U<<pin)))return false;
                used|=1U<<pin;
            } else if(pin)return false;
        }
        return true;
    }
    bool matches(const uint32_t *c) const {
        if(!described() || !c || c[0]>=3)return false;
        if(hdmi&(1U<<c[0]))return TGLHDMIWiring::matches(c);
        if(!(dp&(1U<<c[0])))return false;
        const uint32_t expected[]={c[0],c[0]+1,1,0,0x400,0x201};
        return !memcmp(c,expected,sizeof(expected));
    }
    // Exact donor Combo DP records have the same ABI on Display12/13. This
    // only changes CPU topology; it grants no DDI/AUX/PLL access by itself.
    bool apply(void *platform,uint32_t bytes) const {
        if(!described() || !TGLComboDP::accepts(dp,platform,bytes))return false;
        if(!TGLHDMIWiring::apply(platform,bytes))return false;
        return TGLComboDP::apply(dp,platform,bytes);
    }
};
#endif
