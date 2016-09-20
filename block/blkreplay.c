/*
 * Block protocol for record/replay
 *
 * Copyright (c) 2010-2016 Institute for System Programming
 *                         of the Russian Academy of Sciences.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "block/block_int.h"
#include "sysemu/replay.h"
#include "qapi/error.h"
#include "qapi/qmp/qstring.h"

typedef struct Request {
    Coroutine *co;
    QEMUBH *bh;
} Request;

/* Next request id.
   This counter is global, because requests from different
   block devices should not get overlapping ids. */
static uint64_t request_id;

static BlockDriverState *blkreplay_append_snapshot(BlockDriverState *bs,
                                                   int flags,
                                                   QDict *snapshot_options,
                                                   Error **errp)
{
    int ret;
    BlockDriverState *bs_snapshot;

    /* Create temporary file, if needed */
    if ((flags & BDRV_O_TEMPORARY) || replay_mode == REPLAY_MODE_RECORD) {
        int64_t total_size;
        QemuOpts *opts = NULL;
        const char *tmp_filename = qdict_get_str(snapshot_options,
                                                 "file.filename");

        /* Get the required size from the image */
        total_size = bdrv_getlength(bs);
        if (total_size < 0) {
            error_setg_errno(errp, -total_size, "Could not get image size");
            goto out;
        }

        opts = qemu_opts_create(bdrv_qcow2.create_opts, NULL, 0,
                                &error_abort);
        qemu_opt_set_number(opts, BLOCK_OPT_SIZE, total_size, &error_abort);
        ret = bdrv_create(&bdrv_qcow2, tmp_filename, opts, errp);
        qemu_opts_del(opts);
        if (ret < 0) {
            error_prepend(errp, "Could not create temporary overlay '%s': ",
                          tmp_filename);
            goto out;
        }
    }

    bs_snapshot = bdrv_open(NULL, NULL, snapshot_options, flags, errp);
    snapshot_options = NULL;
    if (!bs_snapshot) {
        ret = -EINVAL;
        goto out;
    }

    /* bdrv_append() consumes a strong reference to bs_snapshot (i.e. it will
     * call bdrv_unref() on it), so in order to be able to return one, we have
     * to increase bs_snapshot's refcount here */
    bdrv_ref(bs_snapshot);
    bdrv_append(bs_snapshot, bs);

    return bs_snapshot;

out:
    QDECREF(snapshot_options);
    return NULL;
}

static QemuOptsList blkreplay_runtime_opts = {
    .name = "blkreplay",
    .head = QTAILQ_HEAD_INITIALIZER(blkreplay_runtime_opts.head),
    .desc = {
        {
            .name = "overlay",
            .type = QEMU_OPT_STRING,
            .help = "Optional overlay file for snapshots",
        },
        { /* end of list */ }
    },
};

static int blkreplay_open(BlockDriverState *bs, QDict *options, int flags,
                          Error **errp)
{
    Error *local_err = NULL;
    int ret;
    QDict *snapshot_options = qdict_new();
    int snapshot_flags = BDRV_O_RDWR;
    const char *snapshot;
    QemuOpts *opts = NULL;

    /* Open the image file */
    bs->file = bdrv_open_child(NULL, options, "image",
                               bs, &child_file, false, &local_err);
    if (local_err) {
        ret = -EINVAL;
        error_propagate(errp, local_err);
        goto fail;
    }

    opts = qemu_opts_create(&blkreplay_runtime_opts, NULL, 0, &error_abort);
    qemu_opts_absorb_qdict(opts, options, &local_err);
    if (local_err) {
        ret = -EINVAL;
        goto fail;
    }

    /* Prepare options QDict for the overlay file */
    qdict_put(snapshot_options, "file.driver",
              qstring_from_str("file"));
    qdict_put(snapshot_options, "driver",
              qstring_from_str("qcow2"));

    snapshot = qemu_opt_get(opts, "overlay");
    if (snapshot) {
        qdict_put(snapshot_options, "file.filename",
                  qstring_from_str(snapshot));
    } else {
        char tmp_filename[PATH_MAX + 1];
        ret = get_tmp_filename(tmp_filename, PATH_MAX + 1);
        if (ret < 0) {
            error_setg_errno(errp, -ret, "Could not get temporary filename");
            goto fail;
        }
        qdict_put(snapshot_options, "file.filename",
                  qstring_from_str(tmp_filename));
        snapshot_flags |= BDRV_O_TEMPORARY;
    }

    /* Add temporary snapshot to preserve the image */
    if (!blkreplay_append_snapshot(bs->file->bs, snapshot_flags,
                      snapshot_options, &local_err)) {
        ret = -EINVAL;
        error_propagate(errp, local_err);
        goto fail;
    }

    ret = 0;
fail:
    if (ret < 0) {
        bdrv_unref_child(bs, bs->file);
    }
    return ret;
}

