/*
 * Virtio GPU Device
 *
 * Copyright Red Hat, Inc. 2013-2014
 *
 * Authors:
 *     Dave Airlie <airlied@redhat.com>
 *     Gerd Hoffmann <kraxel@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "qemu/module.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "hw/virtio/virtio.h"
#include "hw/virtio/virtio-gpu.h"
#include "hw/virtio/virtio-gpu-bswap.h"
#include "hw/virtio/virtio-gpu-pixman.h"
#include "hw/qdev-properties.h"

static void virtio_gpu_gl_reset(VirtIODevice *vdev)
{
    VirtIOGPU *g = VIRTIO_GPU(vdev);

    virtio_gpu_reset(vdev);

    if (g->parent_obj.use_virgl_renderer) {
        if (g->parent_obj.renderer_blocked) {
            g->renderer_reset = true;
        } else {
            virtio_gpu_virgl_reset(g);
        }
        g->parent_obj.use_virgl_renderer = false;
    }
}

static void virtio_gpu_gl_device_realize(DeviceState *qdev, Error **errp)
{
    VirtIOGPU *g = VIRTIO_GPU(qdev);

#if defined(HOST_WORDS_BIGENDIAN)
    error_setg(errp, "virgl is not supported on bigendian platforms");
    return;
#endif

    if (!display_opengl) {
        error_setg(errp, "opengl is not available");
        return;
    }

    g->parent_obj.conf.flags |= (1 << VIRTIO_GPU_FLAG_VIRGL_ENABLED);
    VIRTIO_GPU_BASE(g)->virtio_config.num_capsets =
        virtio_gpu_virgl_get_num_capsets(g);

    virtio_gpu_device_realize(qdev, errp);
}

static Property virtio_gpu_gl_properties[] = {
    DEFINE_PROP_BIT("stats", VirtIOGPU, parent_obj.conf.flags,
                    VIRTIO_GPU_FLAG_STATS_ENABLED, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void virtio_gpu_gl_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);

    vdc->realize = virtio_gpu_gl_device_realize;
    vdc->reset = virtio_gpu_gl_reset;
    device_class_set_props(dc, virtio_gpu_gl_properties);
}

static const TypeInfo virtio_gpu_gl_info = {
    .name = TYPE_VIRTIO_GPU_GL,
    .parent = TYPE_VIRTIO_GPU,
    .instance_size = sizeof(VirtIOGPUGL),
    .class_init = virtio_gpu_gl_class_init,
};

static void virtio_register_types(void)
{
    type_register_static(&virtio_gpu_gl_info);
}

type_init(virtio_register_types)
