# Functional test that boots a complete Linux system via a cloud image
#
# Copyright (c) 2018-2019 Red Hat, Inc.
#
# Author:
#  Cleber Rosa <crosa@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# later.  See the COPYING file in the top-level directory.

import os

from avocado_qemu import Test

from avocado.utils import cloudinit
from avocado.utils import network
from avocado.utils import vmimage


class BootLinux(Test):
    """
    Boots a Linux system, checking for a successful initialization
    """

    timeout = 600
    chksum = None

    def setUp(self):
        super(BootLinux, self).setUp()
        self.prepare_boot()
        self.vm.add_args('-m', '1024')
        self.vm.add_args('-drive', 'file=%s' % self.boot.path)
        self.prepare_cloudinit()

    def prepare_boot(self):
        try:
            self.log.info('Downloading and preparing boot image')
            self.boot = vmimage.get(
                'fedora', arch=self.arch, version='30',
                checksum=self.chksum,
                algorithm='sha256',
                cache_dir=self.cache_dirs[0],
                snapshot_dir=self.workdir)
        except:
            self.cancel('Failed to download boot image')

    def prepare_cloudinit(self):
        cloudinit_iso = os.path.join(self.workdir, 'cloudinit.iso')
        self.phone_home_port = network.find_free_port()
        cloudinit.iso(cloudinit_iso, self.name,
                      username='root',
                      password='password',
                      # QEMU's hard coded usermode router address
                      phone_home_host='10.0.2.2',
                      phone_home_port=self.phone_home_port)
        self.vm.add_args('-drive', 'file=%s,format=raw' % cloudinit_iso)

    def wait_for_boot_confirmation(self):
        self.log.info('VM launched, waiting for boot confirmation from guest')
        cloudinit.wait_for_phone_home(('0.0.0.0', self.phone_home_port), self.name)


class BootLinuxX8664(BootLinux):

    chksum = '72b6ae7b4ed09a4dccd6e966e1b3ac69bd97da419de9760b410e837ba00b4e26'

    def test_pc(self):
        """
        :avocado: tags=arch:x86_64
        :avocado: tags=machine:pc
        """
        self.vm.set_machine('pc')
        self.vm.launch()
        self.wait_for_boot_confirmation()

    def test_q35(self):
        """
        :avocado: tags=arch:x86_64
        :avocado: tags=machine:q35
        """
        self.vm.set_machine('q35')
        self.vm.launch()
        self.wait_for_boot_confirmation()
