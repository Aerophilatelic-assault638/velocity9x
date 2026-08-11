Velocity9x Matrox Millennium II physical recovery guard

PREPARE.BAT copies the active MGAPDX64.DRV and MGAPDX64.VXD into
C:\V9XSAFE\MGA2 and adds an inert AUTOEXEC.BAT call to V9XGUARD.BAT.
V9XAUTO.EXE performs that edit transactionally before the first NUL or DOS
EOF marker, so fixed-size or padded AUTOEXEC.BAT files remain bootable.

ARM.BAT enables a two-boot transaction. On the next boot the guard changes
ARMED.1 to ARMED.2 and permits the candidate files to load. If the guard is
not remotely disarmed after a successful test, the following reboot restores
the stock Matrox files before Windows loads the display driver.

DISARM.BAT commits a successful candidate. RESTORE.BAT arms a DOS-time stock
restore for the next reboot, avoiding sharing violations on a loaded display
driver. LASTBOOT.TXT records the latest guard action.

After a successful candidate boot, DISARM.BAT also copies V9XSETP.DLL and
registers its read-only Velocity9x page in Display Properties. Settings-page
installation occurs only when the candidate is explicitly accepted; automatic
rollback and activation failures leave the registration unchanged.

If KEEPVXD.TAG is present beside ACTIVATE.BAT, the guard stages the validated
stock MGAPDX64.VXD with the candidate display DRV. This permits an isolated
display-driver test without redistributing or replacing the target's mini-VDD.

The first physical candidate must retain the stock filenames MGAPDX64.DRV and
MGAPDX64.VXD so file restoration is sufficient; it must not change the display
class association or install a new INF.
