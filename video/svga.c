#include "video.h"

#include "fs/pci.h"

static struct GraphicsAdapter svga;

int svga_read_io_bar(struct GraphicsAdapter * adapter) {
    const size_t bar_size = get_bar_size(adapter->pcidev, 0);
    const uintptr_t io_port_base = get_bar_address(adapter->pcidev, 0);

    if (!bar_size || !io_port_base)
        return -SVGA_MAP_ERR;

    adapter->io_base = io_port_base;

    return SVGA_OK;
}

uint32_t svga_read(struct GraphicsAdapter * adapter, int reg) {
    outl(adapter->io_base + SVGA_INDEX, reg);
    return inl(adapter->io_base + SVGA_VALUE);
}

void svga_write(struct GraphicsAdapter * adapter, int reg, int value) {
    outl(adapter->io_base + SVGA_INDEX, reg);
    outl(adapter->io_base + SVGA_VALUE, value);
}

int svga_map(struct GraphicsAdapter * adapter) {
    int res;
    int my_envid = sys_getenvid();

    adapter->fb_base_addr = (volatile uint32_t *) VGA_FB_VADDR;
    adapter->fifo = (volatile uint32_t *) VGA_FIFO_VADDR;

    size_t fb_size = svga_read(adapter, SVGA_REG_FB_SIZE);
    uintptr_t fb_start = svga_read(adapter, SVGA_REG_FB_START);
    uintptr_t fb_offset = svga_read(adapter, SVGA_REG_FB_OFFSET);

    adapter->framebuffer_size = fb_size;
    adapter->fb = adapter->fb_base_addr + fb_offset;

    res = sys_map_physical_region(fb_start, my_envid, (void*)adapter->fb_base_addr, fb_size, PROT_RW | PROT_CD);
    if (res)
        return -SVGA_MAP_ERR;

    size_t fifo_size = svga_read(adapter, SVGA_REG_FIFO_SIZE);
    uintptr_t fifo_start = svga_read(adapter, SVGA_REG_FIFO_START);

    adapter->fifo_size = fifo_size;

    res = sys_map_physical_region(fifo_start, my_envid, (void*)adapter->fifo, fifo_size, PROT_RW | PROT_CD);
    if (res)
        return -SVGA_MAP_ERR;

    return SVGA_OK;
}

int svga_read_resolution(struct GraphicsAdapter * adapter) {
    adapter->width = svga_read(adapter, SVGA_REG_WIDTH);
    adapter->height = svga_read(adapter, SVGA_REG_HEIGHT);
    adapter->bits_per_pixel = svga_read(adapter, SVGA_REG_BPP);

    return SVGA_OK;
}

int svga_set_resolution(struct GraphicsAdapter * adapter, uint32_t w, uint32_t h) {
    svga_write(adapter, SVGA_REG_ENABLE, 0);
    svga_write(adapter, SVGA_REG_WIDTH, w);
    svga_write(adapter, SVGA_REG_HEIGHT, h);
    svga_write(adapter, SVGA_REG_BPP, 32);
    svga_write(adapter, SVGA_REG_ENABLE, 0);

    adapter->fb = adapter->fb_base_addr + svga_read(adapter, SVGA_REG_FB_OFFSET);
    adapter->width = w;
    adapter->height = h;
    adapter->bits_per_pixel = 32;

    return SVGA_OK;
}

int svga_set_spec_id(struct GraphicsAdapter * adapter) {
    const int spec_id = 0x90000002;
    svga_write(adapter, SVGA_REG_ID, spec_id);
    if (svga_read(adapter, SVGA_REG_ID) != spec_id)
        return SVGA_PORT_ERR;

    return SVGA_OK;
}

void svga_fill(struct GraphicsAdapter * adapter, uint32_t col) {
    for (int x = 0; x < adapter->width; x++) {
        for (int y = 0; y < adapter->height; y++) {
            adapter->fb[y * adapter->width + x] = col;
        }
    }
}

