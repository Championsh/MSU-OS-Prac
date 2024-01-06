/* System call stubs. */

#include <inc/syscall.h>
#include <inc/lib.h>

static uint64_t freq = 0;

static HeapObj freep[MAX_FREE_HEAPBLOCKS];    /* Simple heap array */
static size_t freep_size = 0;                 /* Whole number of objects */
static size_t real_freep_size = 0;            /* Number of free pages */
static uintptr_t heap_ptr = USER_HEAP_TOP;    /* Pointer to heaptop */
static size_t heap_page_ptr = -PAGE_SIZE;     /* Offset by pages */
static size_t heap_inpage_offset = PAGE_SIZE; /* Offset in page */

static inline int64_t __attribute__((always_inline))
syscall(uintptr_t num, bool check, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
    intptr_t ret;

    /* Generic system call.
     * Pass system call number in RAX,
     * Up to six parameters in RDX, RCX, RBX, RDI, RSI and R8.
     *
     * Registers are assigned using GCC externsion
     */

    register uintptr_t _a0 asm("rax") = num,
                           _a1 asm("rdx") = a1, _a2 asm("rcx") = a2,
                           _a3 asm("rbx") = a3, _a4 asm("rdi") = a4,
                           _a5 asm("rsi") = a5, _a6 asm("r8") = a6;

    /* Interrupt kernel with T_SYSCALL.
     *
     * The "volatile" tells the assembler not to optimize
     * this instruction away just because we don't use the
     * return value.
     *
     * The last clause tells the assembler that this can
     * potentially change the condition codes and arbitrary
     * memory locations. */

    asm volatile("int %1\n"
                 : "=a"(ret)
                 : "i"(T_SYSCALL), "r"(_a0), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6)
                 : "cc", "memory");

    if (check && ret > 0) {
        panic("syscall %zd returned %zd (> 0)", num, ret);
    }

    return ret;
}

void
sys_cputs(const char *s, size_t len) {
    syscall(SYS_cputs, 0, (uintptr_t)s, len, 0, 0, 0, 0);
}

int
sys_cgetc(void) {
    return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_env_destroy(envid_t envid) {
    return syscall(SYS_env_destroy, 1, envid, 0, 0, 0, 0, 0);
}

envid_t
sys_getenvid(void) {
    return syscall(SYS_getenvid, 0, 0, 0, 0, 0, 0, 0);
}

void
sys_yield(void) {
    syscall(SYS_yield, 0, 0, 0, 0, 0, 0, 0);
}

int
sys_region_refs(void *va, size_t size) {
    return syscall(SYS_region_refs, 0, (uintptr_t)va, size, MAX_USER_ADDRESS, 0, 0, 0);
}

int
sys_region_refs2(void *va, size_t size, void *va2, size_t size2) {
    return syscall(SYS_region_refs, 0, (uintptr_t)va, size, (uintptr_t)va2, size2, 0, 0);
}

int
sys_alloc_region(envid_t envid, void *va, size_t size, int perm) {
    int res = syscall(SYS_alloc_region, 1, envid, (uintptr_t)va, size, perm, 0, 0);
#ifdef SANITIZE_USER_SHADOW_BASE
    /* Unpoison the allocated page */
    if (!res && thisenv && envid == CURENVID && ((uintptr_t)va < SANITIZE_USER_SHADOW_BASE || (uintptr_t)va >= SANITIZE_USER_SHADOW_SIZE + SANITIZE_USER_SHADOW_BASE)) {
        platform_asan_unpoison(va, size);
    }
#endif

    return res;
}

int
sys_map_region(envid_t srcenv, void *srcva, envid_t dstenv, void *dstva, size_t size, int perm) {
    int res = syscall(SYS_map_region, 1, srcenv, (uintptr_t)srcva, dstenv, (uintptr_t)dstva, size, perm);
#ifdef SANITIZE_USER_SHADOW_BASE
    if (!res && dstenv == CURENVID)
        platform_asan_unpoison(dstva, size);
#endif
    return res;
}

int
sys_map_physical_region(uintptr_t pa, envid_t dstenv, void *dstva, size_t size, int perm) {
    int res = syscall(SYS_map_physical_region, 1, pa, dstenv, (uintptr_t)dstva, size, perm, 0);
#ifdef SANITIZE_USER_SHADOW_BASE
    platform_asan_unpoison(dstva, size);
#endif
    return res;
}

int
sys_unmap_region(envid_t envid, void *va, size_t size) {
    int res = syscall(SYS_unmap_region, 1, envid, (uintptr_t)va, size, 0, 0, 0);
#ifdef SANITIZE_USER_SHADOW_BASE
    if (!res && ((uintptr_t)va < SANITIZE_USER_SHADOW_BASE ||
                 (uintptr_t)va >= SANITIZE_USER_SHADOW_SIZE + SANITIZE_USER_SHADOW_BASE)) {
        platform_asan_poison(va, size);
    }
#endif
    return res;
}

/* sys_exofork is inlined in lib.h */

int
sys_env_set_status(envid_t envid, int status) {
    return syscall(SYS_env_set_status, 1, envid, status, 0, 0, 0, 0);
}

int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf) {
    return syscall(SYS_env_set_trapframe, 1, envid, (uintptr_t)tf, 0, 0, 0, 0);
}

