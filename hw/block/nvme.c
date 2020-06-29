/*
 * QEMU NVM Express Controller
 *
 * Copyright (c) 2012, Intel Corporation
 *
 * Written by Keith Busch <keith.busch@intel.com>
 *
 * This code is licensed under the GNU GPL v2 or later.
 */

/**
 * Reference Specs: http://www.nvmexpress.org, 1.2, 1.1, 1.0e
 *
 *  http://www.nvmexpress.org/resources/
 */

/**
 * Usage: add options:
 *      -drive file=<file>,if=none,id=<drive_id>
 *      -device nvme,drive=<drive_id>,serial=<serial>,id=<id[optional]>, \
 *              [cmb_size_mb=<cmb_size_mb>,] \
 *              [pmrdev=<mem_backend_file_id>,] \
 *              [max_ioqpairs=<N>,] \
 *              [mdts=<N>]
 *
 * Note cmb_size_mb denotes size of CMB in MB. CMB is assumed to be at
 * offset 0 in BAR2 and supports only WDS, RDS and SQS for now.
 *
 * cmb_size_mb= and pmrdev= options are mutually exclusive due to limitation
 * in available BAR's. cmb_size_mb= will take precedence over pmrdev= when
 * both provided.
 * Enabling pmr emulation can be achieved by pointing to memory-backend-file.
 * For example:
 * -object memory-backend-file,id=<mem_id>,share=on,mem-path=<file_path>, \
 *  size=<size> .... -device nvme,...,pmrdev=<mem_id>
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "hw/block/block.h"
#include "hw/pci/msix.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "sysemu/sysemu.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "sysemu/hostmem.h"
#include "sysemu/block-backend.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/cutils.h"
#include "trace.h"
#include "nvme.h"

#define NVME_MAX_IOQPAIRS 0xffff
#define NVME_REG_SIZE 0x1000
#define NVME_DB_SIZE  4
#define NVME_SPEC_VER 0x00010300
#define NVME_CMB_BIR 2
#define NVME_PMR_BIR 2
#define NVME_TEMPERATURE 0x143
#define NVME_TEMPERATURE_WARNING 0x157
#define NVME_TEMPERATURE_CRITICAL 0x175
#define NVME_NUM_FW_SLOTS 1

#define NVME_GUEST_ERR(trace, fmt, ...) \
    do { \
        (trace_##trace)(__VA_ARGS__); \
        qemu_log_mask(LOG_GUEST_ERROR, #trace \
            " in %s: " fmt "\n", __func__, ## __VA_ARGS__); \
    } while (0)

static void nvme_process_sq(void *opaque);
static void nvme_aio_cb(void *opaque, int ret);

static inline void *nvme_addr_to_cmb(NvmeCtrl *n, hwaddr addr)
{
    return &n->cmbuf[addr - n->ctrl_mem.addr];
}

static bool nvme_addr_is_cmb(NvmeCtrl *n, hwaddr addr)
{
    hwaddr low = n->ctrl_mem.addr;
    hwaddr hi  = n->ctrl_mem.addr + int128_get64(n->ctrl_mem.size);

    return addr >= low && addr < hi;
}

static int nvme_addr_read(NvmeCtrl *n, hwaddr addr, void *buf, int size)
{
    hwaddr hi = addr + size - 1;
    if (hi < addr) {
        return 1;
    }

    if (n->bar.cmbsz && nvme_addr_is_cmb(n, addr) && nvme_addr_is_cmb(n, hi)) {
        memcpy(buf, nvme_addr_to_cmb(n, addr), size);
        return 0;
    }

    return pci_dma_read(&n->parent_obj, addr, buf, size);
}

static int nvme_check_sqid(NvmeCtrl *n, uint16_t sqid)
{
    return sqid < n->params.max_ioqpairs + 1 && n->sq[sqid] != NULL ? 0 : -1;
}

static int nvme_check_cqid(NvmeCtrl *n, uint16_t cqid)
{
    return cqid < n->params.max_ioqpairs + 1 && n->cq[cqid] != NULL ? 0 : -1;
}

static void nvme_inc_cq_tail(NvmeCQueue *cq)
{
    cq->tail++;
    if (cq->tail >= cq->size) {
        cq->tail = 0;
        cq->phase = !cq->phase;
    }
}

static void nvme_inc_sq_head(NvmeSQueue *sq)
{
    sq->head = (sq->head + 1) % sq->size;
}

static uint8_t nvme_cq_full(NvmeCQueue *cq)
{
    return (cq->tail + 1) % cq->size == cq->head;
}

static uint8_t nvme_sq_empty(NvmeSQueue *sq)
{
    return sq->head == sq->tail;
}

static void nvme_irq_check(NvmeCtrl *n)
{
    if (msix_enabled(&(n->parent_obj))) {
        return;
    }
    if (~n->bar.intms & n->irq_status) {
        pci_irq_assert(&n->parent_obj);
    } else {
        pci_irq_deassert(&n->parent_obj);
    }
}

static void nvme_irq_assert(NvmeCtrl *n, NvmeCQueue *cq)
{
    if (cq->irq_enabled) {
        if (msix_enabled(&(n->parent_obj))) {
            trace_pci_nvme_irq_msix(cq->vector);
            msix_notify(&(n->parent_obj), cq->vector);
        } else {
            trace_pci_nvme_irq_pin();
            assert(cq->vector < 32);
            n->irq_status |= 1 << cq->vector;
            nvme_irq_check(n);
        }
    } else {
        trace_pci_nvme_irq_masked();
    }
}

static void nvme_irq_deassert(NvmeCtrl *n, NvmeCQueue *cq)
{
    if (cq->irq_enabled) {
        if (msix_enabled(&(n->parent_obj))) {
            return;
        } else {
            assert(cq->vector < 32);
            n->irq_status &= ~(1 << cq->vector);
            nvme_irq_check(n);
        }
    }
}

static void nvme_req_clear(NvmeRequest *req)
{
    req->ns = NULL;
    memset(&req->cqe, 0x0, sizeof(req->cqe));
    req->status = NVME_SUCCESS;
    req->slba = req->nlb = 0x0;
    req->cb = req->cb_arg = NULL;

    if (req->qsg.sg) {
        qemu_sglist_destroy(&req->qsg);
    }

    if (req->iov.iov) {
        qemu_iovec_destroy(&req->iov);
    }
}

static uint16_t nvme_map_addr_cmb(NvmeCtrl *n, QEMUIOVector *iov, hwaddr addr,
                                  size_t len)
{
    if (!len) {
        return NVME_SUCCESS;
    }

    trace_pci_nvme_map_addr_cmb(addr, len);

    if (!nvme_addr_is_cmb(n, addr) || !nvme_addr_is_cmb(n, addr + len - 1)) {
        return NVME_DATA_TRANSFER_ERROR;
    }

    qemu_iovec_add(iov, nvme_addr_to_cmb(n, addr), len);

    return NVME_SUCCESS;
}

static uint16_t nvme_map_addr(NvmeCtrl *n, QEMUSGList *qsg, QEMUIOVector *iov,
                              hwaddr addr, size_t len)
{
    if (!len) {
        return NVME_SUCCESS;
    }

    trace_pci_nvme_map_addr(addr, len);

    if (nvme_addr_is_cmb(n, addr)) {
        if (qsg && qsg->sg) {
            return NVME_INVALID_USE_OF_CMB | NVME_DNR;
        }

        assert(iov);

        if (!iov->iov) {
            qemu_iovec_init(iov, 1);
        }

        return nvme_map_addr_cmb(n, iov, addr, len);
    }

    if (iov && iov->iov) {
        return NVME_INVALID_USE_OF_CMB | NVME_DNR;
    }

    assert(qsg);

    if (!qsg->sg) {
        pci_dma_sglist_init(qsg, &n->parent_obj, 1);
    }

    qemu_sglist_add(qsg, addr, len);

    return NVME_SUCCESS;
}

static uint16_t nvme_map_prp(NvmeCtrl *n, QEMUSGList *qsg, QEMUIOVector *iov,
                             uint64_t prp1, uint64_t prp2, uint32_t len,
                             NvmeRequest *req)
{
    hwaddr trans_len = n->page_size - (prp1 % n->page_size);
    trans_len = MIN(len, trans_len);
    int num_prps = (len >> n->page_bits) + 1;
    uint16_t status;
    bool prp_list_in_cmb = false;
    int ret;

    trace_pci_nvme_map_prp(nvme_cid(req), trans_len, len, prp1, prp2,
                           num_prps);

    if (unlikely(!prp1)) {
        trace_pci_nvme_err_invalid_prp();
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    if (nvme_addr_is_cmb(n, prp1)) {
        qemu_iovec_init(iov, num_prps);
    } else {
        pci_dma_sglist_init(qsg, &n->parent_obj, num_prps);
    }

    status = nvme_map_addr(n, qsg, iov, prp1, trans_len);
    if (status) {
        return status;
    }

    len -= trans_len;
    if (len) {
        if (unlikely(!prp2)) {
            trace_pci_nvme_err_invalid_prp2_missing();
            return NVME_INVALID_FIELD | NVME_DNR;
        }

        if (len > n->page_size) {
            uint64_t prp_list[n->max_prp_ents];
            uint32_t nents, prp_trans;
            int i = 0;

            if (nvme_addr_is_cmb(n, prp2)) {
                prp_list_in_cmb = true;
            }

            nents = (len + n->page_size - 1) >> n->page_bits;
            prp_trans = MIN(n->max_prp_ents, nents) * sizeof(uint64_t);
            ret = nvme_addr_read(n, prp2, (void *)prp_list, prp_trans);
            if (ret) {
                trace_pci_nvme_err_addr_read(prp2);
                return NVME_DATA_TRANSFER_ERROR;
            }
            while (len != 0) {
                uint64_t prp_ent = le64_to_cpu(prp_list[i]);

                if (i == n->max_prp_ents - 1 && len > n->page_size) {
                    if (unlikely(!prp_ent || prp_ent & (n->page_size - 1))) {
                        trace_pci_nvme_err_invalid_prplist_ent(prp_ent);
                        return NVME_INVALID_FIELD | NVME_DNR;
                    }

                    if (prp_list_in_cmb != nvme_addr_is_cmb(n, prp_ent)) {
                        return NVME_INVALID_USE_OF_CMB | NVME_DNR;
                    }

                    i = 0;
                    nents = (len + n->page_size - 1) >> n->page_bits;
                    prp_trans = MIN(n->max_prp_ents, nents) * sizeof(uint64_t);
                    ret = nvme_addr_read(n, prp_ent, (void *)prp_list,
                                         prp_trans);
                    if (ret) {
                        trace_pci_nvme_err_addr_read(prp_ent);
                        return NVME_DATA_TRANSFER_ERROR;
                    }
                    prp_ent = le64_to_cpu(prp_list[i]);
                }

                if (unlikely(!prp_ent || prp_ent & (n->page_size - 1))) {
                    trace_pci_nvme_err_invalid_prplist_ent(prp_ent);
                    return NVME_INVALID_FIELD | NVME_DNR;
                }

                trans_len = MIN(len, n->page_size);
                status = nvme_map_addr(n, qsg, iov, prp_ent, trans_len);
                if (status) {
                    return status;
                }

                len -= trans_len;
                i++;
            }
        } else {
            if (unlikely(prp2 & (n->page_size - 1))) {
                trace_pci_nvme_err_invalid_prp2_align(prp2);
                return NVME_INVALID_FIELD | NVME_DNR;
            }
            status = nvme_map_addr(n, qsg, iov, prp2, len);
            if (status) {
                return status;
            }
        }
    }

    return NVME_SUCCESS;
}

/*
 * Map 'nsgld' data descriptors from 'segment'. The function will subtract the
 * number of bytes mapped in len.
 */
