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

Tiger Lake laptops need a PNLF ACPI device under the actual iGPU namespace,
with `_HID=APP0002`, `_CID=backlight`, and `_UID=19`. Adapt the SSDT scope to
the firmware's ACPI path; the IORegistry name `IGPU` may be a runtime rename.
WEX supplies a missing generic 16-bit brightness profile for that PNLF device
when the internal framebuffer reports a 0–65535 backlight range. Existing
Apple/custom profiles are preserved. PNLF registration alone does not prove
that brightness reaches the panel; PWM behavior still depends on the display
backend. Mini PCs without an internal panel do not need PNLF.

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
| `-wex-nopanel` | Disable internal-panel detection on Tiger Lake (external HDMI on DDI A remains available). Use for external-display-only operation; remove it to test an internal panel. Alder Lake-N / Twin Lake currently builds an external-display topology automatically and does not use this flag. |

No family-selection or per-feature enable arguments are required for
Alder Lake-N / Twin Lake.

### Optional boot arguments

| Argument | Purpose |
| --- | --- |
| `-wex-display-trace` | Enable detailed Tiger Lake display-transition and PHY diagnostics. Disabled by default. |
| `gfxrst=` | Boot framebuffer handoff: 0 automatic (default), 1 reset, 2 copy, 3 clear, 4 disable. |
| `-wexdbg` | Enable debug logging in a debug build. |
| `-wexoff` | Disable WhateverXe. |
| `-wexbeta` | Bypass the plugin's OS-version check. Exact kernel/driver checks still apply. |
| `-wex-tgl-off` | Disable the Tiger Lake path. |
| `-wex-noaccel` | Tiger Lake framebuffer-only mode; blocks accelerator probing. |

## Tested features and known issues

Tiger Lake and Alder Lake-N have been tested across all six OS families with
dual external 1920×1080 at 60 Hz output, Metal/MPS, OpenGL, OpenCL, hardware
H.264 encode/decode, hardware HEVC decode, display sleep/wake, Safari hardware
video decode and Widevine playback. The common TGLCompat build has also passed
installation and functional checks on Alder Lake-N across all six OS families.
These checks do not establish compatibility with every codec, application or GPU
in the table.

Experimental Tiger Lake laptop support has been verified on Big Sur with an
internal 1366×768 eDP panel and simultaneous 1920×1080 at 60 Hz output through
a USB-C DP-alt-to-HDMI adapter. Connector selection uses firmware VBT data;
the current DP-alt backend admits TC1–TC4; TC1 and TC3 have been tested. This does not establish support for
every laptop, USB-C port, adapter or dock.
Internal-panel brightness through the standard slider and keyboard controls,
and three simultaneous displays (the internal panel plus two external outputs)
have also been verified on the tested Big Sur laptop configuration.
Internal-panel display sleep/wake has been verified with HDMI connected;
this does not qualify system sleep or USB-C wake.

### Known issues and limits

- VA image conversion can crash with a null-pointer access.
- DisplayPort reconnection can cause a black screen or system hang, including
  on Sequoia and Tahoe.
- Occasional Safari frame drops have been observed.
- Four simultaneous displays are outside the current three-pipe driver scope.
- System sleep, higher resolutions and other
  connector layouts require further validation. The experimental DP-alt path
  currently keeps TC-cold blocked for the boot session; power management,
  reconnect and sleep/wake on this path need further validation.

## Roadmap

### Planned

- Alder Lake and Raptor Lake S/H/P/U support
- Additional USB-C ports, adapters and dock configurations
- Broader internal-display and laptop power-management support

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
