/*
 * Hosted file support for semihosting syscalls.
 *
 * Copyright (c) 2005, 2007 CodeSourcery.
 * Copyright (c) 2019 Linaro
 * Copyright © 2020 by Keith Packard <keithp@keithp.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "exec/gdbstub.h"
#include "semihosting/guestfd.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#else
#include "semihosting/softmmu-uaccess.h"
#endif

static GArray *guestfd_array;

/*
 * Allocate a new guest file descriptor and return it; if we
 * couldn't allocate a new fd then return -1.
 * This is a fairly simplistic implementation because we don't
 * expect that most semihosting guest programs will make very
 * heavy use of opening and closing fds.
 */
int alloc_guestfd(void)
{
    guint i;

    if (!guestfd_array) {
        /* New entries zero-initialized, i.e. type GuestFDUnused */
        guestfd_array = g_array_new(FALSE, TRUE, sizeof(GuestFD));
    }

    /* SYS_OPEN should return nonzero handle on success. Start guestfd from 1 */
    for (i = 1; i < guestfd_array->len; i++) {
        GuestFD *gf = &g_array_index(guestfd_array, GuestFD, i);

        if (gf->type == GuestFDUnused) {
            return i;
        }
    }

    /* All elements already in use: expand the array */
    g_array_set_size(guestfd_array, i + 1);
    return i;
}

static void do_dealloc_guestfd(GuestFD *gf)
{
    gf->type = GuestFDUnused;
}

/*
 * Look up the guestfd in the data structure; return NULL
 * for out of bounds, but don't check whether the slot is unused.
 * This is used internally by the other guestfd functions.
 */
static GuestFD *do_get_guestfd(int guestfd)
{
    if (!guestfd_array) {
        return NULL;
    }

    if (guestfd <= 0 || guestfd >= guestfd_array->len) {
        return NULL;
    }

    return &g_array_index(guestfd_array, GuestFD, guestfd);
}

/*
 * Given a guest file descriptor, get the associated struct.
 * If the fd is not valid, return NULL. This is the function
 * used by the various semihosting calls to validate a handle
 * from the guest.
 * Note: calling alloc_guestfd() or dealloc_guestfd() will
 * invalidate any GuestFD* obtained by calling this function.
 */
GuestFD *get_guestfd(int guestfd)
{
    GuestFD *gf = do_get_guestfd(guestfd);

    if (!gf || gf->type == GuestFDUnused) {
        return NULL;
    }
    return gf;
}

/*
 * Associate the specified guest fd (which must have been
 * allocated via alloc_fd() and not previously used) with
 * the specified host/gdb fd.
 */
void associate_guestfd(int guestfd, int hostfd)
{
    GuestFD *gf = do_get_guestfd(guestfd);

    assert(gf);
    gf->type = use_gdb_syscalls() ? GuestFDGDB : GuestFDHost;
    gf->hostfd = hostfd;
}

void staticfile_guestfd(int guestfd, const uint8_t *data, size_t len)
{
    GuestFD *gf = do_get_guestfd(guestfd);

    assert(gf);
    gf->type = GuestFDStatic;
    gf->staticfile.data = data;
    gf->staticfile.len = len;
    gf->staticfile.off = 0;
}

/*
 * Deallocate the specified guest file descriptor. This doesn't
 * close the host fd, it merely undoes the work of alloc_fd().
 */
void dealloc_guestfd(int guestfd)
{
    GuestFD *gf = do_get_guestfd(guestfd);

    assert(gf);
    do_dealloc_guestfd(gf);
}

/*
 * Validate or compute the length of the string (including terminator).
 */
