#include <inc/x86.h>
#include <inc/string.h>
#include <inc/video.h>

#include "fs/pci.h"
#include "video.h"

union Vsipc *vsreq = (union Vsipc*)0x0FFFF000;

int serv_create_window(union Vsipc *ipc) {
    struct Window *window = (struct Window*) malloc(sizeof(struct Window));
    if (!window)
        return VIDEO_WINDOW_CREATE_ERROR;

    struct Vsreq_create_window * req = &ipc->create_window;
    window->width = req->req_width;
    window->height = req->req_height;
    window->mode = req->req_mode;
    req->res_window = window;

    struct GraphicsAdapter * svga = get_svga();

    if (window->mode == WINDOW_MODE_CENTERED) {
        window->x_offset = svga->width / 2 - window->width / 2;
        window->y_offset = svga->height / 2 - window->height / 2;
    }

    svga_fill(svga, 0x00000000);

    return VIDEO_OK;
}

int serv_destroy_window(union Vsipc *ipc) {
    free(ipc->destroy_window.req_window);
    return VIDEO_OK;
}

int serv_create_texture(envid_t dst_envid, union Vsipc *ipc) {
    struct Texture * texture = (struct Texture*)malloc(sizeof(struct Texture));
    if (!texture)
        return -VIDEO_TEXTURE_CREATE_ERROR;

    struct Vsreq_create_texture *req = &ipc->create_texture;
    texture->width = req->req_width;
    texture->height = req->req_height;
    texture->buf = malloc(sizeof(uint32_t) * texture->width * texture->height);
    if (!texture->buf) {
        free(texture);
        return -VIDEO_TEXTURE_CREATE_ERROR;
    }

    req->res_texture = texture;

    if (req->req_need_mapping) {
        uint32_t * texture_map = (uint32_t*)(VIDEO_MAP_TOP + ((char*)texture->buf - USER_HEAP_TOP));
        int res = sys_map_region(0, texture->buf, dst_envid, texture_map, sizeof(uint32_t) * texture->width * texture->height, PROT_RW);
        if (res) {
            free(texture->buf);
            free(texture);
            return -res;
        }

        texture->user_buf_map = texture_map;
        req->res_buffer_map = texture_map;
    }

    return VIDEO_OK;
}

int serv_destroy_texture(envid_t dst_env, union Vsipc *ipc) {
    struct Texture * texture = ipc->destroy_texture.req_texture;
    if (texture->user_buf_map) {
        int res = sys_unmap_region(dst_env, texture->user_buf_map, sizeof(uint32_t) * texture->height * texture->width);
        if (res)
            return res;
    }

    free(ipc->destroy_texture.req_texture->buf);
    free(ipc->destroy_texture.req_texture);
    return VIDEO_OK;
}

int serv_create_renderer(union Vsipc *ipc) {
    struct Renderer * renderer = (struct Renderer*)malloc(sizeof(struct Renderer));
    if (!renderer)
        return -VIDEO_RENDERER_CREATE_ERROR;

    struct Vsreq_create_renderer *req = &ipc->create_renderer;
    renderer->window = req->req_window;
    renderer->back_buffer = malloc(sizeof(uint32_t) * renderer->window->width * renderer->window->height);

    if (!renderer->back_buffer) {
        free(renderer);
        return -VIDEO_RENDERER_CREATE_ERROR;
    }

    req->res_renderer = renderer;
    return VIDEO_OK;
}

int serv_destroy_renderer(union Vsipc *ipc) {
    free(ipc->destroy_renderer.req_renderer->back_buffer);
    free(ipc->destroy_renderer.req_renderer);
    return VIDEO_OK;
}

int serv_update_texture(union Vsipc *ipc) {
    struct Texture * texture = ipc->update_texture.req_texture;
    char * buffer = ipc->update_texture.req_buf;
    size_t size = ipc->update_texture.req_linear_size;
    size_t offset = ipc->update_texture.req_linear_offset;

    memcpy((char*)texture->buf + offset, buffer, size);

    return VIDEO_OK;
}

int serv_copy_texture(union Vsipc *ipc) {
    struct Texture * texture = ipc->copy_texture.req_texture;
    struct Renderer * renderer = ipc->copy_texture.req_renderer;

    memcpy(renderer->back_buffer, texture->buf, renderer->window->width * renderer->window->height * sizeof(uint32_t));

    return VIDEO_OK;
}

