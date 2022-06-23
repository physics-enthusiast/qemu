/*
 * QEMUFile backend for QIOChannel objects
 *
 * Copyright (c) 2015-2016 Red Hat, Inc
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
#include "qemu-file-channel.h"
#include "qemu-file.h"
#include "io/channel-socket.h"
#include "io/channel-tls.h"
#include "qemu/iov.h"
#include "qemu/yank.h"
#include "yank_functions.h"


static QEMUFile *channel_get_input_return_path(void *opaque)
{
    QIOChannel *ioc = QIO_CHANNEL(opaque);

    return qemu_fopen_channel_output(ioc);
}

static QEMUFile *channel_get_output_return_path(void *opaque)
{
    QIOChannel *ioc = QIO_CHANNEL(opaque);

    return qemu_fopen_channel_input(ioc);
}

static const QEMUFileOps channel_input_ops = {
    .get_return_path = channel_get_input_return_path,
};


static const QEMUFileOps channel_output_ops = {
    .get_return_path = channel_get_output_return_path,
};


QEMUFile *qemu_fopen_channel_input(QIOChannel *ioc)
{
    object_ref(OBJECT(ioc));
    return qemu_file_new_input(ioc, &channel_input_ops);
}

QEMUFile *qemu_fopen_channel_output(QIOChannel *ioc)
{
    object_ref(OBJECT(ioc));
    return qemu_file_new_output(ioc, &channel_output_ops);
}
