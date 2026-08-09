# Display Properties settings page

Date: 2026-08-10

The read-only settings surface now also appears inside the native Windows 98
Display Properties dialog as a "Velocity9x" tab. The standalone V9XSET.EXE
panel is unchanged and remains in the package for pre-install checks.

## Mechanism

Windows 98's Display control panel loads 32-bit shell property-sheet
extensions registered under

```
HKLM\Software\Microsoft\Windows\CurrentVersion\
    Controls Folder\Display\shellex\PropertySheetHandlers
```

`V9XSETP.DLL` implements `IShellExtInit` and `IShellPropSheetExt` with
explicit C vtables and static singleton objects, because the module is built
without a C runtime like the other diagnostic binaries. Its CLSID is
`{91925DA2-2EF0-4E20-B4E9-A53ED37E14B1}` and the in-proc server is registered
by bare filename, which resolves through the system directory search path.

The INF copies the DLL to the system directory, registers the CLSID and the
handler key, and deletes both keys in the `[Velocity9x.Previous]` cleanup
section so a future CLSID change cannot leave a stale registration behind.

## Shared status module

The INI-fact collection and clipboard report were extracted from
`settings_win32.c` into `tools/diag/settings_status.c`, shared by the panel
and the page. The framebuffer status string was made adapter-neutral
("linear aperture mapped" without the S3 prefix) because the Matrox
Millennium II backend now exists.

## Build notes

`scripts/build-settings-page.ps1` mirrors the settings-utility build. Two
Open Watcom specifics were required:

- `format windows nt dll` always emits a reference to `__DLLstart_`, even
  with `option start`. The link file needs both `option start` (sets the PE
  entry point) and `alias '__DLLstart_'='_V9xPageEntry@12'` (satisfies the
  reference); the alias alone links but leaves the entry-point RVA zero, so
  the module instance handle is never captured.
- `wdump -r` lists PE resources by numeric type id, so the verification
  matches type 2 (RT_BITMAP, id 101) and type 5 (RT_DIALOG, id 2000) rather
  than the names printed for NE images.

## Scope

The page is read-only status only; the checkboxes are disabled mirrors of the
locked bring-up policy. It registers only through the S3 INF package. The
Matrox drop-in candidate intentionally ships no INF and therefore no page
registration yet.