static uint16_t nvme_map_sgl_data(NvmeCtrl *n, QEMUSGList *qsg,
                                  QEMUIOVector *iov,
                                  NvmeSglDescriptor *segment, uint64_t nsgld,
                                  size_t *len, NvmeRequest *req)
{
    dma_addr_t addr, trans_len;
    uint32_t dlen;
    uint16_t status;

    for (int i = 0; i < nsgld; i++) {
        uint8_t type = NVME_SGL_TYPE(segment[i].type);

        switch (type) {
        case NVME_SGL_DESCR_TYPE_BIT_BUCKET:
            if (nvme_req_is_write(req)) {
                continue;
            }
        case NVME_SGL_DESCR_TYPE_DATA_BLOCK:
            break;
        case NVME_SGL_DESCR_TYPE_SEGMENT:
        case NVME_SGL_DESCR_TYPE_LAST_SEGMENT:
            return NVME_INVALID_NUM_SGL_DESCRS | NVME_DNR;
        default:
            return NVME_SGL_DESCR_TYPE_INVALID | NVME_DNR;
        }

        dlen = le32_to_cpu(segment[i].len);

        if (!dlen) {
            continue;
        }

        if (*len == 0) {
            /*
             * All data has been mapped, but the SGL contains additional
             * segments and/or descriptors. The controller might accept
             * ignoring the rest of the SGL.
             */
            uint16_t sgls = le16_to_cpu(n->id_ctrl.sgls);
            if (sgls & NVME_CTRL_SGLS_EXCESS_LENGTH) {
                break;
            }

            trace_pci_nvme_err_invalid_sgl_excess_length(nvme_cid(req));
            return NVME_DATA_SGL_LEN_INVALID | NVME_DNR;
        }

        trans_len = MIN(*len, dlen);

        if (type == NVME_SGL_DESCR_TYPE_BIT_BUCKET) {
            goto next;
        }

        addr = le64_to_cpu(segment[i].addr);

        if (UINT64_MAX - addr < dlen) {
            return NVME_DATA_SGL_LEN_INVALID | NVME_DNR;
        }

        status = nvme_map_addr(n, qsg, iov, addr, trans_len);
        if (status) {
            return status;
        }

next:
        *len -= trans_len;
    }

    return NVME_SUCCESS;
}

static uint16_t nvme_map_sgl(NvmeCtrl *n, QEMUSGList *qsg, QEMUIOVector *iov,
                             NvmeSglDescriptor sgl, size_t len,
                             NvmeRequest *req)
{
    /*
     * Read the segment in chunks of 256 descriptors (one 4k page) to avoid
     * dynamically allocating a potentially huge SGL. The spec allows the SGL
     * to be larger (as in number of bytes required to describe the SGL
     * descriptors and segment chain) than the command transfer size, so it is
     * not bounded by MDTS.
     */
    const int SEG_CHUNK_SIZE = 256;

    NvmeSglDescriptor segment[SEG_CHUNK_SIZE], *sgld, *last_sgld;
    uint64_t nsgld;
    uint32_t seg_len;
    uint16_t status;
    bool sgl_in_cmb = false;
    hwaddr addr;
    int ret;

    sgld = &sgl;
    addr = le64_to_cpu(sgl.addr);

    trace_pci_nvme_map_sgl(nvme_cid(req), NVME_SGL_TYPE(sgl.type), req->nlb,
                           len);

    /*
     * If the entire transfer can be described with a single data block it can
     * be mapped directly.
     */
    if (NVME_SGL_TYPE(sgl.type) == NVME_SGL_DESCR_TYPE_DATA_BLOCK) {
        status = nvme_map_sgl_data(n, qsg, iov, sgld, 1, &len, req);
        if (status) {
            goto unmap;
        }

        goto out;
    }

    /*
     * If the segment is located in the CMB, the submission queue of the
     * request must also reside there.
     */
    if (nvme_addr_is_cmb(n, addr)) {
        if (!nvme_addr_is_cmb(n, req->sq->dma_addr)) {
            return NVME_INVALID_USE_OF_CMB | NVME_DNR;
        }

        sgl_in_cmb = true;
    }

    for (;;) {
        switch (NVME_SGL_TYPE(sgld->type)) {
        case NVME_SGL_DESCR_TYPE_SEGMENT:
        case NVME_SGL_DESCR_TYPE_LAST_SEGMENT:
            break;
        default:
            return NVME_INVALID_SGL_SEG_DESCR | NVME_DNR;
        }

        seg_len = le32_to_cpu(sgld->len);

        /* check the length of the (Last) Segment descriptor */
        if ((!seg_len || seg_len & 0xf) &&
            (NVME_SGL_TYPE(sgld->type) != NVME_SGL_DESCR_TYPE_BIT_BUCKET)) {
            return NVME_INVALID_SGL_SEG_DESCR | NVME_DNR;
        }

        if (UINT64_MAX - addr < seg_len) {
            return NVME_DATA_SGL_LEN_INVALID | NVME_DNR;
        }

        nsgld = seg_len / sizeof(NvmeSglDescriptor);

        while (nsgld > SEG_CHUNK_SIZE) {
            if (nvme_addr_read(n, addr, segment, sizeof(segment))) {
                trace_pci_nvme_err_addr_read(addr);
                status = NVME_DATA_TRANSFER_ERROR;
                goto unmap;
            }

            status = nvme_map_sgl_data(n, qsg, iov, segment, SEG_CHUNK_SIZE,
                                       &len, req);
            if (status) {
                goto unmap;
            }

            nsgld -= SEG_CHUNK_SIZE;
            addr += SEG_CHUNK_SIZE * sizeof(NvmeSglDescriptor);
        }

        ret = nvme_addr_read(n, addr, segment, nsgld *
                             sizeof(NvmeSglDescriptor));
        if (ret) {
            trace_pci_nvme_err_addr_read(addr);
            status = NVME_DATA_TRANSFER_ERROR;
            goto unmap;
        }

        last_sgld = &segment[nsgld - 1];

        /*
         * If the segment ends with a Data Block or Bit Bucket Descriptor Type,
         * then we are done.
         */
        switch (NVME_SGL_TYPE(last_sgld->type)) {
        case NVME_SGL_DESCR_TYPE_DATA_BLOCK:
        case NVME_SGL_DESCR_TYPE_BIT_BUCKET:
            status = nvme_map_sgl_data(n, qsg, iov, segment, nsgld, &len, req);
            if (status) {
                goto unmap;
            }

            goto out;

        default:
            break;
        }

        /*
         * If the last descriptor was not a Data Block or Bit Bucket, then the
         * current segment must not be a Last Segment.
         */
        if (NVME_SGL_TYPE(sgld->type) == NVME_SGL_DESCR_TYPE_LAST_SEGMENT) {
            status = NVME_INVALID_SGL_SEG_DESCR | NVME_DNR;
            goto unmap;
        }

        sgld = last_sgld;
        addr = le64_to_cpu(sgld->addr);

        /*
         * Do not map the last descriptor; it will be a Segment or Last Segment
         * descriptor and is handled by the next iteration.
         */
        status = nvme_map_sgl_data(n, qsg, iov, segment, nsgld - 1, &len, req);
        if (status) {
            goto unmap;
        }

        /*
         * If the next segment is in the CMB, make sure that the sgl was
         * already located there.
         */
        if (sgl_in_cmb != nvme_addr_is_cmb(n, addr)) {
            status = NVME_INVALID_USE_OF_CMB | NVME_DNR;
            goto unmap;
        }
    }

out:
    /* if there is any residual left in len, the SGL was too short */
    if (len) {
        status = NVME_DATA_SGL_LEN_INVALID | NVME_DNR;
        goto unmap;
    }

    return NVME_SUCCESS;

unmap:
    if (iov->iov) {
        qemu_iovec_destroy(iov);
    }

    if (qsg->sg) {
        qemu_sglist_destroy(qsg);
    }

    return status;
}

static uint16_t nvme_map(NvmeCtrl *n, size_t len, NvmeRequest *req)
{
    uint64_t prp1, prp2;

    switch (NVME_CMD_FLAGS_PSDT(req->cmd.flags)) {
    case NVME_PSDT_PRP:
        prp1 = le64_to_cpu(req->cmd.dptr.prp1);
        prp2 = le64_to_cpu(req->cmd.dptr.prp2);

        return nvme_map_prp(n, &req->qsg, &req->iov, prp1, prp2, len, req);
    case NVME_PSDT_SGL_MPTR_CONTIGUOUS:
    case NVME_PSDT_SGL_MPTR_SGL:
        /* SGLs shall not be used for Admin commands in NVMe over PCIe */
        if (!req->sq->sqid) {
            return NVME_INVALID_FIELD | NVME_DNR;
        }

        return nvme_map_sgl(n, &req->qsg, &req->iov, req->cmd.dptr.sgl, len,
                            req);
    default:
        return NVME_INVALID_FIELD;
    }
}

static uint16_t nvme_dma(NvmeCtrl *n, uint8_t *ptr, uint32_t len,
                         DMADirection dir, NvmeRequest *req)
{
    uint16_t status = NVME_SUCCESS;

    status = nvme_map(n, len, req);
    if (status) {
        return status;
    }

    if (req->qsg.nsg > 0) {
        uint64_t residual;

        if (dir == DMA_DIRECTION_TO_DEVICE) {
            residual = dma_buf_write(ptr, len, &req->qsg);
        } else {
            residual = dma_buf_read(ptr, len, &req->qsg);
        }

        if (unlikely(residual)) {
            trace_pci_nvme_err_invalid_dma();
            status = NVME_INVALID_FIELD | NVME_DNR;
        }
    } else {
        size_t bytes;

        if (dir == DMA_DIRECTION_TO_DEVICE) {
            bytes = qemu_iovec_to_buf(&req->iov, 0, ptr, len);
        } else {
            bytes = qemu_iovec_from_buf(&req->iov, 0, ptr, len);
        }

        if (unlikely(bytes != len)) {
            trace_pci_nvme_err_invalid_dma();
            status = NVME_INVALID_FIELD | NVME_DNR;
        }
    }

    return status;
}

static void nvme_aio_destroy(NvmeAIO *aio)
{
    g_free(aio);
}

/*
 * Submit an asynchronous I/O operation as described by the given NvmeAIO. This
 * function takes care of accounting and special handling of reads and writes
 * going to the Controller Memory Buffer.
 */
static void nvme_submit_aio(NvmeAIO *aio)
{
    BlockBackend *blk = aio->blk;
    BlockAcctCookie *acct = &aio->acct;
    BlockAcctStats *stats = blk_get_stats(blk);

    bool is_write;

    switch (aio->opc) {
    case NVME_AIO_OPC_NONE:
        break;

    case NVME_AIO_OPC_FLUSH:
        block_acct_start(stats, acct, 0, BLOCK_ACCT_FLUSH);
        aio->aiocb = blk_aio_flush(blk, nvme_aio_cb, aio);
        break;

    case NVME_AIO_OPC_WRITE_ZEROES:
        block_acct_start(stats, acct, aio->len, BLOCK_ACCT_WRITE);
        aio->aiocb = blk_aio_pwrite_zeroes(blk, aio->offset, aio->len,
                                           BDRV_REQ_MAY_UNMAP, nvme_aio_cb,
                                           aio);
        break;

    case NVME_AIO_OPC_READ:
    case NVME_AIO_OPC_WRITE:
        is_write = (aio->opc == NVME_AIO_OPC_WRITE);

        block_acct_start(stats, acct, aio->len,
                         is_write ? BLOCK_ACCT_WRITE : BLOCK_ACCT_READ);

        if (aio->flags & NVME_AIO_DMA) {
            QEMUSGList *qsg = (QEMUSGList *)aio->payload;

            if (is_write) {
                aio->aiocb = dma_blk_write(blk, qsg, aio->offset,
                                           BDRV_SECTOR_SIZE, nvme_aio_cb, aio);
            } else {
                aio->aiocb = dma_blk_read(blk, qsg, aio->offset,
                                          BDRV_SECTOR_SIZE, nvme_aio_cb, aio);
            }
        } else {
            QEMUIOVector *iov = (QEMUIOVector *)aio->payload;

            if (is_write) {
                aio->aiocb = blk_aio_pwritev(blk, aio->offset, iov, 0,
                                             nvme_aio_cb, aio);
            } else {
                aio->aiocb = blk_aio_preadv(blk, aio->offset, iov, 0,
                                            nvme_aio_cb, aio);
            }
        }

        break;
    }
}

/*
 * Register an asynchronous I/O operation with the NvmeRequest. The NvmeRequest
 * will not complete until all registered AIO's have completed and the
 * aio_tailq goes empty.
 */
static inline void nvme_req_add_aio(NvmeRequest *req, NvmeAIO *aio)
{
    assert(req);

    trace_pci_nvme_req_add_aio(nvme_cid(req), aio, blk_name(aio->blk),
                               aio->offset, aio->len,
                               nvme_aio_opc_str(aio), req);

    QTAILQ_INSERT_TAIL(&req->aio_tailq, aio, tailq_entry);

    nvme_submit_aio(aio);
}

static void nvme_post_cqes(void *opaque)
{
    NvmeCQueue *cq = opaque;
    NvmeCtrl *n = cq->ctrl;
    NvmeRequest *req, *next;
    int ret;

    QTAILQ_FOREACH_SAFE(req, &cq->req_list, entry, next) {
        NvmeSQueue *sq;
        hwaddr addr;

        if (nvme_cq_full(cq)) {
            break;
        }

        sq = req->sq;
        req->cqe.status = cpu_to_le16((req->status << 1) | cq->phase);
        req->cqe.sq_id = cpu_to_le16(sq->sqid);
        req->cqe.sq_head = cpu_to_le16(sq->head);
        addr = cq->dma_addr + cq->tail * n->cqe_size;
        ret = pci_dma_write(&n->parent_obj, addr, (void *)&req->cqe,
                            sizeof(req->cqe));
        if (ret) {
            trace_pci_nvme_err_addr_write(addr);
            timer_mod(cq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                      500 * SCALE_MS);
            break;
        }
        QTAILQ_REMOVE(&cq->req_list, req, entry);
        nvme_inc_cq_tail(cq);
        nvme_req_clear(req);
        QTAILQ_INSERT_TAIL(&sq->req_list, req, entry);
    }
    if (cq->tail != cq->head) {
        nvme_irq_assert(n, cq);
    }
}

