/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/console.h>
#include <kern/env.h>
#include <kern/kclock.h>
#include <kern/pmap.h>
#include <kern/sched.h>
#include <kern/syscall.h>
#include <kern/trap.h>
#include <kern/traceopt.h>
#include <kern/alloc.h>
#include <kern/timer.h>

/* Print a string to the system console.
 * The string is exactly 'len' characters long.
 * Destroys the environment on memory errors. */
static int
sys_cputs(const char *s, size_t len) {
    /* Check that the user has permission to read memory [s, s+len).
     * Destroy the environment if not. */
    user_mem_assert(curenv, s, len, PROT_R | PROT_USER_);
#ifdef SANITIZE_SHADOW_BASE
    platform_asan_unpoison((void *)s, len);
#endif
    cprintf("%.*s", (int)len, s);
#ifdef SANITIZE_SHADOW_BASE
    platform_asan_poison((void *)s, len);
#endif
    return 0;
}

/* Read a character from the system console without blocking.
 * Returns the character, or 0 if there is no input waiting. */
static int
sys_cgetc(void) {
    return cons_getc();
}

/* Returns the current environment's envid. */
static envid_t
sys_getenvid(void) {
    return curenv->env_id;
}

/* Destroy a given environment (possibly the currently running environment).
 *
 *  Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid. */
static int
sys_env_destroy(envid_t envid) {
    struct Env *env;
    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;

    if (trace_envs) {
        cprintf(env == curenv ?
                        "[%08x] exiting gracefully\n" :
                        "[%08x] destroying %08x\n",
                curenv->env_id, env->env_id);
    }

    env_destroy(env);
    return 0;
}

/* Deschedule current environment and pick a different one to run. */
static void
sys_yield(void) {
    sched_yield();
}

/* Allocate a new environment.
 * Returns envid of new environment, or < 0 on error.  Errors are:
 *  -E_NO_FREE_ENV if no free environment is available.
 *  -E_NO_MEM on memory exhaustion. */
static envid_t
sys_exofork(void) {
    /* Create the new environment with env_alloc(), from kern/env.c.
     * It should be left as env_alloc created it, except that
     * status is set to ENV_NOT_RUNNABLE, and the register set is copied
     * from the current environment -- but tweaked so sys_exofork
     * will appear to return 0. */
    struct Env *env;
    int status = env_alloc(&env, curenv->env_id, ENV_TYPE_USER);
    if (status)
        return status;
    env->env_status = ENV_NOT_RUNNABLE;
    env->binary = curenv->binary;
    env->env_tf = curenv->env_tf;
    env->env_tf.tf_regs.reg_rax = 0;
    return env->env_id;
}

/* Set envid's env_status to status, which must be ENV_RUNNABLE
 * or ENV_NOT_RUNNABLE.
 *
 * Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 *  -E_INVAL if status is not a valid status for an environment. */
static int
sys_env_set_status(envid_t envid, int status) {
    /* Hint: Use the 'envid2env' function from kern/env.c to translate an
     * envid to a struct Env.
     * You should set envid2env's third argument to 1, which will
     * check whether the current environment has permission to set
     * envid's status. */
    struct Env* env;
    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;

    if (status == ENV_NOT_RUNNABLE || status == ENV_RUNNABLE)
        env->env_status = status;
    else 
        return -E_INVAL;
    return 0;
}

/* Set the page fault upcall for 'envid' by modifying the corresponding struct
 * Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
 * kernel will push a fault record onto the exception stack, then branch to
 * 'func'.
 *
 * Returns 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid. */
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func) {
    struct Env* env;
    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;
    env->env_pgfault_upcall = func;
    return 0;
}

/* Allocate a region of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 * The page's contents are set to 0.
 * If a page is already mapped at 'va', that page is unmapped as a
 * side effect.
 *
 * This call should work with or without ALLOC_ZERO/ALLOC_ONE flags
 * (set them if they are not already set)
 *
 * It allocates memory lazily so you need to use map_region
 * with PROT_LAZY and ALLOC_ONE/ALLOC_ZERO set.
 *
 * Don't forget to set PROT_USER_
 *
 * PROT_ALL is useful for validation.
 *
 * Return 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 *  -E_INVAL if va >= MAX_USER_ADDRESS, or va is not page-aligned.
 *  -E_INVAL if perm is inappropriate (see above).
 *  -E_NO_MEM if there's no memory to allocate the new page,
 *      or to allocate any necessary page tables. */