int fifo_init(struct GraphicsAdapter * adapter) {
    volatile uint32_t * fifo = adapter->fifo;

    fifo[SVGA_FIFO_MIN] = fifo[SVGA_FIFO_STOP] = fifo[SVGA_FIFO_NEXT_CMD] = SVGA_FIFO_REGISTERS_SIZE * sizeof(uint32_t);
    fifo[SVGA_FIFO_MAX] = adapter->fifo_size;

    svga_write(adapter, SVGA_REG_CONFIG_DONE, 1);

    if (svga_read(adapter, SVGA_REG_CONFIG_DONE) != 1) {
        return -SVGA_FIFO_ERR;
    }

    return SVGA_OK;
}

uint32_t svga_get_capabilities(struct  GraphicsAdapter * adapter) {
    return svga_read(adapter, SVGA_REG_CAPABILITIES);
}

uint32_t fifo_get_capabilities(const struct GraphicsAdapter * adapter) {
    return adapter->fifo[SVGA_FIFO_CAPABILITIES];
}

struct GraphicsAdapter* get_svga() {
    return &svga;
}

int svga_init(void) {
    struct GraphicsAdapter * adapter = &svga;
    int err;

    struct PciDevice *pcidevice = find_pci_dev(PCI_CLASS_DISPLAY, 0);
    if (pcidevice == NULL)
        panic("VGA device not found\n");

    adapter->pcidev = pcidevice;

    err = svga_read_io_bar(adapter);
    if (err)
        return err;

    err = svga_set_spec_id(adapter);
    if (err)
        return err;

    err = svga_map(adapter);
    if (err)
        return err;

    cprintf("vmware svga capabilities: 0x%X\n", svga_get_capabilities(adapter));

    svga_read_resolution(adapter);

    return SVGA_OK;
}

uint16_t bga_read(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

void bga_write(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

int bga_get_version(void) {
    return bga_read(VBE_DISPI_INDEX_ID);
}

int bga_set_display_mode(struct GraphicsAdapter * adapter, uint16_t width, uint16_t height, uint32_t bit_depth, uint32_t use_linear_framebuffer, uint32_t clear_video_memory) {
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, width);
    bga_write(VBE_DISPI_INDEX_YRES, height);
    bga_write(VBE_DISPI_INDEX_BPP, bit_depth);
    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED |
                                             (use_linear_framebuffer ? VBE_DISPI_LFB_ENABLED : 0) |
                                             (clear_video_memory ? 0 : VBE_DISPI_NOCLEARMEM));

    adapter->width = width;
    adapter->height = height;
    adapter->bits_per_pixel = bit_depth;

    return VIDEO_OK;
}

int bga_map_fb(struct GraphicsAdapter * adapter) {
    adapter->fb_base_addr =  adapter->fb = (volatile uint32_t *) VGA_FB_VADDR;

    const size_t bar_size = get_bar_size(adapter->pcidev, 0);
    const uintptr_t fb_base = get_bar_address(adapter->pcidev, 0);

    cprintf("bar_size = %lu, fb_base = %p\n", bar_size, (void*)fb_base);

    if (!bar_size || !fb_base)
        return -SVGA_MAP_ERR;

    int res = sys_map_physical_region(fb_base, 0, (void*)adapter->fb_base_addr, bar_size, PROT_RW);
    if (res)
        return -SVGA_MAP_ERR;

    return SVGA_OK;
}

int bga_init(uint32_t width, uint32_t hegiht) {
    int version = bga_get_version();
    struct GraphicsAdapter * adapter = &svga;
    struct PciDevice *pcidevice = find_pci_dev(PCI_CLASS_DISPLAY, 0);
    int res;
    adapter->pcidev = pcidevice;

    cprintf("BGA version: 0x%X\n", version);
    res = bga_set_display_mode(adapter, 640, 400, 32, 1, 1);
    if (res)
        return res;

    res = bga_map_fb(adapter);
    if (res)
        return res;

    return 0;
}

int graphics_init(uint32_t width, uint32_t height, int adapter_type) {
    int res;
    switch (adapter_type) {
        case GRAPHICS_ADAPTER_VMWARE:
            return svga_init();
        case GRAPHICS_ADAPTER_BOCHS_VBE:
            return bga_init(width, height);
        default:
            res = svga_init();
            if (res) {
                res = bga_init(width, height);
                if (res)
                    return res;
            }
            return res;
    }
}
