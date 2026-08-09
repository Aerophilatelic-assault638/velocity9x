Velocity9x Matrox Millennium II physical recovery guard

PREPARE.BAT copies the active MGAPDX64.DRV and MGAPDX64.VXD into
C:\V9XSAFE\MGA2 and adds an inert AUTOEXEC.BAT call to V9XGUARD.BAT.

ARM.BAT enables a two-boot transaction. On the next boot the guard changes
ARMED.1 to ARMED.2 and permits the candidate files to load. If the guard is
not remotely disarmed after a successful test, the following reboot restores
the stock Matrox files before Windows loads the display driver.

DISARM.BAT commits a successful candidate. RESTORE.BAT immediately stages the
stock files for the next reboot. LASTBOOT.TXT records the latest guard action.

The first physical candidate must retain the stock filenames MGAPDX64.DRV and
MGAPDX64.VXD so file restoration is sufficient; it must not change the display
class association or install a new INF.
