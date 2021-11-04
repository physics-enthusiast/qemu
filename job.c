/*
 * Background jobs (long-running operations)
 *
 * Copyright (c) 2011 IBM Corp.
 * Copyright (c) 2012, 2018 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/job.h"
#include "qemu/id.h"
#include "qemu/main-loop.h"
#include "block/aio-wait.h"
#include "trace/trace-root.h"
#include "qapi/qapi-events-job.h"

/*
 * The job API is composed of two categories of functions.
 *
 * The first includes functions used by the monitor.  The monitor is
 * peculiar in that it accesses the block job list with job_get, and
 * therefore needs consistency across job_get and the actual operation
 * (e.g. job_user_cancel). To achieve this consistency, the caller
 * calls job_lock/job_unlock itself around the whole operation.
 * These functions are declared in job-monitor.h.
 *
 *
 * The second includes functions used by the block job drivers and sometimes
 * by the core block layer. These delegate the locking to the callee instead,
 * and are declared in job-driver.h.
 */


/*
 * job_mutex protects the jobs list, but also makes the
 * struct job fields thread-safe.
 */
static QemuMutex job_mutex;

/* Protected by job_mutex */
static QLIST_HEAD(, Job) jobs = QLIST_HEAD_INITIALIZER(jobs);

/* Job State Transition Table */
bool JobSTT[JOB_STATUS__MAX][JOB_STATUS__MAX] = {
                                    /* U, C, R, P, Y, S, W, D, X, E, N */
    /* U: */ [JOB_STATUS_UNDEFINED] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    /* C: */ [JOB_STATUS_CREATED]   = {0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
    /* R: */ [JOB_STATUS_RUNNING]   = {0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0},
    /* P: */ [JOB_STATUS_PAUSED]    = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    /* Y: */ [JOB_STATUS_READY]     = {0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0},
    /* S: */ [JOB_STATUS_STANDBY]   = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    /* W: */ [JOB_STATUS_WAITING]   = {0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    /* D: */ [JOB_STATUS_PENDING]   = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0},
    /* X: */ [JOB_STATUS_ABORTING]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0},
    /* E: */ [JOB_STATUS_CONCLUDED] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    /* N: */ [JOB_STATUS_NULL]      = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

bool JobVerbTable[JOB_VERB__MAX][JOB_STATUS__MAX] = {
                                    /* U, C, R, P, Y, S, W, D, X, E, N */
    [JOB_VERB_CANCEL]               = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0},
    [JOB_VERB_PAUSE]                = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    [JOB_VERB_RESUME]               = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    [JOB_VERB_SET_SPEED]            = {0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    [JOB_VERB_COMPLETE]             = {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    [JOB_VERB_FINALIZE]             = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    [JOB_VERB_DISMISS]              = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
};

/* Transactional group of jobs */
struct JobTxn {

    /* Is this txn being cancelled? */
    bool aborting;

    /* List of jobs */
    QLIST_HEAD(, Job) jobs;

    /* Reference count */
    int refcnt;
};

#define JOB_LOCK_GUARD() /* QEMU_LOCK_GUARD(&job_mutex) */

#define WITH_JOB_LOCK_GUARD() /* WITH_QEMU_LOCK_GUARD(&job_mutex) */

void job_lock(void)
{
    /* nop */
}

void job_unlock(void)
{
    /* nop */
}

static void _job_lock(void)
{
    qemu_mutex_lock(&job_mutex);
}

static void _job_unlock(void)
{
    qemu_mutex_unlock(&job_mutex);
}

static void __attribute__((__constructor__)) job_init(void)
{
    qemu_mutex_init(&job_mutex);
}

JobTxn *job_txn_new(void)
{
    JobTxn *txn = g_new0(JobTxn, 1);
    QLIST_INIT(&txn->jobs);
    txn->refcnt = 1;
    return txn;
}

/* Called with job_mutex held. */
static void job_txn_ref(JobTxn *txn)
{
    txn->refcnt++;
}

void job_txn_unref(JobTxn *txn)
{
    if (txn && --txn->refcnt == 0) {
        g_free(txn);
    }
}

void job_txn_add_job(JobTxn *txn, Job *job)
{
    if (!txn) {
        return;
    }

    assert(!job->txn);
    job->txn = txn;

    QLIST_INSERT_HEAD(&txn->jobs, job, txn_list);
    job_txn_ref(txn);
}

/* Called with job_mutex held. */
static void job_txn_del_job(Job *job)
{
    if (job->txn) {
        QLIST_REMOVE(job, txn_list);
        job_txn_unref(job->txn);
        job->txn = NULL;
    }
}

/* Called with job_mutex held. */
static int job_txn_apply(Job *job, int fn(Job *))
{
    Job *other_job, *next;
    JobTxn *txn = job->txn;
    int rc = 0;

    /*
     * Similar to job_completed_txn_abort, we take each job's lock before
     * applying fn, but since we assume that outer_ctx is held by the caller,
     * we need to release it here to avoid holding the lock twice - which would
     * break AIO_WAIT_WHILE from within fn.
     */
    job_ref(job);
    aio_context_release(job->aio_context);

    QLIST_FOREACH_SAFE(other_job, &txn->jobs, txn_list, next) {
        rc = fn(other_job);
        if (rc) {
            break;
        }
    }

    /*
     * Note that job->aio_context might have been changed by calling fn, so we
     * can't use a local variable to cache it.
     */
    aio_context_acquire(job->aio_context);
    job_unref(job);
    return rc;
}

bool job_is_internal(Job *job)
{
    return (job->id == NULL);
}

/* Called with job_mutex held. */
static void job_state_transition(Job *job, JobStatus s1)
{
    JobStatus s0 = job->status;
    assert(s1 >= 0 && s1 < JOB_STATUS__MAX);
    trace_job_state_transition(job, job->ret,
                               JobSTT[s0][s1] ? "allowed" : "disallowed",
                               JobStatus_str(s0), JobStatus_str(s1));
    assert(JobSTT[s0][s1]);
    job->status = s1;

    if (!job_is_internal(job) && s1 != s0) {
        qapi_event_send_job_status_change(job->id, job->status);
    }
}

int job_apply_verb(Job *job, JobVerb verb, Error **errp)
{
    JobStatus s0 = job->status;
    assert(verb >= 0 && verb < JOB_VERB__MAX);
    trace_job_apply_verb(job, JobStatus_str(s0), JobVerb_str(verb),
                         JobVerbTable[verb][s0] ? "allowed" : "prohibited");
    if (JobVerbTable[verb][s0]) {
        return 0;
    }
    error_setg(errp, "Job '%s' in state '%s' cannot accept command verb '%s'",
               job->id, JobStatus_str(s0), JobVerb_str(verb));
    return -EPERM;
}

JobType job_type(const Job *job)
{
    return job->driver->job_type;
}

const char *job_type_str(const Job *job)
{
    return JobType_str(job_type(job));
}

JobStatus job_get_status(Job *job)
{
    JOB_LOCK_GUARD();
    return job->status;
}

int job_get_pause_count(Job *job)
{
    JOB_LOCK_GUARD();
    return job->pause_count;
}

bool job_get_paused(Job *job)
{
    JOB_LOCK_GUARD();
    return job->paused;
}

bool job_get_busy(Job *job)
{
    JOB_LOCK_GUARD();
    return job->busy;
}

bool job_has_failed(Job *job)
{
    JOB_LOCK_GUARD();
    return job->ret < 0;
}

/* Called with job_mutex held. */
static bool job_is_cancelled_locked(Job *job)
{
    /* force_cancel may be true only if cancelled is true, too */
    assert(job->cancelled || !job->force_cancel);
    return job->force_cancel;
}

/* Called with job_mutex *not* held. */
bool job_is_cancelled(Job *job)
{
    JOB_LOCK_GUARD();
    return job_is_cancelled_locked(job);
}

bool job_not_paused_nor_cancelled(Job *job)
{
    JOB_LOCK_GUARD();
    return !job->paused && !job_is_cancelled_locked(job);
}

/* Called with job_mutex held. */
static bool job_cancel_requested_locked(Job *job)
{
    return job->cancelled;
}

/* Called with job_mutex *not* held. */
bool job_cancel_requested(Job *job)
{
    JOB_LOCK_GUARD();
    return job_cancel_requested_locked(job);
}

/* Called with job_mutex held. */
bool job_is_ready_locked(Job *job)
{
    switch (job->status) {
    case JOB_STATUS_UNDEFINED:
    case JOB_STATUS_CREATED:
    case JOB_STATUS_RUNNING:
    case JOB_STATUS_PAUSED:
    case JOB_STATUS_WAITING:
    case JOB_STATUS_PENDING:
    case JOB_STATUS_ABORTING:
    case JOB_STATUS_CONCLUDED:
    case JOB_STATUS_NULL:
        return false;
    case JOB_STATUS_READY:
    case JOB_STATUS_STANDBY:
        return true;
    default:
        g_assert_not_reached();
    }
    return false;
}

/* Called with job_mutex lock *not* held */
bool job_is_ready(Job *job)
{
    JOB_LOCK_GUARD();
    return job_is_ready_locked(job);
}

bool job_is_completed(Job *job)
{
    switch (job->status) {
    case JOB_STATUS_UNDEFINED:
    case JOB_STATUS_CREATED:
    case JOB_STATUS_RUNNING:
    case JOB_STATUS_PAUSED:
    case JOB_STATUS_READY:
    case JOB_STATUS_STANDBY:
        return false;
    case JOB_STATUS_WAITING:
    case JOB_STATUS_PENDING:
    case JOB_STATUS_ABORTING:
    case JOB_STATUS_CONCLUDED:
    case JOB_STATUS_NULL:
        return true;
    default:
        g_assert_not_reached();
    }
    return false;
}

/* Called with job_mutex lock *not* held */
static bool job_is_completed_unlocked(Job *job)
{
    JOB_LOCK_GUARD();
    return job_is_completed(job);
}

static bool job_started(Job *job)
{
    return job->co;
}

/* Called with job_mutex held. */
static bool job_should_pause(Job *job)
{
    return job->pause_count > 0;
}

Job *job_next(Job *job)
{
    if (!job) {
        return QLIST_FIRST(&jobs);
    }
    return QLIST_NEXT(job, job_list);
}

Job *job_get(const char *id)
{
    Job *job;

    QLIST_FOREACH(job, &jobs, job_list) {
        if (job->id && !strcmp(id, job->id)) {
            return job;
        }
    }

    return NULL;
}

/* Called with job_mutex *not* held. */
static void job_sleep_timer_cb(void *opaque)
{
    Job *job = opaque;

    job_enter(job);
}

/* Called with job_mutex *not* held. */
void *job_create(const char *job_id, const JobDriver *driver, JobTxn *txn,
                 AioContext *ctx, int flags, BlockCompletionFunc *cb,
                 void *opaque, Error **errp)
{
    Job *job;

    JOB_LOCK_GUARD();

    if (job_id) {
        if (flags & JOB_INTERNAL) {
            error_setg(errp, "Cannot specify job ID for internal job");
            return NULL;
        }
        if (!id_wellformed(job_id)) {
            error_setg(errp, "Invalid job ID '%s'", job_id);
            return NULL;
        }
        if (job_get(job_id)) {
            error_setg(errp, "Job ID '%s' already in use", job_id);
            return NULL;
        }
    } else if (!(flags & JOB_INTERNAL)) {
        error_setg(errp, "An explicit job ID is required");
        return NULL;
    }

    job = g_malloc0(driver->instance_size);
    job->driver        = driver;
    job->id            = g_strdup(job_id);
    job->refcnt        = 1;
    job->aio_context   = ctx;
    job->busy          = false;
    job->paused        = true;
    job->pause_count   = 1;
    job->auto_finalize = !(flags & JOB_MANUAL_FINALIZE);
    job->auto_dismiss  = !(flags & JOB_MANUAL_DISMISS);
    job->cb            = cb;
    job->opaque        = opaque;

    progress_init(&job->progress);

    notifier_list_init(&job->on_finalize_cancelled);
    notifier_list_init(&job->on_finalize_completed);
    notifier_list_init(&job->on_pending);
    notifier_list_init(&job->on_ready);

    job_state_transition(job, JOB_STATUS_CREATED);
    aio_timer_init(qemu_get_aio_context(), &job->sleep_timer,
                   QEMU_CLOCK_REALTIME, SCALE_NS,
                   job_sleep_timer_cb, job);

    QLIST_INSERT_HEAD(&jobs, job, job_list);

    /* Single jobs are modeled as single-job transactions for sake of
     * consolidating the job management logic */
    if (!txn) {
        txn = job_txn_new();
        job_txn_add_job(txn, job);
        job_txn_unref(txn);
    } else {
        job_txn_add_job(txn, job);
    }

    return job;
}

void job_ref(Job *job)
{
    ++job->refcnt;
}

void job_unref(Job *job)
{
    assert(qemu_in_main_thread());

    if (--job->refcnt == 0) {
        assert(job->status == JOB_STATUS_NULL);
        assert(!timer_pending(&job->sleep_timer));
        assert(!job->txn);

        if (job->driver->free) {
            job_unlock();
            job->driver->free(job);
            job_lock();
        }

        QLIST_REMOVE(job, job_list);

        progress_destroy(&job->progress);
        error_free(job->err);
        g_free(job->id);
        g_free(job);
    }
}

/* Progress API is thread safe */
void job_progress_update(Job *job, uint64_t done)
{
    progress_work_done(&job->progress, done);
}

/* Progress API is thread safe */
void job_progress_set_remaining(Job *job, uint64_t remaining)
{
    progress_set_remaining(&job->progress, remaining);
}

/* Progress API is thread safe */
void job_progress_increase_remaining(Job *job, uint64_t delta)
{
    progress_increase_remaining(&job->progress, delta);
}

/**
 * To be called when a cancelled job is finalised.
 * Called with job_mutex held.
 */
static void job_event_cancelled(Job *job)
{
    notifier_list_notify(&job->on_finalize_cancelled, job);
}

/**
 * To be called when a successfully completed job is finalised.
 * Called with job_mutex held.
 */
static void job_event_completed(Job *job)
{
    notifier_list_notify(&job->on_finalize_completed, job);
}

/* Called with job_mutex held. */
static void job_event_pending(Job *job)
{
    notifier_list_notify(&job->on_pending, job);
}

/* Called with job_mutex held. */
static void job_event_ready(Job *job)
{
    notifier_list_notify(&job->on_ready, job);
}

/* Called with job_mutex held. */
static void job_event_idle(Job *job)
{
    notifier_list_notify(&job->on_idle, job);
}

void job_enter_cond(Job *job, bool(*fn)(Job *job))
{
    if (!job_started(job)) {
        return;
    }
    if (job->deferred_to_main_loop) {
        return;
    }

    _job_lock();
    if (job->busy) {
        _job_unlock();
        return;
    }

    if (fn && !fn(job)) {
        _job_unlock();
        return;
    }

    assert(!job->deferred_to_main_loop);
    timer_del(&job->sleep_timer);
    job->busy = true;
    _job_unlock();
    job_unlock();
    aio_co_enter(job->aio_context, job->co);
    job_lock();
}

/* Called with job_mutex *not* held. */
void job_enter(Job *job)
{
    JOB_LOCK_GUARD();
    job_enter_cond(job, NULL);
}

/* Yield, and schedule a timer to reenter the coroutine after @ns nanoseconds.
 * Reentering the job coroutine with job_enter() before the timer has expired
 * is allowed and cancels the timer.
 *
 * If @ns is (uint64_t) -1, no timer is scheduled and job_enter() must be
 * called explicitly.
 *
 * Called with job_mutex held, but releases it temporarly.
 */
static void coroutine_fn job_do_yield(Job *job, uint64_t ns)
{
    _job_lock();
    if (ns != -1) {
        timer_mod(&job->sleep_timer, ns);
    }
    job->busy = false;
    job_event_idle(job);
    _job_unlock();
    job_unlock();
    qemu_coroutine_yield();
    job_lock();

    /* Set by job_enter_cond() before re-entering the coroutine.  */
    assert(job->busy);
}

/*
 * Called with job_mutex *not* held (we don't want the coroutine
 * to yield with the lock held!).
 */
void coroutine_fn job_pause_point(Job *job)
{
    assert(job && job_started(job));

    job_lock();
    if (!job_should_pause(job)) {
        job_unlock();
        return;
    }
    if (job_is_cancelled_locked(job)) {
        job_unlock();
        return;
    }

    if (job->driver->pause) {
        job_unlock();
        job->driver->pause(job);
        job_lock();
    }

    if (job_should_pause(job) && !job_is_cancelled_locked(job)) {
        JobStatus status = job->status;
        job_state_transition(job, status == JOB_STATUS_READY
                                  ? JOB_STATUS_STANDBY
                                  : JOB_STATUS_PAUSED);
        job->paused = true;
        job_do_yield(job, -1);
        job->paused = false;
        job_state_transition(job, status);
    }
    job_unlock();

    if (job->driver->resume) {
        job->driver->resume(job);
    }
}

/*
 * Called with job_mutex *not* held (we don't want the coroutine
 * to yield with the lock held!).
 */
void job_yield(Job *job)
{
    WITH_JOB_LOCK_GUARD() {
        assert(job->busy);

        /* Check cancellation *before* setting busy = false, too!  */
        if (job_is_cancelled_locked(job)) {
            return;
        }

        if (!job_should_pause(job)) {
            job_do_yield(job, -1);
        }
    }

    job_pause_point(job);
}

/*
 * Called with job_mutex *not* held (we don't want the coroutine
 * to yield with the lock held!).
 */
void coroutine_fn job_sleep_ns(Job *job, int64_t ns)
{
    WITH_JOB_LOCK_GUARD() {
        assert(job->busy);

        /* Check cancellation *before* setting busy = false, too!  */
        if (job_is_cancelled_locked(job)) {
            return;
        }

        if (!job_should_pause(job)) {
            job_do_yield(job, qemu_clock_get_ns(QEMU_CLOCK_REALTIME) + ns);
        }
    }

    job_pause_point(job);
}

/* Assumes the job_mutex is held */
static bool job_timer_not_pending(Job *job)
{
    return !timer_pending(&job->sleep_timer);
}

void job_pause(Job *job)
{
    job->pause_count++;
    if (!job->paused) {
        job_enter_cond(job, NULL);
    }
}

void job_enter_not_paused(Job *job)
{
    JOB_LOCK_GUARD();
    if (!job->paused) {
        job_enter_cond(job, NULL);
    }
}

void job_resume(Job *job)
{
    assert(job->pause_count > 0);
    job->pause_count--;
    if (job->pause_count) {
        return;
    }

    /* kick only if no timer is pending */
    job_enter_cond(job, job_timer_not_pending);
}

void job_user_pause(Job *job, Error **errp)
{
    if (job_apply_verb(job, JOB_VERB_PAUSE, errp)) {
        return;
    }
    if (job->user_paused) {
        error_setg(errp, "Job is already paused");
        return;
    }
    job->user_paused = true;
    job_pause(job);
}

bool job_user_paused(Job *job)
{
    return job->user_paused;
}

void job_user_resume(Job *job, Error **errp)
{
    assert(job);
    assert(qemu_in_main_thread());
    if (!job->user_paused || job->pause_count <= 0) {
        error_setg(errp, "Can't resume a job that was not paused");
        return;
    }
    if (job_apply_verb(job, JOB_VERB_RESUME, errp)) {
        return;
    }
    if (job->driver->user_resume) {
        job_unlock();
        job->driver->user_resume(job);
        job_lock();
    }
    job->user_paused = false;
    job_resume(job);
}

/* Called with job_mutex held. */
static void job_do_dismiss(Job *job)
{
    assert(job);
    job->busy = false;
    job->paused = false;
    job->deferred_to_main_loop = true;

    job_txn_del_job(job);

    job_state_transition(job, JOB_STATUS_NULL);
    job_unref(job);
}

void job_dismiss(Job **jobptr, Error **errp)
{
    Job *job = *jobptr;
    /* similarly to _complete, this is QMP-interface only. */
    assert(job->id);
    if (job_apply_verb(job, JOB_VERB_DISMISS, errp)) {
        return;
    }

    job_do_dismiss(job);
    *jobptr = NULL;
}

void job_early_fail_locked(Job *job)
{
    assert(job->status == JOB_STATUS_CREATED);
    job_do_dismiss(job);
}

void job_early_fail(Job *job)
{
    JOB_LOCK_GUARD();
    job_early_fail_locked(job);
}

/* Called with job_mutex held. */
static void job_conclude(Job *job)
{
    job_state_transition(job, JOB_STATUS_CONCLUDED);
    if (job->auto_dismiss || !job_started(job)) {
        job_do_dismiss(job);
    }
}

/* Called with job_mutex held. */
static void job_update_rc(Job *job)
{
    if (!job->ret && job_is_cancelled_locked(job)) {
        job->ret = -ECANCELED;
    }
    if (job->ret) {
        if (!job->err) {
            error_setg(&job->err, "%s", strerror(-job->ret));
        }
        job_state_transition(job, JOB_STATUS_ABORTING);
    }
}

/* Called with job_mutex held, but releases it temporarly */
static void job_commit(Job *job)
{
    assert(!job->ret);
    assert(qemu_in_main_thread());
    if (job->driver->commit) {
        job_unlock();
        job->driver->commit(job);
        job_lock();
    }
}

/* Called with job_mutex held, but releases it temporarly */
static void job_abort(Job *job)
{
    assert(job->ret);
    assert(qemu_in_main_thread());
    if (job->driver->abort) {
        job_unlock();
        job->driver->abort(job);
        job_lock();
    }
}

/* Called with job_mutex held, but releases it temporarly */
static void job_clean(Job *job)
{
    assert(qemu_in_main_thread());
    if (job->driver->clean) {
        job_unlock();
        job->driver->clean(job);
        job_lock();
    }
}

/* Called with job_mutex held, but releases it temporarly. */
static int job_finalize_single(Job *job)
{
    int job_ret;
    AioContext *ctx = job->aio_context;

    assert(job_is_completed(job));

    /* Ensure abort is called for late-transactional failures */
    job_update_rc(job);

    aio_context_acquire(ctx);

    if (!job->ret) {
        job_commit(job);
    } else {
        job_abort(job);
    }
    job_clean(job);

    aio_context_release(ctx);

    if (job->cb) {
        job_ret = job->ret;
        job_unlock();
        job->cb(job->opaque, job_ret);
        job_lock();
    }

    /* Emit events only if we actually started */
    if (job_started(job)) {
        if (job_is_cancelled_locked(job)) {
            job_event_cancelled(job);
        } else {
            job_event_completed(job);
        }
    }

    job_txn_del_job(job);
    job_conclude(job);
    return 0;
}

/* Called with job_mutex held, but releases it temporarly. */
static void job_cancel_async(Job *job, bool force)
{
    assert(qemu_in_main_thread());
    if (job->driver->cancel) {
        job_unlock();
        force = job->driver->cancel(job, force);
        job_lock();
    } else {
        /* No .cancel() means the job will behave as if force-cancelled */
        force = true;
    }

    if (job->user_paused) {
        /* Do not call job_enter here, the caller will handle it.  */
        if (job->driver->user_resume) {
            job_unlock();
            job->driver->user_resume(job);
            job_lock();
        }
        job->user_paused = false;
        assert(job->pause_count > 0);
        job->pause_count--;
    }

    /*
     * Ignore soft cancel requests after the job is already done
     * (We will still invoke job->driver->cancel() above, but if the
     * job driver supports soft cancelling and the job is done, that
     * should be a no-op, too.  We still call it so it can override
     * @force.)
     */
    if (force || !job->deferred_to_main_loop) {
        job->cancelled = true;
        /* To prevent 'force == false' overriding a previous 'force == true' */
        job->force_cancel |= force;
    }
}

/* Called with job_mutex held. */
static void job_completed_txn_abort(Job *job)
{
    AioContext *ctx;
    JobTxn *txn = job->txn;
    Job *other_job;

    if (txn->aborting) {
        /*
         * We are cancelled by another job, which will handle everything.
         */
        return;
    }
    txn->aborting = true;
    job_txn_ref(txn);

    /*
     * We can only hold the single job's AioContext lock while calling
     * job_finalize_single() because the finalization callbacks can involve
     * calls of AIO_WAIT_WHILE(), which could deadlock otherwise.
     * Note that the job's AioContext may change when it is finalized.
     */
    job_ref(job);
    aio_context_release(job->aio_context);

    /* Other jobs are effectively cancelled by us, set the status for
     * them; this job, however, may or may not be cancelled, depending
     * on the caller, so leave it. */
    QLIST_FOREACH(other_job, &txn->jobs, txn_list) {
        if (other_job != job) {
            ctx = other_job->aio_context;
            aio_context_acquire(ctx);
            /*
             * This is a transaction: If one job failed, no result will matter.
             * Therefore, pass force=true to terminate all other jobs as quickly
             * as possible.
             */
            job_cancel_async(other_job, true);
            aio_context_release(ctx);
        }
    }
    while (!QLIST_EMPTY(&txn->jobs)) {
        other_job = QLIST_FIRST(&txn->jobs);
        /*
         * The job's AioContext may change, so store it in @ctx so we
         * release the same context that we have acquired before.
         */
        ctx = other_job->aio_context;
        aio_context_acquire(ctx);
        if (!job_is_completed(other_job)) {
            assert(job_cancel_requested_locked(other_job));
            job_finish_sync(other_job, NULL, NULL);
        }
        job_finalize_single(other_job);
        aio_context_release(ctx);
    }

    /*
     * Use job_ref()/job_unref() so we can read the AioContext here
     * even if the job went away during job_finalize_single().
     */
    aio_context_acquire(job->aio_context);
    job_unref(job);

    job_txn_unref(txn);
}

/* Called with job_mutex held, but releases it temporarly. */
static int job_prepare(Job *job)
{
    int ret;
    AioContext *ctx = job->aio_context;
    assert(qemu_in_main_thread());

    if (job->ret == 0 && job->driver->prepare) {
        job_unlock();
        aio_context_acquire(ctx);
        ret = job->driver->prepare(job);
        aio_context_release(ctx);
        job_lock();
        job->ret = ret;
        job_update_rc(job);
    }

    return job->ret;
}

/* Called with job_mutex held. */
static int job_needs_finalize(Job *job)
{
    return !job->auto_finalize;
}

/* Called with job_mutex held. */
static void job_do_finalize(Job *job)
{
    int rc;
    assert(job && job->txn);

    /* prepare the transaction to complete */
    rc = job_txn_apply(job, job_prepare);
    if (rc) {
        job_completed_txn_abort(job);
    } else {
        job_txn_apply(job, job_finalize_single);
    }
}

void job_finalize(Job *job, Error **errp)
{
    assert(job && job->id);
    if (job_apply_verb(job, JOB_VERB_FINALIZE, errp)) {
        return;
    }
    job_do_finalize(job);
}

/* Called with job_mutex held. */
static int job_transition_to_pending(Job *job)
{
    job_state_transition(job, JOB_STATUS_PENDING);
    if (!job->auto_finalize) {
        job_event_pending(job);
    }
    return 0;
}

/* Called with job_mutex *not* held. */
void job_transition_to_ready(Job *job)
{
    JOB_LOCK_GUARD();
    job_state_transition(job, JOB_STATUS_READY);
    job_event_ready(job);
}

/* Called with job_mutex held. */
static void job_completed_txn_success(Job *job)
{
    JobTxn *txn = job->txn;
    Job *other_job;

    job_state_transition(job, JOB_STATUS_WAITING);

    /*
     * Successful completion, see if there are other running jobs in this
     * txn.
     */
    QLIST_FOREACH(other_job, &txn->jobs, txn_list) {
        if (!job_is_completed(other_job)) {
            return;
        }
        assert(other_job->ret == 0);
    }

    job_txn_apply(job, job_transition_to_pending);

    /* If no jobs need manual finalization, automatically do so */
    if (job_txn_apply(job, job_needs_finalize) == 0) {
        job_do_finalize(job);
    }
}

/* Called with job_mutex held. */
static void job_completed(Job *job)
{
    assert(job && job->txn && !job_is_completed(job));

    job_update_rc(job);
    trace_job_completed(job, job->ret);
    if (job->ret) {
        job_completed_txn_abort(job);
    } else {
        job_completed_txn_success(job);
    }
}

/**
 * Useful only as a type shim for aio_bh_schedule_oneshot.
 *  Called with job_mutex *not* held.
 */
static void job_exit(void *opaque)
{
    Job *job = (Job *)opaque;
    AioContext *ctx;

    JOB_LOCK_GUARD();
    job_ref(job);
    aio_context_acquire(job->aio_context);

    /* This is a lie, we're not quiescent, but still doing the completion
     * callbacks. However, completion callbacks tend to involve operations that
     * drain block nodes, and if .drained_poll still returned true, we would
     * deadlock. */
    job->busy = false;
    job_event_idle(job);

    job_completed(job);

    /*
     * Note that calling job_completed can move the job to a different
     * aio_context, so we cannot cache from above. job_txn_apply takes care of
     * acquiring the new lock, and we ref/unref to avoid job_completed freeing
     * the job underneath us.
     */
    ctx = job->aio_context;
    job_unref(job);
    aio_context_release(ctx);
}

/**
 * All jobs must allow a pause point before entering their job proper. This
 * ensures that jobs can be paused prior to being started, then resumed later.
 *
 * Called with job_mutex *not* held.
 */
static void coroutine_fn job_co_entry(void *opaque)
{
    Job *job = opaque;
    int ret;
    assert(job && job->driver && job->driver->run);
    job_pause_point(job);
    ret = job->driver->run(job, &job->err);
    WITH_JOB_LOCK_GUARD() {
        job->ret = ret;
        job->deferred_to_main_loop = true;
        job->busy = true;
    }
    aio_bh_schedule_oneshot(qemu_get_aio_context(), job_exit, job);
}

/* Called with job_mutex *not* held. */
void job_start(Job *job)
{
    WITH_JOB_LOCK_GUARD() {
        assert(job && !job_started(job) && job->paused &&
            job->driver && job->driver->run);
        job->co = qemu_coroutine_create(job_co_entry, job);
        job->pause_count--;
        job->busy = true;
        job->paused = false;
        job_state_transition(job, JOB_STATUS_RUNNING);
    }
    aio_co_enter(job->aio_context, job->co);
}

void job_cancel(Job *job, bool force)
{
    if (job->status == JOB_STATUS_CONCLUDED) {
        job_do_dismiss(job);
        return;
    }
    job_cancel_async(job, force);
    if (!job_started(job)) {
        job_completed(job);
    } else if (job->deferred_to_main_loop) {
        /*
         * job_cancel_async() ignores soft-cancel requests for jobs
         * that are already done (i.e. deferred to the main loop).  We
         * have to check again whether the job is really cancelled.
         * (job_cancel_requested() and job_is_cancelled() are equivalent
         * here, because job_cancel_async() will make soft-cancel
         * requests no-ops when deferred_to_main_loop is true.  We
         * choose to call job_is_cancelled() to show that we invoke
         * job_completed_txn_abort() only for force-cancelled jobs.)
         */
        if (job_is_cancelled_locked(job)) {
            job_completed_txn_abort(job);
        }
    } else {
        job_enter_cond(job, NULL);
    }
}

void job_user_cancel(Job *job, bool force, Error **errp)
{
    if (job_apply_verb(job, JOB_VERB_CANCEL, errp)) {
        return;
    }
    job_cancel(job, force);
}

/*
 * A wrapper around job_cancel() taking an Error ** parameter so it may be
 * used with job_finish_sync() without the need for (rather nasty) function
 * pointer casts there.
 *
 * Called with job_mutex held.
 */
static void job_cancel_err(Job *job, Error **errp)
{
    job_cancel(job, false);
}

/**
 * Same as job_cancel_err(), but force-cancel.
 *
 * Called with job_mutex held.
 */
static void job_force_cancel_err(Job *job, Error **errp)
{
    job_cancel(job, true);
}

int job_cancel_sync(Job *job, bool force)
{
    if (force) {
        return job_finish_sync(job, &job_force_cancel_err, NULL);
    } else {
        return job_finish_sync(job, &job_cancel_err, NULL);
    }
}

/*
 * Called with job_lock *not* held, unlike most other APIs consumed
 * by the monitor! This is primarly to avoid adding lock-unlock
 * patterns in the caller.
 */
void job_cancel_sync_all(void)
{
    Job *job;
    AioContext *aio_context;

    JOB_LOCK_GUARD();
    while ((job = job_next(NULL))) {
        aio_context = job->aio_context;
        aio_context_acquire(aio_context);
        job_cancel_sync(job, true);
        aio_context_release(aio_context);
    }
}

int job_complete_sync(Job *job, Error **errp)
{
    return job_finish_sync(job, job_complete, errp);
}

void job_complete(Job *job, Error **errp)
{
    /* Should not be reachable via external interface for internal jobs */
    assert(job->id);
    assert(qemu_in_main_thread());
    if (job_apply_verb(job, JOB_VERB_COMPLETE, errp)) {
        return;
    }
    if (job_cancel_requested_locked(job) || !job->driver->complete) {
        error_setg(errp, "The active block job '%s' cannot be completed",
                   job->id);
        return;
    }

    job_unlock();
    job->driver->complete(job, errp);
    job_lock();
}

int job_finish_sync(Job *job, void (*finish)(Job *, Error **errp), Error **errp)
{
    Error *local_err = NULL;
    int ret;

    job_ref(job);

    if (finish) {
        finish(job, &local_err);
    }
    if (local_err) {
        error_propagate(errp, local_err);
        job_unref(job);
        return -EBUSY;
    }

    job_unlock();
    AIO_WAIT_WHILE(job->aio_context,
                   (job_enter(job), !job_is_completed_unlocked(job)));
    job_lock();

    ret = (job_is_cancelled_locked(job) && job->ret == 0) ?
           -ECANCELED : job->ret;
    job_unref(job);
    return ret;
}