int serv_get_display_info(union Vsipc *ipc) {
    struct GraphicsAdapter * adapter = get_svga();
    ipc->get_display_info.res_display_height = adapter->width;
    ipc->get_display_info.res_display_width = adapter->height;
    return VIDEO_OK;
}


int serv_display(union Vsipc *ipc) {
    struct Renderer * renderer = ipc->display.req_renderer;
    struct GraphicsAdapter * adapter = get_svga();
    struct Window * window = renderer->window;

    if (window->mode == WINDOW_MODE_CORNER || window->mode == WINDOW_MODE_CENTERED) {
        for (uint32_t y = 0; y < window->height; ++y) {
            for (uint32_t x = 0; x < window->width; ++x) {
                uint32_t adapter_x = window->x_offset + x;
                uint32_t adapter_y = window->y_offset + y;
                adapter->fb[adapter_y * adapter->width + adapter_x] = renderer->back_buffer[y * window->width + x];
            }
        }
    } else if (window->mode == WINDOW_MODE_FULLSCREEN) {
        for (uint32_t y = 0; y < adapter->height; ++y) {
            for (uint32_t x = 0; x < adapter->width; ++x) {
                uint32_t w_x = (uint32_t)((double)x / adapter->width * window->width);
                uint32_t w_y = (uint32_t)((double)y / adapter->height * window->height);
                adapter->fb[y * adapter->width + x] = renderer->back_buffer[w_y * window->width + w_x];
            }
        }
    } else if (window->mode == WINDOW_MODE_2XSCALE) {
        for (uint32_t y = 0; y < window->height * 2; ++y) {
            for (uint32_t x = 0; x < window->width * 2; ++x) {
                uint32_t w_x = x / 2;
                uint32_t w_y = y / 2;
                adapter->fb[y * adapter->width + x] = renderer->back_buffer[w_y * window->width + w_x];
            }
        }
    }

    return VIDEO_OK;
}

int serv_clear(union Vsipc *req) {
    struct Renderer *renderer = req->clear.req_renderer;
    memset(renderer->back_buffer, 0, renderer->window->width * renderer->window->height * sizeof(uint32_t));
    return VIDEO_OK;
}


typedef int (*vshandler) (union Vsipc *req);

vshandler handlers[] = {
        [VSREQ_CREATE_WINDOW] = serv_create_window,
        [VSREQ_DESTROY_WINDOW] = serv_destroy_window,
        [VSREQ_CREATE_RENDERER] = serv_create_renderer,
        [VSREQ_DESTROY_RENDERER] = serv_destroy_renderer,
        // [VSREQ_CREATE_TEXTURE] = serv_create_texture,
        // [VSREQ_DESTROY_TEXTURE] = serv_destroy_texture,
        [VSREQ_UPDATE_TEXTURE] = serv_update_texture,
        [VSREQ_COPY_TEXTURE] = serv_copy_texture,
        [VSREQ_DISPLAY] = serv_display,
        [VSREQ_CLEAR] = serv_clear
};
#define NHANDLERS (sizeof(handlers) / sizeof(handlers[0]))


void serve(void) {
    uint32_t req, whom;
    int perm, res;

    while (1) {
        perm = 0;
        size_t sz = PAGE_SIZE;
        req = ipc_recv((int32_t *)&whom, vsreq, &sz, &perm);

        if (!(perm & PROT_R)) {
            continue; /* Just leave it hanging... */
        }

        if (req == VSREQ_CREATE_TEXTURE) {
            res = serv_create_texture(whom, vsreq);
        } else if (req == VSREQ_DESTROY_TEXTURE) {
            res = serv_destroy_texture(whom, vsreq);
        }
        else if (req < NHANDLERS && handlers[req]) {
            res = handlers[req](vsreq);
        }
        else {
            res = -VIDEO_INVALID_REQUEST;
        }
        ipc_send(whom, res, NULL, PAGE_SIZE, perm);
        sys_unmap_region(0, vsreq, PAGE_SIZE);
    }
}

void
umain(int argc, char **argv) {
    cprintf("GS is running\n");
    pci_init(argv);
    graphics_init(640, 400, GRAPHICS_ADAPTER_ANY);
    serve();
}