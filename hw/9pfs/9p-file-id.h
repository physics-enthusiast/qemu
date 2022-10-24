/*
 * 9p file representation for different hosts
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_9P_FILE_ID_H
#define QEMU_9P_FILE_ID_H

/*
 * 9pfs file id
 *
 * This is file descriptor on POSIX platforms, handle on Windows
 */
#ifndef CONFIG_WIN32
typedef int P9_FILE_ID;
#else
typedef HANDLE P9_FILE_ID;
#endif

/* invalid value for P9_FILE_ID */
#ifndef CONFIG_WIN32
#define P9_INVALID_FILE -1
#else
#define P9_INVALID_FILE INVALID_HANDLE_VALUE
#endif

#endif
