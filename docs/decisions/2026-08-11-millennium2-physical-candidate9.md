# Millennium II Candidate 9 physical activation

Status: accepted physical result  
Build: `mga2-640x480x16-stockvxd9`  
Target: physical Matrox Millennium II `102B:051B`, subsystem `1200:102B`

## Change

Candidate 9 isolates the component boundary exposed by Candidate 8. It installs
the Velocity9x `MGAPDX64.DRV` but preserves the target machine's validated
stock Matrox `MGAPDX64.VXD`. The package contains `KEEPVXD.TAG`, not a copy of
the proprietary mini-VDD. During activation the two-boot guard stages its own
saved stock VXD beside the candidate DRV.

## Qualification

The mixed pair passed in 86Box, then passed on the physical machine at boot
counters 21 and 22 with clean 640x480x16 desktops and generic GDI `PASS`
results. The installed 9,726-byte candidate DRV and 75,704-byte stock VXD
matched their staged hashes, and the guard was disarmed after confirmation.

## Consequence

Candidate 9 cleared the first repeatable physical desktop and software-GDI
checkpoint. Later release-mode results are maintained in
`docs/specifications/matrox-millennium2-bringup.md`.
