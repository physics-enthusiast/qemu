/*
 * QEMU System Emulator block driver
 *
 * Copyright (c) 2003 Fabrice Bellard
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

#ifndef BLOCK_INT_GLOBAL_STATE_H
#define BLOCK_INT_GLOBAL_STATE_H

#include "block/blockjob.h"
#include "block/block_int-common.h"
#include "qemu/hbitmap.h"
#include "qemu/main-loop.h"

/*
 * Global state (GS) API. These functions run under the BQL.
 *
 * See include/block/block-global-state.h for more information about
 * the GS API.
 */

/**
 * stream_start:
 * @job_id: The id of the newly-created job, or %NULL to use the
 * device name of @bs.
 * @bs: Block device to operate on.
 * @base: Block device that will become the new base, or %NULL to
 * flatten the whole backing file chain onto @bs.
 * @backing_file_str: The file name that will be written to @bs as the
 * the new backing file if the job completes. Ignored if @base is %NULL.
 * @creation_flags: Flags that control the behavior of the Job lifetime.
 *                  See @BlockJobCreateFlags
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @filter_node_name: The node name that should be assigned to the filter
 *                    driver that the stream job inserts into the graph above
 *                    @bs. NULL means that a node name should be autogenerated.
 * @errp: Error object.
 *
 * Start a streaming operation on @bs.  Clusters that are unallocated
 * in @bs, but allocated in any image between @base and @bs (both
 * exclusive) will be written to @bs.  At the end of a successful
 * streaming job, the backing file of @bs will be changed to
 * @backing_file_str in the written image and to @base in the live
 * BlockDriverState.
 */
void stream_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *base, const char *backing_file_str,
                  BlockDriverState *bottom,
                  int creation_flags, int64_t speed,
                  BlockdevOnError on_error,
                  const char *filter_node_name,
                  Error **errp);

/**
 * commit_start:
 * @job_id: The id of the newly-created job, or %NULL to use the
 * device name of @bs.
 * @bs: Active block device.
 * @top: Top block device to be committed.
 * @base: Block device that will be written into, and become the new top.
 * @creation_flags: Flags that control the behavior of the Job lifetime.
 *                  See @BlockJobCreateFlags
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @backing_file_str: String to use as the backing file in @top's overlay
 * @filter_node_name: The node name that should be assigned to the filter
 * driver that the commit job inserts into the graph above @top. NULL means
 * that a node name should be autogenerated.
 * @errp: Error object.
 *
 */
void commit_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *base, BlockDriverState *top,
                  int creation_flags, int64_t speed,
                  BlockdevOnError on_error, const char *backing_file_str,
                  const char *filter_node_name, Error **errp);
/**
 * commit_active_start:
 * @job_id: The id of the newly-created job, or %NULL to use the
 * device name of @bs.
 * @bs: Active block device to be committed.
 * @base: Block device that will be written into, and become the new top.
 * @creation_flags: Flags that control the behavior of the Job lifetime.
 *                  See @BlockJobCreateFlags
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @on_error: The action to take upon error.
 * @filter_node_name: The node name that should be assigned to the filter
 * driver that the commit job inserts into the graph above @bs. NULL means that
 * a node name should be autogenerated.
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @auto_complete: Auto complete the job.
 * @errp: Error object.
 *
 */
BlockJob *commit_active_start(const char *job_id, BlockDriverState *bs,
                              BlockDriverState *base, int creation_flags,
                              int64_t speed, BlockdevOnError on_error,
                              const char *filter_node_name,
                              BlockCompletionFunc *cb, void *opaque,
                              bool auto_complete, Error **errp);
