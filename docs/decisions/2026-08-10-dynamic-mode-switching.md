# Dynamic mode switching (build modesw-1)

Date: 2026-08-10

Build: `modesw-1`

Target: Windows 98SE, 86Box S3 ViRGE/DX 86C375, 4 MiB VRAM

## Summary

Live same-depth resolution switching now works. `ReEnable` was rewritten on
the working vmdisp9x pattern (`vmdisp9x/enable.c:448-498`, MIT): GDI writes
the requested mode to the registry before calling ReEnable; the driver
re-reads it through `VDD_GET_DISPLAY_CONFIG`, rebuilds the PDEVICE in place
between `DIB_BeginAccess`/`DIB_EndAccess` with `CURSOREXCLUDE`, re-registers
the new visible-byte count with the master VDD, reprograms the DAC from the
preserved realized palette, and refills GDIINFO. Color-depth changes are
refused (`switch-refuse-depth`): Windows 9x never changes depth dynamically
(KB Q127139), and the 8-bpp PDEVICE is 1 KiB larger than the 16-bpp one, so
an in-place rebuild cannot fit. Depth changes continue through the reboot
path, which is unchanged.

`ExtModeSwitch` remained `0`: like vmdisp9x and the 98DDK sample INF, live
switching works without it; the gates are `C1_REINIT_ABLE` plus a working
ReEnable.

## VDD registration fix

`V9XVDDREGISTER` loaded `EAX = VDD_DRIVER_REGISTER` and then executed
`mov ax, SEG RESETHIRESMODE`, overwriting the low word of the service code
before calling the VDD entry point. Every prior "successful" registration
dispatched a garbage service number; the `cmp eax, VDD_DRIVER_REGISTER`
failure convention could not detect it. The sequence now loads `ES:DI`
before `EAX` (the order vmdisp9x's `CallVDDRegister` uses). A new
`V9XVDDREREGISTER` re-issues the registration with the updated visible-byte
count during a live switch, since `V9XVDDREGISTER` is idempotent-guarded and
would otherwise leave the VDD holding a stale save/restore size.

## Guest results (boots 56-62)

- Boot regression: `Stage=enable-ok`, desktop and cursor normal, V9XHW.INI
  publishes `ModeSwitching=live-same-depth`.
- First live switch: 800x600x16 -> 640x480x16 via `ChangeDisplaySettingsA`,
  ~1 second, clean desktop screenshot, `V9XMSW.INI` PASS.
- Cycle tests: 10/10 at 16 bpp, 10/10 at 8 bpp, and 100/100 endurance at
  8 bpp in 104 seconds (~1 s per switch). Selector reuse means no DPMI
  churn per switch.
- V9XGDI PASS at 640x480x16 and 800x600x8 after live switches.
- V9XPAL PASS at 800x600x8 after ten live switches - the realized palette
  survives the in-place rebuild.
- Depth request via `ChangeDisplaySettingsA` returns `DISP_CHANGE_FAILED`
  with the desktop intact (honest refusal, no silent wrong-mode success);
  registry + reboot path enters the other depth correctly (verified
  16 -> 8 -> 16 across boots 61-62).
- DirectDraw gate (V9XDDP from an 800x600x16 desktop):
  `SetDisplayMode(640,480,16)` now yields a real 640x480x16 primary
  (pitch 1280, previously 800x600x16/1600 - the silently-faked mode of the
  2026-08-10 benchmark investigation), flip time fell from ~53 ms to ~35 ms
  per present, and `RestoreDisplayMode` returned the desktop to 800x600x16
  live. Remaining flip cost is the HEL software copy; eliminating it is the
  DirectDraw HAL milestone.

## Test tooling

- `V9XMSW.EXE` (tools/diag/mode_switch_win32.c): `/set:WxHxB`, `/cycle:N`,
  `/depth:N`, `/cursor`; results in `C:\V9XMSW.INI`. Vehicle for the Phase 3
  exit gate and the 1,000-switch reliability target.
- `V9XDDP.EXE` (tools/diag/ddraw_probe_win32.c): DirectDraw mode honesty and
  flip/fill timing; results in `C:\V9XDD.INI`.
- Both are packaged in the active package alongside the existing tests.

## Operational notes

- Windows 9x flushes `WritePrivateProfileString` lazily; reading a result
  INI immediately after a test can return stale content. Wait a few seconds
  or poll.
- V9XGDI launched through the remote agent's direct exec (pipe capture) can
  stall before painting; launching via `START` in a shell completes in
  seconds. This is an agent execution-context quirk, not a driver fault
  (V9XDDP and V9XMSW run fine either way).

## Not yet covered

- Full-screen DOS box transitions across a live switch (the first exercise
  of the now-genuinely-registered `RESETHIRESMODE` callback).
- The 1,000-switch endurance gate (100 proven; scale in a later session).
- Rerunning the Ironfield benchmark suite to confirm game-visible present
  improvement.
