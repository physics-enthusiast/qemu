/*
 *  Linux file-related syscall implementations
 *  Copyright (c) 2003 Fabrice Bellard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

static abi_long do_faccessat(int dirfd, abi_ulong target_path, int mode)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(faccessat(dirfd, p, mode, 0));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_access
SYSCALL_IMPL(access)
{
    return do_faccessat(AT_FDCWD, arg1, arg2);
}
#endif

SYSCALL_IMPL(acct)
{
    abi_ulong target_path = arg1;
    abi_long ret;

    if (target_path == 0) {
        ret = get_errno(acct(NULL));
    } else {
        char *p = lock_user_string(target_path);
        if (!p) {
            return -TARGET_EFAULT;
        }
        ret = get_errno(acct(path(p)));
        unlock_user(p, target_path, 0);
    }
    return ret;
}

SYSCALL_IMPL(chdir)
{
    abi_ulong target_path = arg1;
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(chdir(p));
    unlock_user(p, target_path, 0);
    return ret;
}

static abi_long do_fchmodat(int dirfd, abi_ulong target_path, mode_t mode)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(fchmodat(dirfd, p, mode, 0));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_chmod
SYSCALL_IMPL(chmod)
{
    return do_fchmodat(AT_FDCWD, arg1, arg2);
}
#endif

SYSCALL_IMPL(chroot)
{
    char *p = lock_user_string(arg1);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(chroot(p));
    unlock_user(p, arg1, 0);
    return ret;
}

SYSCALL_IMPL(close)
{
    int fd = arg1;

    fd_trans_unregister(fd);
    return get_errno(close(fd));
}

#ifdef TARGET_NR_creat
SYSCALL_IMPL(creat)
{
    char *p = lock_user_string(arg1);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(creat(p, arg2));
    fd_trans_unregister(ret);
    unlock_user(p, arg1, 0);
    return ret;
}
#endif

SYSCALL_IMPL(dup)
{
    abi_long ret = get_errno(dup(arg1));
    if (ret >= 0) {
        fd_trans_dup(arg1, ret);
    }
    return ret;
}

#ifdef TARGET_NR_dup2
SYSCALL_IMPL(dup2)
{
    abi_long ret = get_errno(dup2(arg1, arg2));
    if (ret >= 0) {
        fd_trans_dup(arg1, arg2);
    }
    return ret;
}
#endif

SYSCALL_IMPL(dup3)
{
    int ofd = arg1;
    int nfd = arg2;
    int host_flags = target_to_host_bitmask(arg3, fcntl_flags_tbl);
    abi_long ret;

#ifdef CONFIG_DUP3
    ret = dup3(ofd, nfd, host_flags);
#else
    if (host_flags == 0) {
        if (ofd == nfd) {
            return -TARGET_EINVAL;
        }
        ret = dup2(ofd, nfd);
    } else {
        ret = syscall(__NR_dup3, ofd, nfd, host_flags);
    }
#endif
    return get_errno(ret);
}

SYSCALL_IMPL(faccessat)
{
    return do_faccessat(arg1, arg2, arg3);
}

SYSCALL_IMPL(fchmod)
{
    return get_errno(fchmod(arg1, arg2));
}

SYSCALL_IMPL(fchmodat)
{
    return do_fchmodat(arg1, arg2, arg3);
}

#ifdef TARGET_NR_ftruncate64
# if TARGET_ABI_BITS == 32
SYSCALL_ARGS(ftruncate64_truncate64)
{
    /* We have already assigned out[0].  */
    int off = regpairs_aligned(cpu_env, TARGET_NR_ftruncate64);
    out[1] = target_offset64(in[1 + off], in[2 + off]);
    return def;
}
# else
#  define args_ftruncate64_truncate64 NULL
# endif
#endif

SYSCALL_IMPL(ftruncate)
{
    return get_errno(ftruncate(arg1, arg2));
}

#ifdef TARGET_NR_futimesat
SYSCALL_IMPL(futimesat)
{
    int dirfd = arg1;
    abi_ulong target_path = arg2;
    abi_ulong target_tv = arg3;
    struct timeval *tvp, tv[2];
    char *p;
    abi_long ret;

    if (target_tv) {
        if (copy_from_user_timeval(&tv[0], target_tv)
            || copy_from_user_timeval(&tv[1],
                                      target_tv +
                                      sizeof(struct target_timeval))) {
            return -TARGET_EFAULT;
        }
        tvp = tv;
    } else {
        tvp = NULL;
    }

    p = lock_user_string(target_path);
    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(futimesat(dirfd, path(p), tvp));
    unlock_user(p, target_path, 0);
    return ret;
}
#endif

static abi_long do_linkat(int olddirfd, abi_ulong target_oldpath,
                          int newdirfd, abi_ulong target_newpath,
                          int flags)
{
    char *oldpath = lock_user_string(target_oldpath);
    char *newpath = lock_user_string(target_newpath);
    abi_long ret = -TARGET_EFAULT;

    if (oldpath && newpath) {
        ret = get_errno(linkat(olddirfd, oldpath, newdirfd, newpath, flags));
    }
    unlock_user(oldpath, target_oldpath, 0);
    unlock_user(newpath, target_newpath, 0);
    return ret;
}

