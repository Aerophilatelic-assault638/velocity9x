#include <dos.h>
#include <stdio.h>
#include <string.h>

#ifndef V9X_BUILD_ID
#define V9X_BUILD_ID "local"
#endif

#define V9X_VBE_REPORT "C:\\V9XVBE.TXT"

static unsigned char controller_info[512];
static unsigned char mode_info[256];

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

static unsigned short v9x_vbe_call(unsigned short function,
                                   unsigned short argument,
                                   void far *buffer)
{
    union REGS input;
    union REGS output;
    struct SREGS segments;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    segread(&segments);
    input.x.ax = function;
    input.x.cx = argument;
    if (buffer != 0) {
        segments.es = FP_SEG(buffer);
        input.x.di = FP_OFF(buffer);
    }
    int86x(0x10, &input, &output, &segments);
    return output.x.ax;
}

static void v9x_report_mode(FILE *report, unsigned short mode)
{
    unsigned short status;
    memset(mode_info, 0, sizeof(mode_info));
    status = v9x_vbe_call(0x4f01u, mode, mode_info);
    fprintf(report, "Mode%04X.Status=%04X\n", mode, status);
    if (status != 0x004fu) return;
    fprintf(report, "Mode%04X.Attributes=%04X\n", mode, v9x_u16(mode_info));
    fprintf(report, "Mode%04X.Width=%u\n", mode, v9x_u16(mode_info + 18));
    fprintf(report, "Mode%04X.Height=%u\n", mode, v9x_u16(mode_info + 20));
    fprintf(report, "Mode%04X.Planes=%u\n", mode, mode_info[24]);
    fprintf(report, "Mode%04X.BitsPerPixel=%u\n", mode, mode_info[25]);
    fprintf(report, "Mode%04X.MemoryModel=%u\n", mode, mode_info[27]);
    fprintf(report, "Mode%04X.BytesPerScanLine=%u\n", mode,
            v9x_u16(mode_info + 16));
    fprintf(report, "Mode%04X.PhysicalBase=%08lX\n", mode,
            v9x_u32(mode_info + 40));
    fprintf(report, "Mode%04X.LinearBytesPerScanLine=%u\n", mode,
            v9x_u16(mode_info + 50));
    fprintf(report, "Mode%04X.RedMask=%u@%u\n", mode,
            mode_info[31], mode_info[32]);
    fprintf(report, "Mode%04X.GreenMask=%u@%u\n", mode,
            mode_info[33], mode_info[34]);
    fprintf(report, "Mode%04X.BlueMask=%u@%u\n", mode,
            mode_info[35], mode_info[36]);
}

int main(void)
{
    static const unsigned short modes[] = {
        0x0101u, 0x0103u, 0x0105u, 0x0111u, 0x0114u, 0x0117u
    };
    FILE *report;
    unsigned short status;
    unsigned short index;
    union REGS input;
    union REGS output;

    report = fopen(V9X_VBE_REPORT, "wt");
    if (report == 0) return 1;
    fprintf(report, "Velocity9x VBE inventory\n");
    fprintf(report, "Build=%s\n", V9X_BUILD_ID);
    fprintf(report, "Access=query-only\n");

    memset(controller_info, 0, sizeof(controller_info));
    memcpy(controller_info, "VBE2", 4);
    status = v9x_vbe_call(0x4f00u, 0u, controller_info);
    fprintf(report, "ControllerStatus=%04X\n", status);
    if (status == 0x004fu) {
        fprintf(report, "Signature=%.4s\n", controller_info);
        fprintf(report, "Version=%04X\n", v9x_u16(controller_info + 4));
        fprintf(report, "TotalMemory64K=%u\n", v9x_u16(controller_info + 18));
    }

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    input.x.ax = 0x4f03u;
    int86(0x10, &input, &output);
    fprintf(report, "CurrentModeStatus=%04X\n", output.x.ax);
    if (output.x.ax == 0x004fu) {
        fprintf(report, "CurrentMode=%04X\n", output.x.bx);
    }

    for (index = 0; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        v9x_report_mode(report, modes[index]);
    }
    fprintf(report, "Result=%s\n", status == 0x004fu ? "PASS" : "FAIL");
    fclose(report);
    puts("Velocity9x VBE inventory complete; no mode was changed.");
    return status == 0x004fu ? 0 : 2;
}
