/*
 * QEMU System Emulator, Code Quality Monitor System
 *
 * Copyright (c) 2019 Vanderson M. do Rosario <vandersonmr2@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"

#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"

#include "qemu/qemu-print.h"

#include "exec/tb-stats.h"
#include "tb-context.h"

/* TBStatistic collection controls */
enum TBStatsStatus {
    TB_STATS_STOPPED = 0,
    TB_STATS_RUNNING
};

static enum TBStatsStatus tcg_collect_tb_stats;
static uint32_t tbstats_flag;

struct jit_profile_info {
    uint64_t translations;
    uint64_t aborted;
    uint64_t ops;
    unsigned ops_max;
    uint64_t del_ops;
    uint64_t temps;
    unsigned temps_max;
    uint64_t host;
    uint64_t guest;
    uint64_t search_data;
};

#define stat_per_translation(stat, name) \
    (stat->translations.total ? stat->name / stat->translations.total : 0)

/* accumulate the statistics from all TBs */
static void collect_jit_profile_info(void *p, uint32_t hash, void *userp)
{
    struct jit_profile_info *jpi = userp;
    TBStatistics *tbs = p;

    jpi->translations += tbs->translations.total;
    jpi->ops += tbs->code.num_tcg_ops;
    if (stat_per_translation(tbs, code.num_tcg_ops) > jpi->ops_max) {
        jpi->ops_max = stat_per_translation(tbs, code.num_tcg_ops);
    }
    jpi->del_ops += tbs->code.deleted_ops;
    jpi->temps += tbs->code.temps;
    if (stat_per_translation(tbs, code.temps) > jpi->temps_max) {
        jpi->temps_max = stat_per_translation(tbs, code.temps);
    }
    jpi->host += tbs->code.out_len;
    jpi->guest += tbs->code.in_len;
    jpi->search_data += tbs->code.search_out_len;
}

/* dump JIT statistics using TBStats */
void dump_jit_profile_info(GString *buf)
{
    if (!tb_stats_collection_enabled()) {
        return;
    }

    struct jit_profile_info *jpi = g_new0(struct jit_profile_info, 1);

    qht_iter(&tb_ctx.tb_stats, collect_jit_profile_info, jpi);

    if (jpi->translations) {
        g_string_append_printf(buf, "translated TBs      %" PRId64 "\n",
                jpi->translations);
        g_string_append_printf(buf, "avg ops/TB          %0.1f max=%d\n",
                jpi->ops / (double) jpi->translations, jpi->ops_max);
        g_string_append_printf(buf, "deleted ops/TB      %0.2f\n",
                jpi->del_ops / (double) jpi->translations);
        g_string_append_printf(buf, "avg temps/TB        %0.2f max=%d\n",
                jpi->temps / (double) jpi->translations, jpi->temps_max);
        g_string_append_printf(buf, "avg host code/TB    %0.1f\n",
                jpi->host / (double) jpi->translations);
        g_string_append_printf(buf, "avg search data/TB  %0.1f\n",
                jpi->search_data / (double) jpi->translations);
    }
    g_free(jpi);
}

static void free_tbstats(void *p, uint32_t hash, void *userp)
{
    g_free(p);
}

void clean_tbstats(void)
{
    /* remove all tb_stats */
    qht_iter(&tb_ctx.tb_stats, free_tbstats, NULL);
    qht_destroy(&tb_ctx.tb_stats);
}

void init_tb_stats_htable(void)
{
    if (!tb_ctx.tb_stats.map && tb_stats_collection_enabled()) {
        qht_init(&tb_ctx.tb_stats, tb_stats_cmp,
                CODE_GEN_HTABLE_SIZE, QHT_MODE_AUTO_RESIZE);
    }
}

void enable_collect_tb_stats(void)
{
    tcg_collect_tb_stats = TB_STATS_RUNNING;
    init_tb_stats_htable();
}

void disable_collect_tb_stats(void)
{
    tcg_collect_tb_stats = TB_STATS_STOPPED;
}

bool tb_stats_collection_enabled(void)
{
    return tcg_collect_tb_stats == TB_STATS_RUNNING;
}

bool tb_stats_collection_disabled(void)
{
    return tcg_collect_tb_stats == TB_STATS_STOPPED;
}

uint32_t get_tbstats_flag(void)
{
    return tbstats_flag;
}

void set_tbstats_flag(uint32_t flag)
{
    tbstats_flag = flag;
}

bool tb_stats_enabled(TranslationBlock *tb, uint32_t flag)
{
    return tb_stats_collection_enabled() &&
           tb->tb_stats &&
           (tbstats_flag & flag);
}
