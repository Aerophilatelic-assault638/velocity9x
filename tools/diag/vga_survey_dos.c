/*
 * Velocity9x VGA hardware survey.
 *
 * One real-mode DOS executable to hand to owners of cards this project does not
 * support yet. It captures what writing a chipset backend actually requires -
 * PCI configuration space, the video BIOS image, the complete VBE mode list,
 * EDID, and the VGA register file - and writes a single INI report.
 *
 * The tool interprets almost nothing. Blobs go into the report verbatim as hex
 * and are decoded host-side by scripts/parse-vga-survey.ps1, so a decoding
 * mistake is fixed in a script instead of by re-shipping an executable to every
 * tester.
 *
 * Two tiers, and the boundary is the point of the design:
 *
 *   Tier 1 always runs. Nothing it does writes a device register except to
 *   select an index, and every index it touches is restored. PCI is read a byte
 *   at a time through the PCI BIOS, the video BIOS is read through far
 *   pointers, and VBE is used only through its query functions.
 *
 *   Tier 2 is opt-in. It writes documented per-vendor unlock keys, reads the
 *   registers behind them, and restores the originals. It is dispatched on the
 *   PCI vendor ID, so an unknown card is never poked speculatively, and it runs
 *   only after the Tier 1 report has been closed on disk - if Tier 2 wedges the
 *   machine, the tester still has a complete Tier 1 report to send.
 *
 * No video mode is ever set, and nothing is written to the card that outlives
 * the run.
 */

#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_SURVEY_SCHEMA "1"
#define V9X_SURVEY_DEFAULT "C:\\V9XSURV.INI"

/* Enough for any real machine; the walk stops at the PCI BIOS's last bus. */
#define V9X_MAX_DISPLAY_DEVICES 8
#define V9X_MAX_VBE_MODES 256
#define V9X_MAX_ROM_STRINGS 64
#define V9X_ROM_STRING_MIN 8
#define V9X_ROM_STRING_MAX 72

/* The report is written a line at a time, so the only large buffers are the
 * fixed BIOS structures. Everything here lives in the single 64 KB data segment
 * of the small memory model. */
static unsigned char controller_info[512];
static unsigned char mode_info[256];
static unsigned char edid_block[256];
static unsigned char config_space[256];
static unsigned char functionality[64];
static unsigned short vbe_modes[V9X_MAX_VBE_MODES];
static unsigned vbe_mode_count;
static unsigned char vga_values[64];

struct display_device {
    unsigned char bus;
    unsigned char devfn;
    unsigned short vendor_id;
    unsigned short device_id;
};

static struct display_device display_devices[V9X_MAX_DISPLAY_DEVICES];
static unsigned display_device_count;

static FILE *report;
static char report_path[128];
static int windows_present;
static int pci_bios_present;
static unsigned char pci_last_bus;
static int want_full_rom;
static int tier2_enabled;
static const char *tier2_reason = "not-requested";
static char survey_note[160];
static unsigned char saved_break_state;

static const char hex_digits[] = "0123456789ABCDEF";

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static unsigned short v9x_u16(const unsigned char *data)
{
    return (unsigned short)((unsigned short)data[0] |
                            ((unsigned short)data[1] << 8));
}

static unsigned long v9x_u32(const unsigned char *data)
{
    return (unsigned long)data[0] | ((unsigned long)data[1] << 8) |
           ((unsigned long)data[2] << 16) | ((unsigned long)data[3] << 24);
}

/* The BIOS data area and the option ROM live outside the data segment, so they
 * need their own accessors - casting a far pointer down to near would quietly
 * drop the segment and read whatever happens to sit at that offset in DS. */
static unsigned short v9x_u16_far(const unsigned char far *data)
{
    return (unsigned short)((unsigned short)data[0] |
                            ((unsigned short)data[1] << 8));
}

static unsigned long v9x_u32_far(const unsigned char far *data)
{
    return (unsigned long)data[0] | ((unsigned long)data[1] << 8) |
           ((unsigned long)data[2] << 16) | ((unsigned long)data[3] << 24);
}

/*
 * Copy a far BIOS string into the data segment.
 *
 * VBE hands back real-mode far pointers into ROM, which the small model cannot
 * print directly. Anything outside printable ASCII becomes '.' so a corrupt
 * pointer cannot inject a newline and split one INI value across two lines.
 */
static void copy_far_string(char *dest, unsigned dest_size,
                            const char far *src)
{
    unsigned index = 0u;

    dest[0] = '\0';
    if (src == 0 || (FP_SEG(src) == 0u && FP_OFF(src) == 0u)) return;
    while (index + 1u < dest_size) {
        char value = src[index];
        if (value == '\0') break;
        if (value < 0x20 || value > 0x7e) value = '.';
        dest[index++] = value;
    }
    while (index > 0u && dest[index - 1u] == ' ') --index;
    dest[index] = '\0';
}

/* ------------------------------------------------------------------ */
/* Report writing                                                      */
/* ------------------------------------------------------------------ */

static void wr_section(const char *name)
{
    fprintf(report, "\n[%s]\n", name);
}

static void wr_str(const char *key, const char *value)
{
    fprintf(report, "%s=%s\n", key, value);
}

static void wr_u(const char *key, unsigned long value)
{
    fprintf(report, "%s=%lu\n", key, value);
}

static void wr_x8(const char *key, unsigned char value)
{
    fprintf(report, "%s=%02X\n", key, value);
}

static void wr_x16(const char *key, unsigned short value)
{
    fprintf(report, "%s=%04X\n", key, value);
}

static void wr_x32(const char *key, unsigned long value)
{
    fprintf(report, "%s=%08lX\n", key, value);
}

static void wr_status(const char *value)
{
    wr_str("Status", value);
}

/*
 * A probe that could not run says so. The host-side parser has to be able to
 * tell "this card does not have that" from "we never looked", which it cannot
 * do if a failed probe simply omits its section.
 */
static void wr_unavailable(const char *section, const char *reason)
{
    wr_section(section);
    wr_status("unavailable");
    wr_str("Reason", reason);
}

/*
 * Blob lines are assembled in a stack buffer and written with one fputs.
 *
 * A 64 KB video BIOS is four thousand of these lines and a million individual
 * characters; going through fprintf or fputc per byte turns a one-second dump
 * into a wait long enough that a tester assumes the machine has hung. The
 * source is far because the ROM is emitted straight out of UMB space rather
 * than being copied into the data segment first.
 */
static void wr_hex_line_far(const char *prefix, unsigned long offset,
                            unsigned digits, const unsigned char far *data,
                            unsigned count)
{
    char line[56];
    unsigned at = 0u;
    unsigned index;

    for (index = digits; index-- > 0u;) {
        line[at++] = hex_digits[(unsigned)(offset >> (index * 4u)) & 0x0fu];
    }
    line[at++] = '=';
    for (index = 0u; index < count; ++index) {
        unsigned char value = data[index];
        line[at++] = hex_digits[(value >> 4) & 0x0fu];
        line[at++] = hex_digits[value & 0x0fu];
    }
    line[at] = '\0';
    fputs(prefix, report);
    fputc('.', report);
    fputs(line, report);
    fputc('\n', report);
}

