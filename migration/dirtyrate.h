/*
 *  Dirtyrate common functions
 *
 *  Copyright (c) 2020 HUAWEI TECHNOLOGIES CO., LTD.
 *
 *  Authors:
 *  Chuan Zheng <zhengchuan@huawei.com>
 *
 *  This work is licensed under the terms of the GNU GPL, version 2 or later.
 *  See the COPYING file in the top-level directory.
 */

#ifndef QEMU_MIGRATION_DIRTYRATE_H
#define QEMU_MIGRATION_DIRTYRATE_H

/*
 * Sample 512 pages per GB as default.
 * TODO: Make it configurable.
 */
#define DIRTYRATE_DEFAULT_SAMPLE_PAGES            512

/*
 * Record ramblock idstr
 */
#define RAMBLOCK_INFO_MAX_LEN                     256

/*
 * Sample page size 4K as default.
 */
#define DIRTYRATE_SAMPLE_PAGE_SIZE                4096

/*
 * Sample page size 4K shift
 */
#define DIRTYRATE_PAGE_SHIFT_KB                   12

/*
 * Sample page size 1G shift
 */
#define DIRTYRATE_PAGE_SHIFT_GB                   30

/* Take 1s as default for calculation duration */
#define DEFAULT_FETCH_DIRTYRATE_TIME_SEC          1

struct DirtyRateConfig {
    uint64_t sample_pages_per_gigabytes; /* sample pages per GB */
    int64_t sample_period_seconds; /* time duration between two sampling */
};

/*
 * Store dirtypage info for each ramblock.
 */
struct RamblockDirtyInfo {
    char idstr[RAMBLOCK_INFO_MAX_LEN]; /* idstr for each ramblock */
    uint8_t *ramblock_addr; /* base address of ramblock we measure */
    uint64_t ramblock_pages; /* ramblock size in 4K-page */
    uint64_t *sample_page_vfn; /* relative offset address for sampled page */
    uint64_t sample_pages_count; /* count of sampled pages */
    uint64_t sample_dirty_count; /* cout of dirty pages we measure */
    uint32_t *hash_result; /* array of hash result for sampled pages */
};

/*
 * Store calculation statistics for each measure.
 */
struct DirtyRateStat {
    uint64_t total_dirty_samples; /* total dirty sampled page */
    uint64_t total_sample_count; /* total sampled pages */
    uint64_t total_block_mem_MB; /* size of total sampled pages in MB */
    int64_t dirty_rate; /* dirty rate in MB/s */
};

void *get_dirtyrate_thread(void *arg);
#endif

