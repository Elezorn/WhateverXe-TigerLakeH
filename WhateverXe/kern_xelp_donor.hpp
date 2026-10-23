// CPU portions of fixes for the shared c004fc1a donor. Caller owns admission.
#ifndef KERN_XELP_DONOR_HPP
#define KERN_XELP_DONOR_HPP
#include <stdint.h>
#include <stddef.h>
namespace XELPDonor {
inline bool displayPoolRequest(uint32_t enabled,uint32_t bytes) {
    return enabled && bytes && !(bytes&4095) && bytes<=0x4000000;
}
struct Poll {uint32_t value {},elapsedUs {},reads {};};
// hwSetPowerWellStatePG checks PG2/3/4 once, 1 us after the state ACK.
// Linux gen9_wait_for_power_well_fuses polls the actual fuse status instead.
// Match the complete donor read/test instructions, including the call target.
constexpr uint32_t fuseReadSites[] {0x39D,0x53F,0x6D3};
inline bool powerWellFuseSites(const uint8_t *function,size_t size) {
    static const uint8_t expected[][14] {
        {0xBE,0x00,0x20,0x04,0x00,0xE8,0x5D,0x29,0x00,0x00,0x0F,0xBA,0xE0,0x19},
        {0xBE,0x00,0x20,0x04,0x00,0xE8,0xBB,0x27,0x00,0x00,0x0F,0xBA,0xE0,0x18},
        {0xBE,0x00,0x20,0x04,0x00,0xE8,0x27,0x26,0x00,0x00,0x0F,0xBA,0xE0,0x17}
    };
    if(!function || size<fuseReadSites[2]+14)return false;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<14;++j)
        if(function[fuseReadSites[i]+j]!=expected[i][j])return false;
    return true;
}
inline uint32_t powerWellFuseMask(uintptr_t function,uintptr_t caller) {
    if(function && caller>=function)for(unsigned i=0;i<3;++i)
        if(caller-function==fuseReadSites[i]+10)return 1U<<(25-i);
    return 0;
}
template<class IO> Poll waitPowerWellFuse(IO &io,uint32_t initial,uint32_t mask) {
    Poll r {initial,0,0};
    while(mask && r.value!=UINT32_MAX && !(r.value&mask) && r.elapsedUs<1000) {
        io.delayUs(10);r.elapsedUs+=10;r.value=io.readStatus();++r.reads;
    }
    return r; // Never fabricate readiness or change a power request/refcount.
}
template<class IO> Poll waitEDIDReady(IO &io,uint32_t initial) {
    Poll r {initial,0,0};
    while(!(r.value&0xC00) && r.elapsedUs<50000) {
        io.delayUs(10);r.elapsedUs+=10;r.value=io.readStatus();++r.reads;
    }
    return r;
}
inline bool skipHDMIDPLanes(bool selected,bool exactCaller,bool native,uint32_t available,uint32_t lanes) {
    return selected && exactCaller && !native && !available && !lanes;
}
}
#endif
