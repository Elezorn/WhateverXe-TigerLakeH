#ifndef KERN_DISPLAY13_FIRMWARE_HPP
#define KERN_DISPLAY13_FIRMWARE_HPP
#include "kern_vbt.hpp"
#include "kern_display13_output_wiring.hpp"
namespace XELP { namespace VBT {
    // ADLP/ADLN VBT DDC1/2 => Combo pins1/2, DDC3..6 => TC pins9..12.
    // Capability bits are not live modes. HDMI-only records may advertise
    // USB-C/TBT capability; the TC runtime must separately prove legacy mode.
    inline Display13OutputWiring display13HDMI(const Ports &ports,uint32_t backend) {
        Display13OutputWiring w;w.valid=true;
        if(!ports.valid())return w;
        uint32_t used=0;
        for(unsigned d=0;d<7;++d) {
            if(!(ports.present&backend&Display13OutputWiring::Physical&(1U<<d)) || !ports.port[d].tmdsOnly())continue;
            const auto selector=ports.port[d].ddc;
            const auto pin=selector>=3 && selector<=6 ? selector+6 : selector<=2 ? selector : 0;
            if(!pin || (d<3 ? pin>2 : pin<9) || (used&(1U<<pin)))return {};
            used|=1U<<pin;w.hdmi|=1U<<d;w.ddc[d]=pin;
        }
        return w;
    }
} }
#endif