static int
sys_alloc_region(envid_t envid, uintptr_t addr, size_t size, int perm) {
    struct Env* env;
    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;
    if (addr >= MAX_USER_ADDRESS || PAGE_OFFSET(addr) || perm & ~(PROT_ALL | ALLOC_ZERO | ALLOC_ONE) || perm & ALLOC_ONE & ALLOC_ZERO)
        return -E_INVAL;

    perm |= PROT_LAZY | PROT_USER_;
    if (perm & ALLOC_ONE)
        perm &= ~ALLOC_ZERO;
    else {
        perm |= ALLOC_ZERO;
        perm &= ~ALLOC_ONE;
    }

    return map_region(&env->address_space, addr, NULL, 0, size, perm) ? -E_NO_MEM : 0;
}

/* Map the region of memory at 'srcva' in srcenvid's address space
 * at 'dstva' in dstenvid's address space with permission 'perm'.
 * Perm has the same restrictions as in sys_alloc_region, except
 * that it also does not supprt ALLOC_ONE/ALLOC_ONE flags.
 *
 * You only need to check alignment of addresses, perm flags and
 * that addresses are a part of user space. Everything else is
 * already checked inside map_region().
 *
 * Return 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
 *      or the caller doesn't have permission to change one of them.
 *  -E_INVAL if srcva >= MAX_USER_ADDRESS or srcva is not page-aligned,
 *      or dstva >= MAX_USER_ADDRESS or dstva is not page-aligned.
 *  -E_INVAL is srcva is not mapped in srcenvid's address space.
 *  -E_INVAL if perm is inappropriate (see sys_page_alloc).
 *  -E_INVAL if (perm & PROT_W), but srcva is read-only in srcenvid's
 *      address space.
 *  -E_NO_MEM if there's no memory to allocate any necessary page tables. */

static int
sys_map_region(envid_t srcenvid, uintptr_t srcva,
               envid_t dstenvid, uintptr_t dstva, size_t size, int perm) {
    struct Env *srcenv, *dstenv;
    if (envid2env(srcenvid, &srcenv, 1) || envid2env(dstenvid, &dstenv, 1))
        return -E_BAD_ENV;

    if (CLASS_MASK(0) & srcva || CLASS_MASK(0) & dstva ||
        srcva >= MAX_USER_ADDRESS || dstva >= MAX_USER_ADDRESS ||
        perm & ~PROT_ALL || perm & ALLOC_ZERO || perm & ALLOC_ONE)
        return -E_INVAL;

    perm |= PROT_USER_;

    return map_region(&dstenv->address_space, dstva, &srcenv->address_space, srcva, size, perm);
}

/* Unmap the region of memory at 'va' in the address space of 'envid'.
 * If no page is mapped, the function silently succeeds.
 *
 * Return 0 on success, < 0 on error.  Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 *  -E_INVAL if va >= MAX_USER_ADDRESS, or va is not page-aligned. */
static int
sys_unmap_region(envid_t envid, uintptr_t va, size_t size) {
    /* Hint: This function is a wrapper around unmap_region(). */
    struct Env* env;
    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;
    if (CLASS_MASK(0) & va || va >= MAX_USER_ADDRESS)
        return -E_INVAL;

    unmap_region(&env->address_space, va, size);
    return 0;
}

/* Map region of physical memory to the userspace address.
 * This is meant to be used by the userspace drivers, of which
 * the only one currently is the filesystem server.
 *
 * Return 0 on succeeds, < 0 on error. Erros are:
 *  -E_BAD_ENV if environment envid doesn't currently exist,
 *      or the caller doesn't have permission to change envid.
 *  -E_BAD_ENV if is not a filesystem driver (ENV_TYPE_FS).
 *  -E_INVAL if va >= MAX_USER_ADDRESS, or va is not page-aligned.
 *  -E_INVAL if pa is not page-aligned.
 *  -E_INVAL if size is not page-aligned.
 *  -E_INVAL if prem contains invalid flags
 *     (including PROT_SHARE, PROT_COMBINE or PROT_LAZY).
 *  -E_NO_MEM if address does not exist.
 *  -E_NO_ENT if address is already used. */
static int
sys_map_physical_region(uintptr_t pa, envid_t envid, uintptr_t va, size_t size, int perm) {
    struct Env * env_fs;
    if (envid2env(envid, &env_fs, true) || (env_fs->env_type != ENV_TYPE_FS && env_fs->env_type != ENV_TYPE_VS))
        return -E_BAD_ENV;

    if (va >= MAX_USER_ADDRESS || PAGE_OFFSET(va) || PAGE_OFFSET(pa) || PAGE_OFFSET(size) ||
        size > MAX_USER_ADDRESS || MAX_USER_ADDRESS - va < size || perm & (PROT_SHARE | PROT_COMBINE | PROT_LAZY))
        return -E_INVAL;
    
    return map_physical_region(&env_fs->address_space, va, pa, size, perm | PROT_USER_ | MAP_USER_MMIO);
}

