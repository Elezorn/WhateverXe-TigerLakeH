#include "kern_display13_tc_mode.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace Display13TC;
struct IO {
    Port port;std::map<uint32_t,uint32_t> regs;
    Display13Power::State power;bool ready {true},adopt {},quiet {true},nativeFail {},auxOK {true};
    unsigned native {},time {};std::vector<unsigned> sequence;
    IO(unsigned d){assert(Port::resolve(d,port));regs={{port.ready,4},{port.buffer,0},{0x45404,0},{0x45454,0},{0x45444,0},{0x42000,0x8e380000}};assert(Display13Power::initialize(*this,power,Display13Power::nativeDomains(0x7b)));}
    bool readable(uint32_t r){return regs.count(r);}
    uint32_t read(uint32_t r){assert(readable(r));return regs[r];}
    void write(uint32_t r,uint32_t v) {
        assert(readable(r));sequence.push_back(r);
        if(r==0x45404 || r==0x45454 || r==0x45444){auto mask=r==0x45404?0xa80aU:0x2aaaU;regs[r]=(v&~(mask>>1))|((v&mask)>>1);}
        else regs[r]=v;
    }
    void delayUs(unsigned t){time+=t;}
    bool admissionReady(){return ready;}
    unsigned nativeMode(){return native;}
    unsigned nativeSetMode(unsigned v){sequence.push_back(0x550);if(nativeFail)return 1;native=v;return 0;}
    bool tcAuxSupported(){return true;}
    bool prepareTCAux(unsigned){return true;}
    bool tcAuxHealthy(unsigned){return auxOK;}
    Display13Power::Result acquireDomains(Display13Power::Lease &l,uint32_t d){return Display13Power::acquire(*this,power,l,d);}
    Display13Power::Result releaseDomains(Display13Power::Lease &l){return Display13Power::release(*this,power,l);}
    bool powerHeld(uintptr_t owner,uintptr_t object,unsigned d){return owner==1 && object==2 && d==port.ddi && Display13Power::held(*this,power,Mode::domains(d));}
    bool inheritedVerified(uintptr_t,uintptr_t,unsigned){return adopt;}
    bool quiescent(uintptr_t,uintptr_t,unsigned){return quiet;}
};
int main() {
    for(unsigned d=3;d<=6;++d) {
        IO io(d);Mode m;assert(m.connect(io,1,2,d).status==Status::Complete);
        assert(m.phase==Mode::Phase::Active && io.native==4 && m.ownership.active && m.lease.owner==&io.power);
        assert(io.sequence.back()==0x550 && io.regs[io.port.buffer]&64);
        auto count=io.sequence.size();assert(m.connect(io,1,2,d).status==Status::Complete && io.sequence.size()==count);
        assert(m.connect(io,1,3,d).status==Status::InUse && io.sequence.size()==count);
        io.quiet=false;assert(m.disconnect(io,1,2,d).status==Status::InUse && m.disconnectPending && m.lease.owner);
        assert(m.connect(io,1,2,d).status==Status::Complete && !m.disconnectPending);
        io.quiet=true;io.regs[io.port.buffer]|=1U<<31;
        assert(m.disconnect(io,1,2,d).status==Status::InUse && m.lease.owner);
        io.regs[io.port.buffer]&=~(1U<<31);
        assert(m.disconnect(io,1,2,d).status==Status::Complete);
        assert(m.phase==Mode::Phase::Idle && !m.ownership.owner && !m.lease.owner && !io.native);
        assert(m.connect(io,1,2,d).status==Status::Complete);assert(m.disconnect(io,1,2,d).status==Status::Complete);
    }
    {IO io(3);Mode m;io.regs[io.port.buffer]=0x80000040;io.adopt=true;
     assert(m.connect(io,1,2,3).status==Status::Complete);assert(io.regs[io.port.buffer]==0x80000040);}
    {IO io(3);Mode m;io.regs[io.port.buffer]=0x80000040;
     assert(m.connect(io,1,2,3).status==Status::NeedsAdoption && m.phase==Mode::Phase::Cleanup && m.lease.owner);
     assert(!io.native && m.connect(io,1,2,3).status==Status::InUse);}
    {IO io(3);Mode m;io.nativeFail=true;
     assert(m.connect(io,1,2,3).status==Status::Readback && m.ownership.owner && m.lease.owner);
     io.nativeFail=false;assert(m.disconnect(io,1,2,3).status==Status::Complete);}
    {IO io(3);Mode m;io.auxOK=false;
     assert(m.connect(io,1,2,3).status==Status::NoPower && !m.ownership.owner && !io.native);}
    {IO io(3);Mode m;io.ready=false;assert(m.connect(io,1,2,3).status==Status::NotReady && io.sequence.empty());}
    puts("PASS Display13 legacy TC mode lifecycle: power/ownership/native order, adoption, retries and pending cleanup");
}
