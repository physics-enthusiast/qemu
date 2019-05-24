/*
 * Linux io_uring support.
 *
 * Copyright (C) 2009 IBM, Corp.
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include <liburing.h>

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "block/aio.h"
#include "qemu/queue.h"
#include "block/block.h"
#include "block/raw-aio.h"
#include "qemu/event_notifier.h"
#include "qemu/coroutine.h"
#include "qapi/error.h"

#define MAX_EVENTS 128

typedef struct LuringAIOCB {
    BlockAIOCB common;
    Coroutine *co;
    struct io_uring_sqe sqeq;
    int ret;
    QSIMPLEQ_ENTRY(LuringAIOCB) next;
} LuringAIOCB;

typedef struct LuringQueue {
    int plugged;
    unsigned int in_queue;
    unsigned int in_flight;
    bool blocked;
    QSIMPLEQ_HEAD(, LuringAIOCB) pending;
} LuringQueue;

typedef struct LuringState {
    AioContext *aio_context;

    struct io_uring ring;

    /* io queue for submit at batch.  Protected by AioContext lock. */
    LuringQueue io_q;

    /* I/O completion processing.  Only runs in I/O thread.  */
    QEMUBH *completion_bh;
    int event_idx;
    int event_max;
} LuringState;

static void ioq_submit(LuringState *s);

static inline int io_cqe_ret(struct io_uring_cqe *cqe)
{
    return cqe->res;
}

/**
 * qemu_luring_process_completions:
 * @s: AIO state
 *
 * Fetches completed I/O requests, consumes cqes and invokes their callbacks.
 *
 */
static void qemu_luring_process_completions(LuringState *s)
{
    struct io_uring_cqe *cqes;

    qemu_bh_schedule(s->completion_bh);

    while ((s->event_max = s->io_q.in_flight) &&
           !io_uring_peek_cqe(&s->ring, &cqes)) {
        for (s->event_idx = 0; s->event_idx < s->event_max;) {
            io_uring_cqe_seen(&s->ring, cqes);

            LuringAIOCB *luringcb;
            luringcb = g_malloc0(sizeof(luringcb));
            luringcb->ret = io_cqe_ret(cqes);
            /* Change counters one-by-one because we can be nested. */
            s->io_q.in_flight--;
            s->event_idx++;
        }
    }

    qemu_bh_cancel(s->completion_bh);

    /*
     *If we are nested we have to notify the level above that we are done
     * by setting event_max to zero, upper level will then jump out of it's
     * own `for` loop.  If we are the last all counters dropped to zero.
     */
    s->event_max = 0;
    s->event_idx = 0;
}

static void qemu_luring_process_completions_and_submit(LuringState *s)
{
    aio_context_acquire(s->aio_context);
    qemu_luring_process_completions(s);

    if (!s->io_q.plugged && !QSIMPLEQ_EMPTY(&s->io_q.pending)) {
        ioq_submit(s);
    }
    aio_context_release(s->aio_context);
}

static void qemu_luring_completion_bh(void *opaque)
{
    LuringState *s = opaque;
    qemu_luring_process_completions_and_submit(s);
}

static void qemu_luring_completion_cb(void *opaque)
{
    LuringState *s = opaque;
    qemu_luring_process_completions_and_submit(s);
}

static const AIOCBInfo luring_aiocb_info = {
    .aiocb_size         = sizeof(LuringAIOCB),
};


static void ioq_init(LuringQueue *io_q)
{
    QSIMPLEQ_INIT(&io_q->pending);
    io_q->plugged = 0;
    io_q->in_queue = 0;
    io_q->in_flight = 0;
    io_q->blocked = false;
}

static void ioq_submit(LuringState *s)
{
    int ret, len;
    LuringAIOCB *luringcb;
    QSIMPLEQ_HEAD(, LuringAIOCB) completed;

    while (s->io_q.in_flight >= MAX_EVENTS && s->io_q.in_queue) {
        len = 0;
        QSIMPLEQ_FOREACH(luringcb, &s->io_q.pending, next) {
            if (s->io_q.in_flight + len++ >= MAX_EVENTS) {
                break;
            }
            struct io_uring_sqe *sqes = io_uring_get_sqe(&s->ring);
            if (sqes)  { /* Prep sqe for subission */
                memset(sqes, 0, sizeof(*sqes));
                *sqes = luringcb->sqeq;
                QSIMPLEQ_REMOVE_HEAD(&s->io_q.pending, next);
            } else {
                break;
            }
        }

        ret =  io_uring_submit(&s->ring);
        if (ret == -EAGAIN) {
            break;
        }

        s->io_q.in_flight += ret;
        s->io_q.in_queue  -= ret;
        QSIMPLEQ_SPLIT_AFTER(&s->io_q.pending, luringcb, next, &completed);
    }
    s->io_q.blocked = (s->io_q.in_queue > 0);

    if (s->io_q.in_flight) {
        /*
         * We can try to complete something just right away if there are
         * still requests in-flight.
         */
        qemu_luring_process_completions(s);
    }
}