#ifdef TARGET_NR_link
SYSCALL_IMPL(link)
{
    return do_linkat(AT_FDCWD, arg1, AT_FDCWD, arg2, 0);
}
#endif

SYSCALL_IMPL(linkat)
{
    return do_linkat(arg1, arg2, arg3, arg4, arg5);
}

#ifdef TARGET_NR_lseek
SYSCALL_IMPL(lseek)
{
    return get_errno(lseek(arg1, arg2, arg3));
}
#endif

#ifdef TARGET_NR_llseek
SYSCALL_ARGS(llseek)
{
    /* The parts for offset are *always* in big-endian order.  */
    abi_ulong lo = in[2], hi = in[1];
    out[1] = (((uint64_t)hi << (TARGET_ABI_BITS - 1)) << 1) | lo;
    out[2] = in[3];
    out[3] = in[4];
    return def;
}

SYSCALL_IMPL(llseek)
{
    int fd = arg1;
    int64_t offset = arg2;
    abi_ulong target_res = arg3;
    int whence = arg4;

    off_t res = lseek(fd, offset, whence);

    if (res == -1) {
        return get_errno(-1);
    } else if (put_user_s64(res, target_res)) {
        return -TARGET_EFAULT;
    }
    return 0;
}
#endif

static abi_long do_mkdirat(int dirfd, abi_ulong target_path, mode_t mode)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(mkdirat(dirfd, p, mode));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_mkdir
SYSCALL_IMPL(mkdir)
{
    return do_mkdirat(AT_FDCWD, arg1, arg2);
}
#endif

SYSCALL_IMPL(mkdirat)
{
    return do_mkdirat(arg1, arg2, arg3);
}

static abi_long do_mknodat(int dirfd, abi_ulong target_path,
                           mode_t mode, dev_t dev)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(mknodat(dirfd, p, mode, dev));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_mknod
SYSCALL_IMPL(mknod)
{
    return do_mknodat(AT_FDCWD, arg1, arg2, arg3);
}
#endif

SYSCALL_IMPL(mknodat)
{
    return do_mknodat(arg1, arg2, arg3, arg4);
}

SYSCALL_IMPL(mount)
{
    abi_ulong target_src = arg1;
    abi_ulong target_tgt = arg2;
    abi_ulong target_fst = arg3;
    abi_ulong mountflags = arg4;
    abi_ulong target_data = arg5;
    char *p_src = NULL, *p_tgt = NULL, *p_fst = NULL, *p_data = NULL;
    abi_long ret = -TARGET_EFAULT;

    if (target_src) {
        p_src = lock_user_string(target_src);
        if (!p_src) {
            goto exit0;
        }
    }

    p_tgt = lock_user_string(target_tgt);
    if (!p_tgt) {
        goto exit1;
    }

    if (target_fst) {
        p_fst = lock_user_string(target_fst);
        if (!p_fst) {
            goto exit2;
        }
    }

    /*
     * FIXME - arg5 should be locked, but it isn't clear how to
     * do that since it's not guaranteed to be a NULL-terminated
     * string.
     */
    if (target_data) {
        p_data = g2h(target_data);
    }
    ret = get_errno(mount(p_src, p_tgt, p_fst, mountflags, p_data));

    unlock_user(p_fst, target_fst, 0);
 exit2:
    unlock_user(p_tgt, target_tgt, 0);
 exit1:
    unlock_user(p_src, target_src, 0);
 exit0:
    return ret;
}

/*
 * Helpers for do_openat, manipulating /proc/self/foo.
 */

static int open_self_cmdline(void *cpu_env, int fd)
{
    CPUState *cpu = ENV_GET_CPU((CPUArchState *)cpu_env);
    struct linux_binprm *bprm = ((TaskState *)cpu->opaque)->bprm;
    int i;

    for (i = 0; i < bprm->argc; i++) {
        size_t len = strlen(bprm->argv[i]) + 1;

        if (write(fd, bprm->argv[i], len) != len) {
            return -1;
        }
    }

    return 0;
}

static int open_self_maps(void *cpu_env, int fd)
{
    CPUState *cpu = ENV_GET_CPU((CPUArchState *)cpu_env);
    TaskState *ts = cpu->opaque;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return -1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        int fields, dev_maj, dev_min, inode;
        uint64_t min, max, offset;
        char flag_r, flag_w, flag_x, flag_p;
        char path[512] = "";
        fields = sscanf(line, "%"PRIx64"-%"PRIx64" %c%c%c%c %"PRIx64" %x:%x %d"
                        " %512s", &min, &max, &flag_r, &flag_w, &flag_x,
                        &flag_p, &offset, &dev_maj, &dev_min, &inode, path);

        if ((fields < 10) || (fields > 11)) {
            continue;
        }
        if (h2g_valid(min)) {
            int flags = page_get_flags(h2g(min));
            max = h2g_valid(max - 1) ? max : (uintptr_t)g2h(GUEST_ADDR_MAX) + 1;
            if (page_check_range(h2g(min), max - min, flags) == -1) {
                continue;
            }
            if (h2g(min) == ts->info->stack_limit) {
                pstrcpy(path, sizeof(path), "      [stack]");
            }
            dprintf(fd, TARGET_ABI_FMT_ptr "-" TARGET_ABI_FMT_ptr
                    " %c%c%c%c %08" PRIx64 " %02x:%02x %d %s%s\n",
                    h2g(min), h2g(max - 1) + 1, flag_r, flag_w,
                    flag_x, flag_p, offset, dev_maj, dev_min, inode,
                    path[0] ? "         " : "", path);
        }
    }

    free(line);
    fclose(fp);

    return 0;
}

