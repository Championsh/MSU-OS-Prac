//
// Created by Иван Черемисенов on 26.12.2023.
//

#ifndef INC_VIDEO_H
#define INC_VIDEO_H

#include <inc/types.h>
#include <inc/mmu.h>

enum {
    VIDEO_OK = 0,
    VIDEO_WINDOW_CREATE_ERROR,
    VIDEO_TEXTURE_CREATE_ERROR,
    VIDEO_TEXTURE_UPDATE_ERROR,
    VIDEO_RENDERER_CREATE_ERROR,
    VIDEO_INVALID_REQUEST
};

enum {
    VSREQ_CREATE_WINDOW = 1,
    VSREQ_DESTROY_WINDOW,
    VSREQ_CREATE_TEXTURE,
    VSREQ_DESTROY_TEXTURE,
    VSREQ_CREATE_RENDERER,
    VSREQ_DESTROY_RENDERER,
    VSREQ_UPDATE_TEXTURE,
    VSREQ_COPY_TEXTURE,
    VSREQ_DISPLAY,
    VSREQ_GET_DISPLAY_INFO,
    VSREQ_CLEAR,
};

enum {
    WINDOW_MODE_CORNER = 0,
    WINDOW_MODE_FULLSCREEN,
    WINDOW_MODE_CENTERED,
    WINDOW_MODE_2XSCALE
};

struct Window {
    uint32_t width;
    uint32_t height;
    uint32_t x_offset;
    uint32_t y_offset;
    int mode;
};

struct Texture {
    uint32_t width;
    uint32_t height;
    uint32_t *buf;
    uint32_t *user_buf_map;
};

struct Renderer {
    struct Window *window;
    uint32_t *back_buffer;
};

typedef struct Window * window_d;
typedef struct Texture * texture_d;
typedef struct Renderer * renderer_d;

union Vsipc {
    struct Vsreq_create_window {
        uint32_t req_width;
        uint32_t req_height;
        int req_mode;
        window_d res_window;
    } create_window;
    struct Vsreq_destroy_window {
        window_d req_window;
    } destroy_window;
    struct Vsreq_create_texture {
        uint32_t  req_width;
        uint32_t  req_height;
        bool req_need_mapping;
        texture_d res_texture;
        uint32_t * res_buffer_map;
    } create_texture;
    struct Vsreq_destroy_texture {
        texture_d req_texture;
    } destroy_texture;
    struct Vsreq_create_renderer {
        window_d req_window;
        renderer_d res_renderer;
    } create_renderer;
    struct Vsreq_destroy_renderer {
        renderer_d req_renderer;
    } destroy_renderer;
    struct Vsreq_update_texture {
        texture_d req_texture;
        size_t req_linear_offset;
        size_t req_linear_size;
        char req_buf[PAGE_SIZE - sizeof (size_t) * 3];
    } update_texture;
    struct Vsreq_copy_texture {
        texture_d req_texture;
        renderer_d req_renderer;
    } copy_texture;
    struct Vsreq_display {
        renderer_d req_renderer;
    } display;
    struct Vsreq_get_display_info {
        uint32_t res_display_width;
        uint32_t res_display_height;
    } get_display_info;
    struct Vsreq_clear {
        renderer_d req_renderer;
    } clear;
    char _pad[PAGE_SIZE];
};


#endif //INC_VIDEO_H
