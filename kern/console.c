/* See COPYRIGHT for copyright information. */

#include <inc/assert.h>
#include <inc/kbdreg.h>
#include <inc/memlayout.h>
#include <inc/string.h>
#include <inc/trap.h>
#include <inc/uefi.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/picirq.h>
#include <kern/pmap.h>

#define COM1 0x3F8

#define COM_RX        0    /* IN:  Receive buffer (DLAB=0) */
#define COM_TX        0    /* OUT: Transmit buffer (DLAB=0) */
#define COM_DLL       0    /* OUT: Divisor Latch Low (DLAB=1) */
#define COM_DLM       1    /* OUT: Divisor Latch High (DLAB=1) */
#define COM_IER       1    /* OUT: Interrupt Enable Register */
#define COM_IER_RDI   0x01 /*     Enable receiver data interrupt */
#define COM_IIR       2    /* IN:  Interrupt ID Register */
#define COM_FCR       2    /* OUT: FIFO Control Register */
#define COM_LCR       3    /* OUT: Line Control Register */
#define COM_LCR_DLAB  0x80 /*     Divisor latch access bit */
#define COM_LCR_WLEN8 0x03 /*     Wordlength: 8 bits */
#define COM_MCR       4    /* OUT: Modem Control Register */
#define COM_MCR_RTS   0x02 /*     RTS complement */
#define COM_MCR_DTR   0x01 /*     DTR complement */
#define COM_MCR_OUT2  0x08 /* OUT2 complement */
#define COM_LSR       5    /* IN:  Line Status Register */
#define COM_LSR_DATA  0x01 /*     Data available */
#define COM_LSR_TXRDY 0x20 /*     Transmit buffer avail */
#define COM_LSR_TSRE  0x40 /*     Transmitter off */

#define TABW 5

static bool graphics_exists = false;
static uint32_t uefi_vres;
static uint32_t uefi_hres;
static uint32_t uefi_stride;
static uint32_t crt_rows;
static uint32_t crt_cols;
static uint32_t crt_size;
static uint16_t crt_pos;
static uint32_t *crt_buf = (uint32_t *)FRAMEBUFFER;

static bool serial_exists;

static void cons_intr(int (*proc)(void));
static void cons_putc(int c);

/* Text-mode framebuffer display output */

