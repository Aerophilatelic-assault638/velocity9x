# Monitor power management boundary (2026-08-11)

## Decision

The S3 ViRGE mini-VDD advertises only `CM_POWERSTATE_D0`. System suspend and
adapter power-down remain outside the display driver boundary. Legacy VESA
4F10 handling and the Windows 98 4.1 monitor-power entries are installed, but
low-power capability bits are withheld until framebuffer-safe resume exists.

## Evidence

The affected Win98SE guest remained responsive while its display was blank.
Its active Always On scheme had a 900-second AC video timeout. A forced
monitor-off/on transition reproduced the lost display even after directly
clearing the S3 SR0D, CR56, and VGA SR01 blanking controls. This showed that
the failure was not merely a DPMS register left asserted.

The final `powerfix4-test` build was installed through the guarded associated-
driver update flow. For validation, the active scheme's AC video timeout was
temporarily changed from 900 seconds to 10 seconds. After reboot, the guest
remained idle for 35 seconds: the desktop stayed visible, the remote agent
reported `DesktopReady=true`, and the framebuffer screenshot was non-blank.
The original 900-second policy was then restored and the guest rebooted.

## Consequence

Win98 no longer schedules monitor sleep while this driver is active, avoiding
the long-standing blank screen. This is deliberately conservative: monitor
energy saving is unavailable, but normal display operation and the user's
system power policy otherwise remain unchanged.