static void nvme_enqueue_req_completion(NvmeCQueue *cq, NvmeRequest *req)
{
    assert(cq->cqid == req->sq->cqid);
    trace_pci_nvme_enqueue_req_completion(nvme_cid(req), cq->cqid,
                                          req->status);

    QTAILQ_REMOVE(&req->sq->out_req_list, req, entry);
    QTAILQ_INSERT_TAIL(&cq->req_list, req, entry);
    timer_mod(cq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 500);
}

static void nvme_process_aers(void *opaque)
{
    NvmeCtrl *n = opaque;
    NvmeAsyncEvent *event, *next;

    trace_pci_nvme_process_aers(n->aer_queued);

    QTAILQ_FOREACH_SAFE(event, &n->aer_queue, entry, next) {
        NvmeRequest *req;
        NvmeAerResult *result;

        /* can't post cqe if there is nothing to complete */
        if (!n->outstanding_aers) {
            trace_pci_nvme_no_outstanding_aers();
            break;
        }

        /* ignore if masked (cqe posted, but event not cleared) */
        if (n->aer_mask & (1 << event->result.event_type)) {
            trace_pci_nvme_aer_masked(event->result.event_type, n->aer_mask);
            continue;
        }

        QTAILQ_REMOVE(&n->aer_queue, event, entry);
        n->aer_queued--;

        n->aer_mask |= 1 << event->result.event_type;
        n->outstanding_aers--;

        req = n->aer_reqs[n->outstanding_aers];

        result = (NvmeAerResult *) &req->cqe.result;
        result->event_type = event->result.event_type;
        result->event_info = event->result.event_info;
        result->log_page = event->result.log_page;
        g_free(event);

        req->status = NVME_SUCCESS;

        trace_pci_nvme_aer_post_cqe(result->event_type, result->event_info,
                                    result->log_page);

        nvme_enqueue_req_completion(&n->admin_cq, req);
    }
}

static void nvme_enqueue_event(NvmeCtrl *n, uint8_t event_type,
                               uint8_t event_info, uint8_t log_page)
{
    NvmeAsyncEvent *event;

    trace_pci_nvme_enqueue_event(event_type, event_info, log_page);

    if (n->aer_queued == n->params.aer_max_queued) {
        trace_pci_nvme_enqueue_event_noqueue(n->aer_queued);
        return;
    }

    event = g_new(NvmeAsyncEvent, 1);
    event->result = (NvmeAerResult) {
        .event_type = event_type,
        .event_info = event_info,
        .log_page   = log_page,
    };

    QTAILQ_INSERT_TAIL(&n->aer_queue, event, entry);
    n->aer_queued++;

    nvme_process_aers(n);
}

static void nvme_clear_events(NvmeCtrl *n, uint8_t event_type)
{
    n->aer_mask &= ~(1 << event_type);
    if (!QTAILQ_EMPTY(&n->aer_queue)) {
        nvme_process_aers(n);
    }
}

static inline uint16_t nvme_check_mdts(NvmeCtrl *n, size_t len)
{
    uint8_t mdts = n->params.mdts;

    if (mdts && len > n->page_size << mdts) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    return NVME_SUCCESS;
}

static inline uint16_t nvme_check_bounds(NvmeCtrl *n, NvmeNamespace *ns,
                                         uint64_t slba, uint32_t nlb)
{
    uint64_t nsze = le64_to_cpu(ns->id_ns.nsze);

    if (unlikely(UINT64_MAX - slba < nlb || slba + nlb > nsze)) {
        return NVME_LBA_RANGE | NVME_DNR;
    }

    return NVME_SUCCESS;
}

static uint16_t nvme_check_rw(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeNamespace *ns = req->ns;
    size_t len = req->nlb << nvme_ns_lbads(ns);
    uint16_t status;

    status = nvme_check_mdts(n, len);
    if (status) {
        trace_pci_nvme_err_mdts(nvme_cid(req), len);
        return status;
    }

    status = nvme_check_bounds(n, ns, req->slba, req->nlb);
    if (status) {
        trace_pci_nvme_err_invalid_lba_range(req->slba, req->nlb,
                                             ns->id_ns.nsze);
        return status;
    }

    return NVME_SUCCESS;
}

static void nvme_rw_cb(NvmeRequest *req, void *opaque)
{
    NvmeSQueue *sq = req->sq;
    NvmeCtrl *n = sq->ctrl;
    NvmeCQueue *cq = n->cq[sq->cqid];

    trace_pci_nvme_rw_cb(nvme_cid(req));

    nvme_enqueue_req_completion(cq, req);
}

static void nvme_aio_cb(void *opaque, int ret)
{
    NvmeAIO *aio = opaque;
    NvmeRequest *req = aio->req;

    BlockBackend *blk = aio->blk;
    BlockAcctCookie *acct = &aio->acct;
    BlockAcctStats *stats = blk_get_stats(blk);

    Error *local_err = NULL;

    trace_pci_nvme_aio_cb(nvme_cid(req), aio, blk_name(blk), aio->offset,
                          nvme_aio_opc_str(aio), req);

    if (req) {
        QTAILQ_REMOVE(&req->aio_tailq, aio, tailq_entry);
    }

    if (!ret) {
        block_acct_done(stats, acct);
    } else {
        block_acct_failed(stats, acct);

        if (req) {
            uint16_t status;

            switch (aio->opc) {
            case NVME_AIO_OPC_READ:
                status = NVME_UNRECOVERED_READ;
                break;
            case NVME_AIO_OPC_WRITE:
            case NVME_AIO_OPC_WRITE_ZEROES:
                status = NVME_WRITE_FAULT;
                break;
            default:
                status = NVME_INTERNAL_DEV_ERROR;
                break;
            }

            trace_pci_nvme_err_aio(nvme_cid(req), aio, blk_name(blk),
                                   aio->offset, nvme_aio_opc_str(aio), req,
                                   status);

            error_setg_errno(&local_err, -ret, "aio failed");
            error_report_err(local_err);

            /*
             * An Internal Error trumps all other errors. For other errors,
             * only set the first error encountered. Any additional errors will
             * be recorded in the error information log page.
             */
            if (!req->status || (status & 0xfff) == NVME_INTERNAL_DEV_ERROR) {
                req->status = status;
            }
        }
    }

    if (aio->cb) {
        aio->cb(aio, aio->cb_arg, ret);
    }

    if (req && QTAILQ_EMPTY(&req->aio_tailq)) {
        if (req->cb) {
            req->cb(req, req->cb_arg);
        } else {
            NvmeSQueue *sq = req->sq;
            NvmeCtrl *n = sq->ctrl;
            NvmeCQueue *cq = n->cq[sq->cqid];

            nvme_enqueue_req_completion(cq, req);
        }
    }

    nvme_aio_destroy(aio);
}

static void nvme_aio_rw(NvmeNamespace *ns, BlockBackend *blk, NvmeAIOOp opc,
                        NvmeRequest *req)
{
    NvmeAIO *aio = g_new(NvmeAIO, 1);

    *aio = (NvmeAIO) {
        .opc = opc,
        .blk = blk,
        .offset = req->slba << nvme_ns_lbads(ns),
        .req = req,
    };

    if (req->qsg.sg) {
        aio->payload = &req->qsg;
        aio->len = req->qsg.size;
        aio->flags |= NVME_AIO_DMA;
    } else {
        aio->payload = &req->iov;
        aio->len = req->iov.size;
    }

    nvme_req_add_aio(req, aio);
}

static uint16_t nvme_flush(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeAIO *aio = g_new0(NvmeAIO, 1);

    *aio = (NvmeAIO) {
        .opc = NVME_AIO_OPC_FLUSH,
        .blk = n->conf.blk,
        .req = req,
    };

    nvme_req_add_aio(req, aio);

    return NVME_NO_COMPLETE;
}

static uint16_t nvme_write_zeroes(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeRwCmd *rw = (NvmeRwCmd *)&req->cmd;
    NvmeNamespace *ns = req->ns;
    NvmeAIO *aio;

    int64_t offset;
    size_t count;
    uint16_t status;

    req->slba = le64_to_cpu(rw->slba);
    req->nlb  = le16_to_cpu(rw->nlb) + 1;

    trace_pci_nvme_write_zeroes(nvme_cid(req), req->slba, req->nlb);

    status = nvme_check_bounds(n, ns, req->slba, req->nlb);
    if (status) {
        trace_pci_nvme_err_invalid_lba_range(req->slba, req->nlb,
                                             ns->id_ns.nsze);
        return status;
    }

    offset = req->slba << nvme_ns_lbads(ns);
    count = req->nlb << nvme_ns_lbads(ns);

    aio = g_new0(NvmeAIO, 1);

    *aio = (NvmeAIO) {
        .opc = NVME_AIO_OPC_WRITE_ZEROES,
        .blk = n->conf.blk,
        .offset = offset,
        .len = count,
        .req = req,
    };

    nvme_req_add_aio(req, aio);

    return NVME_NO_COMPLETE;
}

static uint16_t nvme_rw(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeRwCmd *rw = (NvmeRwCmd *)&req->cmd;
    NvmeNamespace *ns = req->ns;

    uint32_t len;
    int status;

    enum BlockAcctType acct = BLOCK_ACCT_READ;
    NvmeAIOOp opc = NVME_AIO_OPC_READ;

    if (nvme_req_is_write(req)) {
        acct = BLOCK_ACCT_WRITE;
        opc = NVME_AIO_OPC_WRITE;
    }

    req->nlb  = le16_to_cpu(rw->nlb) + 1;
    req->slba = le64_to_cpu(rw->slba);

    len = req->nlb << nvme_ns_lbads(ns);

    trace_pci_nvme_rw(nvme_req_is_write(req) ? "write" : "read", req->nlb,
                      len, req->slba);

    status = nvme_check_rw(n, req);
    if (status) {
        goto invalid;
    }

    status = nvme_map(n, len, req);
    if (status) {
        goto invalid;
    }

    nvme_aio_rw(ns, n->conf.blk, opc, req);
    nvme_req_set_cb(req, nvme_rw_cb, NULL);

    return NVME_NO_COMPLETE;

invalid:
    block_acct_invalid(blk_get_stats(n->conf.blk), acct);
    return status;
}