static int open_self_stat(void *cpu_env, int fd)
{
    CPUState *cpu = ENV_GET_CPU((CPUArchState *)cpu_env);
    TaskState *ts = cpu->opaque;
    abi_ulong start_stack = ts->info->start_stack;
    int i;

    for (i = 0; i < 44; i++) {
        char buf[128];
        int len;
        uint64_t val = 0;

        if (i == 0) {
            /* pid */
            val = getpid();
            snprintf(buf, sizeof(buf), "%"PRId64 " ", val);
        } else if (i == 1) {
            /* app name */
            snprintf(buf, sizeof(buf), "(%s) ", ts->bprm->argv[0]);
        } else if (i == 27) {
            /* stack bottom */
            val = start_stack;
            snprintf(buf, sizeof(buf), "%"PRId64 " ", val);
        } else {
            /* for the rest, there is MasterCard */
            snprintf(buf, sizeof(buf), "0%c", i == 43 ? '\n' : ' ');
        }

        len = strlen(buf);
        if (write(fd, buf, len) != len) {
            return -1;
        }
    }

    return 0;
}

static int open_self_auxv(void *cpu_env, int fd)
{
    CPUState *cpu = ENV_GET_CPU((CPUArchState *)cpu_env);
    TaskState *ts = cpu->opaque;
    abi_ulong auxv = ts->info->saved_auxv;
    abi_ulong len = ts->info->auxv_len;
    char *ptr;

    /*
     * Auxiliary vector is stored in target process stack.
     * read in whole auxv vector and copy it to file
     */
    ptr = lock_user(VERIFY_READ, auxv, len, 0);
    if (ptr != NULL) {
        while (len > 0) {
            ssize_t r;
            r = write(fd, ptr, len);
            if (r <= 0) {
                break;
            }
            len -= r;
            ptr += r;
        }
        lseek(fd, 0, SEEK_SET);
        unlock_user(ptr, auxv, len);
    }

    return 0;
}

static int is_proc_myself(const char *filename, const char *entry)
{
    if (!strncmp(filename, "/proc/", strlen("/proc/"))) {
        filename += strlen("/proc/");
        if (!strncmp(filename, "self/", strlen("self/"))) {
            filename += strlen("self/");
        } else if (*filename >= '1' && *filename <= '9') {
            char myself[80];
            snprintf(myself, sizeof(myself), "%d/", getpid());
            if (!strncmp(filename, myself, strlen(myself))) {
                filename += strlen(myself);
            } else {
                return 0;
            }
        } else {
            return 0;
        }
        if (!strcmp(filename, entry)) {
            return 1;
        }
    }
    return 0;
}

#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
static int is_proc(const char *filename, const char *entry)
{
    return strcmp(filename, entry) == 0;
}

static int open_net_route(void *cpu_env, int fd)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        return -1;
    }

    /* read header */

    read = getline(&line, &len, fp);
    dprintf(fd, "%s", line);

    /* read routes */

    while ((read = getline(&line, &len, fp)) != -1) {
        char iface[16];
        uint32_t dest, gw, mask;
        unsigned int flags, refcnt, use, metric, mtu, window, irtt;
        int fields;

        fields = sscanf(line,
                        "%s\t%08x\t%08x\t%04x\t%d\t%d\t%d\t%08x\t%d\t%u\t%u\n",
                        iface, &dest, &gw, &flags, &refcnt, &use, &metric,
                        &mask, &mtu, &window, &irtt);
        if (fields != 11) {
            continue;
        }
        dprintf(fd, "%s\t%08x\t%08x\t%04x\t%d\t%d\t%d\t%08x\t%d\t%u\t%u\n",
                iface, tswap32(dest), tswap32(gw), flags, refcnt, use,
                metric, tswap32(mask), mtu, window, irtt);
    }

    free(line);
    fclose(fp);

    return 0;
}
#endif

