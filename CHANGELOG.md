# Changelog

## 0.3.0

Xe-LP graphics compatibility for the configurations described in the [README](https://github.com/b00t0x/WhateverXe#compatibility).

- One WEX binary supports Big Sur through Tahoe on the implemented Tiger Lake and Alder Lake-N / Twin Lake backends.
- Graphics installation uses the signed [WhateverXe Graphics pkg](https://github.com/b00t0x/WhateverXe-Graphics/releases), with no Xe System-volume patch or OCLP dependency.
- Includes cross-version OS/driver ABI adaptations, OpenCL compatibility and late semaphore mapping invalidation fixes.
- Supports signed Data graphics bundles with Mod AMFIPass, without `amfi=0x80` or `ipc_control_port_options=0`.
- Preserve debug information in the bundled dSYM before stripping the kext.
- Tiger Lake and Alder Lake-N have completed the six-OS functional checks described in the README. Known issues and hardware coverage limits still apply.

**RELEASE** is for normal use; **DEBUG** is for troubleshooting. Both archives include WhateverXe.kext, matching debug symbols, English/Japanese instructions and the license. SHA256SUMS contains checksums for both archives.

Requires Lilu, the separately installed WhateverXe Graphics pkg, and the [AMFIPass Mod build](https://github.com/b00t0x/WhateverXe-Graphics/blob/main/extras/AMFIPass-v1.4.1-Mod-RELEASE.zip). See the README for SIP, OpenCore configuration and migration from an older Xe System patch.

## 0.2.3

Fix DisplayPort startup and hotplug issues.

- Fix a kernel panic when booting with only DisplayPort connected.
- Fix DisplayPort detection when first connected after an HDMI-only boot.

Use RELEASE for normal use and DEBUG for troubleshooting. The archives include matching debug symbols and instructions.

The text-rendering fix is provided separately by the Xe root patch in [OpenCore Legacy Patcher 3.0.0-rc.3+m.2](https://github.com/b00t0x/OpenCore-Legacy-Patcher/releases/tag/3.0.0-rc.3%2Bm.2).

## 0.2.2

Xe-LP graphics compatibility for the configurations described in the [README](https://github.com/b00t0x/WhateverXe#compatibility).

- **RELEASE**: normal use.
- **DEBUG**: troubleshooting with debug logging.
- Both archives include WhateverXe.kext, matching debug symbols, English/Japanese instructions and the license.
- SHA256SUMS contains checksums for both archives.

Requires Lilu, the Xe root patch from the [OpenCore Legacy Patcher fork](https://github.com/b00t0x/OpenCore-Legacy-Patcher), and the AMFIPass Mod build linked in the README. Root-patch components are installed by OCLP.