/*
 * mirror_start:
 * @job_id: The id of the newly-created job, or %NULL to use the
 * device name of @bs.
 * @bs: Block device to operate on.
 * @target: Block device to write to.
 * @replaces: Block graph node name to replace once the mirror is done. Can
 *            only be used when full mirroring is selected.
 * @creation_flags: Flags that control the behavior of the Job lifetime.
 *                  See @BlockJobCreateFlags
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @granularity: The chosen granularity for the dirty bitmap.
 * @buf_size: The amount of data that can be in flight at one time.
 * @mode: Whether to collapse all images in the chain to the target.
 * @backing_mode: How to establish the target's backing chain after completion.
 * @zero_target: Whether the target should be explicitly zero-initialized
 * @on_source_error: The action to take upon error reading from the source.
 * @on_target_error: The action to take upon error writing to the target.
 * @unmap: Whether to unmap target where source sectors only contain zeroes.
 * @filter_node_name: The node name that should be assigned to the filter
 * driver that the mirror job inserts into the graph above @bs. NULL means that
 * a node name should be autogenerated.
 * @copy_mode: When to trigger writes to the target.
 * @errp: Error object.
 *
 * Start a mirroring operation on @bs.  Clusters that are allocated
 * in @bs will be written to @target until the job is cancelled or
 * manually completed.  At the end of a successful mirroring job,
 * @bs will be switched to read from @target.
 */
void mirror_start(const char *job_id, BlockDriverState *bs,
                  BlockDriverState *target, const char *replaces,
                  int creation_flags, int64_t speed,
                  uint32_t granularity, int64_t buf_size,
                  MirrorSyncMode mode, BlockMirrorBackingMode backing_mode,
                  bool zero_target,
                  BlockdevOnError on_source_error,
                  BlockdevOnError on_target_error,
                  bool unmap, const char *filter_node_name,
                  MirrorCopyMode copy_mode, Error **errp);

/*
 * backup_job_create:
 * @job_id: The id of the newly-created job, or %NULL to use the
 * device name of @bs.
 * @bs: Block device to operate on.
 * @target: Block device to write to.
 * @speed: The maximum speed, in bytes per second, or 0 for unlimited.
 * @sync_mode: What parts of the disk image should be copied to the destination.
 * @sync_bitmap: The dirty bitmap if sync_mode is 'bitmap' or 'incremental'
 * @bitmap_mode: The bitmap synchronization policy to use.
 * @perf: Performance options. All actual fields assumed to be present,
 *        all ".has_*" fields are ignored.
 * @on_source_error: The action to take upon error reading from the source.
 * @on_target_error: The action to take upon error writing to the target.
 * @creation_flags: Flags that control the behavior of the Job lifetime.
 *                  See @BlockJobCreateFlags
 * @cb: Completion function for the job.
 * @opaque: Opaque pointer value passed to @cb.
 * @txn: Transaction that this job is part of (may be NULL).
 *
 * Create a backup operation on @bs.  Clusters in @bs are written to @target
 * until the job is cancelled or manually completed.
 */
BlockJob *backup_job_create(const char *job_id, BlockDriverState *bs,
                            BlockDriverState *target, int64_t speed,
                            MirrorSyncMode sync_mode,
                            BdrvDirtyBitmap *sync_bitmap,
                            BitmapSyncMode bitmap_mode,
                            bool compress,
                            const char *filter_node_name,
                            BackupPerf *perf,
                            BlockdevOnError on_source_error,
                            BlockdevOnError on_target_error,
                            int creation_flags,
                            BlockCompletionFunc *cb, void *opaque,
                            JobTxn *txn, Error **errp);

BdrvChild *bdrv_root_attach_child(BlockDriverState *child_bs,
                                  const char *child_name,
                                  const BdrvChildClass *child_class,
                                  BdrvChildRole child_role,
                                  uint64_t perm, uint64_t shared_perm,
                                  void *opaque, Error **errp);
void bdrv_root_unref_child(BdrvChild *child);

void bdrv_get_cumulative_perm(BlockDriverState *bs, uint64_t *perm,
                              uint64_t *shared_perm);

/**
 * Sets a BdrvChild's permissions.  Avoid if the parent is a BDS; use
 * bdrv_child_refresh_perms() instead and make the parent's
 * .bdrv_child_perm() implementation return the correct values.
 */
int bdrv_child_try_set_perm(BdrvChild *c, uint64_t perm, uint64_t shared,
                            Error **errp);