static void wr_hex_block(const char *prefix, const unsigned char *data,
                         unsigned count)
{
    unsigned offset;

    for (offset = 0u; offset < count; offset += 16u) {
        unsigned span = count - offset;
        if (span > 16u) span = 16u;
        wr_hex_line_far(prefix, offset, 2u,
                        (const unsigned char far *)(data + offset), span);
    }
}

static void wr_hex_block_far(const char *prefix, const unsigned char far *data,
                             unsigned long count)
{
    unsigned long offset;

    for (offset = 0ul; offset < count; offset += 16ul) {
        unsigned long span = count - offset;
        if (span > 16ul) span = 16ul;
        wr_hex_line_far(prefix, offset, 4u, data + offset, (unsigned)span);
    }
}

/* ------------------------------------------------------------------ */
/* Port access                                                         */
/* ------------------------------------------------------------------ */

/* The CRTC pair follows the MISC output register's colour/mono bit, the same
 * rule the display driver uses in src/display16/ddi.c. */
static unsigned crtc_index_port(void)
{
    return (inp(0x03ccu) & 0x01u) != 0u ? 0x03d4u : 0x03b4u;
}

static unsigned char read_indexed(unsigned index_port, unsigned char index)
{
    outp(index_port, index);
    return (unsigned char)inp(index_port + 1u);
}

static void write_indexed(unsigned index_port, unsigned char index,
                          unsigned char value)
{
    outp(index_port, index);
    outp(index_port + 1u, value);
}

/* ------------------------------------------------------------------ */
/* [System]                                                            */
/* ------------------------------------------------------------------ */

/*
 * Bits 12-15 of FLAGS after a PUSHF are always set on an 8086/80186 and always
 * clear on a 286 and later. Separating a 286 from a 386 needs 32-bit EFLAGS
 * work, which would put 386-only opcodes in a binary that should stay runnable
 * on anything, so the survey stops at this coarse split - the CPU is context
 * here, not the subject.
 */
static unsigned short flags_probe(void);
#pragma aux flags_probe =   \
    "pushf"                 \
    "pop  cx"               \
    "mov  ax,cx"            \
    "and  ax,0fffh"         \
    "push ax"               \
    "popf"                  \
    "pushf"                 \
    "pop  ax"               \
    "push cx"               \
    "popf"                  \
    value [ax] modify exact [ax cx];

static void survey_system(void)
{
    union REGS input;
    union REGS output;
    char text[32];
    unsigned short flags;

    wr_section("System");
    wr_status("ok");

    memset(&input, 0, sizeof(input));
    input.h.ah = 0x30u;
    int86(0x21, &input, &output);
    sprintf(text, "%u.%02u", output.h.al, output.h.ah);
    wr_str("DosVersion", text);

    /* INT 2Fh AX=1600h is the standard "is a Windows DPMI host running" check.
     * AL 0 and 80h mean no; 1 and FFh mean a 2.x host; anything else is the
     * major version with AH as the minor. */
    memset(&input, 0, sizeof(input));
    input.x.ax = 0x1600u;
    int86(0x2f, &input, &output);
    if (output.h.al == 0x00u || output.h.al == 0x80u) {
        windows_present = 0;
        wr_str("WindowsPresent", "no");
    } else {
        windows_present = 1;
        wr_str("WindowsPresent", "yes");
        if (output.h.al == 0x01u || output.h.al == 0xffu) {
            wr_str("WindowsVersion", "2.x");
        } else {
            sprintf(text, "%u.%02u", output.h.al, output.h.ah);
            wr_str("WindowsVersion", text);
        }
    }

    int86(0x12, &input, &output);
    wr_u("ConventionalKB", output.x.ax);

    flags = flags_probe();
    wr_str("CpuClass",
           (flags & 0xf000u) == 0xf000u ? "8086-or-186" : "286-or-later");
}

/* ------------------------------------------------------------------ */
/* [BiosData]                                                          */
/* ------------------------------------------------------------------ */

static void survey_bios_data(void)
{
    const unsigned char far *bda = (const unsigned char far *)MK_FP(0x0040u, 0u);
    union REGS input;
    union REGS output;
    struct SREGS segments;

    wr_section("BiosData");
    wr_status("ok");
    wr_x8("VideoMode", bda[0x49]);
    wr_u("TextColumns", v9x_u16_far(&bda[0x4a]));
    wr_x16("CrtcPort", v9x_u16_far(&bda[0x63]));
    wr_u("TextRows", (unsigned long)bda[0x84] + 1ul);
    wr_u("CharacterHeight", v9x_u16_far(&bda[0x85]));
    wr_x8("VideoControl87", bda[0x87]);
    wr_x8("VideoSwitches", bda[0x88]);
    wr_x8("ModeSetOption", bda[0x89]);
    wr_x8("DisplayCombination", bda[0x8a]);
    wr_x32("SaveTablePointer", v9x_u32_far(&bda[0xa8]));

    /* INT 10h AH=1Ah, display combination code. Query subfunction only. */
    memset(&input, 0, sizeof(input));
    input.x.ax = 0x1a00u;
    int86(0x10, &input, &output);
    if (output.h.al == 0x1au) {
        wr_str("DisplayCombinationStatus", "ok");
        wr_x8("ActiveDisplayCode", output.h.bl);
        wr_x8("InactiveDisplayCode", output.h.bh);
    } else {
        wr_str("DisplayCombinationStatus", "unsupported");
    }

    /* INT 10h AH=1Bh returns a 64-byte functionality/state block describing the
     * modes and scan lines the BIOS admits to. Dumped raw and decoded
     * host-side. */
    memset(&input, 0, sizeof(input));
    memset(functionality, 0, sizeof(functionality));
    segread(&segments);
    input.x.ax = 0x1b00u;
    input.x.bx = 0x0000u;
    segments.es = FP_SEG((void far *)functionality);
    input.x.di = FP_OFF((void far *)functionality);
    int86x(0x10, &input, &output, &segments);
    if (output.h.al == 0x1bu) {
        wr_str("FunctionalityStatus", "ok");
        wr_hex_block("Functionality", functionality, sizeof(functionality));
    } else {
        wr_str("FunctionalityStatus", "unsupported");
    }
}

/* ------------------------------------------------------------------ */
/* PCI                                                                 */
/* ------------------------------------------------------------------ */

/*
 * Every configuration read goes through PCI BIOS function B108h, one byte at a
 * time.
 *
 * B109h and B10Ah return their results in CX and ECX, and the 16-bit register
 * union has no 32-bit members - reaching ECX would mean hand-written assembly
 * for no gain. A full 256-byte space costs 256 BIOS calls, which is a few
 * milliseconds, and words and dwords are assembled in C from the bytes.
 */
static int pci_read_byte(unsigned char bus, unsigned char devfn,
                         unsigned char reg, unsigned char *value)
{
    union REGS input;
    union REGS output;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    input.x.ax = 0xb108u;
    input.h.bh = bus;
    input.h.bl = devfn;
    input.x.di = reg;
    int86(0x1a, &input, &output);
    if (output.x.cflag != 0 || output.h.ah != 0u) return 0;
    *value = output.h.cl;
    return 1;
}

