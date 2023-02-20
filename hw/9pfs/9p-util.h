/*
 * 9p utilities
 *
 * Copyright IBM, Corp. 2017
 *
 * Authors:
 *  Greg Kurz <groug@kaod.org>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_9P_UTIL_H
#define QEMU_9P_UTIL_H

#ifdef O_PATH
#define O_PATH_9P_UTIL O_PATH
#else
#define O_PATH_9P_UTIL 0
#endif

/* forward declaration */
union V9fsFidOpenState;
struct V9fsState;
struct FsContext;

#ifdef CONFIG_DARWIN

/*
 * Generates a Linux device number (a.k.a. dev_t) for given device major
 * and minor numbers.
 *
 * To be more precise: it generates a device number in glibc's format
 * (MMMM_Mmmm_mmmM_MMmm, 64 bits) actually, which is compatible with
 * Linux's format (mmmM_MMmm, 32 bits), as described in <bits/sysmacros.h>.
 */
static inline uint64_t makedev_dotl(uint32_t dev_major, uint32_t dev_minor)
{
    uint64_t dev;

    // from glibc sysmacros.h:
    dev  = (((uint64_t) (dev_major & 0x00000fffu)) <<  8);
    dev |= (((uint64_t) (dev_major & 0xfffff000u)) << 32);
    dev |= (((uint64_t) (dev_minor & 0x000000ffu)) <<  0);
    dev |= (((uint64_t) (dev_minor & 0xffffff00u)) << 12);
    return dev;
}

#endif

/*
 * Converts given device number from host's device number format to Linux
 * device number format. As both the size of type dev_t and encoding of
 * dev_t is system dependant, we have to convert them for Linux guests if
 * host is not running Linux.
 */
static inline uint64_t host_dev_to_dotl_dev(dev_t dev)
{
#if defined(CONFIG_LINUX) || defined(CONFIG_WIN32)
    return dev;
#elif defined(CONFIG_DARWIN)
    return makedev_dotl(major(dev), minor(dev));
#else
#error Missing host_dev_to_dotl_dev() implementation for this host system
#endif
}

#include "9p-linux-errno.h"

/* Translates errno from host -> Linux if needed */
static inline int errno_to_dotl(int err)
{
#if defined(CONFIG_LINUX)
    /* nothing to translate (Linux -> Linux) */
#elif defined(CONFIG_DARWIN)
    /*
     * translation mandatory for macOS hosts
     *
     * FIXME: Only most important errnos translated here yet, this should be
     * extended to as many errnos being translated as possible in future.
     */
    switch (err) {
    case ENAMETOOLONG:  return L_ENAMETOOLONG;
    case ENOTEMPTY:     return L_ENOTEMPTY;
    case ELOOP:         return L_ELOOP;
    case ENOATTR:       return L_ENODATA;
    case ENOTSUP        return L_EOPNOTSUPP;
    case EOPNOTSUPP:    return L_EOPNOTSUPP;
    }
#else
#error Missing errno translation to Linux for this host system
#endif
    return err;
}

#if defined(CONFIG_DARWIN)
#define qemu_fgetxattr(...) fgetxattr(__VA_ARGS__, 0, 0)
#elif defined(CONFIG_WIN32)
#define qemu_fgetxattr fgetxattr_win32
#else
#define qemu_fgetxattr fgetxattr
#endif

#ifdef CONFIG_WIN32
#define qemu_openat     openat_win32
#define qemu_fstatat    fstatat_win32
#define qemu_mkdirat    mkdirat_win32
#define qemu_renameat   renameat_win32
#define qemu_utimensat  utimensat_win32
#define qemu_unlinkat   unlinkat_win32

#define qemu_opendir    opendir_win32
#define qemu_closedir   closedir_win32
#define qemu_readdir    readdir_win32
#define qeme_rewinddir  rewinddir_win32
#define qemu_seekdir    seekdir_win32
#define qemu_telldir    telldir_win32
#else
#define qemu_openat     openat
#define qemu_fstatat    fstatat
#define qemu_mkdirat    mkdirat
#define qemu_renameat   renameat
#define qemu_utimensat  utimensat
#define qemu_unlinkat   unlinkat

#define qemu_opendir    opendir
#define qemu_closedir   closedir
#define qemu_readdir    readdir
#define qeme_rewinddir  rewinddir
#define qemu_seekdir    seekdir
#define qemu_telldir    telldir
#endif

#ifdef CONFIG_WIN32
char *get_full_path_win32(HANDLE hDir, const char *name);
ssize_t fgetxattr_win32(int fd, const char *name, void *value, size_t size);
int openat_win32(int dirfd, const char *pathname, int flags, mode_t mode);
int fstatat_win32(int dirfd, const char *pathname,
                  struct stat *statbuf, int flags);
int mkdirat_win32(int dirfd, const char *pathname, mode_t mode);
int renameat_win32(int olddirfd, const char *oldpath,
                   int newdirfd, const char *newpath);
int utimensat_win32(int dirfd, const char *pathname,
                    const struct timespec times[2], int flags);
int unlinkat_win32(int dirfd, const char *pathname, int flags);
int statfs_win32(const char *root_path, struct statfs *stbuf);
int openat_dir(int dirfd, const char *name);
int openat_file(int dirfd, const char *name, int flags, mode_t mode);
DIR *opendir_win32(const char *full_file_name);
int closedir_win32(DIR *pDir);
struct dirent *readdir_win32(DIR *pDir);
void rewinddir_win32(DIR *pDir);
void seekdir_win32(DIR *pDir, long pos);
long telldir_win32(DIR *pDir);
off_t qemu_dirent_off_win32(struct V9fsState *s, union V9fsFidOpenState *fs);
uint64_t qemu_stat_rdev_win32(struct FsContext *fs_ctx);
uint64_t qemu_stat_blksize_win32(struct FsContext *fs_ctx);
#endif