static uint16_t nvme_io_cmd(NvmeCtrl *n, NvmeRequest *req)
{
    uint32_t nsid = le32_to_cpu(req->cmd.nsid);

    trace_pci_nvme_io_cmd(nvme_cid(req), nsid, nvme_sqid(req),
                          req->cmd.opcode);

    if (unlikely(nsid == 0 || nsid > n->num_namespaces)) {
        trace_pci_nvme_err_invalid_ns(nsid, n->num_namespaces);
        return NVME_INVALID_NSID | NVME_DNR;
    }

    req->ns = &n->namespaces[nsid - 1];
    switch (req->cmd.opcode) {
    case NVME_CMD_FLUSH:
        return nvme_flush(n, req);
    case NVME_CMD_WRITE_ZEROES:
        return nvme_write_zeroes(n, req);
    case NVME_CMD_WRITE:
    case NVME_CMD_READ:
        return nvme_rw(n, req);
    default:
        trace_pci_nvme_err_invalid_opc(req->cmd.opcode);
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

static void nvme_free_sq(NvmeSQueue *sq, NvmeCtrl *n)
{
    n->sq[sq->sqid] = NULL;
    timer_del(sq->timer);
    timer_free(sq->timer);
    g_free(sq->io_req);
    if (sq->sqid) {
        g_free(sq);
    }
}

static uint16_t nvme_del_sq(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeDeleteQ *c = (NvmeDeleteQ *)&req->cmd;
    NvmeRequest *r, *next;
    NvmeSQueue *sq;
    NvmeCQueue *cq;
    NvmeAIO *aio;
    uint16_t qid = le16_to_cpu(c->qid);

    if (unlikely(!qid || nvme_check_sqid(n, qid))) {
        trace_pci_nvme_err_invalid_del_sq(qid);
        return NVME_INVALID_QID | NVME_DNR;
    }

    trace_pci_nvme_del_sq(qid);

    sq = n->sq[qid];
    while (!QTAILQ_EMPTY(&sq->out_req_list)) {
        r = QTAILQ_FIRST(&sq->out_req_list);
        while (!QTAILQ_EMPTY(&r->aio_tailq)) {
            aio = QTAILQ_FIRST(&r->aio_tailq);
            assert(aio->aiocb);
            blk_aio_cancel(aio->aiocb);
        }
    }
    if (!nvme_check_cqid(n, sq->cqid)) {
        cq = n->cq[sq->cqid];
        QTAILQ_REMOVE(&cq->sq_list, sq, entry);

        nvme_post_cqes(cq);
        QTAILQ_FOREACH_SAFE(r, &cq->req_list, entry, next) {
            if (r->sq == sq) {
                QTAILQ_REMOVE(&cq->req_list, r, entry);
                QTAILQ_INSERT_TAIL(&sq->req_list, r, entry);
            }
        }
    }

    nvme_free_sq(sq, n);
    return NVME_SUCCESS;
}

static void nvme_init_sq(NvmeSQueue *sq, NvmeCtrl *n, uint64_t dma_addr,
    uint16_t sqid, uint16_t cqid, uint16_t size)
{
    int i;
    NvmeCQueue *cq;

    sq->ctrl = n;
    sq->dma_addr = dma_addr;
    sq->sqid = sqid;
    sq->size = size;
    sq->cqid = cqid;
    sq->head = sq->tail = 0;
    sq->io_req = g_new0(NvmeRequest, sq->size);

    QTAILQ_INIT(&sq->req_list);
    QTAILQ_INIT(&sq->out_req_list);
    for (i = 0; i < sq->size; i++) {
        sq->io_req[i].sq = sq;
        QTAILQ_INIT(&(sq->io_req[i].aio_tailq));
        QTAILQ_INSERT_TAIL(&(sq->req_list), &sq->io_req[i], entry);
    }
    sq->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, nvme_process_sq, sq);

    assert(n->cq[cqid]);
    cq = n->cq[cqid];
    QTAILQ_INSERT_TAIL(&(cq->sq_list), sq, entry);
    n->sq[sqid] = sq;
}

static uint16_t nvme_create_sq(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeSQueue *sq;
    NvmeCreateSq *c = (NvmeCreateSq *)&req->cmd;

    uint16_t cqid = le16_to_cpu(c->cqid);
    uint16_t sqid = le16_to_cpu(c->sqid);
    uint16_t qsize = le16_to_cpu(c->qsize);
    uint16_t qflags = le16_to_cpu(c->sq_flags);
    uint64_t prp1 = le64_to_cpu(c->prp1);

    trace_pci_nvme_create_sq(prp1, sqid, cqid, qsize, qflags);

    if (unlikely(!cqid || nvme_check_cqid(n, cqid))) {
        trace_pci_nvme_err_invalid_create_sq_cqid(cqid);
        return NVME_INVALID_CQID | NVME_DNR;
    }
    if (unlikely(!sqid || !nvme_check_sqid(n, sqid))) {
        trace_pci_nvme_err_invalid_create_sq_sqid(sqid);
        return NVME_INVALID_QID | NVME_DNR;
    }
    if (unlikely(!qsize || qsize > NVME_CAP_MQES(n->bar.cap))) {
        trace_pci_nvme_err_invalid_create_sq_size(qsize);
        return NVME_MAX_QSIZE_EXCEEDED | NVME_DNR;
    }
    if (unlikely(!prp1 || prp1 & (n->page_size - 1))) {
        trace_pci_nvme_err_invalid_create_sq_addr(prp1);
        return NVME_INVALID_FIELD | NVME_DNR;
    }
    if (unlikely(!(NVME_SQ_FLAGS_PC(qflags)))) {
        trace_pci_nvme_err_invalid_create_sq_qflags(NVME_SQ_FLAGS_PC(qflags));
        return NVME_INVALID_FIELD | NVME_DNR;
    }
    sq = g_malloc0(sizeof(*sq));
    nvme_init_sq(sq, n, prp1, sqid, cqid, qsize + 1);
    return NVME_SUCCESS;
}

static uint16_t nvme_smart_info(NvmeCtrl *n, uint8_t rae, uint32_t buf_len,
                                uint64_t off, NvmeRequest *req)
{
    uint32_t nsid = le32_to_cpu(req->cmd.nsid);

    uint32_t trans_len;
    time_t current_ms;
    uint64_t units_read = 0, units_written = 0;
    uint64_t read_commands = 0, write_commands = 0;
    NvmeSmartLog smart;
    BlockAcctStats *s;

    if (nsid && nsid != 0xffffffff) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    s = blk_get_stats(n->conf.blk);

    units_read = s->nr_bytes[BLOCK_ACCT_READ] >> BDRV_SECTOR_BITS;
    units_written = s->nr_bytes[BLOCK_ACCT_WRITE] >> BDRV_SECTOR_BITS;
    read_commands = s->nr_ops[BLOCK_ACCT_READ];
    write_commands = s->nr_ops[BLOCK_ACCT_WRITE];

    if (off > sizeof(smart)) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    trans_len = MIN(sizeof(smart) - off, buf_len);

    memset(&smart, 0x0, sizeof(smart));

    smart.data_units_read[0] = cpu_to_le64(units_read / 1000);
    smart.data_units_written[0] = cpu_to_le64(units_written / 1000);
    smart.host_read_commands[0] = cpu_to_le64(read_commands);
    smart.host_write_commands[0] = cpu_to_le64(write_commands);

    smart.temperature[0] = n->temperature & 0xff;
    smart.temperature[1] = (n->temperature >> 8) & 0xff;

    if ((n->temperature >= n->features.temp_thresh_hi) ||
        (n->temperature <= n->features.temp_thresh_low)) {
        smart.critical_warning |= NVME_SMART_TEMPERATURE;
    }

    current_ms = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
    smart.power_on_hours[0] =
        cpu_to_le64((((current_ms - n->starttime_ms) / 1000) / 60) / 60);

    if (!rae) {
        nvme_clear_events(n, NVME_AER_TYPE_SMART);
    }

    return nvme_dma(n, (uint8_t *) &smart + off, trans_len,
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_fw_log_info(NvmeCtrl *n, uint32_t buf_len, uint64_t off,
                                 NvmeRequest *req)
{
    uint32_t trans_len;
    NvmeFwSlotInfoLog fw_log = {
        .afi = 0x1,
    };

    strpadcpy((char *)&fw_log.frs1, sizeof(fw_log.frs1), "1.0", ' ');

    if (off > sizeof(fw_log)) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    trans_len = MIN(sizeof(fw_log) - off, buf_len);

    return nvme_dma(n, (uint8_t *) &fw_log + off, trans_len,
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_error_info(NvmeCtrl *n, uint8_t rae, uint32_t buf_len,
                                uint64_t off, NvmeRequest *req)
{
    uint32_t trans_len;
    NvmeErrorLog errlog;

    if (!rae) {
        nvme_clear_events(n, NVME_AER_TYPE_ERROR);
    }

    if (off > sizeof(errlog)) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    memset(&errlog, 0x0, sizeof(errlog));

    trans_len = MIN(sizeof(errlog) - off, buf_len);

    return nvme_dma(n, (uint8_t *)&errlog, trans_len,
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_get_log(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeCmd *cmd = &req->cmd;

    uint32_t dw10 = le32_to_cpu(cmd->cdw10);
    uint32_t dw11 = le32_to_cpu(cmd->cdw11);
    uint32_t dw12 = le32_to_cpu(cmd->cdw12);
    uint32_t dw13 = le32_to_cpu(cmd->cdw13);
    uint8_t  lid = dw10 & 0xff;
    uint8_t  lsp = (dw10 >> 8) & 0xf;
    uint8_t  rae = (dw10 >> 15) & 0x1;
    uint32_t numdl, numdu;
    uint64_t off, lpol, lpou;
    size_t   len;
    uint16_t status;

    numdl = (dw10 >> 16);
    numdu = (dw11 & 0xffff);
    lpol = dw12;
    lpou = dw13;

    len = (((numdu << 16) | numdl) + 1) << 2;
    off = (lpou << 32ULL) | lpol;

    if (off & 0x3) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    trace_pci_nvme_get_log(nvme_cid(req), lid, lsp, rae, len, off);

    status = nvme_check_mdts(n, len);
    if (status) {
        trace_pci_nvme_err_mdts(nvme_cid(req), len);
        return status;
    }

    switch (lid) {
    case NVME_LOG_ERROR_INFO:
        return nvme_error_info(n, rae, len, off, req);
    case NVME_LOG_SMART_INFO:
        return nvme_smart_info(n, rae, len, off, req);
    case NVME_LOG_FW_SLOT_INFO:
        return nvme_fw_log_info(n, len, off, req);
    default:
        trace_pci_nvme_err_invalid_log_page(nvme_cid(req), lid);
        return NVME_INVALID_FIELD | NVME_DNR;
    }
}

static void nvme_free_cq(NvmeCQueue *cq, NvmeCtrl *n)
{
    n->cq[cq->cqid] = NULL;
    timer_del(cq->timer);
    timer_free(cq->timer);
    msix_vector_unuse(&n->parent_obj, cq->vector);
    if (cq->cqid) {
        g_free(cq);
    }
}

static uint16_t nvme_del_cq(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeDeleteQ *c = (NvmeDeleteQ *)&req->cmd;
    NvmeCQueue *cq;
    uint16_t qid = le16_to_cpu(c->qid);

    if (unlikely(!qid || nvme_check_cqid(n, qid))) {
        trace_pci_nvme_err_invalid_del_cq_cqid(qid);
        return NVME_INVALID_CQID | NVME_DNR;
    }

    cq = n->cq[qid];
    if (unlikely(!QTAILQ_EMPTY(&cq->sq_list))) {
        trace_pci_nvme_err_invalid_del_cq_notempty(qid);
        return NVME_INVALID_QUEUE_DEL;
    }
    nvme_irq_deassert(n, cq);
    trace_pci_nvme_del_cq(qid);
    nvme_free_cq(cq, n);
    return NVME_SUCCESS;
}

static void nvme_init_cq(NvmeCQueue *cq, NvmeCtrl *n, uint64_t dma_addr,
    uint16_t cqid, uint16_t vector, uint16_t size, uint16_t irq_enabled)
{
    int ret;

    ret = msix_vector_use(&n->parent_obj, vector);
    assert(ret == 0);
    cq->ctrl = n;
    cq->cqid = cqid;
    cq->size = size;
    cq->dma_addr = dma_addr;
    cq->phase = 1;
    cq->irq_enabled = irq_enabled;
    cq->vector = vector;
    cq->head = cq->tail = 0;
    QTAILQ_INIT(&cq->req_list);
    QTAILQ_INIT(&cq->sq_list);
    n->cq[cqid] = cq;
    cq->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, nvme_post_cqes, cq);
}

static uint16_t nvme_create_cq(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeCQueue *cq;
    NvmeCreateCq *c = (NvmeCreateCq *)&req->cmd;
    uint16_t cqid = le16_to_cpu(c->cqid);
    uint16_t vector = le16_to_cpu(c->irq_vector);
    uint16_t qsize = le16_to_cpu(c->qsize);
    uint16_t qflags = le16_to_cpu(c->cq_flags);
    uint64_t prp1 = le64_to_cpu(c->prp1);

    trace_pci_nvme_create_cq(prp1, cqid, vector, qsize, qflags,
                             NVME_CQ_FLAGS_IEN(qflags) != 0);

    if (unlikely(!cqid || !nvme_check_cqid(n, cqid))) {
        trace_pci_nvme_err_invalid_create_cq_cqid(cqid);
        return NVME_INVALID_CQID | NVME_DNR;
    }
    if (unlikely(!qsize || qsize > NVME_CAP_MQES(n->bar.cap))) {
        trace_pci_nvme_err_invalid_create_cq_size(qsize);
        return NVME_MAX_QSIZE_EXCEEDED | NVME_DNR;
    }
    if (unlikely(!prp1)) {
        trace_pci_nvme_err_invalid_create_cq_addr(prp1);
        return NVME_INVALID_FIELD | NVME_DNR;
    }
    if (unlikely(!msix_enabled(&n->parent_obj) && vector)) {
        trace_pci_nvme_err_invalid_create_cq_vector(vector);
        return NVME_INVALID_IRQ_VECTOR | NVME_DNR;
    }
    if (unlikely(vector >= n->params.msix_qsize)) {
        trace_pci_nvme_err_invalid_create_cq_vector(vector);
        return NVME_INVALID_IRQ_VECTOR | NVME_DNR;
    }
    if (unlikely(!(NVME_CQ_FLAGS_PC(qflags)))) {
        trace_pci_nvme_err_invalid_create_cq_qflags(NVME_CQ_FLAGS_PC(qflags));
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    cq = g_malloc0(sizeof(*cq));
    nvme_init_cq(cq, n, prp1, cqid, vector, qsize + 1,
        NVME_CQ_FLAGS_IEN(qflags));

    /*
     * It is only required to set qs_created when creating a completion queue;
     * creating a submission queue without a matching completion queue will
     * fail.
     */
    n->qs_created = true;
    return NVME_SUCCESS;
}

static uint16_t nvme_identify_ctrl(NvmeCtrl *n, NvmeRequest *req)
{
    trace_pci_nvme_identify_ctrl();

    return nvme_dma(n, (uint8_t *)&n->id_ctrl, sizeof(n->id_ctrl),
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_identify_ns(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeNamespace *ns;
    NvmeIdentify *c = (NvmeIdentify *)&req->cmd;
    uint32_t nsid = le32_to_cpu(c->nsid);

    trace_pci_nvme_identify_ns(nsid);

    if (unlikely(nsid == 0 || nsid > n->num_namespaces)) {
        trace_pci_nvme_err_invalid_ns(nsid, n->num_namespaces);
        return NVME_INVALID_NSID | NVME_DNR;
    }

    ns = &n->namespaces[nsid - 1];

    return nvme_dma(n, (uint8_t *)&ns->id_ns, sizeof(ns->id_ns),
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_identify_nslist(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeIdentify *c = (NvmeIdentify *)&req->cmd;
    static const int data_len = NVME_IDENTIFY_DATA_SIZE;
    uint32_t min_nsid = le32_to_cpu(c->nsid);
    uint32_t *list;
    uint16_t ret;
    int i, j = 0;

    trace_pci_nvme_identify_nslist(min_nsid);

    list = g_malloc0(data_len);
    for (i = 0; i < n->num_namespaces; i++) {
        if (i < min_nsid) {
            continue;
        }
        list[j++] = cpu_to_le32(i + 1);
        if (j == data_len / sizeof(uint32_t)) {
            break;
        }
    }
    ret = nvme_dma(n, (uint8_t *)list, data_len, DMA_DIRECTION_FROM_DEVICE,
                   req);
    g_free(list);
    return ret;
}

static uint16_t nvme_identify_ns_descr_list(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeIdentify *c = (NvmeIdentify *)&req->cmd;
    uint32_t nsid = le32_to_cpu(c->nsid);

    uint8_t list[NVME_IDENTIFY_DATA_SIZE];

    struct data {
        struct {
            NvmeIdNsDescr hdr;
            uint8_t v[16];
        } uuid;
    };

    struct data *ns_descrs = (struct data *)list;

    trace_pci_nvme_identify_ns_descr_list(nsid);

    if (unlikely(nsid == 0 || nsid > n->num_namespaces)) {
        trace_pci_nvme_err_invalid_ns(nsid, n->num_namespaces);
        return NVME_INVALID_NSID | NVME_DNR;
    }

    memset(list, 0x0, sizeof(list));

    /*
     * Because the NGUID and EUI64 fields are 0 in the Identify Namespace data
     * structure, a Namespace UUID (nidt = 0x3) must be reported in the
     * Namespace Identification Descriptor. Add a very basic Namespace UUID
     * here.
     */
    ns_descrs->uuid.hdr.nidt = NVME_NIDT_UUID;
    ns_descrs->uuid.hdr.nidl = NVME_NIDT_UUID_LEN;
    stl_be_p(&ns_descrs->uuid.v, nsid);

    return nvme_dma(n, list, NVME_IDENTIFY_DATA_SIZE,
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_identify(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeIdentify *c = (NvmeIdentify *)&req->cmd;

    switch (le32_to_cpu(c->cns)) {
    case NVME_ID_CNS_NS:
        return nvme_identify_ns(n, req);
    case NVME_ID_CNS_CTRL:
        return nvme_identify_ctrl(n, req);
    case NVME_ID_CNS_NS_ACTIVE_LIST:
        return nvme_identify_nslist(n, req);
    case NVME_ID_CNS_NS_DESCR_LIST:
        return nvme_identify_ns_descr_list(n, req);
    default:
        trace_pci_nvme_err_invalid_identify_cns(le32_to_cpu(c->cns));
        return NVME_INVALID_FIELD | NVME_DNR;
    }
}

static uint16_t nvme_abort(NvmeCtrl *n, NvmeRequest *req)
{
    uint16_t sqid = le32_to_cpu(req->cmd.cdw10) & 0xffff;

    req->cqe.result = 1;
    if (nvme_check_sqid(n, sqid)) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    return NVME_SUCCESS;
}

static inline void nvme_set_timestamp(NvmeCtrl *n, uint64_t ts)
{
    trace_pci_nvme_setfeat_timestamp(ts);

    n->host_timestamp = le64_to_cpu(ts);
    n->timestamp_set_qemu_clock_ms = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
}

static inline uint64_t nvme_get_timestamp(const NvmeCtrl *n)
{
    uint64_t current_time = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
    uint64_t elapsed_time = current_time - n->timestamp_set_qemu_clock_ms;

    union nvme_timestamp {
        struct {
            uint64_t timestamp:48;
            uint64_t sync:1;
            uint64_t origin:3;
            uint64_t rsvd1:12;
        };
        uint64_t all;
    };

    union nvme_timestamp ts;
    ts.all = 0;

    /*
     * If the sum of the Timestamp value set by the host and the elapsed
     * time exceeds 2^48, the value returned should be reduced modulo 2^48.
     */
    ts.timestamp = (n->host_timestamp + elapsed_time) & 0xffffffffffff;

    /* If the host timestamp is non-zero, set the timestamp origin */
    ts.origin = n->host_timestamp ? 0x01 : 0x00;

    trace_pci_nvme_getfeat_timestamp(ts.all);

    return cpu_to_le64(ts.all);
}

static uint16_t nvme_get_feature_timestamp(NvmeCtrl *n, NvmeRequest *req)
{
    uint64_t timestamp = nvme_get_timestamp(n);

    return nvme_dma(n, (uint8_t *)&timestamp, sizeof(timestamp),
                    DMA_DIRECTION_FROM_DEVICE, req);
}

static uint16_t nvme_get_feature(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeCmd *cmd = &req->cmd;
    uint32_t dw10 = le32_to_cpu(cmd->cdw10);
    uint32_t dw11 = le32_to_cpu(cmd->cdw11);
    uint32_t nsid = le32_to_cpu(cmd->nsid);
    uint32_t result;
    uint8_t fid = NVME_GETSETFEAT_FID(dw10);
    NvmeGetFeatureSelect sel = NVME_GETFEAT_SELECT(dw10);
    uint16_t iv;

    trace_pci_nvme_getfeat(nvme_cid(req), fid, sel, dw11);

    if (!nvme_feature_support[fid]) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    if (nvme_feature_cap[fid] & NVME_FEAT_CAP_NS) {
        if (!nsid || nsid > n->num_namespaces) {
            /*
             * The Reservation Notification Mask and Reservation Persistence
             * features require a status code of Invalid Field in Command when
             * NSID is 0xFFFFFFFF. Since the device does not support those
             * features we can always return Invalid Namespace or Format as we
             * should do for all other features.
             */
            return NVME_INVALID_NSID | NVME_DNR;
        }
    }

    switch (sel) {
    case NVME_GETFEAT_SELECT_CURRENT:
        break;
    case NVME_GETFEAT_SELECT_SAVED:
        /* no features are saveable by the controller; fallthrough */
    case NVME_GETFEAT_SELECT_DEFAULT:
        goto defaults;
    case NVME_GETFEAT_SELECT_CAP:
        result = cpu_to_le32(nvme_feature_cap[fid]);
        goto out;
    }

    switch (fid) {
    case NVME_TEMPERATURE_THRESHOLD:
        result = 0;

        /*
         * The controller only implements the Composite Temperature sensor, so
         * return 0 for all other sensors.
         */
        if (NVME_TEMP_TMPSEL(dw11) != NVME_TEMP_TMPSEL_COMPOSITE) {
            break;
        }

        switch (NVME_TEMP_THSEL(dw11)) {
        case NVME_TEMP_THSEL_OVER:
            result = cpu_to_le16(n->features.temp_thresh_hi);
            break;
        case NVME_TEMP_THSEL_UNDER:
            result = cpu_to_le16(n->features.temp_thresh_low);
            break;
        }

        break;
    case NVME_VOLATILE_WRITE_CACHE:
        result = cpu_to_le32(blk_enable_write_cache(n->conf.blk));
        trace_pci_nvme_getfeat_vwcache(result ? "enabled" : "disabled");
        break;
    case NVME_ASYNCHRONOUS_EVENT_CONF:
        result = cpu_to_le32(n->features.async_config);
        break;
    case NVME_TIMESTAMP:
        return nvme_get_feature_timestamp(n, req);
    default:
        break;
    }

defaults:
    switch (fid) {
    case NVME_TEMPERATURE_THRESHOLD:
        result = 0;

        if (NVME_TEMP_TMPSEL(dw11) != NVME_TEMP_TMPSEL_COMPOSITE) {
            break;
        }

        if (NVME_TEMP_THSEL(dw11) == NVME_TEMP_THSEL_OVER) {
            result = cpu_to_le16(NVME_TEMPERATURE_WARNING);
        }

        break;
    case NVME_NUMBER_OF_QUEUES:
        result = cpu_to_le32((n->params.max_ioqpairs - 1) |
                             ((n->params.max_ioqpairs - 1) << 16));
        trace_pci_nvme_getfeat_numq(result);
        break;
    case NVME_INTERRUPT_VECTOR_CONF:
        iv = dw11 & 0xffff;
        if (iv >= n->params.max_ioqpairs + 1) {
            return NVME_INVALID_FIELD | NVME_DNR;
        }

        result = iv;
        if (iv == n->admin_cq.vector) {
            result |= NVME_INTVC_NOCOALESCING;
        }

        result = cpu_to_le32(result);
        break;
    default:
        result = cpu_to_le32(nvme_feature_default[fid]);
        break;
    }

out:
    req->cqe.result = result;
    return NVME_SUCCESS;
}

static uint16_t nvme_set_feature_timestamp(NvmeCtrl *n, NvmeRequest *req)
{
    uint16_t ret;
    uint64_t timestamp;

    ret = nvme_dma(n, (uint8_t *)&timestamp, sizeof(timestamp),
                   DMA_DIRECTION_TO_DEVICE, req);
    if (ret != NVME_SUCCESS) {
        return ret;
    }

    nvme_set_timestamp(n, timestamp);

    return NVME_SUCCESS;
}

static uint16_t nvme_set_feature(NvmeCtrl *n, NvmeRequest *req)
{
    NvmeCmd *cmd = &req->cmd;
    uint32_t dw10 = le32_to_cpu(cmd->cdw10);
    uint32_t dw11 = le32_to_cpu(cmd->cdw11);
    uint32_t nsid = le32_to_cpu(cmd->nsid);
    uint8_t fid = NVME_GETSETFEAT_FID(dw10);
    uint8_t save = NVME_SETFEAT_SAVE(dw10);

    trace_pci_nvme_setfeat(nvme_cid(req), fid, save, dw11);

    if (save) {
        return NVME_FID_NOT_SAVEABLE | NVME_DNR;
    }

    if (!nvme_feature_support[fid]) {
        return NVME_INVALID_FIELD | NVME_DNR;
    }

    if (nvme_feature_cap[fid] & NVME_FEAT_CAP_NS) {
        if (!nsid || (nsid != NVME_NSID_BROADCAST &&
                      nsid > n->num_namespaces)) {
            return NVME_INVALID_NSID | NVME_DNR;
        }
    } else if (nsid && nsid != NVME_NSID_BROADCAST) {
        if (nsid > n->num_namespaces) {
            return NVME_INVALID_NSID | NVME_DNR;
        }

        return NVME_FEAT_NOT_NS_SPEC | NVME_DNR;
    }

    if (!(nvme_feature_cap[fid] & NVME_FEAT_CAP_CHANGE)) {
        return NVME_FEAT_NOT_CHANGABLE | NVME_DNR;
    }

    switch (fid) {
    case NVME_TEMPERATURE_THRESHOLD:
        if (NVME_TEMP_TMPSEL(dw11) != NVME_TEMP_TMPSEL_COMPOSITE) {
            break;
        }

        switch (NVME_TEMP_THSEL(dw11)) {
        case NVME_TEMP_THSEL_OVER:
            n->features.temp_thresh_hi = NVME_TEMP_TMPTH(dw11);
            break;
        case NVME_TEMP_THSEL_UNDER:
            n->features.temp_thresh_low = NVME_TEMP_TMPTH(dw11);
            break;
        default:
            return NVME_INVALID_FIELD | NVME_DNR;
        }

        if (((n->temperature >= n->features.temp_thresh_hi) ||
            (n->temperature <= n->features.temp_thresh_low)) &&
            NVME_AEC_SMART(n->features.async_config) & NVME_SMART_TEMPERATURE) {
            nvme_enqueue_event(n, NVME_AER_TYPE_SMART,
                               NVME_AER_INFO_SMART_TEMP_THRESH,
                               NVME_LOG_SMART_INFO);
        }

        break;
    case NVME_VOLATILE_WRITE_CACHE:
        if (!(dw11 & 0x1) && blk_enable_write_cache(n->conf.blk)) {
            blk_flush(n->conf.blk);
        }

        blk_set_enable_write_cache(n->conf.blk, dw11 & 1);
        break;
    case NVME_NUMBER_OF_QUEUES:
        if (n->qs_created) {
            return NVME_CMD_SEQ_ERROR | NVME_DNR;
        }

        /*
         * NVMe v1.3, Section 5.21.1.7: 0xffff is not an allowed value for NCQR
         * and NSQR.
         */
        if ((dw11 & 0xffff) == 0xffff || ((dw11 >> 16) & 0xffff) == 0xffff) {
            return NVME_INVALID_FIELD | NVME_DNR;
        }

        trace_pci_nvme_setfeat_numq((dw11 & 0xFFFF) + 1,
                                    ((dw11 >> 16) & 0xFFFF) + 1,
                                    n->params.max_ioqpairs,
                                    n->params.max_ioqpairs);
        req->cqe.result = cpu_to_le32((n->params.max_ioqpairs - 1) |
                                      ((n->params.max_ioqpairs - 1) << 16));
        break;
    case NVME_ASYNCHRONOUS_EVENT_CONF:
        n->features.async_config = dw11;
        break;
    case NVME_TIMESTAMP:
        return nvme_set_feature_timestamp(n, req);
    default:
        return NVME_FEAT_NOT_CHANGABLE | NVME_DNR;
    }
    return NVME_SUCCESS;
}

static uint16_t nvme_aer(NvmeCtrl *n, NvmeRequest *req)
{
    trace_pci_nvme_aer(nvme_cid(req));

    if (n->outstanding_aers > n->params.aerl) {
        trace_pci_nvme_aer_aerl_exceeded();
        return NVME_AER_LIMIT_EXCEEDED;
    }

    n->aer_reqs[n->outstanding_aers] = req;
    n->outstanding_aers++;

    if (!QTAILQ_EMPTY(&n->aer_queue)) {
        nvme_process_aers(n);
    }

    return NVME_NO_COMPLETE;
}

static uint16_t nvme_admin_cmd(NvmeCtrl *n, NvmeRequest *req)
{
    trace_pci_nvme_admin_cmd(nvme_cid(req), nvme_sqid(req), req->cmd.opcode);

    switch (req->cmd.opcode) {
    case NVME_ADM_CMD_DELETE_SQ:
        return nvme_del_sq(n, req);
    case NVME_ADM_CMD_CREATE_SQ:
        return nvme_create_sq(n, req);
    case NVME_ADM_CMD_GET_LOG_PAGE:
        return nvme_get_log(n, req);
    case NVME_ADM_CMD_DELETE_CQ:
        return nvme_del_cq(n, req);
    case NVME_ADM_CMD_CREATE_CQ:
        return nvme_create_cq(n, req);
    case NVME_ADM_CMD_IDENTIFY:
        return nvme_identify(n, req);
    case NVME_ADM_CMD_ABORT:
        return nvme_abort(n, req);
    case NVME_ADM_CMD_SET_FEATURES:
        return nvme_set_feature(n, req);
    case NVME_ADM_CMD_GET_FEATURES:
        return nvme_get_feature(n, req);
    case NVME_ADM_CMD_ASYNC_EV_REQ:
        return nvme_aer(n, req);
    default:
        trace_pci_nvme_err_invalid_admin_opc(req->cmd.opcode);
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

static void nvme_process_sq(void *opaque)
{
    NvmeSQueue *sq = opaque;
    NvmeCtrl *n = sq->ctrl;
    NvmeCQueue *cq = n->cq[sq->cqid];

    uint16_t status;
    hwaddr addr;
    NvmeCmd cmd;
    NvmeRequest *req;

    while (!(nvme_sq_empty(sq) || QTAILQ_EMPTY(&sq->req_list))) {
        addr = sq->dma_addr + sq->head * n->sqe_size;
        if (nvme_addr_read(n, addr, (void *)&cmd, sizeof(cmd))) {
            trace_pci_nvme_err_addr_read(addr);
            timer_mod(sq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                      500 * SCALE_MS);
            break;
        }
        nvme_inc_sq_head(sq);

        req = QTAILQ_FIRST(&sq->req_list);
        QTAILQ_REMOVE(&sq->req_list, req, entry);
        QTAILQ_INSERT_TAIL(&sq->out_req_list, req, entry);
        req->cqe.cid = cmd.cid;
        memcpy(&req->cmd, &cmd, sizeof(NvmeCmd));

        status = sq->sqid ? nvme_io_cmd(n, req) :
            nvme_admin_cmd(n, req);
        if (status != NVME_NO_COMPLETE) {
            req->status = status;
            nvme_enqueue_req_completion(cq, req);
        }
    }
}

static void nvme_clear_ctrl(NvmeCtrl *n)
{
    int i;

    blk_drain(n->conf.blk);

    for (i = 0; i < n->params.max_ioqpairs + 1; i++) {
        if (n->sq[i] != NULL) {
            nvme_free_sq(n->sq[i], n);
        }
    }
    for (i = 0; i < n->params.max_ioqpairs + 1; i++) {
        if (n->cq[i] != NULL) {
            nvme_free_cq(n->cq[i], n);
        }
    }

    while (!QTAILQ_EMPTY(&n->aer_queue)) {
        NvmeAsyncEvent *event = QTAILQ_FIRST(&n->aer_queue);
        QTAILQ_REMOVE(&n->aer_queue, event, entry);
        g_free(event);
    }

    n->aer_queued = 0;
    n->outstanding_aers = 0;
    n->qs_created = false;

    blk_flush(n->conf.blk);
    n->bar.cc = 0;
}

static int nvme_start_ctrl(NvmeCtrl *n)
{
    uint32_t page_bits = NVME_CC_MPS(n->bar.cc) + 12;
    uint32_t page_size = 1 << page_bits;

    if (unlikely(n->cq[0])) {
        trace_pci_nvme_err_startfail_cq();
        return -1;
    }
    if (unlikely(n->sq[0])) {
        trace_pci_nvme_err_startfail_sq();
        return -1;
    }
    if (unlikely(!n->bar.asq)) {
        trace_pci_nvme_err_startfail_nbarasq();
        return -1;
    }
    if (unlikely(!n->bar.acq)) {
        trace_pci_nvme_err_startfail_nbaracq();
        return -1;
    }
    if (unlikely(n->bar.asq & (page_size - 1))) {
        trace_pci_nvme_err_startfail_asq_misaligned(n->bar.asq);
        return -1;
    }
    if (unlikely(n->bar.acq & (page_size - 1))) {
        trace_pci_nvme_err_startfail_acq_misaligned(n->bar.acq);
        return -1;
    }
    if (unlikely(NVME_CC_MPS(n->bar.cc) <
                 NVME_CAP_MPSMIN(n->bar.cap))) {
        trace_pci_nvme_err_startfail_page_too_small(
                    NVME_CC_MPS(n->bar.cc),
                    NVME_CAP_MPSMIN(n->bar.cap));
        return -1;
    }
    if (unlikely(NVME_CC_MPS(n->bar.cc) >
                 NVME_CAP_MPSMAX(n->bar.cap))) {
        trace_pci_nvme_err_startfail_page_too_large(
                    NVME_CC_MPS(n->bar.cc),
                    NVME_CAP_MPSMAX(n->bar.cap));
        return -1;
    }
    if (unlikely(NVME_CC_IOCQES(n->bar.cc) <
                 NVME_CTRL_CQES_MIN(n->id_ctrl.cqes))) {
        trace_pci_nvme_err_startfail_cqent_too_small(
                    NVME_CC_IOCQES(n->bar.cc),
                    NVME_CTRL_CQES_MIN(n->bar.cap));
        return -1;
    }
    if (unlikely(NVME_CC_IOCQES(n->bar.cc) >
                 NVME_CTRL_CQES_MAX(n->id_ctrl.cqes))) {
        trace_pci_nvme_err_startfail_cqent_too_large(
                    NVME_CC_IOCQES(n->bar.cc),
                    NVME_CTRL_CQES_MAX(n->bar.cap));
        return -1;
    }
    if (unlikely(NVME_CC_IOSQES(n->bar.cc) <
                 NVME_CTRL_SQES_MIN(n->id_ctrl.sqes))) {
        trace_pci_nvme_err_startfail_sqent_too_small(
                    NVME_CC_IOSQES(n->bar.cc),
                    NVME_CTRL_SQES_MIN(n->bar.cap));
        return -1;
    }
    if (unlikely(NVME_CC_IOSQES(n->bar.cc) >
                 NVME_CTRL_SQES_MAX(n->id_ctrl.sqes))) {
        trace_pci_nvme_err_startfail_sqent_too_large(
                    NVME_CC_IOSQES(n->bar.cc),
                    NVME_CTRL_SQES_MAX(n->bar.cap));
        return -1;
    }
    if (unlikely(!NVME_AQA_ASQS(n->bar.aqa))) {
        trace_pci_nvme_err_startfail_asqent_sz_zero();
        return -1;
    }
    if (unlikely(!NVME_AQA_ACQS(n->bar.aqa))) {
        trace_pci_nvme_err_startfail_acqent_sz_zero();
        return -1;
    }

    n->page_bits = page_bits;
    n->page_size = page_size;
    n->max_prp_ents = n->page_size / sizeof(uint64_t);
    n->cqe_size = 1 << NVME_CC_IOCQES(n->bar.cc);
    n->sqe_size = 1 << NVME_CC_IOSQES(n->bar.cc);
    nvme_init_cq(&n->admin_cq, n, n->bar.acq, 0, 0,
        NVME_AQA_ACQS(n->bar.aqa) + 1, 1);
    nvme_init_sq(&n->admin_sq, n, n->bar.asq, 0, 0,
        NVME_AQA_ASQS(n->bar.aqa) + 1);

    nvme_set_timestamp(n, 0ULL);

    QTAILQ_INIT(&n->aer_queue);

    return 0;
}

static void nvme_write_bar(NvmeCtrl *n, hwaddr offset, uint64_t data,
    unsigned size)
{
    if (unlikely(offset & (sizeof(uint32_t) - 1))) {
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_misaligned32,
                       "MMIO write not 32-bit aligned,"
                       " offset=0x%"PRIx64"", offset);
        /* should be ignored, fall through for now */
    }

    if (unlikely(size < sizeof(uint32_t))) {
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_toosmall,
                       "MMIO write smaller than 32-bits,"
                       " offset=0x%"PRIx64", size=%u",
                       offset, size);
        /* should be ignored, fall through for now */
    }

    switch (offset) {
    case 0xc:   /* INTMS */
        if (unlikely(msix_enabled(&(n->parent_obj)))) {
            NVME_GUEST_ERR(pci_nvme_ub_mmiowr_intmask_with_msix,
                           "undefined access to interrupt mask set"
                           " when MSI-X is enabled");
            /* should be ignored, fall through for now */
        }
        n->bar.intms |= data & 0xffffffff;
        n->bar.intmc = n->bar.intms;
        trace_pci_nvme_mmio_intm_set(data & 0xffffffff, n->bar.intmc);
        nvme_irq_check(n);
        break;
    case 0x10:  /* INTMC */
        if (unlikely(msix_enabled(&(n->parent_obj)))) {
            NVME_GUEST_ERR(pci_nvme_ub_mmiowr_intmask_with_msix,
                           "undefined access to interrupt mask clr"
                           " when MSI-X is enabled");
            /* should be ignored, fall through for now */
        }
        n->bar.intms &= ~(data & 0xffffffff);
        n->bar.intmc = n->bar.intms;
        trace_pci_nvme_mmio_intm_clr(data & 0xffffffff, n->bar.intmc);
        nvme_irq_check(n);
        break;
    case 0x14:  /* CC */
        trace_pci_nvme_mmio_cfg(data & 0xffffffff);
        /* Windows first sends data, then sends enable bit */
        if (!NVME_CC_EN(data) && !NVME_CC_EN(n->bar.cc) &&
            !NVME_CC_SHN(data) && !NVME_CC_SHN(n->bar.cc))
        {
            n->bar.cc = data;
        }

        if (NVME_CC_EN(data) && !NVME_CC_EN(n->bar.cc)) {
            n->bar.cc = data;
            if (unlikely(nvme_start_ctrl(n))) {
                trace_pci_nvme_err_startfail();
                n->bar.csts = NVME_CSTS_FAILED;
            } else {
                trace_pci_nvme_mmio_start_success();
                n->bar.csts = NVME_CSTS_READY;
            }
        } else if (!NVME_CC_EN(data) && NVME_CC_EN(n->bar.cc)) {
            trace_pci_nvme_mmio_stopped();
            nvme_clear_ctrl(n);
            n->bar.csts &= ~NVME_CSTS_READY;
        }
        if (NVME_CC_SHN(data) && !(NVME_CC_SHN(n->bar.cc))) {
            trace_pci_nvme_mmio_shutdown_set();
            nvme_clear_ctrl(n);
            n->bar.cc = data;
            n->bar.csts |= NVME_CSTS_SHST_COMPLETE;
        } else if (!NVME_CC_SHN(data) && NVME_CC_SHN(n->bar.cc)) {
            trace_pci_nvme_mmio_shutdown_cleared();
            n->bar.csts &= ~NVME_CSTS_SHST_COMPLETE;
            n->bar.cc = data;
        }
        break;
    case 0x1C:  /* CSTS */
        if (data & (1 << 4)) {
            NVME_GUEST_ERR(pci_nvme_ub_mmiowr_ssreset_w1c_unsupported,
                           "attempted to W1C CSTS.NSSRO"
                           " but CAP.NSSRS is zero (not supported)");
        } else if (data != 0) {
            NVME_GUEST_ERR(pci_nvme_ub_mmiowr_ro_csts,
                           "attempted to set a read only bit"
                           " of controller status");
        }
        break;
    case 0x20:  /* NSSR */
        if (data == 0x4E564D65) {
            trace_pci_nvme_ub_mmiowr_ssreset_unsupported();
        } else {
            /* The spec says that writes of other values have no effect */
            return;
        }
        break;
    case 0x24:  /* AQA */
        n->bar.aqa = data & 0xffffffff;
        trace_pci_nvme_mmio_aqattr(data & 0xffffffff);
        break;
    case 0x28:  /* ASQ */
        n->bar.asq = data;
        trace_pci_nvme_mmio_asqaddr(data);
        break;
    case 0x2c:  /* ASQ hi */
        n->bar.asq |= data << 32;
        trace_pci_nvme_mmio_asqaddr_hi(data, n->bar.asq);
        break;
    case 0x30:  /* ACQ */
        trace_pci_nvme_mmio_acqaddr(data);
        n->bar.acq = data;
        break;
    case 0x34:  /* ACQ hi */
        n->bar.acq |= data << 32;
        trace_pci_nvme_mmio_acqaddr_hi(data, n->bar.acq);
        break;
    case 0x38:  /* CMBLOC */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_cmbloc_reserved,
                       "invalid write to reserved CMBLOC"
                       " when CMBSZ is zero, ignored");
        return;
    case 0x3C:  /* CMBSZ */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_cmbsz_readonly,
                       "invalid write to read only CMBSZ, ignored");
        return;
    case 0xE00: /* PMRCAP */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_pmrcap_readonly,
                       "invalid write to PMRCAP register, ignored");
        return;
    case 0xE04: /* TODO PMRCTL */
        break;
    case 0xE08: /* PMRSTS */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_pmrsts_readonly,
                       "invalid write to PMRSTS register, ignored");
        return;
    case 0xE0C: /* PMREBS */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_pmrebs_readonly,
                       "invalid write to PMREBS register, ignored");
        return;
    case 0xE10: /* PMRSWTP */
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_pmrswtp_readonly,
                       "invalid write to PMRSWTP register, ignored");
        return;
    case 0xE14: /* TODO PMRMSC */
         break;
    default:
        NVME_GUEST_ERR(pci_nvme_ub_mmiowr_invalid,
                       "invalid MMIO write,"
                       " offset=0x%"PRIx64", data=%"PRIx64"",
                       offset, data);
        break;
    }
}

static uint64_t nvme_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    NvmeCtrl *n = (NvmeCtrl *)opaque;
    uint8_t *ptr = (uint8_t *)&n->bar;
    uint64_t val = 0;

    trace_pci_nvme_mmio_read(addr);

    if (unlikely(addr & (sizeof(uint32_t) - 1))) {
        NVME_GUEST_ERR(pci_nvme_ub_mmiord_misaligned32,
                       "MMIO read not 32-bit aligned,"
                       " offset=0x%"PRIx64"", addr);
        /* should RAZ, fall through for now */
    } else if (unlikely(size < sizeof(uint32_t))) {
        NVME_GUEST_ERR(pci_nvme_ub_mmiord_toosmall,
                       "MMIO read smaller than 32-bits,"
                       " offset=0x%"PRIx64"", addr);
        /* should RAZ, fall through for now */
    }

    if (addr < sizeof(n->bar)) {
        /*
         * When PMRWBM bit 1 is set then read from
         * from PMRSTS should ensure prior writes
         * made it to persistent media
         */
        if (addr == 0xE08 &&
            (NVME_PMRCAP_PMRWBM(n->bar.pmrcap) & 0x02)) {
            memory_region_msync(&n->pmrdev->mr, 0, n->pmrdev->size);
        }
        memcpy(&val, ptr + addr, size);
    } else {
        NVME_GUEST_ERR(pci_nvme_ub_mmiord_invalid_ofs,
                       "MMIO read beyond last register,"
                       " offset=0x%"PRIx64", returning 0", addr);
    }

    return val;
}

static void nvme_process_db(NvmeCtrl *n, hwaddr addr, int val)
{
    uint32_t qid;

    if (unlikely(addr & ((1 << 2) - 1))) {
        NVME_GUEST_ERR(pci_nvme_ub_db_wr_misaligned,
                       "doorbell write not 32-bit aligned,"
                       " offset=0x%"PRIx64", ignoring", addr);
        return;
    }

    if (((addr - 0x1000) >> 2) & 1) {
        /* Completion queue doorbell write */

        uint16_t new_head = val & 0xffff;
        int start_sqs;
        NvmeCQueue *cq;

        qid = (addr - (0x1000 + (1 << 2))) >> 3;
        if (unlikely(nvme_check_cqid(n, qid))) {
            NVME_GUEST_ERR(pci_nvme_ub_db_wr_invalid_cq,
                           "completion queue doorbell write"
                           " for nonexistent queue,"
                           " sqid=%"PRIu32", ignoring", qid);

            if (n->outstanding_aers) {
                nvme_enqueue_event(n, NVME_AER_TYPE_ERROR,
                                   NVME_AER_INFO_ERR_INVALID_DB_REGISTER,
                                   NVME_LOG_ERROR_INFO);
            }

            return;
        }

        cq = n->cq[qid];
        if (unlikely(new_head >= cq->size)) {
            NVME_GUEST_ERR(pci_nvme_ub_db_wr_invalid_cqhead,
                           "completion queue doorbell write value"
                           " beyond queue size, sqid=%"PRIu32","
                           " new_head=%"PRIu16", ignoring",
                           qid, new_head);

            if (n->outstanding_aers) {
                nvme_enqueue_event(n, NVME_AER_TYPE_ERROR,
                                   NVME_AER_INFO_ERR_INVALID_DB_VALUE,
                                   NVME_LOG_ERROR_INFO);
            }

            return;
        }

        trace_pci_nvme_mmio_doorbell_cq(cq->cqid, new_head);

        start_sqs = nvme_cq_full(cq) ? 1 : 0;
        cq->head = new_head;
        if (start_sqs) {
            NvmeSQueue *sq;
            QTAILQ_FOREACH(sq, &cq->sq_list, entry) {
                timer_mod(sq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 500);
            }
            timer_mod(cq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 500);
        }

        if (cq->tail == cq->head) {
            nvme_irq_deassert(n, cq);
        }
    } else {
        /* Submission queue doorbell write */

        uint16_t new_tail = val & 0xffff;
        NvmeSQueue *sq;

        qid = (addr - 0x1000) >> 3;
        if (unlikely(nvme_check_sqid(n, qid))) {
            NVME_GUEST_ERR(pci_nvme_ub_db_wr_invalid_sq,
                           "submission queue doorbell write"
                           " for nonexistent queue,"
                           " sqid=%"PRIu32", ignoring", qid);

            if (n->outstanding_aers) {
                nvme_enqueue_event(n, NVME_AER_TYPE_ERROR,
                                   NVME_AER_INFO_ERR_INVALID_DB_REGISTER,
                                   NVME_LOG_ERROR_INFO);
            }

            return;
        }

        sq = n->sq[qid];
        if (unlikely(new_tail >= sq->size)) {
            NVME_GUEST_ERR(pci_nvme_ub_db_wr_invalid_sqtail,
                           "submission queue doorbell write value"
                           " beyond queue size, sqid=%"PRIu32","
                           " new_tail=%"PRIu16", ignoring",
                           qid, new_tail);

            if (n->outstanding_aers) {
                nvme_enqueue_event(n, NVME_AER_TYPE_ERROR,
                                   NVME_AER_INFO_ERR_INVALID_DB_VALUE,
                                   NVME_LOG_ERROR_INFO);
            }

            return;
        }

        trace_pci_nvme_mmio_doorbell_sq(sq->sqid, new_tail);

        sq->tail = new_tail;
        timer_mod(sq->timer, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + 500);
    }
}

static void nvme_mmio_write(void *opaque, hwaddr addr, uint64_t data,
    unsigned size)
{
    NvmeCtrl *n = (NvmeCtrl *)opaque;

    trace_pci_nvme_mmio_write(addr, data);

    if (addr < sizeof(n->bar)) {
        nvme_write_bar(n, addr, data, size);
    } else if (addr >= 0x1000) {
        nvme_process_db(n, addr, data);
    }
}

static const MemoryRegionOps nvme_mmio_ops = {
    .read = nvme_mmio_read,
    .write = nvme_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 2,
        .max_access_size = 8,
    },
};

static void nvme_cmb_write(void *opaque, hwaddr addr, uint64_t data,
    unsigned size)
{
    NvmeCtrl *n = (NvmeCtrl *)opaque;
    stn_le_p(&n->cmbuf[addr], size, data);
}

static uint64_t nvme_cmb_read(void *opaque, hwaddr addr, unsigned size)
{
    NvmeCtrl *n = (NvmeCtrl *)opaque;
    return ldn_le_p(&n->cmbuf[addr], size);
}

static const MemoryRegionOps nvme_cmb_ops = {
    .read = nvme_cmb_read,
    .write = nvme_cmb_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static void nvme_check_constraints(NvmeCtrl *n, Error **errp)
{
    NvmeParams *params = &n->params;

    if (params->num_queues) {
        warn_report("num_queues is deprecated; please use max_ioqpairs "
                    "instead");

        params->max_ioqpairs = params->num_queues - 1;
    }

    if (params->max_ioqpairs < 1 ||
        params->max_ioqpairs > NVME_MAX_IOQPAIRS) {
        error_setg(errp, "max_ioqpairs must be between 1 and %d",
                   NVME_MAX_IOQPAIRS);
        return;
    }

    if (params->msix_qsize < 1 ||
        params->msix_qsize > PCI_MSIX_FLAGS_QSIZE + 1) {
        error_setg(errp, "msix_qsize must be between 1 and %d",
                   PCI_MSIX_FLAGS_QSIZE + 1);
        return;
    }

    if (!n->conf.blk) {
        error_setg(errp, "drive property not set");
        return;
    }

    if (!params->serial) {
        error_setg(errp, "serial property not set");
        return;
    }

    if (!n->params.cmb_size_mb && n->pmrdev) {
        if (host_memory_backend_is_mapped(n->pmrdev)) {
            char *path = object_get_canonical_path_component(OBJECT(n->pmrdev));
            error_setg(errp, "can't use already busy memdev: %s", path);
            g_free(path);
            return;
        }

        if (!is_power_of_2(n->pmrdev->size)) {
            error_setg(errp, "pmr backend size needs to be power of 2 in size");
            return;
        }

        host_memory_backend_set_mapped(n->pmrdev, true);
    }
}

static void nvme_init_state(NvmeCtrl *n)
{
    n->num_namespaces = 1;
    /* add one to max_ioqpairs to account for the admin queue pair */
    n->reg_size = pow2ceil(NVME_REG_SIZE +
                           2 * (n->params.max_ioqpairs + 1) * NVME_DB_SIZE);
    n->namespaces = g_new0(NvmeNamespace, n->num_namespaces);
    n->sq = g_new0(NvmeSQueue *, n->params.max_ioqpairs + 1);
    n->cq = g_new0(NvmeCQueue *, n->params.max_ioqpairs + 1);
    n->temperature = NVME_TEMPERATURE;
    n->features.temp_thresh_hi = NVME_TEMPERATURE_WARNING;
    n->starttime_ms = qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL);
    n->aer_reqs = g_new0(NvmeRequest *, n->params.aerl + 1);
}

static void nvme_init_blk(NvmeCtrl *n, Error **errp)
{
    if (!blkconf_blocksizes(&n->conf, errp)) {
        return;
    }
    blkconf_apply_backend_options(&n->conf, blk_is_read_only(n->conf.blk),
                                  false, errp);
}

static void nvme_init_namespace(NvmeCtrl *n, NvmeNamespace *ns, Error **errp)
{
    int64_t bs_size;
    NvmeIdNs *id_ns = &ns->id_ns;

    bs_size = blk_getlength(n->conf.blk);
    if (bs_size < 0) {
        error_setg_errno(errp, -bs_size, "could not get backing file size");
        return;
    }

    n->ns_size = bs_size;

    id_ns->lbaf[0].ds = BDRV_SECTOR_BITS;
    id_ns->nsze = cpu_to_le64(nvme_ns_nlbas(n, ns));

    /* no thin provisioning */
    id_ns->ncap = id_ns->nsze;
    id_ns->nuse = id_ns->ncap;
}

static void nvme_init_cmb(NvmeCtrl *n, PCIDevice *pci_dev)
{
    NVME_CMBLOC_SET_BIR(n->bar.cmbloc, NVME_CMB_BIR);
    NVME_CMBLOC_SET_OFST(n->bar.cmbloc, 0);

    NVME_CMBSZ_SET_SQS(n->bar.cmbsz, 1);
    NVME_CMBSZ_SET_CQS(n->bar.cmbsz, 0);
    NVME_CMBSZ_SET_LISTS(n->bar.cmbsz, 1);
    NVME_CMBSZ_SET_RDS(n->bar.cmbsz, 1);
    NVME_CMBSZ_SET_WDS(n->bar.cmbsz, 1);
    NVME_CMBSZ_SET_SZU(n->bar.cmbsz, 2); /* MBs */
    NVME_CMBSZ_SET_SZ(n->bar.cmbsz, n->params.cmb_size_mb);

    n->cmbuf = g_malloc0(NVME_CMBSZ_GETSIZE(n->bar.cmbsz));
    memory_region_init_io(&n->ctrl_mem, OBJECT(n), &nvme_cmb_ops, n,
                          "nvme-cmb", NVME_CMBSZ_GETSIZE(n->bar.cmbsz));
    pci_register_bar(pci_dev, NVME_CMBLOC_BIR(n->bar.cmbloc),
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64 |
                     PCI_BASE_ADDRESS_MEM_PREFETCH, &n->ctrl_mem);
}

static void nvme_init_pmr(NvmeCtrl *n, PCIDevice *pci_dev)
{
    /* Controller Capabilities register */
    NVME_CAP_SET_PMRS(n->bar.cap, 1);

    /* PMR Capabities register */
    n->bar.pmrcap = 0;
    NVME_PMRCAP_SET_RDS(n->bar.pmrcap, 0);
    NVME_PMRCAP_SET_WDS(n->bar.pmrcap, 0);
    NVME_PMRCAP_SET_BIR(n->bar.pmrcap, NVME_PMR_BIR);
    NVME_PMRCAP_SET_PMRTU(n->bar.pmrcap, 0);
    /* Turn on bit 1 support */
    NVME_PMRCAP_SET_PMRWBM(n->bar.pmrcap, 0x02);
    NVME_PMRCAP_SET_PMRTO(n->bar.pmrcap, 0);
    NVME_PMRCAP_SET_CMSS(n->bar.pmrcap, 0);

    /* PMR Control register */
    n->bar.pmrctl = 0;
    NVME_PMRCTL_SET_EN(n->bar.pmrctl, 0);

    /* PMR Status register */
    n->bar.pmrsts = 0;
    NVME_PMRSTS_SET_ERR(n->bar.pmrsts, 0);
    NVME_PMRSTS_SET_NRDY(n->bar.pmrsts, 0);
    NVME_PMRSTS_SET_HSTS(n->bar.pmrsts, 0);
    NVME_PMRSTS_SET_CBAI(n->bar.pmrsts, 0);

    /* PMR Elasticity Buffer Size register */
    n->bar.pmrebs = 0;
    NVME_PMREBS_SET_PMRSZU(n->bar.pmrebs, 0);
    NVME_PMREBS_SET_RBB(n->bar.pmrebs, 0);
    NVME_PMREBS_SET_PMRWBZ(n->bar.pmrebs, 0);

    /* PMR Sustained Write Throughput register */
    n->bar.pmrswtp = 0;
    NVME_PMRSWTP_SET_PMRSWTU(n->bar.pmrswtp, 0);
    NVME_PMRSWTP_SET_PMRSWTV(n->bar.pmrswtp, 0);

    /* PMR Memory Space Control register */
    n->bar.pmrmsc = 0;
    NVME_PMRMSC_SET_CMSE(n->bar.pmrmsc, 0);
    NVME_PMRMSC_SET_CBA(n->bar.pmrmsc, 0);

    pci_register_bar(pci_dev, NVME_PMRCAP_BIR(n->bar.pmrcap),
                     PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64 |
                     PCI_BASE_ADDRESS_MEM_PREFETCH, &n->pmrdev->mr);
}

static void nvme_init_pci(NvmeCtrl *n, PCIDevice *pci_dev, Error **errp)
{
    uint8_t *pci_conf = pci_dev->config;

    pci_conf[PCI_INTERRUPT_PIN] = 1;
    pci_config_set_prog_interface(pci_conf, 0x2);
    pci_config_set_class(pci_conf, PCI_CLASS_STORAGE_EXPRESS);
    pcie_endpoint_cap_init(pci_dev, 0x80);

    memory_region_init_io(&n->iomem, OBJECT(n), &nvme_mmio_ops, n, "nvme",
                          n->reg_size);
    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY |
                     PCI_BASE_ADDRESS_MEM_TYPE_64, &n->iomem);
    if (msix_init_exclusive_bar(pci_dev, n->params.msix_qsize, 4, errp)) {
        return;
    }

    if (n->params.cmb_size_mb) {
        nvme_init_cmb(n, pci_dev);
    } else if (n->pmrdev) {
        nvme_init_pmr(n, pci_dev);
    }
}

static void nvme_init_ctrl(NvmeCtrl *n, PCIDevice *pci_dev)
{
    NvmeIdCtrl *id = &n->id_ctrl;
    uint8_t *pci_conf = pci_dev->config;

    id->vid = cpu_to_le16(pci_get_word(pci_conf + PCI_VENDOR_ID));
    id->ssvid = cpu_to_le16(pci_get_word(pci_conf + PCI_SUBSYSTEM_VENDOR_ID));
    strpadcpy((char *)id->mn, sizeof(id->mn), "QEMU NVMe Ctrl", ' ');
    strpadcpy((char *)id->fr, sizeof(id->fr), "1.0", ' ');
    strpadcpy((char *)id->sn, sizeof(id->sn), n->params.serial, ' ');
    id->rab = 6;
    id->ieee[0] = 0x00;
    id->ieee[1] = 0x02;
    id->ieee[2] = 0xb3;
    id->mdts = n->params.mdts;
    id->ver = cpu_to_le32(NVME_SPEC_VER);
    id->oacs = cpu_to_le16(0);

    /*
     * Because the controller always completes the Abort command immediately,
     * there can never be more than one concurrently executing Abort command,
     * so this value is never used for anything. Note that there can easily be
     * many Abort commands in the queues, but they are not considered
     * "executing" until processed by nvme_abort.
     *
     * The specification recommends a value of 3 for Abort Command Limit (four
     * concurrently outstanding Abort commands), so lets use that though it is
     * inconsequential.
     */
    id->acl = 3;
    id->aerl = n->params.aerl;
    id->frmw = (NVME_NUM_FW_SLOTS << 1) | NVME_FRMW_SLOT1_RO;
    id->lpa = NVME_LPA_EXTENDED;

    /* recommended default value (~70 C) */
    id->wctemp = cpu_to_le16(NVME_TEMPERATURE_WARNING);
    id->cctemp = cpu_to_le16(NVME_TEMPERATURE_CRITICAL);

    id->sqes = (0x6 << 4) | 0x6;
    id->cqes = (0x4 << 4) | 0x4;
    id->nn = cpu_to_le32(n->num_namespaces);
    id->oncs = cpu_to_le16(NVME_ONCS_WRITE_ZEROES | NVME_ONCS_TIMESTAMP |
                           NVME_ONCS_FEATURES);

    id->sgls = cpu_to_le32(NVME_CTRL_SGLS_SUPPORTED_NO_ALIGNMENT |
                           NVME_CTRL_SGLS_BITBUCKET);

    pstrcpy((char *) id->subnqn, sizeof(id->subnqn), "nqn.2019-08.org.qemu:");
    pstrcat((char *) id->subnqn, sizeof(id->subnqn), n->params.serial);

    id->psd[0].mp = cpu_to_le16(0x9c4);
    id->psd[0].enlat = cpu_to_le32(0x10);
    id->psd[0].exlat = cpu_to_le32(0x4);
    if (blk_enable_write_cache(n->conf.blk)) {
        id->vwc = 1;
    }

    n->bar.cap = 0;
    NVME_CAP_SET_MQES(n->bar.cap, 0x7ff);
    NVME_CAP_SET_CQR(n->bar.cap, 1);
    NVME_CAP_SET_TO(n->bar.cap, 0xf);
    NVME_CAP_SET_CSS(n->bar.cap, 1);
    NVME_CAP_SET_MPSMAX(n->bar.cap, 4);

    n->bar.vs = NVME_SPEC_VER;
    n->bar.intmc = n->bar.intms = 0;
}

static void nvme_realize(PCIDevice *pci_dev, Error **errp)
{
    NvmeCtrl *n = NVME(pci_dev);
    Error *local_err = NULL;

    int i;

    nvme_check_constraints(n, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    nvme_init_state(n);
    nvme_init_blk(n, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    nvme_init_pci(n, pci_dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    nvme_init_ctrl(n, pci_dev);

    for (i = 0; i < n->num_namespaces; i++) {
        nvme_init_namespace(n, &n->namespaces[i], &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            return;
        }
    }
}

static void nvme_exit(PCIDevice *pci_dev)
{
    NvmeCtrl *n = NVME(pci_dev);

    nvme_clear_ctrl(n);
    g_free(n->namespaces);
    g_free(n->cq);
    g_free(n->sq);
    g_free(n->aer_reqs);

    if (n->params.cmb_size_mb) {
        g_free(n->cmbuf);
    }

    if (n->pmrdev) {
        host_memory_backend_set_mapped(n->pmrdev, false);
    }
    msix_uninit_exclusive_bar(pci_dev);
}

static Property nvme_props[] = {
    DEFINE_BLOCK_PROPERTIES(NvmeCtrl, conf),
    DEFINE_PROP_LINK("pmrdev", NvmeCtrl, pmrdev, TYPE_MEMORY_BACKEND,
                     HostMemoryBackend *),
    DEFINE_PROP_STRING("serial", NvmeCtrl, params.serial),
    DEFINE_PROP_UINT32("cmb_size_mb", NvmeCtrl, params.cmb_size_mb, 0),
    DEFINE_PROP_UINT32("num_queues", NvmeCtrl, params.num_queues, 0),
    DEFINE_PROP_UINT32("max_ioqpairs", NvmeCtrl, params.max_ioqpairs, 64),
    DEFINE_PROP_UINT16("msix_qsize", NvmeCtrl, params.msix_qsize, 65),
    DEFINE_PROP_UINT8("aerl", NvmeCtrl, params.aerl, 3),
    DEFINE_PROP_UINT32("aer_max_queued", NvmeCtrl, params.aer_max_queued, 64),
    DEFINE_PROP_UINT8("mdts", NvmeCtrl, params.mdts, 7),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription nvme_vmstate = {
    .name = "nvme",
    .unmigratable = 1,
};

static void nvme_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    PCIDeviceClass *pc = PCI_DEVICE_CLASS(oc);

    pc->realize = nvme_realize;
    pc->exit = nvme_exit;
    pc->class_id = PCI_CLASS_STORAGE_EXPRESS;
    pc->vendor_id = PCI_VENDOR_ID_INTEL;
    pc->device_id = 0x5845;
    pc->revision = 2;

    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
    dc->desc = "Non-Volatile Memory Express";
    device_class_set_props(dc, nvme_props);
    dc->vmsd = &nvme_vmstate;
}

static void nvme_instance_init(Object *obj)
{
    NvmeCtrl *s = NVME(obj);

    device_add_bootindex_property(obj, &s->conf.bootindex,
                                  "bootindex", "/namespace@1,0",
                                  DEVICE(obj));
}

static const TypeInfo nvme_info = {
    .name          = TYPE_NVME,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(NvmeCtrl),
    .class_init    = nvme_class_init,
    .instance_init = nvme_instance_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { }
    },
};

static void nvme_register_types(void)
{
    type_register_static(&nvme_info);
}

type_init(nvme_register_types)