static abi_long do_openat(void *cpu_env, int dirfd, abi_ulong target_path,
                          int target_flags, mode_t mode)
{
    struct fake_open {
        const char *filename;
        int (*fill)(void *cpu_env, int fd);
        int (*cmp)(const char *s1, const char *s2);
    };
    static const struct fake_open fakes[] = {
        { "maps", open_self_maps, is_proc_myself },
        { "stat", open_self_stat, is_proc_myself },
        { "auxv", open_self_auxv, is_proc_myself },
        { "cmdline", open_self_cmdline, is_proc_myself },
#if defined(HOST_WORDS_BIGENDIAN) != defined(TARGET_WORDS_BIGENDIAN)
        { "/proc/net/route", open_net_route, is_proc },
#endif
    };

    char *pathname = lock_user_string(target_path);
    int flags = target_to_host_bitmask(target_flags, fcntl_flags_tbl);
    abi_long ret;
    size_t i;

    if (!pathname) {
        return -TARGET_EFAULT;
    }

    if (is_proc_myself(pathname, "exe")) {
        ret = qemu_getauxval(AT_EXECFD);
        if (ret == 0) {
            ret = get_errno(safe_openat(dirfd, exec_path, flags, mode));
        }
        goto done;
    }

    for (i = 0; i < ARRAY_SIZE(fakes); ++i) {
        const struct fake_open *fake_open = &fakes[i];
        const char *tmpdir;
        char filename[PATH_MAX];
        int fd;

        if (!fake_open->cmp(pathname, fake_open->filename)) {
            continue;
        }

        /* create temporary file to map stat to */
        tmpdir = getenv("TMPDIR");
        if (!tmpdir) {
            tmpdir = "/tmp";
        }
        snprintf(filename, sizeof(filename), "%s/qemu-open.XXXXXX", tmpdir);
        fd = mkstemp(filename);
        if (fd < 0) {
            ret = -TARGET_ENOENT;
            goto done;
        }
        unlink(filename);

        ret = fake_open->fill(cpu_env, fd);
        if (ret) {
            ret = get_errno(ret);
            close(fd);
            goto done;
        }
        lseek(fd, 0, SEEK_SET);
        ret = fd;
        goto done;
    }

    ret = get_errno(safe_openat(dirfd, path(pathname), flags, mode));
 done:
    fd_trans_unregister(ret);
    unlock_user(pathname, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_open
SYSCALL_IMPL(open)
{
    return do_openat(cpu_env, AT_FDCWD, arg1, arg2, arg3);
}
#endif

SYSCALL_IMPL(openat)
{
    return do_openat(cpu_env, arg1, arg2, arg3, arg4);
}

SYSCALL_IMPL(name_to_handle_at)
{
    struct file_handle *target_fh;
    struct file_handle *fh;
    int mid = 0;
    abi_long ret;
    char *name;
    uint32_t size, total_size;

    if (get_user_s32(size, arg3)) {
        return -TARGET_EFAULT;
    }
    total_size = sizeof(struct file_handle) + size;
    target_fh = lock_user(VERIFY_WRITE, arg3, total_size, 0);
    if (!target_fh) {
        return -TARGET_EFAULT;
    }

    name = lock_user_string(arg2);
    if (!name) {
        unlock_user(target_fh, arg3, 0);
        return -TARGET_EFAULT;
    }

    fh = g_malloc0(total_size);
    fh->handle_bytes = size;

    ret = get_errno(safe_name_to_handle_at(arg1, path(name), fh, &mid, arg5));
    unlock_user(name, arg2, 0);

    /*
     * man name_to_handle_at(2):
     * Other than the use of the handle_bytes field, the caller should treat
     * the file_handle structure as an opaque data type
     */
    if (!is_error(ret)) {
        memcpy(target_fh, fh, total_size);
        target_fh->handle_bytes = tswap32(fh->handle_bytes);
        target_fh->handle_type = tswap32(fh->handle_type);
        g_free(fh);
        unlock_user(target_fh, arg3, total_size);

        if (put_user_s32(mid, arg4)) {
            return -TARGET_EFAULT;
        }
    }
    return ret;
}

SYSCALL_IMPL(open_by_handle_at)
{
    abi_long mount_fd = arg1;
    abi_long handle = arg2;
    int host_flags = target_to_host_bitmask(arg3, fcntl_flags_tbl);
    struct file_handle *target_fh;
    struct file_handle *fh;
    unsigned int size, total_size;
    abi_long ret;

    if (get_user_s32(size, handle)) {
        return -TARGET_EFAULT;
    }
    total_size = sizeof(struct file_handle) + size;
    target_fh = lock_user(VERIFY_READ, handle, total_size, 1);
    if (!target_fh) {
        return -TARGET_EFAULT;
    }

    fh = g_memdup(target_fh, total_size);
    fh->handle_bytes = size;
    fh->handle_type = tswap32(target_fh->handle_type);

    ret = get_errno(safe_open_by_handle_at(mount_fd, fh, host_flags));

    g_free(fh);
    unlock_user(target_fh, handle, total_size);

    fd_trans_unregister(ret);
    return ret;
}

static abi_long do_pipe(CPUArchState *cpu_env, abi_ulong target_fds,
                        int target_flags, bool is_pipe2)
{
    int host_flags = target_to_host_bitmask(target_flags, fcntl_flags_tbl);
    int host_fds[2];
    abi_long ret;

    ret = pipe2(host_fds, host_flags);
    if (is_error(ret)) {
        return get_errno(ret);
    }

    /*
     * Several targets have special calling conventions for the original
     * pipe syscall, but didn't replicate this into the pipe2 syscall.
     */
    if (!is_pipe2) {
#if defined(TARGET_ALPHA)
        cpu_env->ir[IR_A4] = host_fds[1];
        return host_fds[0];
#elif defined(TARGET_MIPS)
        cpu_env->active_tc.gpr[3] = host_fds[1];
        return host_fds[0];
#elif defined(TARGET_SH4)
        cpu_env->gregs[1] = host_fds[1];
        return host_fds[0];
#elif defined(TARGET_SPARC)
        cpu_env->regwptr[1] = host_fds[1];
        return host_fds[0];
#endif
    }

    if (put_user_s32(host_fds[0], target_fds)
        || put_user_s32(host_fds[1], target_fds + 4)) {
        return -TARGET_EFAULT;
    }
    return 0;
}

#ifdef TARGET_NR_pipe
SYSCALL_IMPL(pipe)
{
    return do_pipe(cpu_env, arg1, 0, false);
}
#endif

SYSCALL_IMPL(pipe2)
{
    return do_pipe(cpu_env, arg1, arg2, true);
}

/*
 * Both pread64 and pwrite64 merge args into a 64-bit offset,
 * but the input registers and ordering are target specific.
 */
#if TARGET_ABI_BITS == 32
SYSCALL_ARGS(pread64_pwrite64)
{
    /* We have already assigned out[0-2].  */
    int off = regpairs_aligned(cpu_env, TARGET_NR_pread64);
    out[3] = target_offset64(in[3 + off], in[4 + off]);
    return def;
}
#else
#define args_pread64_pwrite64 NULL
#endif

SYSCALL_IMPL(pread64)
{
    int fd = arg1;
    abi_ulong target_buf = arg2;
    abi_ulong len = arg3;
    uint64_t off = arg4;
    void *p;
    abi_long ret;

    if (target_buf == 0 && len == 0) {
        /* Special-case NULL buffer and zero length, which should succeed */
        p = NULL;
    } else {
        p = lock_user(VERIFY_WRITE, target_buf, len, 0);
        if (!p) {
            return -TARGET_EFAULT;
        }
    }
    ret = get_errno(pread64(fd, p, len, off));
    unlock_user(p, target_buf, ret);
    return ret;
}

SYSCALL_IMPL(pwrite64)
{
    int fd = arg1;
    abi_ulong target_buf = arg2;
    abi_ulong len = arg3;
    uint64_t off = arg4;
    void *p;
    abi_long ret;

    if (target_buf == 0 && len == 0) {
        /* Special-case NULL buffer and zero length, which should succeed */
        p = 0;
    } else {
        p = lock_user(VERIFY_READ, target_buf, len, 1);
        if (!p) {
            return -TARGET_EFAULT;
        }
    }
    ret = get_errno(pwrite64(fd, p, len, off));
    unlock_user(p, target_buf, 0);
    return ret;
}

/*
 * Both preadv and pwritev merge args 4/5 into a 64-bit offset.
 * Moreover, the parts are *always* in little-endian order.
 */
#if TARGET_ABI_BITS == 32
SYSCALL_ARGS(preadv_pwritev)
{
    /* We have already assigned out[0-2].  */
    abi_ulong lo = in[3], hi = in[4];
    out[3] = (((uint64_t)hi << (TARGET_ABI_BITS - 1)) << 1) | lo;
    return def;
}
#else
#define args_preadv_pwritev NULL
#endif

/* Perform the inverse operation for the host.  */
static inline void host_offset64_low_high(unsigned long *l, unsigned long *h,
                                          uint64_t off)
{
    *l = off;
    *h = (off >> (HOST_LONG_BITS - 1)) >> 1;
}

SYSCALL_IMPL(preadv)
{
    int fd = arg1;
    abi_ulong target_vec = arg2;
    int count = arg3;
    uint64_t off = arg4;
    struct iovec *vec = lock_iovec(VERIFY_WRITE, target_vec, count, 0);
    unsigned long lo, hi;
    abi_long ret;

    if (vec == NULL) {
        return -TARGET_EFAULT;
    }

    host_offset64_low_high(&lo, &hi, off);
    ret = get_errno(safe_preadv(fd, vec, count, lo, hi));
    unlock_iovec(vec, target_vec, count, 1);
    return ret;
}

SYSCALL_IMPL(pwritev)
{
    int fd = arg1;
    abi_ulong target_vec = arg2;
    int count = arg3;
    uint64_t off = arg4;
    struct iovec *vec = lock_iovec(VERIFY_READ, target_vec, count, 1);
    unsigned long lo, hi;
    abi_long ret;

    if (vec == NULL) {
        return -TARGET_EFAULT;
    }

    host_offset64_low_high(&lo, &hi, off);
    ret = get_errno(safe_pwritev(fd, vec, count, lo, hi));
    unlock_iovec(vec, target_vec, count, 0);
    return ret;
}

SYSCALL_IMPL(pselect6)
{
    abi_long n = arg1;
    abi_ulong rfd_addr = arg2;
    abi_ulong wfd_addr = arg3;
    abi_ulong efd_addr = arg4;
    abi_ulong ts_addr = arg5;
    fd_set rfds, wfds, efds;
    fd_set *rfds_ptr, *wfds_ptr, *efds_ptr;
    struct timespec ts, *ts_ptr = NULL;
    abi_long ret;

    /*
     * The 6th arg is actually two args smashed together, and since
     * we are using safe_syscall, we must handle this ourselves.
     */
    sigset_t set;
    struct {
        sigset_t *set;
        size_t size;
    } sig, *sig_ptr = NULL;

    abi_ulong arg_sigset, arg_sigsize, *arg7;
    target_sigset_t *target_sigset;

    ret = copy_from_user_fdset_ptr(&rfds, &rfds_ptr, rfd_addr, n);
    if (ret) {
        return ret;
    }
    ret = copy_from_user_fdset_ptr(&wfds, &wfds_ptr, wfd_addr, n);
    if (ret) {
        return ret;
    }
    ret = copy_from_user_fdset_ptr(&efds, &efds_ptr, efd_addr, n);
    if (ret) {
        return ret;
    }

    if (ts_addr) {
        if (target_to_host_timespec(&ts, ts_addr)) {
            return -TARGET_EFAULT;
        }
        ts_ptr = &ts;
    }

    /* Extract the two packed args for the sigset */
    if (arg6) {
        sig_ptr = &sig;
        sig.size = SIGSET_T_SIZE;

        arg7 = lock_user(VERIFY_READ, arg6, sizeof(*arg7) * 2, 1);
        if (!arg7) {
            return -TARGET_EFAULT;
        }
        arg_sigset = tswapal(arg7[0]);
        arg_sigsize = tswapal(arg7[1]);
        unlock_user(arg7, arg6, 0);

        if (arg_sigset) {
            sig.set = &set;
            if (arg_sigsize != sizeof(*target_sigset)) {
                /* Like the kernel, we enforce correct size sigsets */
                return -TARGET_EINVAL;
            }
            target_sigset = lock_user(VERIFY_READ, arg_sigset,
                                      sizeof(*target_sigset), 1);
            if (!target_sigset) {
                return -TARGET_EFAULT;
            }
            target_to_host_sigset(&set, target_sigset);
            unlock_user(target_sigset, arg_sigset, 0);
        } else {
            sig.set = NULL;
        }
    }

    ret = get_errno(safe_pselect6(n, rfds_ptr, wfds_ptr, efds_ptr,
                                  ts_ptr, sig_ptr));

    if (!is_error(ret)) {
        if (rfd_addr && copy_to_user_fdset(rfd_addr, &rfds, n)) {
            return -TARGET_EFAULT;
        }
        if (wfd_addr && copy_to_user_fdset(wfd_addr, &wfds, n)) {
            return -TARGET_EFAULT;
        }
        if (efd_addr && copy_to_user_fdset(efd_addr, &efds, n)) {
            return -TARGET_EFAULT;
        }
        if (ts_addr && host_to_target_timespec(ts_addr, &ts)) {
            return -TARGET_EFAULT;
        }
    }
    return ret;
}

SYSCALL_IMPL(read)
{
    int fd = arg1;
    abi_ulong target_p = arg2;
    abi_ulong size = arg3;
    void *p;
    abi_long ret;

    if (target_p == 0 && size == 0) {
        return get_errno(safe_read(fd, NULL, 0));
    }
    p = lock_user(VERIFY_WRITE, target_p, size, 0);
    if (p == NULL) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(safe_read(fd, p, size));
    if (!is_error(ret)) {
        TargetFdDataFunc trans = fd_trans_host_to_target_data(fd);
        if (trans) {
            ret = trans(p, ret);
        }
    }
    unlock_user(p, target_p, ret);
    return ret;
}

SYSCALL_IMPL(readv)
{
    int fd = arg1;
    abi_ulong target_vec = arg2;
    int count = arg3;
    struct iovec *vec = lock_iovec(VERIFY_WRITE, target_vec, count, 0);
    abi_long ret;

    if (vec != NULL) {
        ret = get_errno(safe_readv(fd, vec, count));
        unlock_iovec(vec, target_vec, count, 1);
    } else {
        ret = -host_to_target_errno(errno);
    }
    return ret;
}

static abi_long do_readlinkat(int dirfd, abi_ulong target_path,
                              abi_ulong target_buf, abi_ulong bufsiz)
{
    char *p = lock_user_string(target_path);
    void *buf = lock_user(VERIFY_WRITE, target_buf, bufsiz, 0);
    abi_long ret;

    if (!p || !buf) {
        ret = -TARGET_EFAULT;
    } else if (!bufsiz) {
        /* Short circuit this for the magic exe check. */
        ret = -TARGET_EINVAL;
    } else if (is_proc_myself((const char *)p, "exe")) {
        char real[PATH_MAX];
        char *temp = realpath(exec_path, real);

        if (temp == NULL) {
            ret = -host_to_target_errno(errno);
        } else {
            ret = MIN(strlen(real), bufsiz);
            /* We cannot NUL terminate the string. */
            memcpy(buf, real, ret);
        }
    } else {
        ret = get_errno(readlinkat(dirfd, path(p), buf, bufsiz));
    }
    unlock_user(buf, target_buf, ret);
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_readlink
SYSCALL_IMPL(readlink)
{
    return do_readlinkat(AT_FDCWD, arg1, arg2, arg3);
}
#endif

#ifdef TARGET_NR_readlinkat
SYSCALL_IMPL(readlinkat)
{
    return do_readlinkat(arg1, arg2, arg3, arg4);
}
#endif

static abi_long do_renameat2(int oldfd, abi_ulong target_oldpath,
                             int newfd, abi_ulong target_newpath,
                             unsigned int flags)
{
    char *p_old = lock_user_string(target_oldpath);
    char *p_new = lock_user_string(target_newpath);
    abi_long ret = -TARGET_EFAULT;

    if (p_old && p_new) {
        if (flags == 0) {
            ret = renameat(oldfd, p_old, newfd, p_new);
        } else {
#ifdef __NR_renameat2
            ret = syscall(__NR_renameat2, oldfd, p_old, newfd, p_new, flags);
#else
            errno = ENOSYS;
            ret = -1;
#endif
        }
        ret = get_errno(ret);
    }
    unlock_user(p_old, target_oldpath, 0);
    unlock_user(p_new, target_newpath, 0);
    return ret;
}

#ifdef TARGET_NR_rename
SYSCALL_IMPL(rename)
{
    return do_renameat2(AT_FDCWD, arg1, AT_FDCWD, arg2, 0);
}
#endif

#ifdef TARGET_NR_renameat
SYSCALL_IMPL(renameat)
{
    return do_renameat2(arg1, arg2, arg3, arg4, 0);
}
#endif

SYSCALL_IMPL(renameat2)
{
    return do_renameat2(arg1, arg2, arg3, arg4, arg5);
}

#if defined(TARGET_NR_select) && defined(TARGET_WANT_OLD_SYS_SELECT)
SYSCALL_ARGS(select)
{
    struct target_sel_arg_struct *sel;
    abi_ulong inp, outp, exp, tvp;
    abi_long nsel;

    if (!lock_user_struct(VERIFY_READ, sel, in[0], 1)) {
        errno = EFAULT;
        return NULL;
    }
    nsel = tswapal(sel->n);
    inp = tswapal(sel->inp);
    outp = tswapal(sel->outp);
    exp = tswapal(sel->exp);
    tvp = tswapal(sel->tvp);
    unlock_user_struct(sel, in[0], 0);

    out[0] = nsel;
    out[1] = inp;
    out[2] = outp;
    out[3] = exp;
    out[4] = tvp;
    return def;
}
#else
# define args_select NULL
#endif

#if (defined(TARGET_NR_select) && !defined(TARGET_WANT_NI_OLD_SELECT)) \
    || defined(TARGET_NR__newselect)
SYSCALL_IMPL(select)
{
    int n = arg1;
    abi_ulong rfd_addr = arg2;
    abi_ulong wfd_addr = arg3;
    abi_ulong efd_addr = arg4;
    abi_ulong target_tv_addr = arg5;
    fd_set rfds, wfds, efds;
    fd_set *rfds_ptr, *wfds_ptr, *efds_ptr;
    struct timeval tv;
    struct timespec ts, *ts_ptr = NULL;
    abi_long ret;

    ret = copy_from_user_fdset_ptr(&rfds, &rfds_ptr, rfd_addr, n);
    if (ret) {
        return ret;
    }
    ret = copy_from_user_fdset_ptr(&wfds, &wfds_ptr, wfd_addr, n);
    if (ret) {
        return ret;
    }
    ret = copy_from_user_fdset_ptr(&efds, &efds_ptr, efd_addr, n);
    if (ret) {
        return ret;
    }

    if (target_tv_addr) {
        if (copy_from_user_timeval(&tv, target_tv_addr))
            return -TARGET_EFAULT;
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
        ts_ptr = &ts;
    }

    ret = get_errno(safe_pselect6(n, rfds_ptr, wfds_ptr, efds_ptr,
                                  ts_ptr, NULL));

    if (!is_error(ret)) {
        if (rfd_addr && copy_to_user_fdset(rfd_addr, &rfds, n)) {
            return -TARGET_EFAULT;
        }
        if (wfd_addr && copy_to_user_fdset(wfd_addr, &wfds, n)) {
            return -TARGET_EFAULT;
        }
        if (efd_addr && copy_to_user_fdset(efd_addr, &efds, n)) {
            return -TARGET_EFAULT;
        }
        if (target_tv_addr) {
            tv.tv_sec = ts.tv_sec;
            tv.tv_usec = ts.tv_nsec / 1000;
            if (copy_to_user_timeval(target_tv_addr, &tv)) {
                return -TARGET_EFAULT;
            }
        }
    }

    return ret;
}
#endif

SYSCALL_IMPL(swapoff)
{
    char *p = lock_user_string(arg1);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(swapoff(p));
    unlock_user(p, arg1, 0);
    return ret;
}

SYSCALL_IMPL(swapon)
{
    char *p = lock_user_string(arg1);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(swapon(p, arg2));
    unlock_user(p, arg1, 0);
    return ret;
}

static abi_long do_symlinkat(abi_ulong guest_target, int dirfd,
                             abi_ulong guest_path)
{
    char *target = lock_user_string(guest_target);
    char *path = lock_user_string(guest_path);
    abi_long ret = -TARGET_EFAULT;

    if (target && path) {
        ret = get_errno(symlinkat(target, dirfd, path));
    }
    unlock_user(path, guest_path, 0);
    unlock_user(target, guest_target, 0);

    return ret;
}

#ifdef TARGET_NR_symlink
SYSCALL_IMPL(symlink)
{
    return do_symlinkat(arg1, AT_FDCWD, arg2);
}
#endif

SYSCALL_IMPL(symlinkat)
{
    return do_symlinkat(arg1, arg2, arg3);
}

SYSCALL_IMPL(sync)
{
    sync();
    return 0;
}

SYSCALL_IMPL(syncfs)
{
    return get_errno(syncfs(arg1));
}

SYSCALL_IMPL(truncate)
{
    char *p = lock_user_string(arg1);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(truncate(p, arg2));
    unlock_user(p, arg1, 0);
    return ret;
}

static abi_long do_umount2(abi_ulong target_path, int flags)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(umount2(p, flags));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_umount
SYSCALL_IMPL(umount)
{
    return do_umount2(arg1, 0);
}
#endif

SYSCALL_IMPL(umount2)
{
    return do_umount2(arg1, arg2);
}

static abi_long do_unlinkat(int dirfd, abi_ulong target_path, int flags)
{
    char *p = lock_user_string(target_path);
    abi_long ret;

    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(unlinkat(dirfd, p, flags));
    unlock_user(p, target_path, 0);
    return ret;
}

#ifdef TARGET_NR_unlink
SYSCALL_IMPL(unlink)
{
    return do_unlinkat(AT_FDCWD, arg1, 0);
}
#endif

#ifdef TARGET_NR_rmdir
SYSCALL_IMPL(rmdir)
{
    return do_unlinkat(AT_FDCWD, arg1, AT_REMOVEDIR);
}
#endif

SYSCALL_IMPL(umask)
{
    return get_errno(umask(arg1));
}

SYSCALL_IMPL(unlinkat)
{
    return do_unlinkat(arg1, arg2, arg3);
}

#ifdef TARGET_NR_utime
SYSCALL_IMPL(utime)
{
    abi_ulong target_path = arg1;
    abi_ulong target_times = arg2;
    struct utimbuf tbuf, *host_tbuf;
    struct target_utimbuf *target_tbuf;
    char *p;
    abi_long ret;

    if (target_times) {
        if (!lock_user_struct(VERIFY_READ, target_tbuf, target_times, 1)) {
            return -TARGET_EFAULT;
        }
        tbuf.actime = tswapal(target_tbuf->actime);
        tbuf.modtime = tswapal(target_tbuf->modtime);
        unlock_user_struct(target_tbuf, arg2, 0);
        host_tbuf = &tbuf;
    } else {
        host_tbuf = NULL;
    }

    p = lock_user_string(target_path);
    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(utime(p, host_tbuf));
    unlock_user(p, target_path, 0);
    return ret;
}
#endif

#ifdef TARGET_NR_utimes
SYSCALL_IMPL(utimes)
{
    abi_ulong target_path = arg1;
    abi_ulong target_tv = arg2;
    struct timeval *tvp, tv[2];
    char *p;
    abi_long ret;

    if (target_tv) {
        if (copy_from_user_timeval(&tv[0], target_tv)
            || copy_from_user_timeval(&tv[1],
                                      target_tv +
                                      sizeof(struct target_timeval))) {
            return -TARGET_EFAULT;
        }
        tvp = tv;
    } else {
        tvp = NULL;
    }

    p = lock_user_string(target_path);
    if (!p) {
        return -TARGET_EFAULT;
    }
    ret = get_errno(utimes(p, tvp));
    unlock_user(p, target_path, 0);
    return ret;
}
#endif

SYSCALL_IMPL(write)
{
    int fd = arg1;
    abi_ulong target_p = arg2;
    abi_ulong size = arg3;
    TargetFdDataFunc trans;
    abi_long ret;
    void *p;

    if (target_p == 0 && size == 0) {
        return get_errno(safe_write(fd, NULL, 0));
    }
    p = lock_user(VERIFY_READ, target_p, size, 1);
    if (p == NULL) {
        return -TARGET_EFAULT;
    }
    trans = fd_trans_target_to_host_data(fd);
    if (trans) {
        void *copy = g_malloc(size);
        memcpy(copy, p, size);
        ret = trans(copy, size);
        if (ret >= 0) {
            ret = get_errno(safe_write(fd, copy, ret));
        }
        g_free(copy);
    } else {
        ret = get_errno(safe_write(fd, p, size));
    }
    unlock_user(p, target_p, 0);
    return ret;
}

SYSCALL_IMPL(writev)
{
    int fd = arg1;
    abi_ulong target_vec = arg2;
    int count = arg3;
    struct iovec *vec = lock_iovec(VERIFY_READ, target_vec, count, 1);
    abi_long ret;

    if (vec != NULL) {
        ret = get_errno(safe_writev(fd, vec, count));
        unlock_iovec(vec, target_vec, count, 0);
    } else {
        ret = -host_to_target_errno(errno);
    }
    return ret;
}
