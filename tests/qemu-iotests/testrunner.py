# Class for actually running tests.
#
# Copyright (c) 2020-2021 Virtuozzo International GmbH
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

import os
from pathlib import Path
import datetime
import time
import difflib
import subprocess
import contextlib
import json
import termios
import sys
from contextlib import contextmanager
from contextlib import AbstractContextManager
from typing import List, Optional, Iterator, Any, Sequence

from testenv import TestEnv


def silent_unlink(path: Path) -> None:
    try:
        path.unlink()
    except OSError:
        pass


def file_diff(file1: str, file2: str) -> List[str]:
    with open(file1) as f1, open(file2) as f2:
        # We want to ignore spaces at line ends. There are a lot of mess about
        # it in iotests.
        # TODO: fix all tests to not produce extra spaces, fix all .out files
        # and use strict diff here!
        seq1 = [line.rstrip() for line in f1]
        seq2 = [line.rstrip() for line in f2]
        res = [line.rstrip()
               for line in difflib.unified_diff(seq1, seq2, file1, file2)]
        return res


# We want to save current tty settings during test run,
# since an aborting qemu call may leave things screwed up.
@contextmanager
def savetty() -> Iterator[None]:
    isterm = sys.stdin.isatty()
    if isterm:
        fd = sys.stdin.fileno()
        attr = termios.tcgetattr(fd)

    try:
        yield
    finally:
        if isterm:
            termios.tcsetattr(fd, termios.TCSADRAIN, attr)


class LastElapsedTime(AbstractContextManager['LastElapsedTime']):
    """ Cache for elapsed time for tests, to show it during new test run

    It is safe to use get() at any time.  To use update(), you must either
    use it inside with-block or use save() after update().
    """
    def __init__(self, cache_file: str, env: TestEnv) -> None:
        self.env = env
        self.cache_file = cache_file

        try:
            with open(cache_file) as f:
                self.cache = json.load(f)
        except (OSError, ValueError):
            self.cache = {}

    def get(self, test: str,
            default: Optional[float] = None) -> Optional[float]:
        if test not in self.cache:
            return default

        if self.env.imgproto not in self.cache[test]:
            return default

        return self.cache[test][self.env.imgproto].get(self.env.imgfmt,
                                                       default)

    def update(self, test: str, elapsed: float) -> None:
        d = self.cache.setdefault(test, {})
        d = d.setdefault(self.env.imgproto, {})
        d[self.env.imgfmt] = elapsed

    def save(self) -> None:
        with open(self.cache_file, 'w') as f:
            json.dump(self.cache, f)

    def __enter__(self) -> 'LastElapsedTime':
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self.save()


class TestResult:
    def __init__(self, status: str, description: str = '',
                 elapsed: Optional[float] = None, diff: Sequence[str] = (),
                 casenotrun: str = '', interrupted: bool = False) -> None:
        self.status = status
        self.description = description
        self.elapsed = elapsed
        self.diff = diff
        self.casenotrun = casenotrun
        self.interrupted = interrupted


