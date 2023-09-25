/*
 *  miscellaneous FreeBSD system call shims
 *
 *  Copyright (c) 2013-14 Stacey D. Son
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

#ifndef OS_MISC_H
#define OS_MISC_H

#include <sys/cpuset.h>
#include <sys/random.h>
#include <sched.h>

int shm_open2(const char *path, int flags, mode_t mode, int shmflags,
    const char *);

#if defined(__FreeBSD_version) && __FreeBSD_version >= 1300048
/* shm_open2(2) */
static inline abi_long do_freebsd_shm_open2(abi_ulong pathptr, abi_ulong flags,
    abi_long mode, abi_ulong shmflags, abi_ulong nameptr)
{
    int ret;
    void *uname, *upath;

    if (pathptr == (uintptr_t)SHM_ANON) {
        upath = SHM_ANON;
    } else {
        upath = lock_user_string(pathptr);
        if (upath == NULL) {
            return -TARGET_EFAULT;
        }
    }

    uname = NULL;
    if (nameptr != 0) {
        uname = lock_user_string(nameptr);
        if (uname == NULL) {
            unlock_user(upath, pathptr, 0);
            return -TARGET_EFAULT;
        }
    }
    ret = get_errno(shm_open2(upath,
                target_to_host_bitmask(flags, fcntl_flags_tbl), mode,
                target_to_host_bitmask(shmflags, shmflag_flags_tbl), uname));

    if (upath != SHM_ANON) {
        unlock_user(upath, pathptr, 0);
    }
    if (uname != NULL) {
        unlock_user(uname, nameptr, 0);
    }
    return ret;
}
#endif /* __FreeBSD_version >= 1300048 */


#endif /* OS_MISC_H */
