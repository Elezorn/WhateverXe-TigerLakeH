// ADL-S coordinates from Linux v6.12 intel_bios.c, intel_ddi.c and i915_reg.h.
// Physical DDI, Combo PHY and GMBUS pin are distinct namespaces. This module
// describes hardware; it does not grant donor or runtime admission.
#ifndef KERN_ALDERLAKE_S_HPP
#define KERN_ALDERLAKE_S_HPP
#include <stdint.h>
#include "kern_vbt.hpp"

namespace AlderLakeS {
constexpr uint32_t PortMask=0x79; // A, TC1, TC2, TC3, TC4
// Initial GT qualification is the measured UHD770 D0 configuration. Do not
// infer fused EU/engine resources for other products from their family name.
inline bool identity(uint32_t pci,uint32_t revision) {return pci==0x46908086 && revision==0x0C;}
struct GT {
    uint32_t slices {},dss {},nativeSubslices {},eus {},l3Banks {},urbKiB {};
    static bool decode(uint32_t pci,uint32_t revision,const uint32_t *fuses,GT &out) {
        out={};
        if(!identity(pci,revision) || !fuses || fuses[0]!=1 || fuses[1]!=3 ||
           fuses[2]!=0 || fuses[3]!=0xF0 || fuses[4]!=0xE00FA)return false;
        out={1,2,4,32,4,256};return true;
    }
};
struct Port {
    uint32_t ddi {},phy {},buffer {},clock {},clockShift {},clockOff {},hpd {};
    static bool resolve(uint32_t ddi,Port &out) {
        out={};
        if(ddi>6 || !(PortMask&(1U<<ddi)))return false;
        const auto phy=ddi ? ddi-2 : 0;
        const uint32_t off[]={10,11,24,4,5};
        out={ddi,phy,0x64000+0x100*ddi,phy<3?0x164280U:0x1642BCU,
             2*(phy%3),1U<<off[phy],1U<<(ddi?ddi+21:16)};
        return true;
    }
    bool connected(uint32_t sde) const {return sde!=UINT32_MAX && (sde&hpd);}
};
// ADL-S uses TGL PW_2/PW_3 for physical TC lane domains, including Combo
// PHY HDMI. Native pipe refs do not include this independent lane consumer.
struct LanePower {
    static uint32_t target(uint32_t native,bool tc1) {
        return native | (tc1 ? 0x2AU : 0U);
    }
    static uint32_t pipeRequests(const uint32_t *refs) {
        uint32_t out=0;
        for(unsigned i=0;i<4;++i)if(refs[i])out|=2U<<(2*i);
        return out;
    }
};
// VBT numbers A/TC1..4 as 1..5; GMBUS uses 1/9..12. Zero means absent.
inline uint32_t gmbus(uint32_t vbtPin) {
    return vbtPin==1?1:vbtPin>=2 && vbtPin<=5?vbtPin+7:0;
}
struct HDMI {
    uint32_t selected {},pin[XELP::PortCount] {};
    bool valid {};
    bool matches(const uint32_t *c) const {
        return valid && c && c[0]<XELP::PortCount && (selected&(1U<<c[0])) &&
            c[1]==pin[c[0]] && c[2]==2 && !c[3] && c[4]==0x800 && (c[5]&0xB)==2;
    }
    bool usesDDC(uint32_t bus) const {
        if(!valid)return false;
        for(unsigned d=0;d<XELP::PortCount;++d)if((selected&(1U<<d)) && pin[d]==bus)return true;
        return false;
    }
    // Existing physical donor slots are retained. TC-labelled DDIs are
    // Combo PHYs on ADL-S; connector kind alone must not select Dekel.
    bool apply(void *platform,uint32_t bytes) const {
        if(!valid || !selected || (selected&~PortMask) || !platform || bytes<0x1C0)return false;
        auto p=static_cast<uint8_t *>(platform);
        if(p[0x12]!=9)return false;
        for(unsigned d=0;d<XELP::PortCount;++d)if(selected&(1U<<d)) {
            const uint32_t expected[]={d,d>=3?d+6:d+1,1,d>=3?1U:0U,0x400,0x201};
            const uint32_t panel[]={0,0,0,0,2,0x18};
            if(memcmp(p+0x20+24*d,expected,sizeof(expected)) &&
               (d || memcmp(p+0x20,panel,sizeof(panel))))return false;
        }
        for(unsigned d=0;d<XELP::PortCount;++d)if(selected&(1U<<d)) {
            auto c=reinterpret_cast<uint32_t *>(p+0x20+24*d);
            c[1]=pin[d];c[2]=2;c[3]=0;c[4]=0x800;c[5]=0x202;
        }
        return true;
    }
    static HDMI fromFirmware(const XELP::VBT::Ports &ports) {
        HDMI out;
        if(!ports.valid())return out;
        uint32_t used=0;
        for(unsigned d=0;d<XELP::PortCount;++d) {
            if(!(ports.present&(1U<<d)) || !ports.port[d].fixedTMDS())continue;
            Port port;
            const auto pin=gmbus(ports.port[d].ddc);
            if(!Port::resolve(d,port) || !pin || (used&(1U<<pin)))return {};
            used|=1U<<pin;out.selected|=1U<<d;out.pin[d]=pin;
        }
        out.valid=true;return out;
    }
};
// First fixed external DP backend: VBT TC2 / AUX USBC2 / Combo C.
// Reject reversal, LSPCON, internal and Type-C intent instead of guessing.
struct DP {
    static bool qualified(const XELP::VBT::Ports &ports) {
        if(!ports.valid() || !(ports.present&16))return false;
        const auto &p=ports.port[4];
        if(p.type!=0x60D6 || p.dvo!=8 || p.ddc!=3 || p.aux!=0x20 ||
           (p.flags&0x17) || (p.capabilities&3))return false;
        for(unsigned d=0;d<XELP::PortCount;++d)
            if(d!=4 && (ports.present&(1U<<d)) && ports.port[d].dp() && ports.port[d].aux==p.aux)return false;
        return true;
    }
    static bool matches(const uint32_t *c) {
        const uint32_t expected[]={4,10,1,0,0x400,0x201};
        return c && !memcmp(c,expected,sizeof(expected));
    }
    static uint32_t hotplugMode(bool qualified,const uint32_t *config,const uint32_t *bound,
                                uintptr_t caller,uintptr_t hpdCaller,uint32_t requested) {
        return qualified && config==bound && matches(config) && hpdCaller && caller==hpdCaller && requested==4 ? 1 : requested;
    }
    static bool apply(void *platform,size_t bytes) {
        if(!platform || bytes<0x1C0)return false;
        auto p=static_cast<uint8_t *>(platform);
        const uint32_t expected[]={4,10,1,1,0x400,0x201};
        if(p[0x12]!=9 || memcmp(p+0x80,expected,sizeof(expected)))return false;
        reinterpret_cast<uint32_t *>(p+0x80)[3]=0;return true;
    }
};
struct PLL {
    uint32_t enable {},cfg0 {},cfg1 {};
    static bool resolve(uint32_t id,PLL &out) {
        out={};if(id>3)return false;
        const uint32_t enables[]={0x46010,0x46014,0x46018,0x46030};
        const uint32_t configs[]={0x164284,0x16428C,0x164294,0x1642C0};
        out={enables[id],configs[id],configs[id]+4};return true;
    }
};
// PortHAL::init(c004fc1a) constructs DDI controls independently from the
// Combo PHY table at +0x5AC. Its TC1 case leaves that table zero. Populate
// only PHY B's fields; retain TC1's buffer, physical record and AUX slots.
inline bool comboBRegisters(uint32_t *registers,size_t count) {
    if(!registers || count!=0x128/4 || registers[(0x590-0x580)/4]!=0x64300 ||
       registers[(0x64C-0x580)/4]!=0x4610C)return false;
    for(unsigned off=0x5AC;off<=0x648;off+=4)if(registers[(off-0x580)/4])return false;
    const uint32_t phy[] {
        0x64C04,0x6C100,0x6C104,0x6C10C,0x6C124,0x6C128,
        0x6C014,0x6C028,0x6C030,0x6C03C,
        0x6C688,0x6C888,0x6C988,0x6CA88,0x6CB88,0x6C388,
        0x6C690,0x6C890,0x6C990,0x6CA90,0x6CB90,0x6C390,
        0x6C694,0x6C894,0x6C994,0x6CA94,0x6CB94,0x6C394,
        0x6C604,0x6C804,0x6C904,0x6CA04,0x6CB04,0x6C304,
        0x6C624,0x6C824,0x6C924,0x6CA24,0x6CB24,0x6C324
    };
    static_assert(sizeof(phy)==0x648-0x5AC+4,"Combo PHY register table span");
    memcpy(registers+(0x5AC-0x580)/4,phy,sizeof(phy));return true;
}
// The donor's EHL Combo C bank matches ADL-S PHY C. Keep physical
// TC2's DDI/AUX register slots and translate only the separate PHY table.
inline bool comboCRegisters(uint32_t *registers,size_t count) {
    if(!registers || count!=0x128/4 || registers[4]!=0x64400 ||
       registers[(0x64C-0x580)/4]!=0x46110)return false;
    for(unsigned off=0x5AC;off<=0x648;off+=4)if(registers[(off-0x580)/4])return false;
    uint32_t temporary[0x128/4] {};temporary[4]=0x64300;temporary[(0x64C-0x580)/4]=0x4610C;
    if(!comboBRegisters(temporary,count))return false;
    for(unsigned off=0x5AC;off<=0x648;off+=4) {
        const auto index=(off-0x580)/4;
        registers[index]=off==0x5AC ? 0x64C08 : temporary[index]-0x6C000+0x160000;
    }
    return true;
}
// Five-word ClockParams ABI at the exact donor's combo-HDMI call boundary.
// Keep logical divisors separate from register encodings. Display WA
// 22010492432 halves only the DCO fraction for a 38.4 MHz reference.
struct HDMIClock {
    uint32_t cfg0 {},cfg1 {};
    static bool encode(const uint32_t *params,uint32_t refCode,HDMIClock &out) {
        out={};if(!params || refCode>2 || !params[0] || params[0]>0x3FF ||
            params[1]>0x7FFF || !params[3] || params[3]>255 || !params[4] || params[4]>3)return false;
        uint32_t p=0;
        switch(params[2]) {case 2:p=1;break;case 3:p=2;break;case 5:p=4;break;case 7:p=8;break;default:return false;}
        const auto fraction=refCode==2?(params[1]+1)/2:params[1];
        const uint32_t k[]={0,1,2,4};
        out.cfg0=params[0]|(fraction<<10);
        out.cfg1=(p<<2)|(k[params[4]]<<6);
        if(params[3]!=1)out.cfg1|=0x200|(params[3]<<10);
        return true;
    }
};
}
#endif