int
sys_env_set_pgfault_upcall(envid_t envid, void *upcall) {
    return syscall(SYS_env_set_pgfault_upcall, 1, envid, (uintptr_t)upcall, 0, 0, 0, 0);
}

int
sys_ipc_try_send(envid_t envid, uintptr_t value, void *srcva, size_t size, int perm) {
    return syscall(SYS_ipc_try_send, 0, envid, value, (uintptr_t)srcva, size, perm, 0);
}

int
sys_ipc_recv(void *dstva, size_t size) {
    int res = syscall(SYS_ipc_recv, 1, (uintptr_t)dstva, size, 0, 0, 0, 0);
#ifdef SANITIZE_USER_SHADOW_BASE
    if (!res) platform_asan_unpoison(dstva, thisenv->env_ipc_maxsz);
#endif
    return res;
}

int
sys_gettime(void) {
    return syscall(SYS_gettime, 0, 0, 0, 0, 0, 0, 0);
}

void *
malloc(size_t size) {
    if (!size || freep_size >= MAX_FREE_HEAPBLOCKS)
        return NULL;

    if (real_freep_size) {
        for (size_t i = 0; i < freep_size; i++)
            if (freep[i].is_free && freep[i].size >= size) {
                real_freep_size--;
                freep[i].is_free = false;
                freep[i].size = size;
                return (void *)(freep[i].heap_addr);
            }
    }

    if (size + heap_inpage_offset > PAGE_SIZE) {
        heap_page_ptr += PAGE_SIZE;
        heap_inpage_offset = 0;
        int res = sys_alloc_region(sys_getenvid(), (void *)heap_ptr + heap_page_ptr, ROUNDUP(size % PAGE_SIZE ? size : size + PAGE_SIZE, PAGE_SIZE),
                                   PROT_USER_ | PROT_R | PROT_W);
        if (res < 0)
            return NULL;
#ifdef SANITIZE_USER_SHADOW_BASE
        if (!res)
            platform_asan_unpoison((void *)heap_ptr + heap_page_ptr, ROUNDUP(size % PAGE_SIZE ? size : size + PAGE_SIZE, PAGE_SIZE));
#endif
    }

    freep[freep_size].heap_addr = heap_ptr + heap_page_ptr + heap_inpage_offset;
    freep[freep_size].size = size;
    freep[freep_size].is_free = false;
    heap_inpage_offset += size % PAGE_SIZE;
    heap_page_ptr += size - size % PAGE_SIZE;

    return (void*)(freep[freep_size++].heap_addr);
}

void *
calloc(size_t num, size_t size) {
    size_t dim = num * size;
    return memset(malloc(dim), 0, dim);
}

size_t get_allocsize(void* ptr) {
    if (freep_size == real_freep_size)
        return 0;
    for (size_t i = 0; i < freep_size; i++)
        if ((uintptr_t)ptr == freep[i].heap_addr)
            return freep[i].is_free ? 0 : freep[i].size;
    return 0;
}

void *
realloc(void *ptr, size_t newsize) {
    size_t oldsize = get_allocsize(ptr);
    if (oldsize > newsize)
        return ptr;
    void *newptr = memcpy(malloc(newsize), ptr, oldsize);
    free(ptr);
    return newptr;
}

void
free(void *ptr) {
    if (!ptr)
        return;

    if (freep_size == real_freep_size)
        panic("trying to free unallocated memory.\n");

    for (size_t i = 0; i < freep_size; i++) {
        if ((uintptr_t)ptr == freep[i].heap_addr) {
            if (freep[i].is_free)
                panic("trying to free already freed memory.\n");
            freep[i].is_free = true;
            real_freep_size++;
            return;
        }
    }

    panic("trying to free invalid memory pointer.\n");
}

char*
strdup (const char *src) {
    size_t n = strlen(src) + 1;
    return memcpy(malloc(n), src, n);
}

uint64_t
get_cpufreq() {
    return syscall(SYS_get_cpufreq, 0, 0, 0, 0, 0, 0, 0);
}

uint64_t
get_ticks() {
    if (!freq)
        freq = get_cpufreq();
    return read_tsc() * 1000 / freq;
}

void
sleep(uint64_t ms) {
    if (!freq)
        freq = get_cpufreq();
    uint64_t tick_start = read_tsc(), tick_cnt = ms * freq / 1000, g;
    while((g = read_tsc() - tick_start) < tick_cnt);
}

int poll_kbd() {
    return syscall(SYS_poll_kbd, 0, 0, 0, 0, 0, 0, 0);
}

void draw_char(uint32_t *buffer, uint32_t x, uint32_t y, uint32_t color, uint32_t stride, uint8_t charcode) {
    syscall(SYS_drawchar, 0, (uintptr_t)buffer, (uint32_t)x, (uint32_t)y, (uint32_t)color, (uint32_t)stride, (uint8_t)charcode);
}