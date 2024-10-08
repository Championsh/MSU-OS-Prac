#ifndef DOOM_GENERIC
#define DOOM_GENERIC


#define DOOMGENERIC_RESX 640
#define DOOMGENERIC_RESY 400

#include "inc/lib.h"

extern uint32_t* DG_ScreenBuffer;

extern window_d window;
extern renderer_d renderer;
extern texture_d texture;

void doomgeneric_Create(int argc, char** argv);
void doomgeneric_Tick();


// Implement below functions for your platform
void DG_Init();
void DG_DrawFrame();
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs();
int DG_GetKey(int* pressed, unsigned char* key);
void DG_SetWindowTitle(const char* title);

#endif // DOOM_GENERIC
