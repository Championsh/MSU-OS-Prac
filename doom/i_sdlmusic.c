//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
//	System interface for music.
//


#include "config.h"
#include "doomtype.h"
#include "memio.h"
#include "mus2mid.h"

#include "deh_str.h"
#include "gusconf.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "sha1.h"
#include "w_wad.h"
#include "z_zone.h"

#define MAXMIDLENGTH     (96 * 1024)
#define MID_HEADER_MAGIC "MThd"
#define MUS_HEADER_MAGIC "MUS\x1a"

#define FLAC_HEADER "fLaC"
#define OGG_HEADER  "OggS"

// Looping Vorbis metadata tag names. These have been defined by ZDoom
// for specifying the start and end positions for looping music tracks
// in .ogg and .flac files.
// More information is here: http://zdoom.org/wiki/Audio_loop
#define LOOP_START_TAG "LOOP_START"
#define LOOP_END_TAG   "LOOP_END"

// FLAC metadata headers that we care about.
#define FLAC_STREAMINFO     0
#define FLAC_VORBIS_COMMENT 4

// Ogg metadata headers that we care about.
#define OGG_ID_HEADER      1
#define OGG_COMMENT_HEADER 3

// Structure for music substitution.
// We store a mapping based on SHA1 checksum -> filename of substitute music
// file to play, so that substitution occurs based on content rather than
// lump name. This has some inherent advantages:
//  * Music for Plutonia (reused from Doom 1) works automatically.
//  * If a PWAD replaces music, the replacement music is used rather than
//    the substitute music for the IWAD.
//  * If a PWAD reuses music from an IWAD (even from a different game), we get
//    the high quality version of the music automatically (neat!)

typedef struct
{
    sha1_digest_t hash;
    char *filename;
} subst_music_t;

// Structure containing parsed metadata read from a digital music track:
typedef struct
{
    boolean valid;
    unsigned int samplerate_hz;
    int start_time, end_time;
} file_metadata_t;

static subst_music_t *subst_music = NULL;
static unsigned int subst_music_len = 0;

static const char *subst_config_filenames[] =
        {
                "doom1-music.cfg",
                "doom2-music.cfg",
                "tnt-music.cfg",
                "heretic-music.cfg",
                "hexen-music.cfg",
                "strife-music.cfg",
};

static boolean music_initialized = false;

// If this is true, this module initialized SDL sound and has the
// responsibility to shut it down

static boolean sdl_was_initialized = false;

static boolean musicpaused = false;
static int current_music_volume;

char *timidity_cfg_path = "";

static char *temp_timidity_cfg = NULL;

// If true, we are playing a substitute digital track rather than in-WAD
// MIDI/MUS track, and file_metadata contains loop metadata.
static boolean playing_substitute = false;
static file_metadata_t file_metadata;

// Position (in samples) that we have reached in the current track.
// This is updated by the TrackPositionCallback function.
static unsigned int current_track_pos;

// If true, the currently playing track is being played on loop.
static boolean current_track_loop;

// Given a time string (for LOOP_START/LOOP_END), parse it and return
// the time (in # samples since start of track) it represents.
static unsigned int
ParseVorbisTime(unsigned int samplerate_hz, char *value) {
    return 0;
}

// Given a vorbis comment string (eg. "LOOP_START=12345"), set fields
// in the metadata structure as appropriate.
static void
ParseVorbisComment(file_metadata_t *metadata, char *comment) {
}

static void
ReadLoopPoints(char *filename, file_metadata_t *metadata) {
}

// Given a MUS lump, look up a substitute MUS file to play instead
// (or NULL to just use normal MIDI playback).

static char *
GetSubstituteMusicFile(void *data, size_t data_len) {
    return (char *)0;
}

// Add a substitute music file to the lookup list.

static void
AddSubstituteMusic(subst_music_t *subst) {
}

static int
ParseHexDigit(char c) {
    return -1;
}

static char *
GetFullPath(char *base_filename, char *path) {
    return (char *)0;
}

// Parse a line from substitute music configuration file; returns error
// message or NULL for no error.

static char *
ParseSubstituteLine(char *filename, char *line) {
    return (char *)0;
}

// Read a substitute music configuration file.

static boolean
ReadSubstituteConfig(char *filename) {
    return false;
}

// Find substitute configs and try to load them.

static void
LoadSubstituteConfigs(void) {
}

// Returns true if the given lump number is a music lump that should
// be included in substitute configs.
// Identifying music lumps by name is not feasible; some games (eg.
// Heretic, Hexen) don't have a common naming pattern for music lumps.

static boolean
IsMusicLump(int lumpnum) {
    return false;
}

// Dump an example config file containing checksums for all MIDI music
// found in the WAD directory.

static void
DumpSubstituteConfig(char *filename) {
}

// If the temp_timidity_cfg config variable is set, generate a "wrapper"
// config file for Timidity to point to the actual config file. This
// is needed to inject a "dir" command so that the patches are read
// relative to the actual config file.

static boolean
WriteWrapperTimidityConfig(char *write_path) {
    return false;
}


static void
RemoveTimidityConfig(void) {
}

// Shutdown music

static void
I_SDL_ShutdownMusic(void) {
}

static boolean
SDLIsInitialized(void) {
    return false;
}

// Callback function that is invoked to track current track position.
void
TrackPositionCallback(int chan, void *stream, int len, void *udata) {
}

// Initialize music subsystem
static boolean
I_SDL_InitMusic(void) {
}

//
// SDL_mixer's native MIDI music playing does not pause properly.
// As a workaround, set the volume to 0 when paused.
//

static void
UpdateMusicVolume(void) {
}

// Set music volume (0 - 127)

static void
I_SDL_SetMusicVolume(int volume) {
}

// Start playing a mid

static void
I_SDL_PlaySong(void *handle, boolean looping) {
}

static void
I_SDL_PauseSong(void) {
}

static void
I_SDL_ResumeSong(void) {
}

static void
I_SDL_StopSong(void) {
}

static void
I_SDL_UnRegisterSong(void *handle) {
}

// Determine whether memory block is a .mid file

static boolean
IsMid(byte *mem, int len) {
    return false;
}

static boolean
ConvertMus(byte *musdata, int len, char *filename) {
    return false;
}

static void *
I_SDL_RegisterSong(void *data, int len) {
    return (void *)0;
}

// Is the song playing?
static boolean
I_SDL_MusicIsPlaying(void) {
    return false;
}

// Get position in substitute music track, in seconds since start of track.
static int
GetMusicPosition(void) {
    return 0;
}

static void
RestartCurrentTrack(void) {
}

// Poll music position; if we have passed the loop point end position
// then we need to go back.
static void
I_SDL_PollMusic(void) {
}

static snddevice_t music_sdl_devices[] =
        {
                SNDDEVICE_PAS,
                SNDDEVICE_GUS,
                SNDDEVICE_WAVEBLASTER,
                SNDDEVICE_SOUNDCANVAS,
                SNDDEVICE_GENMIDI,
                SNDDEVICE_AWE32,
};

music_module_t DG_music_module =
        {
                music_sdl_devices,
                arrlen(music_sdl_devices),
                I_SDL_InitMusic,
                I_SDL_ShutdownMusic,
                I_SDL_SetMusicVolume,
                I_SDL_PauseSong,
                I_SDL_ResumeSong,
                I_SDL_RegisterSong,
                I_SDL_UnRegisterSong,
                I_SDL_PlaySong,
                I_SDL_StopSong,
                I_SDL_MusicIsPlaying,
                I_SDL_PollMusic,
};