void luring_io_plug(BlockDriverState *bs, LuringState *s)
{
    s->io_q.plugged++;
}

void luring_io_unplug(BlockDriverState *bs, LuringState *s)
{
    assert(s->io_q.plugged);
    if (--s->io_q.plugged == 0 &&
        !s->io_q.blocked && !QSIMPLEQ_EMPTY(&s->io_q.pending)) {
        ioq_submit(s);
    }
}

static int luring_do_submit(int fd, LuringAIOCB *luringcb, LuringState *s,
                            uint64_t offset, QEMUIOVector *qiov, int type)
{
    struct io_uring_sqe *sqes = io_uring_get_sqe(&s->ring);
    if (!sqes) {
        sqes = &luringcb->sqeq;
        QSIMPLEQ_INSERT_TAIL(&s->io_q.pending, luringcb, next);
    }

    switch (type) {
    case QEMU_AIO_WRITE:
        io_uring_prep_writev(sqes, fd, qiov->iov, qiov->niov, offset);
        break;
    case QEMU_AIO_READ:
        io_uring_prep_readv(sqes, fd, qiov->iov, qiov->niov, offset);
        break;
    case QEMU_AIO_FLUSH:
        io_uring_prep_fsync(sqes, fd, IORING_FSYNC_DATASYNC);
        break;
    default:
        fprintf(stderr, "%s: invalid AIO request type, aborting 0x%x.\n",
                        __func__, type);
        abort();
    }

    s->io_q.in_queue++;
    if (!s->io_q.blocked &&
        (!s->io_q.plugged ||
         s->io_q.in_flight + s->io_q.in_queue >= MAX_EVENTS)) {
        ioq_submit(s);
    }

    return 0;
}

int coroutine_fn luring_co_submit(BlockDriverState *bs, LuringState *s, int fd,
                                uint64_t offset, QEMUIOVector *qiov, int type)
{
    int ret;
    LuringAIOCB luringcb = {
        .co         = qemu_coroutine_self(),
        .ret        = -EINPROGRESS,
    };

    ret = luring_do_submit(fd, &luringcb, s, offset, qiov, type);
    if (ret < 0) {
        return ret;
    }

    if (luringcb.ret == -EINPROGRESS) {
        qemu_coroutine_yield();
    }
    return luringcb.ret;
}

BlockAIOCB *luring_submit(BlockDriverState *bs, LuringState *s, int fd,
        int64_t sector_num, QEMUIOVector *qiov, BlockCompletionFunc *cb,
        void *opaque, int type)
{
    LuringAIOCB *luringcb;
    off_t offset = sector_num * BDRV_SECTOR_SIZE;
    int ret;

    luringcb = qemu_aio_get(&luring_aiocb_info, bs, cb, opaque);
    luringcb->ret = -EINPROGRESS;
    ret = luring_do_submit(fd, luringcb, s, offset, qiov, type);
    if (ret < 0) {
        qemu_aio_unref(luringcb);
        return NULL;
    }

    return &luringcb->common;
}

void luring_detach_aio_context(LuringState *s, AioContext *old_context)
{
    aio_set_fd_handler(old_context, s->ring.ring_fd, false, NULL, NULL, NULL,
                       &s);
    qemu_bh_delete(s->completion_bh);
    s->aio_context = NULL;
}

void luring_attach_aio_context(LuringState *s, AioContext *new_context)
{
    s->aio_context = new_context;
    s->completion_bh = aio_bh_new(new_context, qemu_luring_completion_bh, s);
    aio_set_fd_handler(s->aio_context, s->ring.ring_fd, false,
                       (IOHandler *)qemu_luring_completion_cb, NULL, NULL, &s);
}

LuringState *luring_init(Error **errp)
{
    int rc;
    LuringState *s;
    s = g_malloc0(sizeof(*s));
    struct io_uring *ring = &s->ring;
    rc =  io_uring_queue_init(MAX_EVENTS, ring, 0);
    if (rc == -1) {
        error_setg_errno(errp, errno, "failed to init linux io_uring ring");
        goto out_close_efd;
    }

    ioq_init(&s->io_q);
    return s;

out_close_efd:
    g_free(s);
    return NULL;
}

void luring_cleanup(LuringState *s)
{
    io_uring_queue_exit(&s->ring);
    g_free(s);
}
