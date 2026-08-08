# Phase 3 mode matrix

Status: host-audited; guest mode-matrix test pending  
Recorded: 2026-08-09

The next candidate expands the guest-visible mode table while preserving the
proven `active-640-vdd1` milestone and its Standard-VGA recovery path.

Advertised modes, all at 60 Hz:

- 640x480, 800x600, and 1024x768 at 8 bpp indexed colour;
- 640x480, 800x600, and 1024x768 at 16 bpp RGB 5:6:5.

The display driver asks the Windows master VDD for the boot-time display
configuration, validates it against this exact table, then selects the matching
VBE linear-framebuffer mode, pitch, visible-byte count, GDI metrics, and palette
contract. Unsupported or unavailable configuration falls back to 640x480x8.

Dynamic in-session switching remains disabled. Each Display Properties change
must be followed by a full reboot, serial checkpoint review, and one GDI/pixel/
software-cursor test. Acceleration, 24/32-bpp modes, DDC, and hardware cursor
support remain out of scope.

The `phase3-matrix-v1` package passed the Open Watcom and MSVC host suites,
tree check, strict-warning builds, NE/LE/PE image audits, DIB/VDD disassembly
checks, six accepted-mode checks, and rejected 1280x1024x8 and 640x480x24
checks. Its package `SHA256.TXT` digest is
`A49153FB0E945B5E6D37B3A288415832B4908601C58C87AC2DC461E350F8CA55`.