/* Try to send 'value' to the target env 'envid'.
 * If srcva < MAX_USER_ADDRESS, then also send region currently mapped at 'srcva',
 * so that receiver gets mapping.
 *
 * The send fails with a return value of -E_IPC_NOT_RECV if the
 * target is not blocked, waiting for an IPC.
 *
 * The send also can fail for the other reasons listed below.
 *
 * Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends;
 *    env_ipc_maxsz is set to min of size and it's current vlaue;
 *    env_ipc_from is set to the sending envid;
 *    env_ipc_value is set to the 'value' parameter;
 *    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
 * The target environment is marked runnable again, returning 0
 * from the paused sys_ipc_recv system call.  (Hint: does the
 * sys_ipc_recv function ever actually return?)
 *
 * If the sender wants to send a page but the receiver isn't asking for one,
 * then no page mapping is transferred, but no error occurs.
 * The ipc only happens when no errors occur.
 * Send region size is the minimum of sized specified in sys_ipc_try_send() and sys_ipc_recv()
 *
 * Returns 0 on success, < 0 on error.
 * Errors are:
 *  -E_BAD_ENV if environment envid doesn't currently exist.
 *      (No need to check permissions.)
 *  -E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
 *      or another environment managed to send first.
 *  -E_INVAL if srcva < MAX_USER_ADDRESS but srcva is not page-aligned.
 *  -E_INVAL if srcva < MAX_USER_ADDRESS and perm is inappropriate
 *      (see sys_page_alloc).
 *  -E_INVAL if srcva < MAX_USER_ADDRESS but srcva is not mapped in the caller's
 *      address space.
 *  -E_INVAL if (perm & PTE_W), but srcva is read-only in the
 *      current environment's address space.
 *  -E_NO_MEM if there's not enough memory to map srcva in envid's
 *      address space. */
static int
sys_ipc_try_send(envid_t envid, uint32_t value, uintptr_t srcva, size_t size, int perm) {
    struct Env* env;
    if (envid2env(envid, &env, 0))
        return -E_BAD_ENV;
    if (!env->env_ipc_recving)
        return -E_IPC_NOT_RECV;
    if ((srcva < MAX_USER_ADDRESS && PAGE_OFFSET(srcva)) ||
        (srcva < MAX_USER_ADDRESS && perm & ~PROT_ALL) || 
        (srcva < MAX_USER_ADDRESS && (perm & PROT_W) && user_mem_check(curenv, (void *)srcva, size, PROT_W) < 0))
        return -E_INVAL;

    if (!env->env_ipc_recving)
        return -E_IPC_NOT_RECV;
    env->env_ipc_recving = false;

    size_t actual_size = MIN(size, env->env_ipc_maxsz);
    if (srcva < MAX_USER_ADDRESS && env->env_ipc_dstva < MAX_USER_ADDRESS) {
        if (map_region(&env->address_space, env->env_ipc_dstva, 
            &curenv->address_space, srcva, actual_size, perm | PROT_USER_)) {
                env->env_ipc_recving = true;
                return -E_NO_MEM;
            }
        env->env_ipc_maxsz = actual_size;
        env->env_ipc_perm = perm;
    } else 
        env->env_ipc_perm = 0;

    env->env_ipc_value = value;
    env->env_ipc_from = curenv->env_id;
    env->env_status = ENV_RUNNABLE;
    return 0;
}

/* Block until a value is ready.  Record that you want to receive
 * using the env_ipc_recving, env_ipc_maxsz and env_ipc_dstva fields of struct Env,
 * mark yourself not runnable, and then give up the CPU.
 *
 * If 'dstva' is < MAX_USER_ADDRESS, then you are willing to receive a page of data.
 * 'dstva' is the virtual address at which the sent page should be mapped.
 *
 * This function only returns on error, but the system call will eventually
 * return 0 on success.
 * Return < 0 on error.  Errors are:
 *  -E_INVAL if dstva < MAX_USER_ADDRESS but dstva is not page-aligned;
 *  -E_INVAL if dstva is valid and maxsize is 0,
 *  -E_INVAL if maxsize is not page aligned. */
static int
sys_ipc_recv(uintptr_t dstva, uintptr_t maxsize) {
    if (PAGE_OFFSET(maxsize) || (dstva < MAX_USER_ADDRESS &&
        (PAGE_OFFSET(dstva) || maxsize == 0)))
        return -E_INVAL;

    curenv->env_ipc_recving = 1;
    curenv->env_status = ENV_NOT_RUNNABLE;
    if (dstva < MAX_USER_ADDRESS) {
        curenv->env_ipc_dstva = dstva;
        curenv->env_ipc_maxsz = maxsize;
    }
    curenv->env_tf.tf_regs.reg_rax = 0;
    sched_yield();
    return 0;
}

