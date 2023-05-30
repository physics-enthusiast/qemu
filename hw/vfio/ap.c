/*
 * VFIO based AP matrix device assignment
 *
 * Copyright 2018 IBM Corp.
 * Author(s): Tony Krowiak <akrowiak@linux.ibm.com>
 *            Halil Pasic <pasic@linux.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */

#include "qemu/osdep.h"
#include <linux/vfio.h>
#include <sys/ioctl.h>
#include "qapi/error.h"
#include "hw/vfio/vfio.h"
#include "hw/vfio/vfio-common.h"
#include "hw/s390x/ap-device.h"
#include "qemu/error-report.h"
#include "qemu/event_notifier.h"
#include "qemu/main-loop.h"
#include "qemu/module.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "kvm/kvm_s390x.h"
#include "migration/vmstate.h"
#include "hw/qdev-properties.h"
#include "hw/s390x/ap-bridge.h"
#include "exec/address-spaces.h"
#include "qom/object.h"

#define TYPE_VFIO_AP_DEVICE      "vfio-ap"

struct VFIOAPDevice {
    APDevice apdev;
    VFIODevice vdev;
    EventNotifier req_notifier;
};

OBJECT_DECLARE_SIMPLE_TYPE(VFIOAPDevice, VFIO_AP_DEVICE)

static void vfio_ap_compute_needs_reset(VFIODevice *vdev)
{
    vdev->needs_reset = false;
}

/*
 * We don't need vfio_hot_reset_multi and vfio_eoi operations for
 * vfio-ap device now.
 */
struct VFIODeviceOps vfio_ap_ops = {
    .vfio_compute_needs_reset = vfio_ap_compute_needs_reset,
};

static void vfio_ap_put_device(VFIOAPDevice *vapdev)
{
    g_free(vapdev->vdev.name);
    vfio_put_base_device(&vapdev->vdev);
}

static VFIOGroup *vfio_ap_get_group(VFIOAPDevice *vapdev, Error **errp)
{
    GError *gerror = NULL;
    char *symlink, *group_path;
    int groupid;

    symlink = g_strdup_printf("%s/iommu_group", vapdev->vdev.sysfsdev);
    group_path = g_file_read_link(symlink, &gerror);
    g_free(symlink);

    if (!group_path) {
        error_setg(errp, "%s: no iommu_group found for %s: %s",
                   TYPE_VFIO_AP_DEVICE, vapdev->vdev.sysfsdev, gerror->message);
        g_error_free(gerror);
        return NULL;
    }

    if (sscanf(basename(group_path), "%d", &groupid) != 1) {
        error_setg(errp, "vfio: failed to read %s", group_path);
        g_free(group_path);
        return NULL;
    }

    g_free(group_path);

    return vfio_get_group(groupid, &address_space_memory, errp);
}

static void vfio_ap_req_notifier_handler(void *opaque)
{
    VFIOAPDevice *vapdev = opaque;
    Error *err = NULL;

    if (!event_notifier_test_and_clear(&vapdev->req_notifier)) {
        return;
    }

    qdev_unplug(DEVICE(vapdev), &err);

    if (err) {
        warn_reportf_err(err, VFIO_MSG_PREFIX, vapdev->vdev.name);
    }
}

static void vfio_ap_register_irq_notifier(VFIOAPDevice *vapdev,
                                          unsigned int irq, Error **errp)
{
    int fd;
    size_t argsz;
    IOHandler *fd_read;
    EventNotifier *notifier;
    struct vfio_irq_info *irq_info;
    VFIODevice *vdev = &vapdev->vdev;

    switch (irq) {
    case VFIO_AP_REQ_IRQ_INDEX:
        notifier = &vapdev->req_notifier;
        fd_read = vfio_ap_req_notifier_handler;
        break;
    default:
        error_setg(errp, "vfio: Unsupported device irq(%d)", irq);
        return;
    }

    if (vdev->num_irqs < irq + 1) {
        error_setg(errp, "vfio: IRQ %u not available (number of irqs %u)",
                   irq, vdev->num_irqs);
        return;
    }

    argsz = sizeof(*irq_info);
    irq_info = g_malloc0(argsz);
    irq_info->index = irq;
    irq_info->argsz = argsz;

    if (ioctl(vdev->fd, VFIO_DEVICE_GET_IRQ_INFO,
              irq_info) < 0 || irq_info->count < 1) {
        error_setg_errno(errp, errno, "vfio: Error getting irq info");
        goto out_free_info;
    }

    if (event_notifier_init(notifier, 0)) {
        error_setg_errno(errp, errno,
                         "vfio: Unable to init event notifier for irq (%d)",
                         irq);
        goto out_free_info;
    }

    fd = event_notifier_get_fd(notifier);
    qemu_set_fd_handler(fd, fd_read, NULL, vapdev);

    if (vfio_set_irq_signaling(vdev, irq, 0, VFIO_IRQ_SET_ACTION_TRIGGER, fd,
                               errp)) {
        qemu_set_fd_handler(fd, NULL, NULL, vapdev);
        event_notifier_cleanup(notifier);
    }

out_free_info:
    g_free(irq_info);

}