static int validate_strlen(CPUState *cs, target_ulong str, target_ulong tlen)
{
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    char c;

    if (tlen == 0) {
        ssize_t slen = target_strlen(str);

        if (slen < 0) {
            return -EFAULT;
        }
        if (slen >= INT32_MAX) {
            return -ENAMETOOLONG;
        }
        return slen + 1;
    }
    if (tlen > INT32_MAX) {
        return -ENAMETOOLONG;
    }
    if (get_user_u8(c, str + tlen - 1)) {
        return -EFAULT;
    }
    if (c != 0) {
        return -EINVAL;
    }
    return tlen;
}

static int validate_lock_user_string(char **pstr, CPUState *cs,
                                     target_ulong tstr, target_ulong tlen)
{
    int ret = validate_strlen(cs, tstr, tlen);
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    char *str = NULL;

    if (ret > 0) {
        str = lock_user(VERIFY_READ, tstr, ret, true);
        ret = str ? 0 : -EFAULT;
    }
    *pstr = str;
    return ret;
}

/*
 * File-related semihosting syscall implementations.
 */

static gdb_syscall_complete_cb gdb_open_complete;

static void gdb_open_cb(CPUState *cs, target_ulong ret, target_ulong err)
{
    if (!err) {
        int guestfd = alloc_guestfd();
        associate_guestfd(guestfd, ret);
        ret = guestfd;
    }
    gdb_open_complete(cs, ret, err);
}

static void gdb_open(CPUState *cs, gdb_syscall_complete_cb complete,
                     target_ulong fname, target_ulong fname_len,
                     int gdb_flags, int mode)
{
    int len = validate_strlen(cs, fname, fname_len);
    if (len < 0) {
        complete(cs, -1, -len);
        return;
    }

    gdb_open_complete = complete;
    gdb_do_syscall(gdb_open_cb, "open,%s,%x,%x",
                   fname, len, (target_ulong)gdb_flags, (target_ulong)mode);
}

static void host_open(CPUState *cs, gdb_syscall_complete_cb complete,
                      target_ulong fname, target_ulong fname_len,
                      int gdb_flags, int mode)
{
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    char *p;
    int ret, host_flags;

    ret = validate_lock_user_string(&p, cs, fname, fname_len);
    if (ret < 0) {
        complete(cs, -1, -ret);
        return;
    }

    if (gdb_flags & GDB_O_WRONLY) {
        host_flags = O_WRONLY;
    } else if (gdb_flags & GDB_O_RDWR) {
        host_flags = O_RDWR;
    } else {
        host_flags = O_RDONLY;
    }
    if (gdb_flags & GDB_O_CREAT) {
        host_flags |= O_CREAT;
    }
    if (gdb_flags & GDB_O_TRUNC) {
        host_flags |= O_TRUNC;
    }
    if (gdb_flags & GDB_O_EXCL) {
        host_flags |= O_EXCL;
    }

    ret = open(p, host_flags, mode);
    if (ret < 0) {
        complete(cs, -1, errno);
    } else {
        int guestfd = alloc_guestfd();
        associate_guestfd(guestfd, ret);
        complete(cs, guestfd, 0);
    }
    unlock_user(p, fname, 0);
}

void semihost_sys_open(CPUState *cs, gdb_syscall_complete_cb complete,
                       target_ulong fname, target_ulong fname_len,
                       int gdb_flags, int mode)
{
    if (use_gdb_syscalls()) {
        gdb_open(cs, complete, fname, fname_len, gdb_flags, mode);
    } else {
        host_open(cs, complete, fname, fname_len, gdb_flags, mode);
    }
}

static void gdb_close(CPUState *cs, gdb_syscall_complete_cb complete,
                      GuestFD *gf)
{
    gdb_do_syscall(complete, "close,%x", (target_ulong)gf->hostfd);
}

static void host_close(CPUState *cs, gdb_syscall_complete_cb complete,
                       GuestFD *gf)
{
    /*
     * Only close the underlying host fd if it's one we opened on behalf
     * of the guest in SYS_OPEN.
     */
    if (gf->hostfd != STDIN_FILENO &&
        gf->hostfd != STDOUT_FILENO &&
        gf->hostfd != STDERR_FILENO &&
        close(gf->hostfd) < 0) {
        complete(cs, -1, errno);
    } else {
        complete(cs, 0, 0);
    }
}