static int pci_read_config(unsigned char bus, unsigned char devfn)
{
    unsigned index;

    for (index = 0u; index < 256u; ++index) {
        if (!pci_read_byte(bus, devfn, (unsigned char)index,
                           &config_space[index])) {
            return 0;
        }
    }
    return 1;
}

static void survey_pci_bios(void)
{
    union REGS input;
    union REGS output;

    wr_section("PciBios");
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    input.x.ax = 0xb101u;
    int86(0x1a, &input, &output);
    if (output.x.cflag != 0 || output.h.ah != 0u) {
        pci_bios_present = 0;
        wr_status("unavailable");
        wr_str("Reason", "int1a-b101-failed");
        return;
    }
    pci_bios_present = 1;
    pci_last_bus = output.h.cl;
    wr_status("ok");
    wr_x16("Version", output.x.bx);
    wr_x8("HardwareMechanism", output.h.al);
    wr_x8("LastBus", output.h.cl);
}

/*
 * Walk every bus, device and function rather than asking the BIOS to find a
 * display device.
 *
 * B103h takes its class code in ECX, which the 16-bit register union cannot
 * reach, and the walk finds more anyway: secondary adapters the BIOS would not
 * return, and the host bridge, whose identity matters because it decides how
 * the framebuffer aperture behaves.
 */
static void survey_pci_devices(void)
{
    unsigned bus;
    unsigned device;
    unsigned function;
    unsigned total = 0u;

    if (!pci_bios_present) {
        wr_unavailable("PciInventory", "no-pci-bios");
        return;
    }

    wr_section("PciInventory");
    wr_status("ok");
    wr_str("Fields", "Bus,Device,Function,VendorId,DeviceId,ClassCode,Revision");

    for (bus = 0u; bus <= (unsigned)pci_last_bus; ++bus) {
        for (device = 0u; device < 32u; ++device) {
            unsigned char header_type = 0u;

            for (function = 0u; function < 8u; ++function) {
                unsigned char devfn = (unsigned char)((device << 3) | function);
                unsigned char vendor_low;
                unsigned char vendor_high;
                unsigned char device_low;
                unsigned char device_high;
                unsigned char revision;
                unsigned char prog_if;
                unsigned char sub_class;
                unsigned char base_class;
                unsigned short vendor_id;
                unsigned short device_id;

                if (!pci_read_byte((unsigned char)bus, devfn, 0x00u,
                                   &vendor_low) ||
                    !pci_read_byte((unsigned char)bus, devfn, 0x01u,
                                   &vendor_high)) {
                    break;
                }
                vendor_id = (unsigned short)(vendor_low |
                                             ((unsigned short)vendor_high << 8));
                if (vendor_id == 0xffffu || vendor_id == 0x0000u) {
                    if (function == 0u) break;
                    continue;
                }

                pci_read_byte((unsigned char)bus, devfn, 0x02u, &device_low);
                pci_read_byte((unsigned char)bus, devfn, 0x03u, &device_high);
                pci_read_byte((unsigned char)bus, devfn, 0x08u, &revision);
                pci_read_byte((unsigned char)bus, devfn, 0x09u, &prog_if);
                pci_read_byte((unsigned char)bus, devfn, 0x0au, &sub_class);
                pci_read_byte((unsigned char)bus, devfn, 0x0bu, &base_class);
                device_id = (unsigned short)(device_low |
                                             ((unsigned short)device_high << 8));

                fprintf(report,
                        "Device.%02u=%02X,%02X,%02X,%04X,%04X,%02X%02X%02X,%02X\n",
                        total, bus, device, function, vendor_id, device_id,
                        base_class, sub_class, prog_if, revision);
                ++total;

                if (base_class == 0x03u &&
                    display_device_count < V9X_MAX_DISPLAY_DEVICES) {
                    display_devices[display_device_count].bus =
                        (unsigned char)bus;
                    display_devices[display_device_count].devfn = devfn;
                    display_devices[display_device_count].vendor_id = vendor_id;
                    display_devices[display_device_count].device_id = device_id;
                    ++display_device_count;
                }

                if (function == 0u) {
                    pci_read_byte((unsigned char)bus, devfn, 0x0eu,
                                  &header_type);
                    /* Bit 7 clear means the device is single-function and the
                     * remaining function numbers are aliases of this one. */
                    if ((header_type & 0x80u) == 0u) break;
                }
            }
        }
    }
    wr_u("Count", total);
    wr_u("DisplayDeviceCount", display_device_count);
}

static void survey_display_device(unsigned index)
{
    char section[32];
    const struct display_device *device = &display_devices[index];

    sprintf(section, "PciDevice.%u", index);
    wr_section(section);
    if (!pci_read_config(device->bus, device->devfn)) {
        wr_status("error");
        wr_str("Reason", "config-read-failed");
        return;
    }

    wr_status("ok");
    wr_x8("Bus", device->bus);
    wr_x8("Device", (unsigned char)(device->devfn >> 3));
    wr_x8("Function", (unsigned char)(device->devfn & 0x07u));
    wr_x16("VendorId", v9x_u16(config_space + 0x00));
    wr_x16("DeviceId", v9x_u16(config_space + 0x02));
    wr_x16("Command", v9x_u16(config_space + 0x04));
    wr_x16("StatusRegister", v9x_u16(config_space + 0x06));
    wr_x8("Revision", config_space[0x08]);
    fprintf(report, "ClassCode=%02X%02X%02X\n", config_space[0x0b],
            config_space[0x0a], config_space[0x09]);
    wr_x8("HeaderType", config_space[0x0e]);
    wr_x32("Bar0", v9x_u32(config_space + 0x10));
    wr_x32("Bar1", v9x_u32(config_space + 0x14));
    wr_x32("Bar2", v9x_u32(config_space + 0x18));
    wr_x32("Bar3", v9x_u32(config_space + 0x1c));
    wr_x32("Bar4", v9x_u32(config_space + 0x20));
    wr_x32("Bar5", v9x_u32(config_space + 0x24));
    wr_x16("SubsystemVendorId", v9x_u16(config_space + 0x2c));
    wr_x16("SubsystemId", v9x_u16(config_space + 0x2e));
    wr_x32("RomBar", v9x_u32(config_space + 0x30));
    wr_x8("CapabilitiesPtr", config_space[0x34]);
    wr_x8("InterruptLine", config_space[0x3c]);
    wr_x8("InterruptPin", config_space[0x3d]);
    wr_x8("MinGrant", config_space[0x3e]);
    wr_x8("MaxLatency", config_space[0x3f]);
    wr_hex_block("Config", config_space, sizeof(config_space));
}

/* ------------------------------------------------------------------ */
/* [VideoBios]                                                         */
/* ------------------------------------------------------------------ */

/*
 * The option ROM carries the strings that actually name a chip - "S3 86C375",
 * "Trident TGUI9440", "Cirrus Logic GD5446" - which makes it the single most
 * useful artefact when the PCI device ID alone is not enough to place a card.
 */
