#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

#define MAX_FREE_HEAPBLOCKS 0x10000

#include "inc/types.h"

typedef struct {
    uintptr_t heap_addr;
    size_t size, is_free;
} HeapObj;

/* system call numbers */
enum {
    SYS_cputs = 0,
    SYS_cgetc,
    SYS_getenvid,
    SYS_env_destroy,
    SYS_alloc_region,
    SYS_map_region,
    SYS_map_physical_region,
    SYS_unmap_region,
    SYS_region_refs,
    SYS_exofork,
    SYS_env_set_status,
    SYS_env_set_trapframe,
    SYS_env_set_pgfault_upcall,
    SYS_yield,
    SYS_ipc_try_send,
    SYS_ipc_recv,
    SYS_gettime,
    SYS_get_cpufreq,
    SYS_poll_kbd,
    SYS_drawchar,
    NSYSCALLS
};

#endif /* !JOS_INC_SYSCALL_H */
