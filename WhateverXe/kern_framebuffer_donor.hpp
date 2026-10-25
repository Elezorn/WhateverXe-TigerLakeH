// Object contracts of the c004fc1a framebuffer donor, independent of Display IP.
#ifndef KERN_FRAMEBUFFER_DONOR_HPP
#define KERN_FRAMEBUFFER_DONOR_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace FramebufferDonor {
template<class T> T field(const void *object,size_t offset) {
    T value {};if(object)memcpy(&value,static_cast<const uint8_t *>(object)+offset,sizeof(value));return value;
}
// Native FB init leaves borrowed MMIO-manager fields empty. Validate the whole
// controller-owned graph before changing any reference. Never retain/release a
// borrowed manager, overwrite a foreign owner, or partly bind a rejected graph.
template<class Objects> bool bindManagers(Objects &objects,void *fb,void *controller,uint32_t index) {
    if(index>=3 || objects.size(fb)<0x4B90 || objects.size(controller)<0x1C20 ||
       field<void *>(fb,0x1D0)!=controller || field<uint32_t>(fb,0x1DC)!=index)return false;
    auto manager=field<void *>(controller,0xC40);
    if(objects.size(manager)<0x68 || !field<uintptr_t>(manager,0x50) || field<uint32_t>(manager,0x60)<=0x60404)return false;
    const size_t arrays[] {0xF58,0xF28},identities[] {0x7C,0x1C},slots[] {0x90,0x28};
    const unsigned counts[] {15,6},minimum[] {0x1C8,0x58};
    for(unsigned group=0;group<2;++group)for(unsigned i=0;i<counts[group];++i) {
        auto object=field<void *>(controller,arrays[group]+sizeof(void *)*i);
        if(objects.size(object)<minimum[group] || field<uint32_t>(object,identities[group])!=i)return false;
        auto stored=field<void *>(object,slots[group]);
        if(stored && stored!=manager)return false;
    }
    auto stored=field<void *>(fb,0x4A40);if(stored && stored!=manager)return false;
    for(unsigned group=0;group<2;++group)for(unsigned i=0;i<counts[group];++i) {
        auto object=field<uint8_t *>(controller,arrays[group]+sizeof(void *)*i);
        memcpy(object+slots[group],&manager,sizeof(manager));
    }
    memcpy(static_cast<uint8_t *>(fb)+0x4A40,&manager,sizeof(manager));return true;
}
}
#endif
