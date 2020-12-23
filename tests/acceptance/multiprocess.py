# Test for multiprocess qemu
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# later.  See the COPYING file in the top-level directory.


from avocado_qemu import Test
from avocado_qemu import wait_for_console_pattern
from avocado_qemu import exec_command_and_wait_for_pattern

from qemu.accel import kvm_available

import os
import socket

ACCEL_NOT_AVAILABLE_FMT = "%s accelerator does not seem to be available"
KVM_NOT_AVAILABLE = ACCEL_NOT_AVAILABLE_FMT % "KVM"

class Multiprocess(Test):
    """
    :avocado: tags=multiprocess
    """
    KERNEL_COMMON_COMMAND_LINE = 'printk.time=0 '

    def wait_for_console_pattern(self, success_message, vm=None):
        wait_for_console_pattern(self, success_message,
                                 failure_message='Kernel panic - not syncing',
                                 vm=vm)

    def do_test(self, kernel_url, initrd_url, kernel_command_line,
                machine_type):
        if not kvm_available(self.arch, self.qemu_bin):
            self.cancel(KVM_NOT_AVAILABLE)

        # Create socketpair to connect proxy and remote processes
        proxy_sock, remote_sock = socket.socketpair(socket.AF_UNIX,
                                                    socket.SOCK_STREAM)
        os.set_inheritable(proxy_sock.fileno(), True)
        os.set_inheritable(remote_sock.fileno(), True)

        kernel_path = self.fetch_asset(kernel_url)
        initrd_path = self.fetch_asset(initrd_url)

        # Create remote process
        remote_vm = self.get_vm()
        remote_vm.add_args('-machine', 'x-remote')
        remote_vm.add_args('-nodefaults')
        remote_vm.add_args('-device', 'lsi53c895a,id=lsi1')
        remote_vm.add_args('-object', 'x-remote-object,id=robj1,'
                           'devid=lsi1,fd='+str(remote_sock.fileno()))
        remote_vm.launch()

        # Create proxy process
        self.vm.set_console()
        self.vm.add_args('-machine', machine_type)
        self.vm.add_args('-accel', 'kvm')
        self.vm.add_args('-cpu', 'host')
        self.vm.add_args("-object",
                         "memory-backend-memfd,id=sysmem-file,size=2G")
        self.vm.add_args("--numa", "node,memdev=sysmem-file")
        self.vm.add_args("-m", "2048")
        self.vm.add_args('-kernel', kernel_path,
                         '-initrd', initrd_path,
                         '-append', kernel_command_line)
        self.vm.add_args('-device',
                         'x-pci-proxy-dev,'
                         'id=lsi1,fd='+str(proxy_sock.fileno()))
        self.vm.launch()
        self.wait_for_console_pattern("as init process")
        exec_command_and_wait_for_pattern(self, "mount -t sysfs sysfs /sys",
                                          '', '')
        exec_command_and_wait_for_pattern(self,
                                          "cat /sys/bus/pci/devices/*/uevent",
                                          "PCI_ID=1000:0012", '')

    def test_multiprocess_x86_64(self):
        """
        :avocado: tags=arch:x86_64
        """
        kernel_url = ('https://archives.fedoraproject.org/pub/archive/fedora'
                      '/linux/releases/31/Everything/x86_64/os/images'
                      '/pxeboot/vmlinuz')
        initrd_url = ('https://archives.fedoraproject.org/pub/archive/fedora'
                      '/linux/releases/31/Everything/x86_64/os/images'
                      '/pxeboot/initrd.img')
        kernel_command_line = (self.KERNEL_COMMON_COMMAND_LINE +
                               'console=ttyS0 rdinit=/bin/bash')
        machine = 'pc'
        self.do_test(kernel_url, initrd_url, kernel_command_line, machine)

    def test_multiprocess_aarch64(self):
        """
        :avocado: tags=arch:aarch64
        """
        kernel_url = ('https://archives.fedoraproject.org/pub/archive/fedora'
                      '/linux/releases/31/Everything/aarch64/os/images'
                      '/pxeboot/vmlinuz')
        initrd_url = ('https://archives.fedoraproject.org/pub/archive/fedora'
                      '/linux/releases/31/Everything/aarch64/os/images'
                      '/pxeboot/initrd.img')
        kernel_command_line = (self.KERNEL_COMMON_COMMAND_LINE +
                               'rdinit=/bin/bash console=ttyAMA0')
        machine_type = 'virt,gic-version=3'
        self.do_test(kernel_url, initrd_url, kernel_command_line, machine_type)
