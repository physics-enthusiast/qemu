/*
 * QEMU Guest Agent common/cross-platform common commands
 *
 * Copyright (c) 2020 Red Hat, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef QGA_COMMANDS_COMMON_H
#define QGA_COMMANDS_COMMON_H

#include "qga-qapi-types.h"

typedef struct GuestFileHandle GuestFileHandle;

GuestFileHandle *guest_file_handle_find(int64_t id, Error **errp);

GuestFileRead *guest_file_read_unsafe(GuestFileHandle *gfh,
                                      int64_t count, Error **errp);

/**
 * qga_get_host_name:
 * @errp: Error object
 *
 * Operating system agnostic way of querying host name.
 * Compared to g_get_host_name(), it doesn't cache the result.
 *
 * Returns allocated hostname (caller should free), NULL on failure.
 */
char *qga_get_host_name(Error **errp);

#endif
