#!/usr/bin/env python3
#
# Test to compare performance of write requests for two qemu-img binary files.
#
# Copyright (c) 2020 Virtuozzo International GmbH.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


import sys
import os
import subprocess
import simplebench


def bench_func(env, case):
    """ Handle one "cell" of benchmarking table. """
    return bench_write_req(env['qemu_img'], env['image_name'],
                           case['block_size'], case['block_offset'],
                           case['requests'], case['empty_image'])


def qemu_img_pipe(*args):
    '''Run qemu-img and return its output'''
    subp = subprocess.Popen(list(args),
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            universal_newlines=True)
    exitcode = subp.wait()
    if exitcode < 0:
        sys.stderr.write('qemu-img received signal %i: %s\n'
                         % (-exitcode, ' '.join(list(args))))
    return subp.communicate()[0]


def bench_write_req(qemu_img, image_name, block_size, block_offset, requests,
                    empty_image):
    """Benchmark write requests

    The function creates a QCOW2 image with the given path/name and fills it
    with random data optionally. Then it runs the 'qemu-img bench' command and
    makes series of write requests on the image clusters. Finally, it returns
    the total time of the write operations on the disk.

    qemu_img     -- path to qemu_img executable file
    image_name   -- QCOW2 image name to create
    block_size   -- size of a block to write to clusters
    block_offset -- offset of the block in clusters
    requests     -- number of write requests per cluster
    empty_image  -- if not True, fills image with random data

    Returns {'seconds': int} on success and {'error': str} on failure.
    Return value is compatible with simplebench lib.
    """

    if not os.path.isfile(qemu_img):
        print(f'File not found: {qemu_img}')
        sys.exit(1)

    image_dir = os.path.dirname(os.path.abspath(image_name))
    if not os.path.isdir(image_dir):
        print(f'Path not found: {image_name}')
        sys.exit(1)

    cluster_size = 1024 * 1024
    image_size = 1024 * cluster_size
    seek = 4
    dd_count = int(image_size / cluster_size) - seek

    args_create = [qemu_img, 'create', '-f', 'qcow2', '-o',
                   f'cluster_size={cluster_size}',
                   image_name, str(image_size)]

    if requests:
        count = requests * int(image_size / cluster_size)
        step = str(cluster_size)
    else:
        # Create unaligned write requests
        assert block_size
        shift = int(block_size * 1.01)
        count = int((image_size - block_offset) / shift)
        step = str(shift)
        depth = ['-d', '2']

    offset = str(block_offset)
    cnt = str(count)
    size = []
    if block_size:
        size = ['-s', f'{block_size}']

    args_bench = [qemu_img, 'bench', '-w', '-n', '-t', 'none', '-c', cnt,
                  '-S', step, '-o', offset, '-f', 'qcow2', image_name]
    if block_size:
        args_bench.extend(size)
    if not requests:
        args_bench.extend(depth)

    try:
        qemu_img_pipe(*args_create)

        if not empty_image:
            dd = ['dd', 'if=/dev/urandom', f'of={image_name}',
                  f'bs={cluster_size}', f'seek={seek}',
                  f'count={dd_count}']
            devnull = open('/dev/null', 'w')
            subprocess.run(dd, stderr=devnull, stdout=devnull)
            subprocess.run('sync')

    except OSError as e:
        os.remove(image_name)
        return {'error': 'qemu_img create failed: ' + str(e)}

    try:
        ret = qemu_img_pipe(*args_bench)
    finally:
        os.remove(image_name)
        if not ret:
            return {'error': 'qemu_img bench failed'}
        if 'seconds' in ret:
            ret_list = ret.split()
            index = ret_list.index('seconds.')
            return {'seconds': float(ret_list[index-1])}
        else:
            return {'error': 'qemu_img bench failed: ' + ret}


if __name__ == '__main__':

    if len(sys.argv) < 4:
        program = os.path.basename(sys.argv[0])
        print(f'USAGE: {program} <path to qemu-img binary file> '
              '<path to another qemu-img to compare performance with> '
              '<full or relative name for QCOW2 image to create>')
        exit(1)

    # Test-cases are "rows" in benchmark resulting table, 'id' is a caption
    # for the row, other fields are handled by bench_func.
    test_cases = [
        {
            'id': '<simple case>',
            'block_size': 0,
            'block_offset': 0,
            'requests': 10,
            'empty_image': True
        },
        {
            'id': '<general case>',
            'block_size': 4096,
            'block_offset': 0,
            'requests': 10,
            'empty_image': True
        },
        {
            'id': '<cluster middle>',
            'block_size': 4096,
            'block_offset': 524288,
            'requests': 10,
            'empty_image': True
        },
        {
            'id': '<cluster overlap>',
            'block_size': 524288,
            'block_offset': 4096,
            'requests': 2,
            'empty_image': True
        },
    ]

    # Test-envs are "columns" in benchmark resulting table, 'id is a caption
    # for the column, other fields are handled by bench_func.
    # Set the paths below to desired values
    test_envs = [
        {
            'id': '<qemu-img binary 1>',
            'qemu_img': f'{sys.argv[1]}',
            'image_name': f'{sys.argv[3]}'
        },
        {
            'id': '<qemu-img binary 2>',
            'qemu_img': f'{sys.argv[2]}',
            'image_name': f'{sys.argv[3]}'
        },
    ]

    result = simplebench.bench(bench_func, test_envs, test_cases, count=3,
                               initial_run=False)
    print(simplebench.ascii(result))