static inline void close_preserve_errno(int fd)
{
    int serrno = errno;
    close(fd);
    errno = serrno;
}

#ifndef CONFIG_WIN32
static inline int openat_dir(int dirfd, const char *name)
{
    return qemu_openat(dirfd, name,
                       O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_PATH_9P_UTIL);
}

static inline int openat_file(int dirfd, const char *name, int flags,
                              mode_t mode)
{
    int fd, serrno, ret;

#ifndef CONFIG_DARWIN
again:
#endif
    fd = qemu_openat(dirfd, name, flags | O_NOFOLLOW | O_NOCTTY | O_NONBLOCK,
                     mode);
    if (fd == -1) {
#ifndef CONFIG_DARWIN
        if (errno == EPERM && (flags & O_NOATIME)) {
            /*
             * The client passed O_NOATIME but we lack permissions to honor it.
             * Rather than failing the open, fall back without O_NOATIME. This
             * doesn't break the semantics on the client side, as the Linux
             * open(2) man page notes that O_NOATIME "may not be effective on
             * all filesystems". In particular, NFS and other network
             * filesystems ignore it entirely.
             */
            flags &= ~O_NOATIME;
            goto again;
        }
#endif
        return -1;
    }

    serrno = errno;
    /* O_NONBLOCK was only needed to open the file. Let's drop it. We don't
     * do that with O_PATH since fcntl(F_SETFL) isn't supported, and openat()
     * ignored it anyway.
     */
    if (!(flags & O_PATH_9P_UTIL)) {
        ret = fcntl(fd, F_SETFL, flags);
        assert(!ret);
    }
    errno = serrno;
    return fd;
}
#endif

ssize_t fgetxattrat_nofollow(int dirfd, const char *path, const char *name,
                             void *value, size_t size);
int fsetxattrat_nofollow(int dirfd, const char *path, const char *name,
                         void *value, size_t size, int flags);
ssize_t flistxattrat_nofollow(int dirfd, const char *filename,
                              char *list, size_t size);
ssize_t fremovexattrat_nofollow(int dirfd, const char *filename,
                                const char *name);

/*
 * Darwin has d_seekoff, which appears to function similarly to d_off.
 * However, it does not appear to be supported on all file systems,
 * so ensure it is manually injected earlier and call here when
 * needed.
 */
static inline off_t qemu_dirent_off(struct dirent *dent, struct V9fsState *s,
                                    union V9fsFidOpenState *fs)
{
#if defined(CONFIG_DARWIN)
    return dent->d_seekoff;
#elif defined(CONFIG_LINUX)
    return dent->d_off;
#elif defined(CONFIG_WIN32)
    return qemu_dirent_off_win32(s, fs);
#else
#error Missing qemu_dirent_off() implementation for this host system
#endif
}

/**
 * qemu_dirent_dup() - Duplicate directory entry @dent.
 *
 * @dent: original directory entry to be duplicated
 * Return: duplicated directory entry which should be freed with g_free()
 *
 * It is highly recommended to use this function instead of open coding
 * duplication of dirent objects, because the actual struct dirent
 * size may be bigger or shorter than sizeof(struct dirent) and correct
 * handling is platform specific (see gitlab issue #841).
 */
static inline struct dirent *qemu_dirent_dup(struct dirent *dent)
{
    size_t sz = 0;
#if defined _DIRENT_HAVE_D_RECLEN
    /* Avoid use of strlen() if platform supports d_reclen. */
    sz = dent->d_reclen;
#endif
    /*
     * Test sz for zero even if d_reclen is available
     * because some drivers may set d_reclen to zero.
     */
    if (sz == 0) {
        /* Fallback to the most portable way. */
        sz = offsetof(struct dirent, d_name) +
                      strlen(dent->d_name) + 1;
    }
    return g_memdup(dent, sz);
}

static inline uint64_t qemu_stat_rdev(const struct stat *stbuf,
                                      struct FsContext *fs_ctx)
{
#if defined(CONFIG_LINUX) || defined(CONFIG_DARWIN)
    return stbuf->st_rdev;
#elif defined(CONFIG_WIN32)
    return qemu_stat_rdev_win32(fs_ctx);
#else
#error Missing qemu_stat_rdev() implementation for this host system
#endif
}

static inline uint64_t qemu_stat_blksize(const struct stat *stbuf,
                                         struct FsContext *fs_ctx)
{
#if defined(CONFIG_LINUX) || defined(CONFIG_DARWIN)
    return stbuf->st_blksize;
#elif defined(CONFIG_WIN32)
    return qemu_stat_blksize_win32(fs_ctx);
#else
#error Missing qemu_stat_blksize() implementation for this host system
#endif
}

/*
 * As long as mknodat is not available on macOS, this workaround
 * using pthread_fchdir_np is needed. qemu_mknodat is defined in
 * os-posix.c. pthread_fchdir_np is weakly linked here as a guard
 * in case it disappears in future macOS versions, because it is
 * is a private API.
 */
#if defined CONFIG_DARWIN && defined CONFIG_PTHREAD_FCHDIR_NP
int pthread_fchdir_np(int fd) __attribute__((weak_import));
#endif
int qemu_mknodat(int dirfd, const char *filename, mode_t mode, dev_t dev);

#endif