void semihost_sys_close(CPUState *cs, gdb_syscall_complete_cb complete, int fd)
{
    GuestFD *gf = get_guestfd(fd);

    if (!gf) {
        complete(cs, -1, EBADF);
        return;
    }
    switch (gf->type) {
    case GuestFDGDB:
        gdb_close(cs, complete, gf);
        break;
    case GuestFDHost:
        host_close(cs, complete, gf);
        break;
    case GuestFDStatic:
        complete(cs, 0, 0);
        break;
    default:
        g_assert_not_reached();
    }
    do_dealloc_guestfd(gf);
}

static void gdb_read(CPUState *cs, gdb_syscall_complete_cb complete,
                     GuestFD *gf, target_ulong buf, target_ulong len)
{
    gdb_do_syscall(complete, "read,%x,%x,%x",
                   (target_ulong)gf->hostfd, buf, len);
}

static void host_read(CPUState *cs, gdb_syscall_complete_cb complete,
                      GuestFD *gf, target_ulong buf, target_ulong len)
{
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    void *ptr = lock_user(VERIFY_WRITE, buf, len, 0);
    ssize_t ret;

    if (!ptr) {
        complete(cs, -1, EFAULT);
        return;
    }
    do {
        ret = read(gf->hostfd, ptr, len);
    } while (ret == -1 && errno == EINTR);
    if (ret == -1) {
        complete(cs, -1, errno);
        unlock_user(ptr, buf, 0);
    } else {
        complete(cs, ret, 0);
        unlock_user(ptr, buf, ret);
    }
}

static void staticfile_read(CPUState *cs, gdb_syscall_complete_cb complete,
                            GuestFD *gf, target_ulong buf, target_ulong len)
{
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    target_ulong rest = gf->staticfile.len - gf->staticfile.off;
    void *ptr;

    if (len > rest) {
        len = rest;
    }
    ptr = lock_user(VERIFY_WRITE, buf, len, 0);
    if (!ptr) {
        complete(cs, -1, EFAULT);
        return;
    }
    memcpy(ptr, gf->staticfile.data + gf->staticfile.off, len);
    gf->staticfile.off += len;
    complete(cs, len, 0);
    unlock_user(ptr, buf, len);
}

void semihost_sys_read(CPUState *cs, gdb_syscall_complete_cb complete,
                       int fd, target_ulong buf, target_ulong len)
{
    GuestFD *gf = get_guestfd(fd);

    if (!gf) {
        complete(cs, -1, EBADF);
        return;
    }
    /*
     * Bound length for 64-bit guests on 32-bit hosts, not overlowing ssize_t.
     * Note the Linux kernel does this with MAX_RW_COUNT, so it's not a bad
     * idea to do this unconditionally.
     */
    if (len > INT32_MAX) {
        len = INT32_MAX;
    }
    switch (gf->type) {
    case GuestFDGDB:
        gdb_read(cs, complete, gf, buf, len);
        break;
    case GuestFDHost:
        host_read(cs, complete, gf, buf, len);
        break;
    case GuestFDStatic:
        staticfile_read(cs, complete, gf, buf, len);
        break;
    default:
        g_assert_not_reached();
    }
}

static void gdb_write(CPUState *cs, gdb_syscall_complete_cb complete,
                      GuestFD *gf, target_ulong buf, target_ulong len)
{
    gdb_do_syscall(complete, "write,%x,%x,%x",
                   (target_ulong)gf->hostfd, buf, len);
}

