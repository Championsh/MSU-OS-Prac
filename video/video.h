#ifndef VIDEO_H
#define VIDEO_H

#include "fs/pci.h"
#include "fs/pci_classes.h"

#include <inc/x86.h>
#include <inc/string.h>
#include <inc/lib.h>
#include <inc/video.h>

// SVGA external ports
#define SVGA_INDEX (0)
#define SVGA_VALUE (1)
#define SVGA_BIOS (2)
#define SVGA_IRQSTATUS (8)

// SVGA internal ports
#define SVGA_REG_ID (0) // register used to negociate specification ID
#define SVGA_REG_ENABLE (1) // flag set by the driver when the device should enter SVGA mode
#define SVGA_REG_WIDTH (2) // current screen width
#define SVGA_REG_HEIGHT (3) // current screen height
#define SVGA_REG_MAX_WIDTH (4) // maximum supported screen width
#define SVGA_REG_MAX_HEIGHT (5) // maximum supported screen height
#define SVGA_REG_BPP (7) // current screen bits per pixel
#define SVGA_REG_FB_START (13) // address in system memory of the frame buffer
#define SVGA_REG_FB_OFFSET (14) // offset in the frame buffer to the visible pixel data
#define SVGA_REG_VRAM_SIZE (15) // size of the video RAM
#define SVGA_REG_FB_SIZE (16) // size of the frame buffer
#define SVGA_REG_CAPABILITIES (17) // device capabilities
#define SVGA_REG_FIFO_START (18) // address in system memory of the FIFO
#define SVGA_REG_FIFO_SIZE (19) // FIFO size
#define SVGA_REG_CONFIG_DONE (20) // flag to enable FIFO operation
#define SVGA_REG_SYNC (21) // flag set by the driver to flush FIFO changes
#define SVGA_REG_BUSY (22) // flag set by the FIFO when it's processed

#define SVGA_FIFO_MIN 0
#define SVGA_FIFO_MAX 1
#define SVGA_FIFO_NEXT_CMD 2
#define SVGA_FIFO_STOP 3
#define SVGA_FIFO_CAPABILITIES 4
#define SVGA_FIFO_FLAGS 5

#define SVGA_FIFO_REGISTERS_SIZE 293

#define SVGA_CAP_NONE               0x00000000
#define SVGA_CAP_RECT_COPY          0x00000002
#define SVGA_CAP_CURSOR             0x00000020
#define SVGA_CAP_CURSOR_BYPASS      0x00000040   // Legacy (Use Cursor Bypass 3 instead)
#define SVGA_CAP_CURSOR_BYPASS_2    0x00000080   // Legacy (Use Cursor Bypass 3 instead)
#define SVGA_CAP_8BIT_EMULATION     0x00000100
#define SVGA_CAP_ALPHA_CURSOR       0x00000200
#define SVGA_CAP_3D                 0x00004000
#define SVGA_CAP_EXTENDED_FIFO      0x00008000
#define SVGA_CAP_MULTIMON           0x00010000   // Legacy multi-monitor support
#define SVGA_CAP_PITCHLOCK          0x00020000
#define SVGA_CAP_IRQMASK            0x00040000
#define SVGA_CAP_DISPLAY_TOPOLOGY   0x00080000   // Legacy multi-monitor support
#define SVGA_CAP_GMR                0x00100000
#define SVGA_CAP_TRACES             0x00200000
#define SVGA_CAP_GMR2               0x00400000
#define SVGA_CAP_SCREEN_OBJECT_2    0x00800000

#define MAX_OPEN_WINDOWS 128
#define MAX_OPEN_RENDERERS 128
#define MAX_OPEN_TEXTURES 128


#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF

#define VBE_DISPI_INDEX_ID (0)
#define VBE_DISPI_INDEX_XRES (1)
#define VBE_DISPI_INDEX_YRES (2)
#define VBE_DISPI_INDEX_BPP (3)
#define VBE_DISPI_INDEX_ENABLE (4)
#define VBE_DISPI_INDEX_BANK (5)
#define VBE_DISPI_INDEX_VIRT_WIDTH (6)
#define VBE_DISPI_INDEX_VIRT_HEIGHT (7)
#define VBE_DISPI_INDEX_X_OFFSET (8)
#define VBE_DISPI_INDEX_Y_OFFSET (9)

#define VBE_DISPI_DISABLED 0x00
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_NOCLEARMEM 0x80

enum adapters {
    GRAPHICS_ADAPTER_ANY = 0,
    GRAPHICS_ADAPTER_VMWARE,
    GRAPHICS_ADAPTER_BOCHS_VBE
};

enum svga_error {
    SVGA_OK = 0,
    SVGA_MAP_ERR = 1,
    SVGA_PORT_ERR = 2,
    SVGA_FIFO_ERR = 3
};

struct GraphicsAdapter {
    struct PciDevice * pcidev;

    uintptr_t io_base;
    volatile uint32_t * fb_base_addr;
    volatile uint32_t * fifo;
    volatile uint32_t * fb;

    uint32_t framebuffer_size;
    uint32_t fifo_size;

    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t capabilities;
};

struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// struct color COLOR_WHITE = {255, 255, 255, 255};
// struct color COLOR_BLACK = {0, 0, 0, 255};
// struct color COLOR_RED = {255, 0, 0, 255};
// struct color COLOR_GREEN = {0, 255, 0, 255};
// struct color COLOR_BLUE = {0, 0, 255, 255};

int svga_read_io_bar(struct GraphicsAdapter * adapter);
uint32_t svga_read(struct GraphicsAdapter * adapter, int reg);
void svga_write(struct GraphicsAdapter * adapter, int reg, int value);
int svga_map(struct GraphicsAdapter * adapter);
int svga_read_resolution(struct GraphicsAdapter * adapter);
int svga_set_resolution(struct GraphicsAdapter * adapter, uint32_t w, uint32_t h);
int svga_set_spec_id(struct GraphicsAdapter * adapter);
void svga_fill(struct GraphicsAdapter * adapter, uint32_t col);
int fifo_init(struct GraphicsAdapter * adapter);
int svga_init();
struct GraphicsAdapter * get_svga();
int bga_init(uint32_t width, uint32_t height);
int graphics_init(uint32_t width, uint32_t height, int adapter_type);


#endif //VIDEO_H
