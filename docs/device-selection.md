# Device selection and DeviceProperties

WhateverXe selects the GPU backend from a PCI device table. No CPU model or
machine-name setting is required.

## Automatic selection

By default, the physical Intel GPU device ID selects the backend. The implemented
IDs are Tiger Lake `9A40`, `9A49`, `9A60`, `9A68`, `9A70`, `9A78` and Alder Lake-N / Twin Lake
`46D0`–`46D4`, plus the ADL-S/HX and RPL-S/HX IDs below.

| Family | Accepted IDs |
| --- | --- |
| ADL-S / HX | `4680`, `4682`, `4688`, `468A`, `468B`, `4690`, `4692`, `4693` |
| RPL-S / HX | `A780`, `A781`, `A782`, `A783`, `A788`, `A789`, `A78A`, `A78B` |

All PCI revisions use the selected family backend. Hardware test coverage is
recorded separately and does not gate selection. A stepping-specific branch
requires a concrete documented difference or an observed failure. OS, driver
and hardware-resource checks still apply after selection.

Other entries in the table supply identification and display names only. A
listed ID does not imply an implemented backend or a tested machine. See the
[compatibility summary](../README.md#compatibility) for the current scope.

## Advanced override: device-id

To select an existing backend explicitly, set `device-id` on the integrated
GPU in OpenCore DeviceProperties. Its value must be **Data containing exactly
four little-endian bytes**, with a 16-bit device ID and two zero bytes.

For example, Data `D2460000` selects device ID `0x46D2`. This can select a backend
for an unlisted physical Intel ID, but it does not establish hardware compatibility.
The physical PCI identity and revision remain available to hardware checks.
EU counts, memory sizes and connector wiring are determined independently and
are not fabricated by the override.

Malformed values, zero/FFFF, unknown target IDs, identification-only targets and
non-Intel physical devices are rejected. An invalid override does not silently
fall back to the physical ID. Remove an obsolete ICL or other unsupported spoof
to use automatic selection. A restart is required after changing the selection.

The Display12 small-GT backends (TGL GT1, ADL-S/HX and RPL-S/HX) require
their physical ID without an override. Topology comes from the Gen12 slice,
DSS and EU-pair fuses; the supported four-bank capacity is checked separately.
HX IDs `4688/468B/A788/A78B` select the S-family backend, independent of CPU
branding. RKL remains identification-only: its absent PW_2, PLL/PHY mapping
and HTI resource reservations require a separate Display adapter.

Do not use an override merely to change the displayed GPU name.

## Display name

A user-supplied `model` property is preserved. Otherwise, the name associated
with the physical ID is used; when that ID is unknown, the selected ID's name is
used instead. Names are descriptive labels and do not control functionality or
EU counts. SKU and system configuration can affect Intel's marketing name.
Metal may still report the Apple driver's internal name.

PCI ID/family groupings were checked against Linux v6.12's i915 tables; individual
marketing names are not a claim of independently verified SKU branding.

## Display limits

Connector labels alone do not identify the electrical path. Firmware VBT routing
must match the implemented path; ambiguous or cross-routed AUX layouts are excluded.

- TGL: fixed Combo A/B DP and existing HDMI, eDP and USB-C paths share display
  operations. Additional fixed-DP layouts have CPU coverage, not hardware coverage.
- ADL-S/HX and RPL-S/HX: Combo PHY A–C are implemented; PHY D/E and Type-C/TBT-managed
  routes are not. A USB-C socket wired as fixed DP is different from a Type-C-managed
  path. Firmware-derived DDI A eDP is implemented but not hardware-tested.
- TGL and ADL-S panel handling supports the qualified PPS0/PWM0 path. Other PPS
  controllers and AUX-controlled backlights need additional work. Panel timing
  comes from VBT, firmware state and explicit properties. High-rate link selection
  has CPU coverage; not every panel or link rate has been tested on hardware.
- TGL panels requesting HOBL use standard signal settings; the HOBL power-saving
  optimization is disabled until its retry behavior is supported.
- Display 13: implemented Combo HDMI/DP paths do not imply USB-C/eDP support.

The Display12 small-GT implementation accepts one slice, up to two DSS,
16/24/32 EUs, four L3 banks and the supported media engines. Different resource
layouts need additional implementation. These checks are independent of whether
an individual PCI revision has been tested.
