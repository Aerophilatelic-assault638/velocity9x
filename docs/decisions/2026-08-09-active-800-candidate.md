# Fixed 800x600x8 bring-up candidate

Status: superseded before guest testing  
Recorded: 2026-08-09

The proven `active-640-vdd1` package is preserved byte-for-byte under
`build/milestones/active-640-vdd1`. This planned isolated experiment changed
only the fixed display contract to 800x600x8 at 60 Hz:

- VBE linear-framebuffer mode changes from `0x101` to `0x103`;
- pitch changes from 640 to 800 bytes;
- visible framebuffer registration changes from 307,200 to 480,000 bytes;
- GDI metric mappings use the Windows 98 DDK's 800x600 values;
- the INF advertises only 800x600x8, retaining 640x480 standard VGA recovery;
- dynamic mode switching and all acceleration remain disabled.

The `active-800-v1` host build was not installed in the guest. At the user's
request it was superseded by the Phase 3 six-mode matrix so the next guest test
can cover 640x480, 800x600, and 1024x768 at both 8 and 16 bpp.