/*
 * This function sets trapframe and is unsafe
 * so you need:
 *   -Check environment id to be valid and accessible
 *   -Check argument to be valid memory
 *   -Use nosan_memcpy to copy from usespace
 *   -Prevent privilege escalation by overriding segments
 *   -Only allow program to set safe flags in RFLAGS register
 *   -Force IF to be set in RFLAGS
 */
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf) {
    struct Env* env;
    if (envid2env(envid, &env, 0))
        return -E_BAD_ENV;

    user_mem_assert(curenv, tf, sizeof(*tf), PROT_R | PROT_USER_);

    nosan_memcpy(&env->env_tf, tf, sizeof(*tf));

    env->env_tf.tf_ds = GD_UD | 3;
    env->env_tf.tf_es = GD_UD | 3;
    env->env_tf.tf_ss = GD_UD | 3;
    env->env_tf.tf_cs = GD_UT | 3;
    env->env_tf.tf_rflags &= 0xFFF;
    env->env_tf.tf_rflags |= FL_IF;

    return 0;
}

/* Return date and time in UNIX timestamp format: seconds passed
 * from 1970-01-01 00:00:00 UTC. */
static int
sys_gettime(void) {
    return gettime();
}

/*
 * This function return the difference between maximal
 * number of references of regions [addr, addr + size] and [addr2,addr2+size2]
 * if addr2 is less than MAX_USER_ADDRESS, or just
 * maximal number of references to [addr, addr + size]
 *
 * Use region_maxref() here.
 */
static int
sys_region_refs(uintptr_t addr, size_t size, uintptr_t addr2, uintptr_t size2) {
    if (addr2 != MAX_USER_ADDRESS)
        return region_maxref(&curenv->address_space, addr, size) -
               region_maxref(&curenv->address_space, addr2, size2);
    else
        return region_maxref(&curenv->address_space, addr, size);
}

uint64_t sys_get_cpufreq() {
    return hpet_cpu_frequency();
}

uint8_t
sys_poll_kbd() {
    return kbd_proc_data_handle();
}

void
sys_drawchar(uint32_t *buffer, uint32_t x, uint32_t y, uint32_t color, uint32_t stride, uint8_t charcode) {
    draw_char_stride(buffer, x, y, color, stride, charcode);
}

/* Dispatches to the correct kernel function, passing the arguments. */
uintptr_t
syscall(uintptr_t syscallno, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
    /* Call the function corresponding to the 'syscallno' parameter.
     * Return any appropriate return value. */
    switch (syscallno) {
        case SYS_cputs:
            return sys_cputs((const char *)a1, (size_t)a2);
        case SYS_cgetc:
            return sys_cgetc();
        case SYS_getenvid:
            return sys_getenvid();
        case SYS_env_destroy:
            return sys_env_destroy((envid_t)a1);
        case SYS_exofork:
            return sys_exofork();
        case SYS_alloc_region:
            return sys_alloc_region((envid_t)a1, a2, (size_t)a3, (int)a4);
        case SYS_map_region:
            return sys_map_region((envid_t) a1, a2,(envid_t)a3, a4, (size_t)a5, (int)a6);
        case SYS_unmap_region:
            return sys_unmap_region((envid_t) a1, a2,(size_t)a3);
        case SYS_env_set_status:
            return sys_env_set_status((envid_t)a1, (int)a2);
        case SYS_env_set_pgfault_upcall:
            return sys_env_set_pgfault_upcall((envid_t) a1, (void *)a2);
        case SYS_yield:
            sys_yield();
            return 0;
        case SYS_ipc_try_send:
            return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, a3,(size_t)a4,(int)a5);
        case SYS_ipc_recv:
            return sys_ipc_recv(a1, a2);
        case SYS_region_refs:
            return sys_region_refs(a1, (size_t)a2, a3, a4);
        case SYS_map_physical_region:
            return sys_map_physical_region(a1, (envid_t)a2, a3, (size_t)a4, (int)a5);
        case SYS_env_set_trapframe:
            return sys_env_set_trapframe((envid_t)a1, (struct Trapframe *)a2);
        case SYS_gettime:
            return sys_gettime();
        case SYS_get_cpufreq:
            return sys_get_cpufreq();
        case SYS_poll_kbd:
            return sys_poll_kbd();
        case SYS_drawchar:
            sys_drawchar((uint32_t *)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)a4, (uint32_t)a5, (uint8_t)a6);
            return 0;
        default:
            return -E_NO_SYS;
    }
    return -E_NO_SYS;
}