static char font8x8_basic[128][8] = {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0000 (nul) */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0001 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0002 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0003 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0004 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0005 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0006 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0007 */
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, /* U+0008 XXX CHANGED to clear any symbol */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0009 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000A */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000B */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000C */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000D */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000E */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+000F */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0010 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0011 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0012 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0013 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0014 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0015 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0016 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0017 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0018 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0019 */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001A */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001B */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001C */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001D */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001E */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+001F */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0020 (space) */
        {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, /* U+0021 (!) */
        {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0022 (") */
        {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, /* U+0023 (#) */
        {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, /* U+0024 ($) */
        {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, /* U+0025 (%) */
        {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, /* U+0026 (&) */
        {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0027 (') */
        {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, /* U+0028 (() */
        {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, /* U+0029 ()) */
        {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, /* U+002A (*) */
        {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, /* U+002B (+) */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, /* U+002C (,) */
        {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, /* U+002D (-) */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, /* U+002E (.) */
        {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, /* U+002F (/) */
        {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, /* U+0030 (0) */
        {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, /* U+0031 (1) */
        {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, /* U+0032 (2) */
        {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, /* U+0033 (3) */
        {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, /* U+0034 (4) */
        {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, /* U+0035 (5) */
        {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, /* U+0036 (6) */
        {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, /* U+0037 (7) */
        {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, /* U+0038 (8) */
        {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, /* U+0039 (9) */
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, /* U+003A (:) */
        {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, /* U+003B (//) */
        {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, /* U+003C (<) */
        {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, /* U+003D (=) */
        {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, /* U+003E (>) */
        {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, /* U+003F (?) */
        {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, /* U+0040 (@) */
        {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, /* U+0041 (A) */
        {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, /* U+0042 (B) */
        {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, /* U+0043 (C) */
        {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, /* U+0044 (D) */
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, /* U+0045 (E) */
        {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, /* U+0046 (F) */
        {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, /* U+0047 (G) */
        {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, /* U+0048 (H) */
        {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* U+0049 (I) */
        {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, /* U+004A (J) */
        {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, /* U+004B (K) */
        {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, /* U+004C (L) */
        {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, /* U+004D (M) */
        {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, /* U+004E (N) */
        {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, /* U+004F (O) */
        {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, /* U+0050 (P) */
        {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, /* U+0051 (Q) */
        {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, /* U+0052 (R) */
        {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, /* U+0053 (S) */
        {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* U+0054 (T) */
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, /* U+0055 (U) */
        {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* U+0056 (V) */
        {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, /* U+0057 (W) */
        {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, /* U+0058 (X) */
        {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, /* U+0059 (Y) */
        {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, /* U+005A (Z) */
        {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, /* U+005B ([) */
        {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, /* U+005C (\) */
        {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, /* U+005D (]) */
        {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, /* U+005E (^) */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, /* U+005F (_) */
        {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+0060 (`) */
        {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, /* U+0061 (a) */
        {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, /* U+0062 (b) */
        {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, /* U+0063 (c) */
        {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, /* U+0064 (d) */
        {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, /* U+0065 (e) */
        {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, /* U+0066 (f) */
        {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* U+0067 (g) */
        {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, /* U+0068 (h) */
        {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* U+0069 (i) */
        {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, /* U+006A (j) */
        {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, /* U+006B (k) */
        {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, /* U+006C (l) */
        {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, /* U+006D (m) */
        {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, /* U+006E (n) */
        {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, /* U+006F (o) */
        {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, /* U+0070 (p) */
        {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, /* U+0071 (q) */
        {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, /* U+0072 (r) */
        {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, /* U+0073 (s) */
        {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, /* U+0074 (t) */
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, /* U+0075 (u) */
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, /* U+0076 (v) */
        {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, /* U+0077 (w) */
        {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, /* U+0078 (x) */
        {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, /* U+0079 (y) */
        {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, /* U+007A (z) */
        {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, /* U+007B ({) */
        {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, /* U+007C (|) */
        {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, /* U+007D (}) */
        {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+007E (~) */
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* U+007F */
};

void
draw_char(uint32_t *buffer, uint32_t x, uint32_t y, uint32_t color, uint8_t charcode) {
    char *chr = font8x8_basic[(unsigned)charcode];
    uint32_t *buf = buffer + uefi_stride * SYMBOL_SIZE * y + SYMBOL_SIZE * x;

    for (size_t heigth = 0; heigth < 8; heigth++) {
        for (size_t width = 0; width < 8; width++) {
            buf[uefi_stride * heigth + width] = color * ((chr[heigth] >> width) & 1);
        }
    }
}

void
draw_char_stride(uint32_t *buffer, uint32_t x, uint32_t y, uint32_t color, uint32_t stride, uint8_t charcode) {
    char *chr = font8x8_basic[(unsigned)charcode];
    uint32_t *buf = buffer + stride * SYMBOL_SIZE * y + SYMBOL_SIZE * x;

    for (size_t heigth = 0; heigth < 8; heigth++) {
        for (size_t width = 0; width < 8; width++) {
            buf[stride * heigth + width] = color * ((chr[heigth] >> width) & 1);
        }
    }
}

void
fb_init(void) {
    LOADER_PARAMS *lp = (LOADER_PARAMS *)uefi_lp;
    uefi_vres = lp->VerticalResolution;
    uefi_hres = lp->HorizontalResolution;
    uefi_stride = lp->PixelsPerScanLine;
    crt_rows = uefi_vres / SYMBOL_SIZE;
    crt_cols = uefi_hres / SYMBOL_SIZE;
    crt_size = crt_rows * crt_cols;
    crt_pos = crt_cols;

    /* Clear screen */
    memset(crt_buf, 0, lp->FrameBufferSize);

    graphics_exists = true;
}

static void
fb_putc(int c) {
    if (!graphics_exists) return;

    /* If no attribute given, then use black on white */
    if (!(c & ~0xFF)) c |= 0x0700;

    switch (c & 0xFF) {
    case '\b':
        if (crt_pos > 0) {
            crt_pos--;
            draw_char(crt_buf, crt_pos % crt_cols, crt_pos / crt_cols, 0x0, 0x8);
        }
        break;
    case '\n':
        crt_pos += crt_cols;
        /* fallthrough */
    case '\r':
        crt_pos -= (crt_pos % crt_cols);
        break;
    case '\t':
        for (size_t i = 0; i < TABW; i++)
            fb_putc(' ');
        break;
    default:
        /* write the character */
        draw_char(crt_buf, crt_pos % crt_cols, crt_pos / crt_cols, 0xFFFFFFFF, (uint8_t)c);
        crt_pos++;
    }

    /* Scoll up when we have reached the bottom of screen */
    if (crt_pos >= crt_size) {
        nosan_memmove(crt_buf, crt_buf + uefi_stride * SYMBOL_SIZE,
                      uefi_stride * (uefi_vres - SYMBOL_SIZE) * sizeof(uint32_t));

        size_t i = (uefi_vres - (uefi_vres % SYMBOL_SIZE) - SYMBOL_SIZE);
        nosan_memset(crt_buf + i * uefi_stride, 0, uefi_stride * (uefi_vres - i) * sizeof(uint32_t));
        crt_pos -= crt_cols;
    }
}

/* Serial I/O code */


/* Stupid I/O delay routine necessitated
 * by historical PC design flaws */
static void
delay(void) {
    inb(0x84);
    inb(0x84);
    inb(0x84);
    inb(0x84);
}

static int
serial_proc_data(void) {
    if (!(inb(COM1 + COM_LSR) & COM_LSR_DATA)) return -1;
    return inb(COM1 + COM_RX);
}

void
serial_intr(void) {
    if (serial_exists) cons_intr(serial_proc_data);
}

static void
serial_putc(int c) {
    for (size_t i = 0; i < 12800; i++) {
        if (inb(COM1 + COM_LSR) & COM_LSR_TXRDY) break;
        delay();
    }

    outb(COM1 + COM_TX, c);
}

static void
serial_init(void) {
    /* Turn off the FIFO */
    outb(COM1 + COM_FCR, 0);

    /* Set speed; requires DLAB latch */
    outb(COM1 + COM_LCR, COM_LCR_DLAB);
    outb(COM1 + COM_DLL, (uint8_t)(115200 / 9600));
    outb(COM1 + COM_DLM, 0);

    /* 8 data bits, 1 stop bit, parity off; turn off DLAB latch */
    outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

    /* No modem controls */
    outb(COM1 + COM_MCR, 0);
    /* Enable RCV interrupts */
    outb(COM1 + COM_IER, COM_IER_RDI);

    /* Clear any preexisting overrun indications and interrupts
     * Serial port doesn't exist if COM_LSR returns 0xFF */
    serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
    (void)inb(COM1 + COM_IIR);
    (void)inb(COM1 + COM_RX);

    /* Enable serial interrupts */
    if (serial_exists) pic_irq_unmask(IRQ_SERIAL);
}

/* Parallel port output code */

/* For information on PC parallel port programming,
 * see the class References page */

static void
lpt_putc(int c) {
    for (size_t i = 0; i < 12800; i++) {
        if (inb(0x378 + 1) & 0x80) break;
        delay();
    }

    outb(0x378 + 0, c);
    outb(0x378 + 2, 0x08 | 0x04 | 0x01);
    outb(0x378 + 2, 0x08);
}


/* Keyboard input code */

#define NO 0

#define SHIFT (1 << 0)
#define CTL   (1 << 1)
#define ALT   (1 << 2)

#define CAPSLOCK   (1 << 3)
#define NUMLOCK    (1 << 4)
#define SCROLLLOCK (1 << 5)

#define E0ESC (1 << 6)

static uint8_t shiftcode[256] = {
        [0x1D] = CTL,
        [0x2A] = SHIFT,
        [0x36] = SHIFT,
        [0x38] = ALT,
        [0x9D] = CTL,
        [0xB8] = ALT};

static uint8_t togglecode[256] = {
        [0x3A] = CAPSLOCK,
        [0x45] = NUMLOCK,
        [0x46] = SCROLLLOCK};

static uint8_t normalmap[256] = {
        NO, 033, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', NO, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', NO, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', NO, '*', NO, ' ', NO, NO, NO, NO, NO, NO,
        NO, NO, NO, NO, NO, NO, NO, '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.', NO, NO, NO, NO,
        [0x9C] = '\n' /*KP_Enter*/,
        [0xB5] = '/' /*KP_Div*/,
        [0xC7] = KEY_HOME,
        [0xC8] = KEY_UP,
        [0xC9] = KEY_PGUP,
        [0xCB] = KEY_LF,
        [0xCD] = KEY_RT,
        [0xCF] = KEY_END,
        [0xD0] = KEY_DN,
        [0xD1] = KEY_PGDN,
        [0xD2] = KEY_INS,
        [0xD3] = KEY_DEL};

static uint8_t shiftmap[256] = {
        NO, 033, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', NO, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', NO, '|', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', NO, '*', NO, ' ', NO, NO, NO, NO, NO, NO,
        NO, NO, NO, NO, NO, NO, NO, '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.', NO, NO, NO, NO,
        [0xC7] = KEY_HOME,
        [0x9C] = '\n' /*KP_Enter*/,
        [0xB5] = '/' /*KP_Div*/,
        [0xC8] = KEY_UP,
        [0xC9] = KEY_PGUP,
        [0xCB] = KEY_LF,
        [0xCD] = KEY_RT,
        [0xCF] = KEY_END,
        [0xD0] = KEY_DN,
        [0xD1] = KEY_PGDN,
        [0xD2] = KEY_INS,
        [0xD3] = KEY_DEL};

#define C(x) (x - '@')

static uint8_t ctlmap[256] = {
        NO, NO, NO, NO, NO, NO, NO, NO,
        NO, NO, NO, NO, NO, NO, NO, NO,
        C('Q'), C('W'), C('E'), C('R'), C('T'), C('Y'), C('U'), C('I'),
        C('O'), C('P'), NO, NO, '\r', NO, C('A'), C('S'),
        C('D'), C('F'), C('G'), C('H'), C('J'), C('K'), C('L'), NO,
        NO, NO, NO, C('\\'), C('Z'), C('X'), C('C'), C('V'),
        C('B'), C('N'), C('M'), NO, NO, C('/'), NO, NO,
        [0x97] = KEY_HOME,
        [0xB5] = C('/'),
        [0xC8] = KEY_UP,
        [0xC9] = KEY_PGUP,
        [0xCB] = KEY_LF,
        [0xCD] = KEY_RT,
        [0xCF] = KEY_END,
        [0xD0] = KEY_DN,
        [0xD1] = KEY_PGDN,
        [0xD2] = KEY_INS,
        [0xD3] = KEY_DEL};

static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

/* Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data. */
static int
kbd_proc_data(void) {
    int c;
    uint8_t data;
    static uint32_t shift;

    if (!(inb(KBSTATP) & KBS_DIB)) return -1;

    data = inb(KBDATAP);

    if (data == 0xE0) {
        /* E0 escape character */
        shift |= E0ESC;
        return 0;
    } else if (data & 0x80) {
        /* Key released */
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    } else if (shift & E0ESC) {
        /* Last character was an E0 escape; or with 0x80 */
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK) {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }

    /* Process special keys:
     * Ctrl-Alt-Del -- reboot */
    if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
        cprintf("Rebooting!\n");

        /* Courtesy of Chris Frost */
        outb(0x92, 0x3);
    }

    return c;
}

int
kbd_proc_data_handle(void) {
    int c;
    uint8_t data, pre_data;
    static uint32_t shift;

    if (!(inb(KBSTATP) & KBS_DIB)) return -1;

    data = inb(KBDATAP);

    if (data == 0xE0) {
        /* E0 escape character */
        shift |= E0ESC;
        return 0;
    } else if (data & 0x80) {
        /* Key released */
        pre_data = data;
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return pre_data;
    } else if (shift & E0ESC) {
        /* Last character was an E0 escape; or with 0x80 */
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK) {
        if ('a' <= c && c <= 'z')
            c += 'A' - 'a';
        else if ('A' <= c && c <= 'Z')
            c += 'a' - 'A';
    }

    /* Process special keys:
     * Ctrl-Alt-Del -- reboot */
    if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
        cprintf("Rebooting!\n");

        /* Courtesy of Chris Frost */
        outb(0x92, 0x3);
    }

    return c;
}

void
kbd_intr(void) {
    cons_intr(kbd_proc_data);
}

static void
kbd_init(void) {
    /* Drain the kbd buffer so that Bochs generates interrupts. */
    kbd_intr();
    pic_irq_unmask(IRQ_KBD);
}

/* General device-independent console code
 *
 * Here we manage the console input buffer,
 * where we stash characters received from the keyboard or serial port
 * whenever the corresponding interrupt occurs.
 */

#define CONSBUFSIZE 512

static struct {
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos;
    uint32_t wpos;
} cons;

/* called by device interrupt routines to feed input characters
 * into the circular console input buffer */
static void
cons_intr(int (*proc)(void)) {
    int ch;

    while ((ch = (*proc)()) != -1) {
        if (!ch) continue;
        cons.buf[cons.wpos++] = ch;
        if (cons.wpos == CONSBUFSIZE) cons.wpos = 0;
    }
}

/* Return the next input character from the console, or 0 if none waiting */
int
cons_getc(void) {

    /* Poll for any pending input characters,
     * so that this function works even when interrupts are disabled
     * (e.g., when called from the kernel monitor) */
    serial_intr();
    kbd_intr();

    /* Grab the next character from the input buffer */
    if (cons.rpos != cons.wpos) {
        uint8_t ch = cons.buf[cons.rpos++];
        cons.rpos %= CONSBUFSIZE;
        return ch;
    }
    return 0;
}

/* Output a character to the console */
static void
cons_putc(int c) {
    /* Characters with codes
     * higher than 127 are not supported yet */
    c &= 0x7F;

    serial_putc(c);
    lpt_putc(c);
    fb_putc(c);
}

/* Initialize the console devices */
void
cons_init(void) {
    kbd_init();
    serial_init();

    if (!serial_exists)
        cprintf("Serial port does not exist!\n");
}

/* `High'-level console I/O.  Used by readline and cprintf. */

void
cputchar(int c) {
    cons_putc(c);
}

int
getchar(void) {
    int ch;

    while (!(ch = cons_getc()))
        /* nothing */;

    return ch;
}

int
iscons(int fdnum) {
    /* Used by readline */

    return 1;
}