static void blkreplay_close(BlockDriverState *bs)
{
    bdrv_unref(bs->file->bs);
}

static int64_t blkreplay_getlength(BlockDriverState *bs)
{
    return bdrv_getlength(bs->file->bs);
}

/* This bh is used for synchronization of return from coroutines.
   It continues yielded coroutine which then finishes its execution.
   BH is called adjusted to some replay checkpoint, therefore
   record and replay will always finish coroutines deterministically.
*/
static void blkreplay_bh_cb(void *opaque)
{
    Request *req = opaque;
    qemu_coroutine_enter(req->co);
    qemu_bh_delete(req->bh);
    g_free(req);
}

static void block_request_create(uint64_t reqid, BlockDriverState *bs,
                                 Coroutine *co)
{
    Request *req = g_new(Request, 1);
    *req = (Request) {
        .co = co,
        .bh = aio_bh_new(bdrv_get_aio_context(bs), blkreplay_bh_cb, req),
    };
    replay_block_event(req->bh, reqid);
}

static int coroutine_fn blkreplay_co_preadv(BlockDriverState *bs,
    uint64_t offset, uint64_t bytes, QEMUIOVector *qiov, int flags)
{
    uint64_t reqid = request_id++;
    int ret = bdrv_co_preadv(bs->file, offset, bytes, qiov, flags);
    block_request_create(reqid, bs, qemu_coroutine_self());
    qemu_coroutine_yield();

    return ret;
}

static int coroutine_fn blkreplay_co_pwritev(BlockDriverState *bs,
    uint64_t offset, uint64_t bytes, QEMUIOVector *qiov, int flags)
{
    uint64_t reqid = request_id++;
    int ret = bdrv_co_pwritev(bs->file, offset, bytes, qiov, flags);
    block_request_create(reqid, bs, qemu_coroutine_self());
    qemu_coroutine_yield();

    return ret;
}

static int coroutine_fn blkreplay_co_pwrite_zeroes(BlockDriverState *bs,
    int64_t offset, int count, BdrvRequestFlags flags)
{
    uint64_t reqid = request_id++;
    int ret = bdrv_co_pwrite_zeroes(bs->file, offset, count, flags);
    block_request_create(reqid, bs, qemu_coroutine_self());
    qemu_coroutine_yield();

    return ret;
}

static int coroutine_fn blkreplay_co_pdiscard(BlockDriverState *bs,
                                              int64_t offset, int count)
{
    uint64_t reqid = request_id++;
    int ret = bdrv_co_pdiscard(bs->file->bs, offset, count);
    block_request_create(reqid, bs, qemu_coroutine_self());
    qemu_coroutine_yield();

    return ret;
}

static int coroutine_fn blkreplay_co_flush(BlockDriverState *bs)
{
    uint64_t reqid = request_id++;
    int ret = bdrv_co_flush(bs->file->bs);
    block_request_create(reqid, bs, qemu_coroutine_self());
    qemu_coroutine_yield();

    return ret;
}

static bool blkreplay_recurse_is_first_non_filter(BlockDriverState *bs,
                                                  BlockDriverState *candidate)
{
    return bdrv_recurse_is_first_non_filter(bs->file->bs, candidate);
}

static BlockDriver bdrv_blkreplay = {
    .format_name            = "blkreplay",
    .protocol_name          = "blkreplay",
    .instance_size          = 0,

    .bdrv_file_open         = blkreplay_open,
    .bdrv_close             = blkreplay_close,
    .bdrv_getlength         = blkreplay_getlength,

    .bdrv_co_preadv         = blkreplay_co_preadv,
    .bdrv_co_pwritev        = blkreplay_co_pwritev,

    .bdrv_co_pwrite_zeroes  = blkreplay_co_pwrite_zeroes,
    .bdrv_co_pdiscard       = blkreplay_co_pdiscard,
    .bdrv_co_flush          = blkreplay_co_flush,

    .is_filter              = true,
    .bdrv_recurse_is_first_non_filter = blkreplay_recurse_is_first_non_filter,
};

static void bdrv_blkreplay_init(void)
{
    bdrv_register(&bdrv_blkreplay);
}

block_init(bdrv_blkreplay_init);
