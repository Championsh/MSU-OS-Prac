#include <inc/lib.h>
#include <inc/video.h>

union Vsipc vsipcbuf __attribute__((aligned(PAGE_SIZE)));

static int vsipc(unsigned type, void *dstva) {

    static envid_t vsenv;
    if (!vsenv) vsenv = ipc_find_env(ENV_TYPE_VS);

    static_assert(sizeof(vsipcbuf) == PAGE_SIZE, "Invalid vsipcbuf size");

    ipc_send(vsenv, type, &vsipcbuf, PAGE_SIZE, PROT_RW);
    size_t maxsz = PAGE_SIZE;
    return ipc_recv(NULL, dstva, &maxsz, NULL);
}

window_d create_window(uint32_t width, uint32_t height, int mode) {
    vsipcbuf.create_window.req_width = width;
    vsipcbuf.create_window.req_height = height;
    vsipcbuf.create_window.req_mode = mode;

    int res = vsipc(VSREQ_CREATE_WINDOW, &vsipcbuf);
    if (res)
        return NULL;

    return vsipcbuf.create_window.res_window;
}

int destroy_window(window_d window) {
    vsipcbuf.destroy_window.req_window = window;
    return vsipc(VSREQ_DESTROY_WINDOW, &vsipcbuf);
}

texture_d create_texture(uint32_t width, uint32_t height, bool need_mapping, uint32_t **buffer_map) {
    vsipcbuf.create_texture.req_width = width;
    vsipcbuf.create_texture.req_height = height;
    vsipcbuf.create_texture.req_need_mapping = need_mapping;

    int res = vsipc(VSREQ_CREATE_TEXTURE, &vsipcbuf);
    if (res)
        return NULL;

    if (need_mapping) {
        *buffer_map = vsipcbuf.create_texture.res_buffer_map;
#ifdef SANITIZE_USER_SHADOW_BASE
        platform_asan_unpoison(*buffer_map, sizeof(uint32_t) * width * height);
#endif
    }

    return vsipcbuf.create_texture.res_texture;
}

int destroy_texture(texture_d texture) {
    vsipcbuf.destroy_texture.req_texture = texture;
#ifdef SANITIZE_USER_SHADOW_BASE
    platform_asan_poison(texture->user_buf_map, sizeof(uint32_t) * texture->width * texture->height);
#endif
    return vsipc(VSREQ_DESTROY_TEXTURE, &vsipcbuf);
}

renderer_d create_renderer(window_d window) {
    vsipcbuf.create_renderer.req_window = window;
    int res =  vsipc(VSREQ_CREATE_RENDERER, &vsipcbuf);
    if (res)
        return NULL;

    return vsipcbuf.create_renderer.res_renderer;
}

int destroy_renderer(renderer_d renderer) {
    vsipcbuf.destroy_renderer.req_renderer = renderer;
    return vsipc(VSREQ_DESTROY_RENDERER, &vsipcbuf);
}

int update_texture(texture_d texture, char * buffer, size_t size) {
    vsipcbuf.update_texture.req_texture = texture;
    for (size_t offset = 0; offset < size; offset += sizeof(vsipcbuf.update_texture.req_buf)) {
        size_t block_size = MIN(size - offset, sizeof(vsipcbuf.update_texture.req_buf));
        memcpy(vsipcbuf.update_texture.req_buf, buffer + offset, block_size);
        vsipcbuf.update_texture.req_linear_offset = offset;
        vsipcbuf.update_texture.req_linear_size = block_size;
        int res = vsipc(VSREQ_UPDATE_TEXTURE, &vsipcbuf);
        if (res)
            return -VIDEO_TEXTURE_UPDATE_ERROR;
    }

    return VIDEO_OK;
}

int copy_texture(renderer_d renderer, texture_d texture) {
    vsipcbuf.copy_texture.req_texture = texture;
    vsipcbuf.copy_texture.req_renderer = renderer;
    return vsipc(VSREQ_COPY_TEXTURE, &vsipcbuf);
}

int display(renderer_d renderer) {
    vsipcbuf.display.req_renderer = renderer;
    return vsipc(VSREQ_DISPLAY, &vsipcbuf);
}

int clear(renderer_d renderer) {
    vsipcbuf.clear.req_renderer = renderer;
    return vsipc(VSREQ_CLEAR, &vsipcbuf);
}