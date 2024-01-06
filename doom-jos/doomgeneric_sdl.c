// doomgeneric for cross-platform development library 'Simple DirectMedia Layer'

#include <inc/lib.h>

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

window_d window = NULL;
renderer_d renderer = NULL;
texture_d texture = NULL;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;
static unsigned char release_handlers[256];

static unsigned char
convertToDoomKey(unsigned int key) {
    switch (key) {
        case 10:
            key = KEY_ENTER;
            break;
        case -28:
        case 'a':
            key = KEY_LEFTARROW;
            break;
        case -27:
        case 'd':
            key = KEY_RIGHTARROW;
            break;
        case -30:
        case 'w':
            key = KEY_UPARROW;
            break;
        case -29:
        case 's':
            key = KEY_DOWNARROW;
            break;
        case 'q':
            key = KEY_FIRE;
            break;
        case 32:
        case 'e':
            key = KEY_USE;
            break;
        case 27:
            key = KEY_ESCAPE;
            break;
        case 'c':
            key = KEY_RSHIFT;
            break;
        default:
            key = tolower(key);
            break;
    }
    return key;
}

static void
addKeyToQueue(int pressed, char keyCode) {
    unsigned char key = convertToDoomKey(keyCode);
    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void
handleKeyInput() {
    char e;
    while((e = poll_kbd()) != -1) {
        if (e == '`') {
            destroy_texture(texture);
            destroy_renderer(renderer);
            destroy_window(window);
            printf("Quit requested\n");
            exit();
        }
        if (e & 0x80 && (uint8_t)e < 226)
            addKeyToQueue(0, release_handlers[(uint8_t)(e ^ 0x80)]);
        else
            addKeyToQueue(1, e);
    }
}


void
DG_Init() {
    window = create_window(DOOMGENERIC_RESX, DOOMGENERIC_RESY, WINDOW_MODE_CENTERED);

    renderer = create_renderer(window);

    display(renderer);

    texture = create_texture(DOOMGENERIC_RESX, DOOMGENERIC_RESY, 1, &DG_ScreenBuffer);
}

void
DG_DrawFrame() {
    clear(renderer);

    copy_texture(renderer, texture);

    display(renderer);

    handleKeyInput();
}

void
DG_SleepMs(uint32_t ms) {
    sleep(ms);
}

uint32_t
DG_GetTicksMs() {
    return get_ticks();
}

int
DG_GetKey(int* pressed, unsigned char* doomKey) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        //key queue is empty
        return 0;
    } else {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void
DG_SetWindowTitle(const char* title) {

}

void
umain(int argc, char** argv) {
    /* Remap release keys handlers */
    release_handlers[2] = '1';
    release_handlers[3] = '2';
    release_handlers[4] = '3';
    release_handlers[5] = '4';
    release_handlers[6] = '5';
    release_handlers[7] = '6';
    release_handlers[8] = '7';
    release_handlers[9] = '8';
    release_handlers[10] = '9';
    release_handlers[11] = '0';
    release_handlers[12] = '-';
    release_handlers[13] = '=';
    release_handlers[46] = 'c';
    release_handlers[18] = 'e';
    release_handlers[16] = 'q';
    release_handlers[17] = 'w';
    release_handlers[31] = 's';
    release_handlers[30] = 'a';
    release_handlers[32] = 'd';
    release_handlers[72] = -30;
    release_handlers[80] = -29;
    release_handlers[75] = -28;
    release_handlers[77] = -27;
    release_handlers[57] = 32;

    doomgeneric_Create(argc, argv);

    printf("Doom has initialized! GRAPHIC API IS IMPLEMENTED!!!!!!!!!!!!!!!\n");

    for (int i = 0;; i++) {
        doomgeneric_Tick();
    }
}