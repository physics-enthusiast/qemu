# Record/replay test that boots a Linux kernel
#
# Copyright (c) 2020 ISP RAS
#
# Author:
#  Pavel Dovgalyuk <Pavel.Dovgaluk@ispras.ru>
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# later.  See the COPYING file in the top-level directory.

import os
import gzip

from avocado_qemu import wait_for_console_pattern
from avocado.utils import process
from avocado.utils import archive
from boot_linux_console import LinuxKernelUtils

class ReplayKernel(LinuxKernelUtils):
    """
    Boots a Linux kernel in record mode and checks that the console
    is operational and the kernel command line is properly passed
    from QEMU to the kernel.
    Then replays the same scenario and verifies, that QEMU correctly
    terminates.
    """

    timeout = 90

    def run_vm(self, kernel_path, kernel_command_line, console_pattern,
               record, shift, args):
        vm = self.get_vm()
        vm.set_console()
        if record:
            mode = 'record'
        else:
            mode = 'replay'
        vm.add_args('-icount', 'shift=%s,rr=%s,rrfile=%s' %
                    (shift, mode, os.path.join(self.workdir, 'replay.bin')),
                    '-kernel', kernel_path,
                    '-append', kernel_command_line,
                    '-net', 'none')
        if args:
            vm.add_args(*args)
        vm.launch()
        self.wait_for_console_pattern(console_pattern, vm)
        if record:
            vm.shutdown()
        else:
            vm.wait()

    def run_rr(self, kernel_path, kernel_command_line, console_pattern,
        shift=7, args=None):
        self.run_vm(kernel_path, kernel_command_line, console_pattern,
                    True, shift, args)
        self.run_vm(kernel_path, kernel_command_line, console_pattern,
                    False, shift, args)
