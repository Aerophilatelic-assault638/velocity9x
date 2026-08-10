# Windows 98SE active-package gate

This directory contains the source for the first quarantined activation INF.
Its presence does not mean the driver has passed guest activation or recovery.
Only `scripts/build-active-package.ps1` should assemble the transferable folder.

Host-audited properties:

- the NE display DRV has the documented export surface and DIBENG imports;
- the LE mini-VDD has a valid DDB and installs no dispatch callbacks;
- binary names and system destination directory are fixed;
- the INF matches only `PCI\VEN_5333&DEV_8A01`;
- 640x480, 800x600, and 1024x768 are advertised at 8 and 16 bpp;
- same-depth resolution changes apply live; depth changes require a reboot;
- 640x480 standard VGA remains available as the recovery fallback;
- every component carries a build identifier and the package carries hashes;
- external DDK and Open Watcom inputs remain outside the package.

Still required before this becomes an accepted installable driver:

- a cold copy of the active VHD, `86box.cfg`, and NVR directory;
- one captured cold boot showing mini-VDD and display `enable-ok` checkpoints;
- visible desktop, palette, drawing, and software-cursor checks;
- a demonstrated standard-VGA rollback or exact cold-backup restore;
- repeated install, boot, unload, and removal cycles required by Phase 1.

Never install directly from this source directory. Build the quarantined output,
read its `FIRSTBOOT.TXT`, `INSTALL.TXT`, and `RECOVER.TXT`, and test only
against the backed-up Windows 98SE S3 VM.
