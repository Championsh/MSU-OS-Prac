/* See COPYRIGHT for copyright information. */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>

#define CRT_ROWS    25
#define CRT_COLS    80
#define CRT_SIZE    (CRT_ROWS * CRT_COLS)
#define SYMBOL_SIZE 8

void cons_init(void);
void fb_init(void);
int cons_getc(void);

/* IRQ1 */
void kbd_intr(void);
/* IRQ4 */
void serial_intr(void);

int kbd_proc_data_handle(void);

void draw_char_stride(uint32_t *buffer, uint32_t x, uint32_t y, uint32_t color, uint32_t stride, uint8_t charcode);

#endif /* _CONSOLE_H_ */
