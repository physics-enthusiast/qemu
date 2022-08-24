/*
 * Virtio Block Device common helpers
 *
 * Copyright IBM, Corp. 2007
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef VIRTIO_BLK_COMMON_H
#define VIRTIO_BLK_COMMON_H

#include <stddef.h>
#include <stdint.h>

size_t virtio_blk_common_get_config_size(uint64_t host_features);

#endif
