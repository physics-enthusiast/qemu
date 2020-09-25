/*
 * Dirtyrate implement code
 *
 * Copyright (c) 2020 HUAWEI TECHNOLOGIES CO.,LTD.
 *
 * Authors:
 *  Chuan Zheng <zhengchuan@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <zlib.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "qemu/config-file.h"
#include "exec/memory.h"
#include "exec/ramblock.h"
#include "exec/target_page.h"
#include "qemu/rcu_queue.h"
#include "qapi/qapi-commands-migration.h"
#include "migration.h"
#include "ram.h"
#include "dirtyrate.h"

static int CalculatingState = DIRTY_RATE_STATUS_UNSTARTED;
static struct DirtyRateStat DirtyStat;

static int dirtyrate_set_state(int *state, int old_state, int new_state)
{
    assert(new_state < DIRTY_RATE_STATUS__MAX);
    if (qatomic_cmpxchg(state, old_state, new_state) == old_state) {
        return 0;
    } else {
        return -1;
    }
}

static void reset_dirtyrate_stat(void)
{
    DirtyStat.total_dirty_samples = 0;
    DirtyStat.total_sample_count = 0;
    DirtyStat.total_block_mem_MB = 0;
    DirtyStat.dirty_rate = -1;
    DirtyStat.start_time = 0;
    DirtyStat.calc_time = 0;
}

static void update_dirtyrate_stat(struct RamblockDirtyInfo *info)
{
    DirtyStat.total_dirty_samples += info->sample_dirty_count;
    DirtyStat.total_sample_count += info->sample_pages_count;
    /* size of total pages in MB */
    DirtyStat.total_block_mem_MB += (info->ramblock_pages *
                                     TARGET_PAGE_SIZE) >> 20;
}

static void update_dirtyrate(uint64_t msec)
{
    uint64_t dirtyrate;
    uint64_t total_dirty_samples = DirtyStat.total_dirty_samples;
    uint64_t total_sample_count = DirtyStat.total_sample_count;
    uint64_t total_block_mem_MB = DirtyStat.total_block_mem_MB;

    dirtyrate = total_dirty_samples * total_block_mem_MB *
                1000 / (total_sample_count * msec);

    DirtyStat.dirty_rate = dirtyrate;
}

/*
 * get hash result for the sampled memory with length of TARGET_PAGE_SIZE
 * in ramblock, which starts from ramblock base address.
 */
static uint32_t get_ramblock_vfn_hash(struct RamblockDirtyInfo *info,
                                      uint64_t vfn)
{
    uint32_t crc;

    crc = crc32(0, (info->ramblock_addr +
                vfn * TARGET_PAGE_SIZE), TARGET_PAGE_SIZE);

    return crc;
}

static bool save_ramblock_hash(struct RamblockDirtyInfo *info)
{
    unsigned int sample_pages_count;
    int i;
    GRand *rand;

    sample_pages_count = info->sample_pages_count;

    /* ramblock size less than one page, return success to skip this ramblock */
    if (unlikely(info->ramblock_pages == 0 || sample_pages_count == 0)) {
        return true;
    }

    info->hash_result = g_try_malloc0_n(sample_pages_count,
                                        sizeof(uint32_t));
    if (!info->hash_result) {
        return false;
    }

    info->sample_page_vfn = g_try_malloc0_n(sample_pages_count,
                                            sizeof(uint64_t));
    if (!info->sample_page_vfn) {
        g_free(info->hash_result);
        return false;
    }

    rand  = g_rand_new();
    for (i = 0; i < sample_pages_count; i++) {
        info->sample_page_vfn[i] = g_rand_int_range(rand, 0,
                                                    info->ramblock_pages - 1);
        info->hash_result[i] = get_ramblock_vfn_hash(info,
                                                     info->sample_page_vfn[i]);
    }
    g_rand_free(rand);

    return true;
}

static void get_ramblock_dirty_info(RAMBlock *block,
                                    struct RamblockDirtyInfo *info,
                                    struct DirtyRateConfig *config)
{
    uint64_t sample_pages_per_gigabytes = config->sample_pages_per_gigabytes;