static void host_write(CPUState *cs, gdb_syscall_complete_cb complete,
                       GuestFD *gf, target_ulong buf, target_ulong len)
{
    CPUArchState *env G_GNUC_UNUSED = cs->env_ptr;
    void *ptr = lock_user(VERIFY_READ, buf, len, 1);
    ssize_t ret;

    if (!ptr) {
        complete(cs, -1, EFAULT);
        return;
    }
    ret = write(gf->hostfd, ptr, len);
    complete(cs, ret, ret == -1 ? errno : 0);
    unlock_user(ptr, buf, 0);
}

void semihost_sys_write(CPUState *cs, gdb_syscall_complete_cb complete,
                        int fd, target_ulong buf, target_ulong len)
{
    GuestFD *gf = get_guestfd(fd);

    if (!gf) {
        goto ebadf;
    }
    /*
     * Bound length for 64-bit guests on 32-bit hosts, not overlowing ssize_t.
     * Note the Linux kernel does this with MAX_RW_COUNT, so it's not a bad
     * idea to do this unconditionally.
     */
    if (len > INT32_MAX) {
        len = INT32_MAX;
    }
    switch (gf->type) {
    case GuestFDGDB:
        gdb_write(cs, complete, gf, buf, len);
        break;
    case GuestFDHost:
        host_write(cs, complete, gf, buf, len);
        break;
    case GuestFDStatic:
    ebadf:
        /* Static files are never open for writing: EBADF. */
        complete(cs, -1, EBADF);
        break;
    default:
        g_assert_not_reached();
    }
}

static void gdb_lseek(CPUState *cs, gdb_syscall_complete_cb complete,
                      GuestFD *gf, int64_t off, int gdb_whence)
{
    gdb_do_syscall(complete, "lseek,%x,%lx,%x",
                   (target_ulong)gf->hostfd, off, (target_ulong)gdb_whence);
}

static void host_lseek(CPUState *cs, gdb_syscall_complete_cb complete,
                       GuestFD *gf, int64_t off, int whence)
{
    /* So far, all hosts use the same values. */
    QEMU_BUILD_BUG_ON(GDB_SEEK_SET != SEEK_SET);
    QEMU_BUILD_BUG_ON(GDB_SEEK_CUR != SEEK_CUR);
    QEMU_BUILD_BUG_ON(GDB_SEEK_END != SEEK_END);

    off_t ret = off;
    int err = 0;

    if (ret == off) {
        ret = lseek(gf->hostfd, ret, whence);
        if (ret == -1) {
            err = errno;
        }
    } else {
        ret = -1;
        err = EOVERFLOW;
    }
    complete(cs, ret, err);
}

static void staticfile_lseek(CPUState *cs, gdb_syscall_complete_cb complete,
                             GuestFD *gf, int64_t off, int gdb_whence)
{
    int64_t ret;

    switch (gdb_whence) {
    case GDB_SEEK_SET:
        ret = off;
        break;
    case GDB_SEEK_CUR:
        ret = gf->staticfile.off + off;
        break;
    case GDB_SEEK_END:
        ret = gf->staticfile.len - off;
        break;
    default:
        ret = -1;
        break;
    }
    if (ret >= 0 && ret <= gf->staticfile.len) {
        gf->staticfile.off = ret;
        complete(cs, ret, 0);
    } else {
        complete(cs, -1, EINVAL);
    }
}

void semihost_sys_lseek(CPUState *cs, gdb_syscall_complete_cb complete,
                        int fd, int64_t off, int gdb_whence)
{
    GuestFD *gf = get_guestfd(fd);

    if (!gf) {
        complete(cs, -1, EBADF);
        return;
    }
    switch (gf->type) {
    case GuestFDGDB:
        gdb_lseek(cs, complete, gf, off, gdb_whence);
        return;
    case GuestFDHost:
        host_lseek(cs, complete, gf, off, gdb_whence);
        break;
    case GuestFDStatic:
        staticfile_lseek(cs, complete, gf, off, gdb_whence);
        break;
    default:
        g_assert_not_reached();
    }
}