static void vfio_ap_unregister_irq_notifier(VFIOAPDevice *vapdev,
                                            unsigned int irq)
{
    Error *err = NULL;
    EventNotifier *notifier;

    switch (irq) {
    case VFIO_AP_REQ_IRQ_INDEX:
        notifier = &vapdev->req_notifier;
        break;
    default:
        error_report("vfio: Unsupported device irq(%d)", irq);
        return;
    }

    if (vfio_set_irq_signaling(&vapdev->vdev, irq, 0,
                               VFIO_IRQ_SET_ACTION_TRIGGER, -1, &err)) {
        warn_reportf_err(err, VFIO_MSG_PREFIX, vapdev->vdev.name);
    }

    qemu_set_fd_handler(event_notifier_get_fd(notifier),
                        NULL, NULL, vapdev);
    event_notifier_cleanup(notifier);
}

static void vfio_ap_realize(DeviceState *dev, Error **errp)
{
    int ret;
    char *mdevid;
    Error *err = NULL;
    VFIOGroup *vfio_group;
    APDevice *apdev = AP_DEVICE(dev);
    VFIOAPDevice *vapdev = VFIO_AP_DEVICE(apdev);

    vfio_group = vfio_ap_get_group(vapdev, errp);
    if (!vfio_group) {
        return;
    }

    vapdev->vdev.ops = &vfio_ap_ops;
    vapdev->vdev.type = VFIO_DEVICE_TYPE_AP;
    mdevid = basename(vapdev->vdev.sysfsdev);
    vapdev->vdev.name = g_strdup_printf("%s", mdevid);
    vapdev->vdev.dev = dev;

    /*
     * vfio-ap devices operate in a way compatible with discarding of
     * memory in RAM blocks, as no pages are pinned in the host.
     * This needs to be set before vfio_get_device() for vfio common to
     * handle ram_block_discard_disable().
     */
    vapdev->vdev.ram_block_discard_allowed = true;

    ret = vfio_get_device(vfio_group, mdevid, &vapdev->vdev, errp);
    if (ret) {
        goto out_get_dev_err;
    }

    vfio_ap_register_irq_notifier(vapdev, VFIO_AP_REQ_IRQ_INDEX, &err);
    if (err) {
        /*
         * Report this error, but do not make it a failing condition.
         * Lack of this IRQ in the host does not prevent normal operation.
         */
        error_report_err(err);
    }

    return;

out_get_dev_err:
    vfio_ap_put_device(vapdev);
    vfio_put_group(vfio_group);
}

static void vfio_ap_unrealize(DeviceState *dev)
{
    APDevice *apdev = AP_DEVICE(dev);
    VFIOAPDevice *vapdev = VFIO_AP_DEVICE(apdev);
    VFIOGroup *group = vapdev->vdev.group;

    vfio_ap_unregister_irq_notifier(vapdev, VFIO_AP_REQ_IRQ_INDEX);
    vfio_ap_put_device(vapdev);
    vfio_put_group(group);
}

static Property vfio_ap_properties[] = {
    DEFINE_PROP_STRING("sysfsdev", VFIOAPDevice, vdev.sysfsdev),
    DEFINE_PROP_END_OF_LIST(),
};

static void vfio_ap_reset(DeviceState *dev)
{
    int ret;
    APDevice *apdev = AP_DEVICE(dev);
    VFIOAPDevice *vapdev = VFIO_AP_DEVICE(apdev);

    ret = ioctl(vapdev->vdev.fd, VFIO_DEVICE_RESET);
    if (ret) {
        error_report("%s: failed to reset %s device: %s", __func__,
                     vapdev->vdev.name, strerror(errno));
    }
}

static const VMStateDescription vfio_ap_vmstate = {
    .name = "vfio-ap",
    .unmigratable = 1,
};

static void vfio_ap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, vfio_ap_properties);
    dc->vmsd = &vfio_ap_vmstate;
    dc->desc = "VFIO-based AP device assignment";
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    dc->realize = vfio_ap_realize;
    dc->unrealize = vfio_ap_unrealize;
    dc->hotpluggable = true;
    dc->reset = vfio_ap_reset;
    dc->bus_type = TYPE_AP_BUS;
}

static const TypeInfo vfio_ap_info = {
    .name = TYPE_VFIO_AP_DEVICE,
    .parent = TYPE_AP_DEVICE,
    .instance_size = sizeof(VFIOAPDevice),
    .class_init = vfio_ap_class_init,
};

static void vfio_ap_type_init(void)
{
    type_register_static(&vfio_ap_info);
}

type_init(vfio_ap_type_init)
