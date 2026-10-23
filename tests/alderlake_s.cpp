#include "kern_alderlake_s.hpp"
#include <cassert>
#include <cstdio>
#include <climits>
#include <initializer_list>
using namespace AlderLakeS;
int main() {
    uint32_t fuses[]={1,3,0,0xF0,0xE00FA};GT gt;
    assert(GT::decode(0x46908086,0xC,fuses,gt) && gt.eus==32 && gt.nativeSubslices==4 && gt.l3Banks==4 && gt.urbKiB==256);
    for(unsigned i=0;i<5;++i)for(unsigned bit=0;bit<32;++bit) {
        fuses[i]^=1U<<bit;assert(!GT::decode(0x46908086,0xC,fuses,gt) && !gt.eus);fuses[i]^=1U<<bit;
    }
    assert(!GT::decode(0x46D18086,0,fuses,gt));assert(!GT::decode(0x46908086,0,fuses,gt));
    assert(!GT::decode(0x46908086,0xC,nullptr,gt));
    const uint32_t physical[]={0,3,4,5,6},dvo[]={10,1,8,9,11};
    const uint32_t expectedPin[]={1,9,10,11,12};
    const uint32_t expectedHPD[]={0x10000,0x1000000,0x2000000,0x4000000,0x8000000};
    for(unsigned n=0;n<5;++n) {
        assert(XELP::VBT::ddi(XELP::Family::AlderLakeS,dvo[n])==physical[n]);
        Port port;assert(Port::resolve(physical[n],port));
        assert(port.phy==n && port.buffer==0x64000+physical[n]*256);
        assert(port.hpd==expectedHPD[n] && port.connected(expectedHPD[n]));
        assert(!port.connected(UINT32_MAX) && !port.connected(~expectedHPD[n]));
        assert(gmbus(n+1)==expectedPin[n]);
        assert(port.clock==(n<3?0x164280U:0x1642BCU));
    }
    for(auto d:{1U,2U,7U,8U,9U,31U,32U,UINT32_MAX}) {
        Port port;assert(!Port::resolve(d,port) && !port.buffer && !port.hpd);
    }
    for(auto pin:{0U,6U,7U,8U,9U,UINT32_MAX})assert(!gmbus(pin));
    // Recorded firmware topology: USB-C A, HDMI TC1, DP TC2, unwired TC3.
    XELP::VBT::Ports firmware;firmware.status=XELP::VBT::Status::Valid;firmware.present=0x39;
    firmware.port[0]={0x60d6,10,1,0x40,0xc0,0};
    firmware.port[3]={0x60d2,1,2,0,0xc0,0};
    firmware.port[4]={0x60d6,8,3,0x20,0xc0,0};
    firmware.port[5]={0x68c6,9,0,0x30,0xc0,0};
    assert(DP::qualified(firmware));
    const uint32_t dpConfig[]={4,10,1,0,0x400,0x201};
    assert(DP::hotplugMode(true,dpConfig,dpConfig,0x100,0x100,4)==1);
    assert(DP::hotplugMode(false,dpConfig,dpConfig,0x100,0x100,4)==4);
    assert(DP::hotplugMode(true,dpConfig,nullptr,0x100,0x100,4)==4);
    assert(DP::hotplugMode(true,dpConfig,dpConfig,0x101,0x100,4)==4);
    assert(DP::hotplugMode(true,dpConfig,dpConfig,0,0,4)==4);
    for(unsigned m:{0u,1u,2u,3u})assert(DP::hotplugMode(true,dpConfig,dpConfig,0x100,0x100,m)==m);
    for(unsigned n=0;n<6;++n){uint32_t bad[6];memcpy(bad,dpConfig,sizeof(bad));bad[n]^=1;
        assert(DP::hotplugMode(true,bad,bad,0x100,0x100,4)==4);}

    for(unsigned i=0;i<6;++i){auto bad=firmware;reinterpret_cast<uint32_t *>(&bad.port[4])[i]^=1;assert(!DP::qualified(bad));}
    auto dupe=firmware;dupe.port[5].aux=0x20;assert(!DP::qualified(dupe));
    auto h=HDMI::fromFirmware(firmware);assert(h.valid && h.selected==8 && h.pin[3]==9);
    alignas(8) uint8_t platform[0x1C0] {};platform[0x12]=9;
    for(unsigned d=0;d<9;++d) {
        const uint32_t c[]={d,d>=3?d+6:d+1,1,d>=3?1U:0U,0x400,0x201};
        memcpy(platform+0x20+24*d,c,sizeof(c));
    }
    assert(DP::apply(platform,sizeof(platform)) && DP::matches(reinterpret_cast<uint32_t *>(platform+0x80)));
    assert(!DP::apply(platform,sizeof(platform)));
    reinterpret_cast<uint32_t *>(platform+0x80)[3]=1;
    uint8_t before[sizeof(platform)];memcpy(before,platform,sizeof(before));
    assert(h.apply(platform,sizeof(platform)));
    assert(h.matches(reinterpret_cast<uint32_t *>(platform+0x68)) && h.usesDDC(9) && !h.usesDDC(2));
    assert(!memcmp(platform,before,0x68) && !memcmp(platform+0x80,before+0x80,sizeof(platform)-0x80));
    memcpy(platform,before,sizeof(platform));platform[0x68+12]=0;
    memcpy(before,platform,sizeof(before));assert(!h.apply(platform,sizeof(platform)));
    assert(!memcmp(before,platform,sizeof(platform))); // no partial mutation on a wrong donor
    auto bad=firmware;bad.port[3].ddc=0;assert(!HDMI::fromFirmware(bad).valid);
    bad=firmware;bad.port[4]=firmware.port[3];assert(!HDMI::fromFirmware(bad).valid);
    bad=firmware;bad.port[3].capabilities=1;h=HDMI::fromFirmware(bad);assert(h.valid && !h.selected);
    assert(!HDMI::fromFirmware({}).valid);
    // Physical TC1 still uses Combo B's clock selector, not DKL PLL1.
    Port hdmi;assert(Port::resolve(3,hdmi));
    assert(hdmi.clock==0x164280 && hdmi.clockShift==2 && hdmi.clockOff==0x800);
    // The 148.5 MHz firmware state, encoded from logical divisors rather than
    // copied into the implementation as a fixed display mode.
    const uint32_t params[]={464,2048,2,3,2};HDMIClock clock;
    assert(HDMIClock::encode(params,2,clock) && clock.cfg0==0x1001D0 && clock.cfg1==0xE84);
    assert(HDMIClock::encode(params,1,clock) && clock.cfg0==0x2001D0);
    for(unsigned word=0;word<5;++word) {
        uint32_t bad[5];memcpy(bad,params,sizeof(bad));bad[word]=UINT32_MAX;
        assert(!HDMIClock::encode(bad,2,clock) && !clock.cfg0 && !clock.cfg1);
    }
    assert(!HDMIClock::encode(nullptr,2,clock) && !HDMIClock::encode(params,3,clock));
    PLL pll;assert(PLL::resolve(2,pll) && pll.cfg0==0x164294 && pll.enable==0x46018);
    assert(PLL::resolve(3,pll) && pll.cfg0==0x1642C0 && pll.enable==0x46030);
    assert(!PLL::resolve(4,pll) && !pll.enable);
    uint32_t registers[0x128/4] {};registers[4]=0x64300;registers[(0x64C-0x580)/4]=0x4610C;
    uint32_t saved[0x128/4];memcpy(saved,registers,sizeof(saved));
    assert(comboBRegisters(registers,sizeof(registers)/4));
    assert(registers[(0x5AC-0x580)/4]==0x64C04 && registers[(0x5B8-0x580)/4]==0x6C10C);
    assert(registers[(0x648-0x580)/4]==0x6C324 && registers[4]==0x64300);
    assert(!memcmp(registers,saved,0x5AC-0x580));
    assert(!memcmp(registers+(0x64C-0x580)/4,saved+(0x64C-0x580)/4,0x6A8-0x64C));
    assert(!comboBRegisters(registers,sizeof(registers)/4)); // no repeated or foreign table
    memcpy(registers,saved,sizeof(saved));registers[4]=0x64400;
    assert(!comboBRegisters(registers,sizeof(registers)/4));
    puts("PASS ADL-S physical DDI/Combo PHY/HPD/PLL/DDC coordinates and ambiguous HDMI rejection");
}
