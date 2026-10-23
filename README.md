# WhateverXe

[日本語](README_ja.md)

WhateverXe is a Lilu plugin that adapts Apple graphics drivers for selected
Intel Xe-LP integrated GPUs. It is derived from WhateverGreen.

This project is experimental and requires the signed graphics package from
[WhateverXe Graphics](https://github.com/b00t0x/WhateverXe-Graphics). It installs
the graphics stack under `/Library` and updates the Auxiliary Kernel Collection
(AuxKC), without patching System files or snapshots. WEX and the graphics package
have independent release versions.

## Compatibility

One WEX binary and one graphics pkg cover the following macOS versions:

| macOS | Required version |
| --- | --- |
| Big Sur | 11.7.11 |
| Monterey | 12.7.6 |
| Ventura | 13.7.8 |
| Sonoma | 14.8.9 |
| Sequoia | 15.8.x |
| Tahoe | 26.7 or newer |

Big Sur through Sonoma require the qualified kernel identities. Sequoia and
Tahoe accept servicing updates within their OS family while retaining runtime
instruction and ABI checks. Future updates are not prevalidated.

WEX selects the OS ABI and GPU backend independently. OS-specific graphics
payloads and the common AppleIntelTGLCompat provider are installed by WhateverXe
Graphics; GPU generation and connector routing are handled by WEX.

| GPU family | Device IDs with an implemented backend |
| --- | --- |
| Tiger Lake | `9A40`, `9A49`, `9A78` |
| Alder Lake-N / Twin Lake | `46D0`–`46D4` |

Support varies by system and connector layout. The table includes implemented
backends, not a guarantee for every machine. See [device selection](docs/device-selection.md)
for detailed selection conditions and overrides.

## Required components

Download the **RELEASE** ZIP from [Releases](https://github.com/b00t0x/WhateverXe/releases).
The DEBUG ZIP is for troubleshooting. Each ZIP contains only `WhateverXe.kext`
and its matching dSYM. `SHA256SUMS` contains checksums for both archives.
See [CHANGELOG](CHANGELOG.md) for release notes.

| Component | Role |
| --- | --- |
| Lilu | Runtime required by WhateverXe |
| WhateverXe.kext | Load through OpenCore after Lilu |
| [WhateverXe Graphics pkg](https://github.com/b00t0x/WhateverXe-Graphics/releases) | Install the signed graphics stack into `/Library` and rebuild AuxKC |
| [AMFIPass v1.4.1 Mod](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/extras/AMFIPass-v1.4.1-Mod-RELEASE.zip) | Load through OpenCore for the modified graphics stack |

Configure SIP with `csr-active-config=03080000` (value `0x803`). Full SIP is not
supported. With the signed graphics package and linked Mod AMFIPass,
`amfi=0x80` and `ipc_control_port_options=0` are not required.

Tahoe requires `CoreMedia / AllowOnePassMetalScaling` to be disabled; the
WhateverXe Graphics installer applies this setting.

Boot the target OS with WEX **0.3.0 or newer** before installing the graphics
pkg. If an older Xe System patch is installed, restore it using its original
recovery procedure first. The pkg does not change EFI, System files, snapshots
or SIP settings. See [installation and removal](https://github.com/b00t0x/WhateverXe-Graphics#install)
for the package instructions and the standalone uninstall script.

Extract `AMFIPass.kext` from the linked **Mod** archive, place it in
`EFI/OC/Kexts`, and enable it in `Kernel > Add`. Replace an existing AMFIPass
with this Mod build; do not load both copies. Upstream AMFIPass does not support
the Mod signing chain.

If you use OCLP for other root patches, use the
[custom OCLP build](https://github.com/b00t0x/OpenCore-Legacy-Patcher), which uses
the same Mod signing chain. Xe graphics installation itself uses WhateverXe
Graphics independently of OCLP.

WhateverGreen and WhateverXe must not be enabled together, including in
mixed-GPU configurations.

## Configuration

GPU selection is automatic from the PCI device ID. Connector routing is read
from firmware VBT where supported; fixed HDMI and fixed DisplayPort layouts
normally need no manual wiring properties. Advanced `device-id` overrides are
described in [device selection](docs/device-selection.md).

On supported GPUs, WhateverXe also normalizes the detected Intel Management
Engine device name to `IMEI`, as WhateverGreen does. A separate ACPI rename
from `HECI` or `MEI` to `IMEI` is unnecessary.

### Internal-panel brightness

Add `SSDT-PNLF` to OpenCore to enable internal-display brightness control.

### DVMT

Set **DVMT Pre-Allocated to 96MB** in the BIOS/UEFI settings where possible.

If it is fixed at **60MB**, add `-igfxdvmt` to `boot-args` and set both properties
below on the iGPU under OpenCore's `DeviceProperties > Add`
(usually `PciRoot(0x0)/Pci(0x2,0x0)`).

| Property | Type | Value (hex) |
| --- | --- | --- |
| `framebuffer-patch-enable` | Data | `01000000` |
| `framebuffer-stolenmem` | Data | `0000C003` |

These Data values use little-endian byte order. `-igfxdvmt` corrects the capacity
calculation, while the properties limit the display memory pool to fit within
60MB. Neither setting increases the memory allocated by the firmware.
With 96MB, these DVMT-specific settings are unnecessary.

### Required boot arguments

Add the following to OpenCore's `boot-args`:

```text
-allow3d -disablegfxfirmware
```

| Argument | Purpose |
| --- | --- |
| `-allow3d` | Enable the Apple driver's 3D path. |
| `-disablegfxfirmware` | Disable the Apple driver's graphics-firmware loading path for the current backend. |

The `-wex-nopanel` boot argument has been removed and is no longer needed.
WEX automatically handles systems with or without an internal display.

No family-selection or per-feature enable arguments are required for
Alder Lake-N / Twin Lake.

The supported Display 13 backend selects Y-tiled scanout automatically;
`AAPL,GfxYTile` is not required in OpenCore. Existing values are preserved.
Do not use the Apple driver's `-yTileDisable` argument with this backend:
its X-tiled scanout path is unsupported. Display 12 tiling selection is unchanged.

## Tested features and known issues

| Hardware | Verified features |
| --- | --- |
| Tiger Lake / Alder Lake-N | Big Sur–Tahoe: dual 1920×1080 at 60 Hz output, Metal/MPS, OpenGL, OpenCL, hardware H.264 encode/decode, HEVC decode, display sleep/wake, Safari video decoding and Widevine playback |
| Tiger Lake laptops | Big Sur–Tahoe: internal 1366×768 eDP display, brightness control, HDMI and USB-C DisplayPort Alt Mode, up to three simultaneous displays; HDMI and both USB-C ports reconnected successfully on Tahoe |
| Alder Lake-N | Big Sur/Tahoe: automatic Y tiling without an EFI `AAPL,GfxYTile` setting |

These results cover tested systems, not every GPU, display, adapter or dock.

### Known issues and limits

- Some image-conversion operations can crash the application.
- Reconnecting a monitor to a dedicated DisplayPort connector can leave the
  screen black or freeze the computer. USB-C DisplayPort Alt Mode passed the
  reconnection checks above.
- Safari video playback may occasionally drop frames.
- Up to three displays can be used simultaneously; a fourth display will not
  receive an image.
- Full system sleep/wake and resolutions above those listed above are unverified.
  USB-C display output currently prevents part of the display hardware from
  entering a low-power state.

## Roadmap

### Planned

- Alder Lake and Raptor Lake S/H/P/U support. Development support for Alder Lake-S
  `4690` has been tested from Big Sur through Tahoe with Metal and OpenGL
  acceleration and three simultaneous displays (two 1080p60 and one 720p60).
  It requires unreleased companion graphics changes and is not supported by the
  published graphics pkg. Fixed DP reconnection has been tested on Big Sur, but
  initial display output and reconnection can be delayed. HDMI/fixed-DP routing is derived
  from VBT rather than a board or connector preset, within the current Combo
  PHY A–C backend. Ambiguous routes, cross-routed AUX, Type-C/TBT capability
  and PHY D/E are excluded. GPU admission remains limited to `4690` revision
  `0C` with the qualified fuse configuration. Other port layouts have CPU-test
  coverage but are not hardware-qualified. A USB-C connector exposed by VBT
  as fixed DP on Combo PHY A has also been tested. This does not qualify
  Type-C/TBT-managed routes. Sleep/wake remains unqualified.
- Display 13 USB-C and eDP support

### Not Planned

- Xe-LPG support
- Xe2 and later architectures

## Build and test

Use macOS with Xcode command-line tools, Python 3.9 or newer, MacKernelSDK and
Lilu's development headers/library. CI uses the Lilu
[1.7.2 release tag](https://github.com/acidanthera/Lilu/tree/1.7.2),
which includes the Tahoe kernel definition. Place `MacKernelSDK/` and `Lilu.kext/` in the
repository root, or pass their paths with `--sdk` and `--lilu`.

```sh
python3 tools/build.py --release
python3 tools/test.py
```

WhateverXe and AppleIntelTGLCompat kexts and a source manifest are written to
`build/`. TGLCompat source and shared headers are maintained here; its signed
binary is distributed through WhateverXe Graphics. The WEX release ZIP contains
only the OpenCore plugin, not the graphics package. Optional donor replay tests
accept `--donor /path/to/AppleIntelTGLGraphicsFramebuffer` and
`--accelerator /path/to/AppleIntelTGLGraphics`, using the exact binaries accepted
by each test. CPU tests do not establish physical GPU compatibility.
WhateverGreen source is not a build dependency.

## License and credits

WhateverXe retains WhateverGreen's copyright notices and BSD-3-Clause license.
See [LICENSE.txt](LICENSE.txt). Lilu and MacKernelSDK are separate dependencies.