/**
 * Calls bs->drv->bdrv_child_perm() and updates the child's permission
 * masks with the result.
 * Drivers should invoke this function whenever an event occurs that
 * makes their .bdrv_child_perm() implementation return different
 * values than before, but which will not result in the block layer
 * automatically refreshing the permissions.
 */
int bdrv_child_refresh_perms(BlockDriverState *bs, BdrvChild *c, Error **errp);

bool bdrv_recurse_can_replace(BlockDriverState *bs,
                              BlockDriverState *to_replace);

/*
 * Default implementation for BlockDriver.bdrv_child_perm() that can
 * be used by block filters and image formats, as long as they use the
 * child_of_bds child class and set an appropriate BdrvChildRole.
 */
void bdrv_default_perms(BlockDriverState *bs, BdrvChild *c,
                        BdrvChildRole role, BlockReopenQueue *reopen_queue,
                        uint64_t perm, uint64_t shared,
                        uint64_t *nperm, uint64_t *nshared);

void blk_dev_change_media_cb(BlockBackend *blk, bool load, Error **errp);
bool blk_dev_has_removable_media(BlockBackend *blk);
void blk_dev_eject_request(BlockBackend *blk, bool force);
bool blk_dev_is_medium_locked(BlockBackend *blk);

void bdrv_restore_dirty_bitmap(BdrvDirtyBitmap *bitmap, HBitmap *backup);

void bdrv_set_monitor_owned(BlockDriverState *bs);

void blockdev_close_all_bdrv_states(void);

BlockDriverState *bds_tree_init(QDict *bs_opts, Error **errp);

/**
 * Simple implementation of bdrv_co_create_opts for protocol drivers
 * which only support creation via opening a file
 * (usually existing raw storage device)
 */
int coroutine_fn bdrv_co_create_opts_simple(BlockDriver *drv,
                                            const char *filename,
                                            QemuOpts *opts,
                                            Error **errp);

BdrvDirtyBitmap *block_dirty_bitmap_lookup(const char *node,
                                           const char *name,
                                           BlockDriverState **pbs,
                                           Error **errp);
BdrvDirtyBitmap *block_dirty_bitmap_merge(const char *node, const char *target,
                                          BlockDirtyBitmapOrStrList *bms,
                                          HBitmap **backup, Error **errp);
BdrvDirtyBitmap *block_dirty_bitmap_remove(const char *node, const char *name,
                                           bool release,
                                           BlockDriverState **bitmap_bs,
                                           Error **errp);


BlockDriverState *bdrv_skip_implicit_filters(BlockDriverState *bs);

/**
 * bdrv_add_aio_context_notifier:
 *
 * If a long-running job intends to be always run in the same AioContext as a
 * certain BDS, it may use this function to be notified of changes regarding the
 * association of the BDS to an AioContext.
 *
 * attached_aio_context() is called after the target BDS has been attached to a
 * new AioContext; detach_aio_context() is called before the target BDS is being
 * detached from its old AioContext.
 */
void bdrv_add_aio_context_notifier(BlockDriverState *bs,
        void (*attached_aio_context)(AioContext *new_context, void *opaque),
        void (*detach_aio_context)(void *opaque), void *opaque);

/**
 * bdrv_remove_aio_context_notifier:
 *
 * Unsubscribe of change notifications regarding the BDS's AioContext. The
 * parameters given here have to be the same as those given to
 * bdrv_add_aio_context_notifier().
 */
void bdrv_remove_aio_context_notifier(BlockDriverState *bs,
                                      void (*aio_context_attached)(AioContext *,
                                                                   void *),
                                      void (*aio_context_detached)(void *),
                                      void *opaque);

/**
 * End all quiescent sections started by bdrv_drain_all_begin(). This is
 * needed when deleting a BDS before bdrv_drain_all_end() is called.
 *
 * NOTE: this is an internal helper for bdrv_close() *only*. No one else
 * should call it.
 */
void bdrv_drain_all_end_quiesce(BlockDriverState *bs);

#endif /* BLOCK_INT_GLOBAL_STATE_H */
