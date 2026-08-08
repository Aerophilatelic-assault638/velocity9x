# Windows 98SE package gate

This directory intentionally contains no installable INF yet. An INF can change
the active display driver early in boot; publishing one before the DRV/VxD ABI,
copy lists, registry entries, uninstall behavior, and standard-VGA recovery are
validated would create a false and unsafe milestone.

Before adding `velocity9x.inf`, all of the following must be true:

- the NE display DRV loads and unloads in a disposable VM;
- the LE mini-VDD loads and logs without touching unsupported devices;
- the exact binary names and system destination directories are fixed;
- the PCI match is restricted to `PCI\VEN_5333&DEV_8A01`;
- failed installation and uninstall return to standard VGA;
- the serial build identifier is visible before any mode programming;
- all external DDK/tool inputs are recorded and absent from the package;
- the pre-install VM snapshot and recovery steps have been tested.

The first INF must be reviewed line-by-line and tested only against a snapshot of
the standard-VGA Windows 98SE VM.