static void report_rom_strings(const unsigned char far *rom,
                               unsigned long size)
{
    unsigned long offset = 0ul;
    unsigned found = 0u;
    char text[V9X_ROM_STRING_MAX + 1];

    while (offset < size && found < V9X_MAX_ROM_STRINGS) {
        unsigned length = 0u;

        while (offset + length < size && length < V9X_ROM_STRING_MAX) {
            unsigned char value = rom[offset + length];
            if (value < 0x20u || value > 0x7eu) break;
            text[length] = (char)value;
            ++length;
        }
        if (length >= V9X_ROM_STRING_MIN) {
            text[length] = '\0';
            fprintf(report, "String.%02u=%s\n", found, text);
            ++found;
            offset += length;
        } else {
            offset += length + 1ul;
        }
    }
    wr_u("StringCount", found);
    /* Say so when the cap bit. A bare count that happens to equal the limit
     * reads as "that is all there was", and on a chatty ROM it is not. */
    wr_str("StringsTruncated", offset < size ? "1" : "0");
}

static void survey_rom_image(const char *section, unsigned segment,
                             int is_primary)
{
    const unsigned char far *rom =
        (const unsigned char far *)MK_FP(segment, 0u);
    unsigned long size;
    unsigned long pcir_offset;
    unsigned long checksum = 0ul;
    unsigned long offset;
    char text[32];

    wr_section(section);
    if (rom[0] != 0x55u || rom[1] != 0xaau) {
        wr_status("unavailable");
        wr_str("Reason", "no-option-rom-signature");
        return;
    }

    wr_status("ok");
    wr_x16("Segment", (unsigned short)segment);
    size = (unsigned long)rom[2] * 512ul;
    if (size == 0ul || size > 65536ul) size = 32768ul;
    wr_u("SizeBytes", size);

    for (offset = 0ul; offset < size; ++offset) {
        checksum += rom[offset];
    }
    wr_str("ChecksumStatus", (checksum & 0xfful) == 0ul ? "ok" : "mismatch");

    /* The PCI Data Structure is reached through the pointer at offset 18h and
     * repeats the device identity independently of configuration space, which
     * is how a card whose PCI ID is ambiguous still gets pinned down. */
    pcir_offset = v9x_u16_far(&rom[0x18]);
    if (pcir_offset != 0ul && pcir_offset + 24ul <= size &&
        rom[pcir_offset] == 'P' && rom[pcir_offset + 1ul] == 'C' &&
        rom[pcir_offset + 2ul] == 'I' && rom[pcir_offset + 3ul] == 'R') {
        wr_str("PcirStatus", "ok");
        wr_x16("PcirOffset", (unsigned short)pcir_offset);
        wr_x16("PcirVendorId", v9x_u16_far(&rom[pcir_offset + 4ul]));
        wr_x16("PcirDeviceId", v9x_u16_far(&rom[pcir_offset + 6ul]));
        fprintf(report, "PcirClassCode=%02X%02X%02X\n",
                rom[pcir_offset + 0x0dul], rom[pcir_offset + 0x0cul],
                rom[pcir_offset + 0x0bul]);
        wr_u("PcirImageLength",
             (unsigned long)v9x_u16_far(&rom[pcir_offset + 0x10ul]) * 512ul);
        wr_x16("PcirCodeRevision", v9x_u16_far(&rom[pcir_offset + 0x12ul]));
    } else {
        wr_str("PcirStatus", "unavailable");
    }

    /* $PnP expansion header, if the card has one. */
    {
        int found = 0;
        for (offset = 0ul; offset + 32ul <= size && offset < 65536ul;
             offset += 16ul) {
            if (rom[offset] == '$' && rom[offset + 1ul] == 'P' &&
                rom[offset + 2ul] == 'n' && rom[offset + 3ul] == 'P') {
                wr_str("PnpStatus", "ok");
                wr_x16("PnpOffset", (unsigned short)offset);
                wr_x16("PnpManufacturerPtr",
                       v9x_u16_far(&rom[offset + 0x0eul]));
                wr_x16("PnpProductPtr", v9x_u16_far(&rom[offset + 0x10ul]));
                found = 1;
                break;
            }
        }
        if (!found) wr_str("PnpStatus", "unavailable");
    }

    /* Offset 1Eh carries a mm/dd/yy build date on some video BIOSes and an
     * identification string on others - the S3 ViRGE puts "IBM VGA" there. The
     * bytes are reported under a neutral name rather than being labelled as a
     * date the host-side parser would then try to parse. */
    copy_far_string(text, 17u, (const char far *)&rom[0x1e]);
    wr_str("IdStringAt1E", text);

    report_rom_strings(rom, size);

    if (is_primary) {
        unsigned long dumped = want_full_rom ? size : 1024ul;
        if (dumped > size) dumped = size;
        wr_u("DumpBytes", dumped);
        wr_str("DumpScope", want_full_rom ? "full-image" : "header-only");
        wr_hex_block_far("Rom", rom, dumped);
    } else {
        wr_str("DumpScope", "none");
    }
}

/*
 * A second display adapter has its own option ROM somewhere between C000 and
 * E000. Reading unmapped real-mode memory returns FFh, so the sweep cannot
 * fault - it just finds nothing.
 */
static void survey_secondary_roms(void)
{
    unsigned segment;
    unsigned found = 0u;

    for (segment = 0xc080u; segment < 0xe000u; segment += 0x80u) {
        const unsigned char far *rom =
            (const unsigned char far *)MK_FP(segment, 0u);
        char section[32];

        if (rom[0] != 0x55u || rom[1] != 0xaau) continue;
        sprintf(section, "OptionRom.%u", found);
        survey_rom_image(section, segment, 0);
        ++found;
        if (found >= 4u) break;
    }
    wr_section("OptionRomScan");
    wr_status("ok");
    wr_u("Count", found);
}

/* ------------------------------------------------------------------ */
/* VBE                                                                 */
/* ------------------------------------------------------------------ */

static unsigned short vbe_call(unsigned short function, unsigned short bx,
                               unsigned short cx, unsigned short dx,
                               void far *buffer)
{
    union REGS input;
    union REGS output;
    struct SREGS segments;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    segread(&segments);
    input.x.ax = function;
    input.x.bx = bx;
    input.x.cx = cx;
    input.x.dx = dx;
    if (buffer != 0) {
        segments.es = FP_SEG(buffer);
        input.x.di = FP_OFF(buffer);
    }
    int86x(0x10, &input, &output, &segments);
    return output.x.ax;
}