class TestRunner(AbstractContextManager['TestRunner']):
    def __init__(self, env: TestEnv, makecheck: bool = False,
                 color: str = 'auto') -> None:
        self.env = env
        self.test_run_env = self.env.get_env()
        self.makecheck = makecheck
        self.last_elapsed = LastElapsedTime('.last-elapsed-cache', env)

        assert color in ('auto', 'on', 'off')
        self.color = (color == 'on') or (color == 'auto' and
                                         sys.stdout.isatty())

        self._stack: contextlib.ExitStack

    def __enter__(self) -> 'TestRunner':
        self._stack = contextlib.ExitStack()
        self._stack.enter_context(self.env)
        self._stack.enter_context(self.last_elapsed)
        self._stack.enter_context(savetty())
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self._stack.close()

    def test_print_one_line(self, test: str, starttime: str,
                            endtime: Optional[str] = None, status: str = '...',
                            lasttime: Optional[float] = None,
                            thistime: Optional[float] = None,
                            description: str = '',
                            test_field_width: Optional[int] = None,
                            end: str = '\n') -> None:
        """ Print short test info before/after test run """
        test = os.path.basename(test)

        if test_field_width is None:
            test_field_width = 8

        if self.makecheck and status != '...':
            if status and status != 'pass':
                status = f' [{status}]'
            else:
                status = ''

            print(f'  TEST   iotest-{self.env.imgfmt}: {test}{status}')
            return

        if lasttime:
            lasttime_s = f' (last: {lasttime:.1f}s)'
        else:
            lasttime_s = ''
        if thistime:
            thistime_s = f'{thistime:.1f}s'
        else:
            thistime_s = '...'

        if endtime:
            endtime = f'[{endtime}]'
        else:
            endtime = ''

        if self.color:
            if status == 'pass':
                col = '\033[32m'
            elif status == 'fail':
                col = '\033[1m\033[31m'
            elif status == 'not run':
                col = '\033[33m'
            else:
                col = ''

            col_end = '\033[0m'
        else:
            col = ''
            col_end = ''

        print(f'{test:{test_field_width}} {col}{status:10}{col_end} '
              f'[{starttime}] {endtime:13}{thistime_s:5} {lasttime_s:14} '
              f'{description}', end=end)

    def find_reference(self, test: str) -> str:
        if self.env.cachemode == 'none':
            ref = f'{test}.out.nocache'
            if os.path.isfile(ref):
                return ref

        ref = f'{test}.out.{self.env.imgfmt}'
        if os.path.isfile(ref):
            return ref

        ref = f'{test}.{self.env.qemu_default_machine}.out'
        if os.path.isfile(ref):
            return ref

        return f'{test}.out'

    def do_run_test(self, test: str) -> TestResult:
        f_test = Path(test)
        f_bad = Path(f_test.name + '.out.bad')
        f_notrun = Path(f_test.name + '.notrun')
        f_casenotrun = Path(f_test.name + '.casenotrun')
        f_reference = Path(self.find_reference(test))

        if not f_test.exists():
            return TestResult(status='fail',
                              description=f'No such test file: {f_test}')

        if not os.access(str(f_test), os.X_OK):
            sys.exit(f'Not executable: {f_test}')

        if not f_reference.exists():
            return TestResult(status='not run',
                              description='No qualified output '
                                          f'(expected {f_reference})')

        for p in (f_bad, f_notrun, f_casenotrun):
            silent_unlink(p)

        args = [str(f_test.resolve())]
        if self.env.debug:
            args.append('-d')

        with f_test.open() as f:
            try:
                if f.readline() == '#!/usr/bin/env python3':
                    args.insert(0, self.env.python)
            except UnicodeDecodeError:  # binary test? for future.
                pass

        env = os.environ.copy()
        env.update(self.test_run_env)

        t0 = time.time()
        with f_bad.open('w') as f:
            proc = subprocess.Popen(args, cwd=str(f_test.parent), env=env,
                                    stdout=f, stderr=subprocess.STDOUT)
            try:
                proc.wait()
            except KeyboardInterrupt:
                proc.terminate()
                proc.wait()
                return TestResult(status='not run',
                                  description='Interrupted by user',
                                  interrupted=True)
            ret = proc.returncode

        elapsed = round(time.time() - t0, 1)

        if ret != 0:
            return TestResult(status='fail', elapsed=elapsed,
                              description=f'failed, exit status {ret}',
                              diff=file_diff(str(f_reference), str(f_bad)))

        if f_notrun.exists():
            return TestResult(status='not run',
                              description=f_notrun.read_text().strip())

        casenotrun = ''
        if f_casenotrun.exists():
            casenotrun = f_casenotrun.read_text()

        diff = file_diff(str(f_reference), str(f_bad))
        if diff:
            return TestResult(status='fail', elapsed=elapsed,
                              description=f'output mismatch (see {f_bad})',
                              diff=diff, casenotrun=casenotrun)
        else:
            f_bad.unlink()
            self.last_elapsed.update(test, elapsed)
            return TestResult(status='pass', elapsed=elapsed,
                              casenotrun=casenotrun)

    def run_test(self, test: str,
                 test_field_width: Optional[int] = None) -> TestResult:
        last_el = self.last_elapsed.get(test)
        start = datetime.datetime.now().strftime('%H:%M:%S')

        self.test_print_one_line(test=test, starttime=start, lasttime=last_el,
                                 end='\r', test_field_width=test_field_width)

        res = self.do_run_test(test)

        end = datetime.datetime.now().strftime('%H:%M:%S')
        self.test_print_one_line(test=test, status=res.status,
                                 starttime=start, endtime=end,
                                 lasttime=last_el, thistime=res.elapsed,
                                 description=res.description,
                                 test_field_width=test_field_width)

        if res.casenotrun:
            print(res.casenotrun)

        return res

    def run_tests(self, tests: List[str]) -> None:
        n_run = 0
        failed = []
        notrun = []
        casenotrun = []

        if not self.makecheck:
            self.env.print_env()
            print()

        test_field_width = max(len(os.path.basename(t)) for t in tests) + 2

        for t in tests:
            name = os.path.basename(t)
            res = self.run_test(t, test_field_width=test_field_width)

            assert res.status in ('pass', 'fail', 'not run')

            if res.casenotrun:
                casenotrun.append(t)

            if res.status != 'not run':
                n_run += 1

            if res.status == 'fail':
                failed.append(name)
                if self.makecheck:
                    self.env.print_env()
                if res.diff:
                    print('\n'.join(res.diff))
            elif res.status == 'not run':
                notrun.append(name)

            if res.interrupted:
                break

        if notrun:
            print('Not run:', ' '.join(notrun))

        if casenotrun:
            print('Some cases not run in:', ' '.join(casenotrun))

        if failed:
            print('Failures:', ' '.join(failed))
            print(f'Failed {len(failed)} of {n_run} iotests')
        else:
            print(f'Passed all {n_run} iotests')
