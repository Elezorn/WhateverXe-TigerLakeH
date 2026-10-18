// Exact OS/donor ABI selection is independent of GPU family and board wiring.
#ifndef KERN_XELP_OS_HPP
#define KERN_XELP_OS_HPP
namespace XELP {
enum class OS { Unknown, Donor, BigSur, Monterey, Ventura, Sonoma, Sequoia, Tahoe };
inline OS kernelOS(const char *version) {
    static const char *const versions[] = {
        "Darwin Kernel Version 20.0.0: Sat Jun 13 17:58:16 PDT 2020; root:xnu-7090.0.0.111.5~1/RELEASE_X86_64",
        "Darwin Kernel Version 20.6.0: Thu Jul  6 22:12:47 PDT 2023; root:xnu-7195.141.49.702.12~1/RELEASE_X86_64",
        "Darwin Kernel Version 21.6.0: Mon Jun 24 00:56:10 PDT 2024; root:xnu-8020.240.18.709.2~1/RELEASE_X86_64",
        "Darwin Kernel Version 22.6.0: Tue Jul 15 08:22:28 PDT 2025; root:xnu-8796.141.3.713.2~2/RELEASE_X86_64",
        "Darwin Kernel Version 23.6.0: Tue Jul 21 21:56:37 PDT 2026; root:xnu-10063.141.1.713.39~1/RELEASE_X86_64",
        "Darwin Kernel Version 24.6.0: Sun Aug 23 20:30:56 PDT 2026; root:xnu-11417.140.69.712.69~1/RELEASE_X86_64",
        "Darwin Kernel Version 25.6.0: Tue Aug 18 17:46:40 PDT 2026; root:xnu-12377.161.15.700.19~2/RELEASE_X86_64",
    };
    if (!version) return OS::Unknown;
    for (unsigned i=0;i<sizeof(versions)/sizeof(versions[0]);++i) {
        const char *a=version,*b=versions[i];
        while (*a && *a==*b) {++a;++b;}
        if (*a==*b) return static_cast<OS>(i+1);
    }
    return OS::Unknown;
}
}
#endif