static void survey_vbe(void)
{
    unsigned short status;
    unsigned long mode_pointer;
    char text[80];

    wr_section("VBE");
    memset(controller_info, 0, sizeof(controller_info));
    /* Asking for "VBE2" is what makes a VBE 2.0+ BIOS fill in the OEM vendor,
     * product and revision strings. A VBE 1.2 BIOS ignores it. */
    memcpy(controller_info, "VBE2", 4);
    status = vbe_call(0x4f00u, 0u, 0u, 0u, controller_info);
    wr_x16("CallStatus", status);
    if (status != 0x004fu) {
        wr_status("unavailable");
        wr_str("Reason", "vbe-4f00-failed");
        wr_section("VBEModes");
        wr_status("unavailable");
        wr_str("Reason", "no-vbe-controller-info");
        return;
    }

    /*
     * Copy the mode numbers out before issuing any other VBE call. On most
     * BIOSes VideoModePtr points back into the 512-byte block we just handed
     * to 4F00h, and a later call is free to rebuild that block - walking the
     * list at the end of this function would then be walking whatever replaced
     * it.
     */
    mode_pointer = v9x_u32(controller_info + 14);
    vbe_mode_count = 0u;
    if (mode_pointer != 0ul) {
        const unsigned short far *list =
            (const unsigned short far *)mode_pointer;

        while (vbe_mode_count < V9X_MAX_VBE_MODES) {
            unsigned short mode = list[vbe_mode_count];
            if (mode == 0xffffu) break;
            vbe_modes[vbe_mode_count] = mode;
            ++vbe_mode_count;
        }
    }

    wr_status("ok");
    fprintf(report, "Signature=%.4s\n", controller_info);
    wr_x16("Version", v9x_u16(controller_info + 4));
    wr_x32("Capabilities", v9x_u32(controller_info + 10));
    wr_u("TotalMemory64K", v9x_u16(controller_info + 18));
    wr_u("TotalMemoryBytes",
         (unsigned long)v9x_u16(controller_info + 18) * 65536ul);
    wr_x16("OemSoftwareRev", v9x_u16(controller_info + 20));

    copy_far_string(text, sizeof(text),
                    (const char far *)v9x_u32(controller_info + 6));
    wr_str("OemString", text);
    if (v9x_u16(controller_info + 4) >= 0x0200u) {
        copy_far_string(text, sizeof(text),
                        (const char far *)v9x_u32(controller_info + 22));
        wr_str("OemVendorName", text);
        copy_far_string(text, sizeof(text),
                        (const char far *)v9x_u32(controller_info + 26));
        wr_str("OemProductName", text);
        copy_far_string(text, sizeof(text),
                        (const char far *)v9x_u32(controller_info + 30));
        wr_str("OemProductRev", text);
    }

    status = vbe_call(0x4f03u, 0u, 0u, 0u, 0);
    if (status == 0x004fu) {
        union REGS input;
        union REGS output;

        memset(&input, 0, sizeof(input));
        input.x.ax = 0x4f03u;
        int86(0x10, &input, &output);
        wr_x16("CurrentMode", output.x.bx);
    } else {
        wr_str("CurrentModeStatus", "unavailable");
    }

    /* Protected-mode interface presence. Query only - the returned table is
     * not called, only counted. */
    {
        union REGS input;
        union REGS output;
        struct SREGS segments;

        memset(&input, 0, sizeof(input));
        segread(&segments);
        input.x.ax = 0x4f0au;
        input.x.bx = 0x0000u;
        int86x(0x10, &input, &output, &segments);
        if (output.x.ax == 0x004fu) {
            wr_str("PmInterfaceStatus", "ok");
            wr_u("PmInterfaceBytes", output.x.cx);
        } else {
            wr_str("PmInterfaceStatus", "unavailable");
        }
    }

    /* DPMS capabilities, and the flat-panel query that a handful of BIOSes
     * mishandle - both are guarded separately so one bad call cannot take the
     * section with it. */
    {
        union REGS input;
        union REGS output;

        memset(&input, 0, sizeof(input));
        input.x.ax = 0x4f10u;
        input.x.bx = 0x0000u;
        int86(0x10, &input, &output);
        if (output.x.ax == 0x004fu) {
            wr_str("DpmsStatus", "ok");
            wr_x16("DpmsCapabilities", output.x.bx);
        } else {
            wr_str("DpmsStatus", "unsupported");
        }

        memset(&input, 0, sizeof(input));
        input.x.ax = 0x4f11u;
        input.x.bx = 0x0000u;
        int86(0x10, &input, &output);
        if (output.x.ax == 0x004fu) {
            wr_str("FlatPanelStatus", "ok");
            wr_x16("FlatPanelInfo", output.x.bx);
        } else {
            wr_str("FlatPanelStatus", "unsupported");
        }
    }

    /* The list came from the BIOS's own pointer, so a card reports every mode
     * it has rather than only the ones this project already knows to ask
     * about. */
    wr_section("VBEModes");
    if (mode_pointer == 0ul) {
        wr_status("unavailable");
        wr_str("Reason", "null-mode-pointer");
        return;
    }

    {
        unsigned count = vbe_mode_count;
        unsigned index;

        wr_status("ok");
        wr_x32("ModeListPointer", mode_pointer);
        wr_u("Count", count);
        wr_str("Truncated", count >= V9X_MAX_VBE_MODES ? "1" : "0");
        wr_str("Fields",
               "Mode,Attributes,Width,Height,Planes,Bpp,MemoryModel,Pitch,"
               "PhysBase,LinearPitch,RedSize,RedPos,GreenSize,GreenPos,"
               "BlueSize,BluePos");

        for (index = 0u; index < count; ++index) {
            unsigned short mode = vbe_modes[index];

            memset(mode_info, 0, sizeof(mode_info));
            status = vbe_call(0x4f01u, 0u, mode, 0u, mode_info);
            if (status != 0x004fu) {
                fprintf(report, "Mode.%02u=%04X,ERROR%04X\n", index, mode,
                        status);
                continue;
            }
            fprintf(report,
                    "Mode.%02u=%04X,%04X,%u,%u,%u,%u,%u,%u,%08lX,%u,"
                    "%u,%u,%u,%u,%u,%u\n",
                    index, mode, v9x_u16(mode_info), v9x_u16(mode_info + 18),
                    v9x_u16(mode_info + 20), mode_info[24], mode_info[25],
                    mode_info[27], v9x_u16(mode_info + 16),
                    v9x_u32(mode_info + 40), v9x_u16(mode_info + 50),
                    mode_info[31], mode_info[32], mode_info[33], mode_info[34],
                    mode_info[35], mode_info[36]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* [EDID]                                                              */
/* ------------------------------------------------------------------ */

static void survey_edid(void)
{
    unsigned short status;

    wr_section("EDID");
    status = vbe_call(0x4f15u, 0x0000u, 0u, 0u, 0);
    if (status != 0x004fu) {
        wr_status("unsupported");
        fprintf(report, "Reason=vbe-4f15-capabilities-returned-%04X\n", status);
        return;
    }

    {
        union REGS input;
        union REGS output;

        memset(&input, 0, sizeof(input));
        input.x.ax = 0x4f15u;
        input.x.bx = 0x0000u;
        input.x.cx = 0x0000u;
        int86(0x10, &input, &output);
        wr_x8("DdcLevel", output.h.bh);
        wr_u("DdcBlockTransferMs", output.h.bl);
    }

    memset(edid_block, 0, sizeof(edid_block));
    status = vbe_call(0x4f15u, 0x0001u, 0u, 0u, edid_block);
    if (status != 0x004fu) {
        wr_status("unavailable");
        fprintf(report, "Reason=vbe-4f15-read-returned-%04X\n", status);
        return;
    }

    wr_status("ok");
    wr_u("BlockCount", 1ul + (unsigned long)edid_block[126]);
    wr_hex_block("Block0", edid_block, 128u);

    /* Only fetch an extension block if block 0 says one exists. */
    if (edid_block[126] != 0u) {
        memset(edid_block, 0, sizeof(edid_block));
        status = vbe_call(0x4f15u, 0x0001u, 0u, 0x0001u, edid_block);
        if (status == 0x004fu) {
            wr_hex_block("Block1", edid_block, 128u);
        }
    }
}

/* ------------------------------------------------------------------ */
/* [VGARegisters]                                                      */
/* ------------------------------------------------------------------ */

static void dump_indexed_range(const char *prefix, unsigned index_port,
                               unsigned first, unsigned last)
{
    unsigned char saved_index = (unsigned char)inp(index_port);
    unsigned index;

    for (index = first; index <= last; ++index) {
        vga_values[index - first] = read_indexed(index_port,
                                                 (unsigned char)index);
    }
    outp(index_port, saved_index);
    wr_hex_block(prefix, vga_values, last - first + 1u);
}

/*
 * The attribute controller is the one part of the Tier 1 dump that has to be
 * got exactly right. Reading it means clearing the palette-address-source bit,
 * which blanks the display until it is set again, so the flip-flop is reset
 * before every access and the closing write restores index 0 with the bit set.
 * On a text-mode DOS screen the effect is a single frame of flicker.
 */
static void dump_attribute_controller(unsigned status_port)
{
    unsigned index;

    for (index = 0u; index <= 0x14u; ++index) {
        (void)inp(status_port);
        outp(0x03c0u, index);
        vga_values[index] = (unsigned char)inp(0x03c1u);
    }
    (void)inp(status_port);
    outp(0x03c0u, 0x20u);
    wr_hex_block("Atc", vga_values, 0x15u);
}

static void survey_vga_registers(void)
{
    unsigned index_port = crtc_index_port();
    unsigned status_port = (index_port == 0x03d4u) ? 0x03dau : 0x03bau;

    wr_section("VGARegisters");
    wr_status("ok");
    wr_str("Access", "read-only");
    /*
     * Under Windows the virtual display driver traps VGA port I/O and hands
     * back per-VM values rather than what the silicon holds. The numbers still
     * mean something, but nothing about the chipset can be concluded from
     * them, so the report says which kind of data this is and the host-side
     * parser refuses to identify a chip from a virtualised capture.
     */
    wr_str("Trust", windows_present ? "virtualized" : "hardware");
    wr_x16("CrtcIndexPort", (unsigned short)index_port);
    wr_x8("Misc", (unsigned char)inp(0x03ccu));
    wr_x8("FeatureControl", (unsigned char)inp(0x03cau));
    wr_x8("InputStatus0", (unsigned char)inp(0x03c2u));
    wr_x8("DacState", (unsigned char)inp(0x03c7u));
    wr_x8("DacPixelMask", (unsigned char)inp(0x03c6u));

    /* The extended ranges are read without unlocking anything. A locked chip
     * returns whatever it returns, which is recorded as-is; several families
     * leak a usable identity there regardless, and reading costs nothing. */
    dump_indexed_range("Seq", 0x03c4u, 0x00u, 0x1fu);
    dump_indexed_range("Crtc", index_port, 0x00u, 0x3fu);
    dump_indexed_range("Gdc", 0x03ceu, 0x00u, 0x08u);
    dump_attribute_controller(status_port);
}

/* ------------------------------------------------------------------ */
/* Tier 2 - opt-in vendor probes                                       */
/* ------------------------------------------------------------------ */

static void tier2_s3(void)
{
    unsigned index_port = crtc_index_port();
    unsigned char saved_crtc_index = (unsigned char)inp(index_port);
    unsigned char saved_seq_index = (unsigned char)inp(0x03c4u);
    unsigned char saved_lock1;
    unsigned char saved_lock2;
    unsigned char saved_seq_unlock;
    unsigned char cr[16];
    unsigned char sr[8];

    wr_section("Chipset.S3");
    wr_status("ok");
    wr_str("Method", "cr38-cr39-and-sr08-unlock");

    /* The same unlock and restore the display driver performs in
     * src/display16/ddi.c; CR38 and CR39 gate the extended CRTC bank. */
    saved_lock1 = read_indexed(index_port, 0x38u);
    saved_lock2 = read_indexed(index_port, 0x39u);
    write_indexed(index_port, 0x38u, 0x48u);
    write_indexed(index_port, 0x39u, 0xa5u);
    cr[0] = read_indexed(index_port, 0x2du);
    cr[1] = read_indexed(index_port, 0x2eu);
    cr[2] = read_indexed(index_port, 0x2fu);
    cr[3] = read_indexed(index_port, 0x30u);
    cr[4] = read_indexed(index_port, 0x36u);
    cr[5] = read_indexed(index_port, 0x37u);
    cr[6] = read_indexed(index_port, 0x58u);
    cr[7] = read_indexed(index_port, 0x59u);
    cr[8] = read_indexed(index_port, 0x5au);
    cr[9] = read_indexed(index_port, 0x40u);
    cr[10] = read_indexed(index_port, 0x53u);
    write_indexed(index_port, 0x39u, saved_lock2);
    write_indexed(index_port, 0x38u, saved_lock1);
    outp(index_port, saved_crtc_index);

    wr_x8("DeviceIdHigh", cr[0]);
    wr_x8("DeviceIdLow", cr[1]);
    wr_x8("Revision", cr[2]);
    wr_x8("ChipId", cr[3]);
    wr_x8("CR36", cr[4]);
    wr_x8("CR37", cr[5]);
    wr_x8("CR58", cr[6]);
    wr_x8("CR59", cr[7]);
    wr_x8("CR5A", cr[8]);
    wr_x8("CR40", cr[9]);
    wr_x8("CR53", cr[10]);
    wr_x32("LinearApertureBase",
           ((unsigned long)cr[7] << 24) | ((unsigned long)cr[8] << 16));

    /* SR08 gates the extended sequencer bank that holds the MCLK PLL. */
    outp(0x03c4u, 0x08u);
    saved_seq_unlock = (unsigned char)inp(0x03c5u);
    outp(0x03c5u, 0x06u);
    sr[0] = read_indexed(0x03c4u, 0x10u);
    sr[1] = read_indexed(0x03c4u, 0x11u);
    sr[2] = read_indexed(0x03c4u, 0x12u);
    sr[3] = read_indexed(0x03c4u, 0x13u);
    sr[4] = read_indexed(0x03c4u, 0x15u);
    outp(0x03c4u, 0x08u);
    outp(0x03c5u, saved_seq_unlock);
    outp(0x03c4u, saved_seq_index);

    wr_x8("SR10", sr[0]);
    wr_x8("SR11", sr[1]);
    wr_x8("SR12", sr[2]);
    wr_x8("SR13", sr[3]);
    wr_x8("SR15", sr[4]);
    wr_str("Restored", "yes");
}

static void tier2_cirrus(void)
{
    unsigned index_port = crtc_index_port();
    unsigned char saved_crtc_index = (unsigned char)inp(index_port);
    unsigned char saved_seq_index = (unsigned char)inp(0x03c4u);
    unsigned char saved_sr06;
    unsigned char unlocked;
    unsigned char sr0f;
    unsigned char sr17;
    unsigned char sr1f;
    unsigned char cr27;

    wr_section("Chipset.Cirrus");
    wr_str("Method", "sr06-key-unlock");

    saved_sr06 = read_indexed(0x03c4u, 0x06u);
    write_indexed(0x03c4u, 0x06u, 0x12u);
    unlocked = read_indexed(0x03c4u, 0x06u);
    if (unlocked != 0x12u) {
        write_indexed(0x03c4u, 0x06u, 0x0fu);
        outp(0x03c4u, saved_seq_index);
        wr_status("unavailable");
        wr_str("Reason", "sr06-key-not-accepted");
        wr_str("Restored", "yes");
        return;
    }

    sr0f = read_indexed(0x03c4u, 0x0fu);
    sr17 = read_indexed(0x03c4u, 0x17u);
    sr1f = read_indexed(0x03c4u, 0x1fu);
    cr27 = read_indexed(index_port, 0x27u);
    /* 0Fh is the documented lock value; the original SR06 contents are a key
     * latch rather than state, so re-locking is the correct restore. */
    write_indexed(0x03c4u, 0x06u, 0x0fu);
    outp(0x03c4u, saved_seq_index);
    outp(index_port, saved_crtc_index);

    wr_status("ok");
    wr_x8("SavedSR06", saved_sr06);
    wr_x8("SR0F", sr0f);
    wr_x8("SR17", sr17);
    wr_x8("SR1F", sr1f);
    wr_x8("CR27", cr27);
    wr_str("Restored", "yes");
}

/*
 * Records where a family's registers live without going there.
 *
 * The excluded families all need a probe this tool has no safe way to make.
 * Mach64 registers sit at a sparse I/O base recovered from a table inside the
 * video BIOS, so reaching them means firing reads at port addresses derived
 * from a ROM on a card nobody here has seen. Trident's chip ID read switches
 * the register file into "new mode" as a side effect. Tseng's unlock writes
 * 3BFh and 3D8h, which are the Hercules and CGA mode-control ports - on a
 * non-Tseng chip emulating CGA that changes the display mode. Matrox, nVidia
 * and 3dfx keep everything in MMIO above 1 MB, which real mode cannot reach
 * without unreal mode or a DPMI mapping.
 *
 * Each is still named in the report, so a survey from one of these cards is
 * self-explanatory rather than mysteriously empty. The Tier 1 capture - full
 * config space, the whole ROM image, and the unlocked-nothing extended
 * register banks - is what these cards contribute instead.
 */
static void tier2_unsupported(const char *section, const char *reason)
{
    wr_section(section);
    wr_status("unsupported");
    wr_str("Reason", reason);
}

static void run_tier2(void)
{
    unsigned index;

    if (display_device_count == 0u) {
        tier2_unsupported("Chipset", "no-pci-display-device-identified");
        return;
    }

    for (index = 0u; index < display_device_count; ++index) {
        switch (display_devices[index].vendor_id) {
        case 0x5333u:
            tier2_s3();
            break;
        case 0x1013u:
            tier2_cirrus();
            break;
        case 0x1002u:
            tier2_unsupported("Chipset.ATI",
                              "io-base-derived-from-rom-table");
            break;
        case 0x1023u:
            tier2_unsupported("Chipset.Trident",
                              "sr0b-read-switches-register-mode");
            break;
        case 0x100cu:
            tier2_unsupported("Chipset.Tseng",
                              "unlock-writes-nonindexed-cga-port");
            break;
        case 0x102bu:
        case 0x10deu:
        case 0x12d2u:
        case 0x121au:
        case 0x1039u:
            tier2_unsupported("Chipset",
                              "mmio-requires-protected-mode");
            break;
        default:
            tier2_unsupported("Chipset",
                              "no-port-level-probe-for-this-vendor");
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Startup                                                             */
/* ------------------------------------------------------------------ */

static void print_usage(void)
{
    puts("Velocity9x VGA hardware survey (" V9X_BUILD_ID ")");
    puts("");
    puts("  V9XSURV            run the survey, ask about vendor probes");
    puts("  V9XSURV /tier2     run vendor probes without asking");
    puts("  V9XSURV /notier2   skip vendor probes without asking");
    puts("  V9XSURV /rom       include the whole video BIOS image");
    puts("  V9XSURV /out:PATH  write the report somewhere else");
    puts("  V9XSURV /note:TEXT record what the card physically is");
    puts("                     (or put it in V9XNOTE.TXT next to this file)");
    puts("");
    puts("No video mode is changed and nothing is written to the card.");
}

/*
 * Ask before Tier 2 rather than assuming. Under Windows the answer defaults to
 * no: a DOS box shares the adapter with the display driver, so an unlock and
 * restore that is harmless on bare DOS races against whatever Windows is doing
 * with the same registers.
 */
static int ask_tier2(void)
{
    int answer;

    puts("");
    puts("Vendor-specific register probe");
    puts("------------------------------");
    puts("This writes documented unlock keys for your card's chipset family,");
    puts("reads the registers behind them, and puts the originals back. It is");
    puts("where the memory size, clock and aperture details come from.");
    puts("");
    if (windows_present) {
        puts("Windows is running, so this is NOT recommended - the display");
        puts("driver owns these registers right now. Say no, and if you can,");
        puts("re-run this from MS-DOS mode instead.");
        printf("Run the vendor probe anyway? [y/N] ");
    } else {
        puts("The basic report is already written to disk before this runs,");
        puts("so declining still gives us a useful file.");
        printf("Run the vendor probe? [Y/n] ");
    }
    fflush(stdout);

    answer = getchar();
    puts("");
    if (answer == 'y' || answer == 'Y') return 1;
    if (answer == 'n' || answer == 'N') return 0;
    return windows_present ? 0 : 1;
}

/* Fall back to the executable's own directory when the report cannot be
 * written to C:\ - a machine booted from read-only media still has to produce
 * a file the tester can send. */
static void derive_fallback_path(const char *argv0)
{
    unsigned length = 0u;
    unsigned cut = 0u;

    if (argv0 == 0 || argv0[0] == '\0') {
        strcpy(report_path, "V9XSURV.INI");
        return;
    }
    while (argv0[length] != '\0' && length < 100u) {
        if (argv0[length] == '\\' || argv0[length] == ':') cut = length + 1u;
        ++length;
    }
    memcpy(report_path, argv0, cut);
    strcpy(report_path + cut, "V9XSURV.INI");
}

static void write_header(int argc, char **argv)
{
    union REGS input;
    union REGS output;
    char text[80];
    int index;

    fprintf(report, "; Velocity9x VGA hardware survey\n");
    fprintf(report, "; Send this whole file back unmodified.\n");
    wr_section("Report");
    wr_str("SchemaVersion", V9X_SURVEY_SCHEMA);
    wr_str("Tool", "V9XSURV");
    wr_str("Build", V9X_BUILD_ID);
    wr_str("Access", "query-only");

    memset(&input, 0, sizeof(input));
    input.h.ah = 0x2au;
    int86(0x21, &input, &output);
    sprintf(text, "%04u-%02u-%02u", output.x.cx, output.h.dh, output.h.dl);
    wr_str("Date", text);

    memset(&input, 0, sizeof(input));
    input.h.ah = 0x2cu;
    int86(0x21, &input, &output);
    sprintf(text, "%02u:%02u:%02u", output.h.ch, output.h.cl, output.h.dh);
    wr_str("Time", text);

    text[0] = '\0';
    for (index = 1; index < argc; ++index) {
        if (strlen(text) + strlen(argv[index]) + 2u >= sizeof(text)) break;
        if (text[0] != '\0') strcat(text, " ");
        strcat(text, argv[index]);
    }
    wr_str("CommandLine", text);
    wr_str("Note", survey_note);
}

/*
 * What the card physically is - the sticker, the markings on the chip, the box
 * it came in - is something no register can tell us and the tester can. DOS
 * mangles quoting badly enough that /note: alone is not enough, so a
 * V9XNOTE.TXT sitting next to the executable is accepted as well and is the
 * route worth recommending.
 */
static void load_note_file(const char *argv0)
{
    char path[128];
    unsigned length = 0u;
    unsigned cut = 0u;
    FILE *note;
    int value;
    unsigned at;

    if (survey_note[0] != '\0') return;
    if (argv0 == 0 || argv0[0] == '\0') return;
    while (argv0[length] != '\0' && length < 100u) {
        if (argv0[length] == '\\' || argv0[length] == ':') cut = length + 1u;
        ++length;
    }
    memcpy(path, argv0, cut);
    strcpy(path + cut, "V9XNOTE.TXT");

    note = fopen(path, "rt");
    if (note == 0) return;
    at = 0u;
    while (at + 1u < sizeof(survey_note) && (value = fgetc(note)) != EOF) {
        if (value < 0x20 || value > 0x7e) value = ' ';
        survey_note[at++] = (char)value;
    }
    survey_note[at] = '\0';
    fclose(note);
}

static void restore_break_state(void)
{
    union REGS input;
    union REGS output;

    memset(&input, 0, sizeof(input));
    input.x.ax = 0x3301u;
    input.h.dl = saved_break_state;
    int86(0x21, &input, &output);
}

int main(int argc, char **argv)
{
    int index;
    int tier2_forced = 0;
    int tier2_refused = 0;
    const char *requested_path = V9X_SURVEY_DEFAULT;

    for (index = 1; index < argc; ++index) {
        const char *option = argv[index];

        if (option[0] != '/' && option[0] != '-') continue;
        ++option;
        if (stricmp(option, "tier2") == 0) {
            tier2_forced = 1;
        } else if (stricmp(option, "notier2") == 0) {
            tier2_refused = 1;
        } else if (stricmp(option, "rom") == 0) {
            want_full_rom = 1;
        } else if (strnicmp(option, "out:", 4) == 0) {
            requested_path = option + 4;
        } else if (strnicmp(option, "note:", 5) == 0) {
            strncpy(survey_note, option + 5, sizeof(survey_note) - 1u);
            survey_note[sizeof(survey_note) - 1u] = '\0';
        } else if (option[0] == '?' || stricmp(option, "help") == 0) {
            print_usage();
            return 0;
        }
    }
    load_note_file(argv[0]);

    /*
     * Take Ctrl-Break out of play for the duration. The VGA register dump has
     * a handful of short windows where an index has been written and not yet
     * restored, and DOS aborting the program inside one of those would leave
     * the adapter in a state the tester has to reboot out of.
     */
    {
        union REGS input;
        union REGS output;

        memset(&input, 0, sizeof(input));
        input.x.ax = 0x3300u;
        int86(0x21, &input, &output);
        saved_break_state = output.h.dl;
        memset(&input, 0, sizeof(input));
        input.x.ax = 0x3301u;
        input.h.dl = 0x00u;
        int86(0x21, &input, &output);
    }

    strncpy(report_path, requested_path, sizeof(report_path) - 1u);
    report_path[sizeof(report_path) - 1u] = '\0';
    report = fopen(report_path, "wt");
    if (report == 0) {
        derive_fallback_path(argv[0]);
        report = fopen(report_path, "wt");
    }
    if (report == 0) {
        restore_break_state();
        puts("Velocity9x survey: could not create a report file.");
        puts("Try V9XSURV /out:A:\\V9XSURV.INI to write to a floppy instead.");
        return 3;
    }

    puts("Velocity9x VGA hardware survey (" V9X_BUILD_ID ")");
    puts("Collecting. No video mode will be changed.");

    write_header(argc, argv);
    survey_system();
    survey_bios_data();
    survey_pci_bios();
    survey_pci_devices();
    for (index = 0; index < (int)display_device_count; ++index) {
        survey_display_device((unsigned)index);
    }
    survey_rom_image("VideoBios", 0xc000u, 1);
    survey_secondary_roms();
    survey_vbe();
    survey_edid();
    survey_vga_registers();

    /* Tier 1 is committed to disk before anything vendor-specific runs, so a
     * probe that wedges an unfamiliar card still leaves a usable report. */
    wr_section("Tier1");
    wr_status("ok");
    fclose(report);

    if (tier2_forced && !tier2_refused) {
        tier2_enabled = 1;
        tier2_reason = "command-line";
    } else if (tier2_refused) {
        tier2_enabled = 0;
        tier2_reason = "command-line";
    } else {
        tier2_enabled = ask_tier2();
        tier2_reason = "prompted";
    }

    report = fopen(report_path, "at");
    if (report == 0) {
        restore_break_state();
        puts("Velocity9x survey: the report was written but could not be");
        puts("reopened for the vendor probe. Send what is on disk.");
        return 1;
    }

    wr_section("Tier2");
    wr_str("Requested", tier2_enabled ? "yes" : "no");
    wr_str("Decision", tier2_reason);
    if (tier2_enabled) {
        wr_status("ok");
        run_tier2();
    } else {
        wr_status("declined");
    }

    /* Complete is the last key written. A report without it was cut off part
     * way - by a full disk, a power cycle, or a mangled transfer - and the
     * host-side parser rejects it rather than drawing conclusions from a
     * fragment. */
    wr_section("Result");
    wr_str("Status", display_device_count != 0u ? "PASS" : "PARTIAL");
    wr_u("DisplayDeviceCount", display_device_count);
    wr_str("Complete", "yes");
    fclose(report);
    restore_break_state();

    puts("");
    printf("Report written to %s\n", report_path);
    puts("Please send that file back. It contains hardware identifiers only.");
    if (display_device_count == 0u) {
        puts("");
        puts("No PCI display adapter was found. If this card is ISA or VLB");
        puts("that is expected - the report is still worth sending.");
        return 2;
    }
    return 0;
}
