//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	System interface for sound.
//

#include "inc/lib.h"

#include "config.h"

#include "deh_str.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomtype.h"

#define LOW_PASS_FILTER 1
//#define DEBUG_DUMP_WAVS
#define NUM_CHANNELS 16

typedef struct allocated_sound_s allocated_sound_t;

struct allocated_sound_s {
    sfxinfo_t *sfxinfo;
    int use_count;
    allocated_sound_t *prev, *next;
};

static boolean setpanning_workaround = false;

static boolean sound_initialized = false;

static sfxinfo_t *channels_playing[NUM_CHANNELS];

static int mixer_freq;
static int mixer_channels;
static boolean use_sfx_prefix;
static boolean (*ExpandSoundData)(sfxinfo_t *sfxinfo,
                                  byte *data,
                                  int samplerate,
                                  int length) = NULL;

// Doubly-linked list of allocated sounds.
// When a sound is played, it is moved to the head, so that the oldest
// sounds not used recently are at the tail.

static allocated_sound_t *allocated_sounds_head = NULL;
static allocated_sound_t *allocated_sounds_tail = NULL;
static int allocated_sounds_size = 0;

int use_libsamplerate = 0;

// Scale factor used when converting libsamplerate floating point numbers
// to integers. Too high means the sounds can clip; too low means they
// will be too quiet. This is an amount that should avoid clipping most
// of the time: with all the Doom IWAD sound effects, at least. If a PWAD
// is used, clipping might occur.

int libsamplerate_scale = 1;

// Hook a sound into the linked list at the head.

static void
AllocatedSoundLink(allocated_sound_t *snd) {
}

// Unlink a sound from the linked list.

static void
AllocatedSoundUnlink(allocated_sound_t *snd) {
}

static void
FreeAllocatedSound(allocated_sound_t *snd) {
}

// Search from the tail backwards along the allocated sounds list, find
// and free a sound that is not in use, to free up memory.  Return true
// for success.

static boolean
FindAndFreeSound(void) {
    return false;
}

// Enforce SFX cache size limit.  We are just about to allocate "len"
// bytes on the heap for a new sound effect, so free up some space
// so that we keep allocated_sounds_size < snd_cachesize

static void
ReserveCacheSpace(size_t len) {
}

// Lock a sound, to indicate that it may not be freed.

static void
LockAllocatedSound(allocated_sound_t *snd) {
}

// Unlock a sound to indicate that it may now be freed.

static void
UnlockAllocatedSound(allocated_sound_t *snd) {
}

// When a sound stops, check if it is still playing.  If it is not,
// we can mark the sound data as CACHE to be freed back for other
// means.

static void
ReleaseSoundOnChannel(int channel) {
}

static boolean
ConvertibleRatio(int freq1, int freq2) {
    return false;
}

// Generic sound expansion function for any sample rate.
// Returns number of clipped samples (always 0).

static boolean
ExpandSoundData_SDL(sfxinfo_t *sfxinfo,
                    byte *data,
                    int samplerate,
                    int length) {
    return false;
}

// Load and convert a sound effect
// Returns true if successful

static boolean
CacheSFX(sfxinfo_t *sfxinfo) {
    return false;
}

static void
GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len) {
}


static void
I_SDL_PrecacheSounds(sfxinfo_t *sounds, int num_sounds) {
}

// Load a SFX chunk into memory and ensure that it is locked.

static boolean
LockSound(sfxinfo_t *sfxinfo) {
    return false;
}

//
// Retrieve the raw data lump index
//  for a given SFX name.
//

static int
I_SDL_GetSfxLumpNum(sfxinfo_t *sfx) {
    return 0;
}

static void
I_SDL_UpdateSoundParams(int handle, int vol, int sep) {
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//

static int
I_SDL_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep) {
    return 0;
}

static void
I_SDL_StopSound(int handle) {
}


static boolean
I_SDL_SoundIsPlaying(int handle) {
    return 0;
}

//
// Periodically called to update the sound system
//

static void
I_SDL_UpdateSound(void) {
}

static void
I_SDL_ShutdownSound(void) {
    sound_initialized = false;
}

// Calculate slice size, based on snd_maxslicetime_ms.
// The result must be a power of two.

static int
GetSliceSize(void) {
    return 0;
}

static boolean
I_SDL_InitSound(boolean _use_sfx_prefix) {
    sound_initialized = false;
    return false;
}

static snddevice_t sound_sdl_devices[] =
        {
                SNDDEVICE_SB,
                SNDDEVICE_PAS,
                SNDDEVICE_GUS,
                SNDDEVICE_WAVEBLASTER,
                SNDDEVICE_SOUNDCANVAS,
                SNDDEVICE_AWE32,
};

sound_module_t DG_sound_module =
        {
                sound_sdl_devices,
                arrlen(sound_sdl_devices),
                I_SDL_InitSound,
                I_SDL_ShutdownSound,
                I_SDL_GetSfxLumpNum,
                I_SDL_UpdateSound,
                I_SDL_UpdateSoundParams,
                I_SDL_StartSound,
                I_SDL_StopSound,
                I_SDL_SoundIsPlaying,
                I_SDL_PrecacheSounds,
};