    /* Right shift 30 bits to calc ramblock size in GB */
    info->sample_pages_count = (qemu_ram_get_used_length(block) *
                                sample_pages_per_gigabytes) >> 30;
    /* Right shift TARGET_PAGE_BITS to calc page count */
    info->ramblock_pages = qemu_ram_get_used_length(block) >>
                           TARGET_PAGE_BITS;
    info->ramblock_addr = qemu_ram_get_host_addr(block);
    strcpy(info->idstr, qemu_ram_get_idstr(block));
}

static bool skip_sample_ramblock(RAMBlock *block)
{
    /*
     * Sample only blocks larger than MIN_RAMBLOCK_SIZE.
     */
    if (qemu_ram_get_used_length(block) < (MIN_RAMBLOCK_SIZE << 10)) {
        return true;
    }

    return false;
}

static bool record_ramblock_hash_info(struct RamblockDirtyInfo **block_dinfo,
                                      struct DirtyRateConfig config,
                                      int *block_count)
{
    struct RamblockDirtyInfo *info = NULL;
    struct RamblockDirtyInfo *dinfo = NULL;
    RAMBlock *block = NULL;
    int total_count = 0;
    int index = 0;
    bool ret = false;

    RAMBLOCK_FOREACH_MIGRATABLE(block) {
        if (skip_sample_ramblock(block)) {
            continue;
        }
        total_count++;
    }

    dinfo = g_try_malloc0_n(total_count, sizeof(struct RamblockDirtyInfo));
    if (dinfo == NULL) {
        goto out;
    }

    RAMBLOCK_FOREACH_MIGRATABLE(block) {
        if (skip_sample_ramblock(block)) {
            continue;
        }
        if (index >= total_count) {
            break;
        }
        info = &dinfo[index];
        get_ramblock_dirty_info(block, info, &config);
        if (!save_ramblock_hash(info)) {
            goto out;
        }
        index++;
    }
    ret = true;

out:
    *block_count = index;
    *block_dinfo = dinfo;
    return ret;
}

static void calc_page_dirty_rate(struct RamblockDirtyInfo *info)
{
    uint32_t crc;
    int i;

    for (i = 0; i < info->sample_pages_count; i++) {
        crc = get_ramblock_vfn_hash(info, info->sample_page_vfn[i]);
        if (crc != info->hash_result[i]) {
            info->sample_dirty_count++;
        }
    }
}

static struct RamblockDirtyInfo *
find_block_matched(RAMBlock *block, int count,
                  struct RamblockDirtyInfo *infos)
{
    int i;
    struct RamblockDirtyInfo *matched;

    for (i = 0; i < count; i++) {
        if (!strcmp(infos[i].idstr, qemu_ram_get_idstr(block))) {
            break;
        }
    }

    if (i == count) {
        return NULL;
    }

    if (infos[i].ramblock_addr != qemu_ram_get_host_addr(block) ||
        infos[i].ramblock_pages !=
            (qemu_ram_get_used_length(block) >> TARGET_PAGE_BITS)) {
        return NULL;
    }

    matched = &infos[i];

    return matched;
}

static bool compare_page_hash_info(struct RamblockDirtyInfo *info,
                                  int block_count)
{
    struct RamblockDirtyInfo *block_dinfo = NULL;
    RAMBlock *block = NULL;

    RAMBLOCK_FOREACH_MIGRATABLE(block) {
        if (skip_sample_ramblock(block)) {
            continue;
        }
        block_dinfo = find_block_matched(block, block_count, info);
        if (block_dinfo == NULL) {
            continue;
        }
        calc_page_dirty_rate(block_dinfo);
        update_dirtyrate_stat(block_dinfo);
    }

    if (DirtyStat.total_sample_count == 0) {
        return false;
    }

    return true;
}

static void calculate_dirtyrate(struct DirtyRateConfig config)
{
    /* todo */
    return;
}

void *get_dirtyrate_thread(void *arg)
{
    struct DirtyRateConfig config = *(struct DirtyRateConfig *)arg;
    int ret;

    ret = dirtyrate_set_state(&CalculatingState, DIRTY_RATE_STATUS_UNSTARTED,
                              DIRTY_RATE_STATUS_MEASURING);
    if (ret == -1) {
        error_report("change dirtyrate state failed.");
        return NULL;
    }

    calculate_dirtyrate(config);

    ret = dirtyrate_set_state(&CalculatingState, DIRTY_RATE_STATUS_MEASURING,
                              DIRTY_RATE_STATUS_MEASURED);
    if (ret == -1) {
        error_report("change dirtyrate state failed.");
    }
    return NULL;
}
