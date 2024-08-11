#
# Copyright (C) 2023-2023 Intel Corporation.
# SPDX-License-Identifier: MIT
#

import argparse
import logging
import sys
import statistics
import subprocess
import json
import platform
import re

# Global test parameter and descriptions
# Includes:
#   commands to be executed
#   How many threads to use
#   Test names
# For a description of the tests, see the file readme.txt
global_config = {
    "Windows": {
        # List of tests supported
        "Read": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test Read --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test Read --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "ReadUIO": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test ReadUIO --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test ReadUIO --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Write": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test Write --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test Write --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "WriteUIO": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test WriteUIO --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test WriteUIO --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "QueryProcess": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test QueryProcess --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test QueryProcess --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Allocate": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test Allocate --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test Allocate --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Execdelay": {
            "cmd1": "..\\..\\..\\pin -t obj-{0}/syscall.dll -- obj-{0}\\syscall_app.exe --test Execdelay --thread {1} --duration {2}",
            "cmd2": "obj-{0}\\syscall_app.exe --test Execdelay --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Big": {
            "cmd1": "..\\..\\..\\pin -- obj-{0}\\big.exe",
            "cmd2": "obj-{0}\\big.exe",
            "threads": [1],
        },
        "Inline": {
            "cmd1": "..\\..\\..\\pin -inline 0 -t ../ManualExamples/obj-{0}/inscount0.dll -- ..\\Utils\\obj-{0}\\cp-pin.exe makefile.rules obj-{0}\\tmp.txt -s",
            "cmd2": "..\\..\\..\\pin -t ../ManualExamples/obj-{0}/inscount0.dll -- ..\\Utils\\obj-{0}\\cp-pin.exe makefile.rules obj-{0}\\tmp.txt -s",
            "threads": [1],
        },
    },
    "Linux": {
        # List of tests supported
        "Read": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test Read --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test Read --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "ReadUIO": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test ReadUIO --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test ReadUIO --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Write": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test Write --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test Write --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "WriteUIO": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test WriteUIO --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test WriteUIO --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Allocate": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test Allocate --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test Allocate --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Execdelay": {
            "cmd1": "../../../pin -t obj-{0}/syscall.so -- obj-{0}/syscall_app.exe --test Execdelay --thread {1} --duration {2}",
            "cmd2": "obj-{0}/syscall_app.exe --test Execdelay --thread {1} --duration {2}",
            "threads": [1, 2, 4, 6, 8],
        },
        "Big": {
            "cmd1": "../../../pin -- obj-{0}/big.exe",
            "cmd2": "obj-{0}/big.exe",
            "threads": [1],
        },
        "Inline": {
            "cmd1": "../../../pin -inline 0 -t ../ManualExamples/obj-{0}/inscount0.so -- ../Utils/obj-{0}/cp-pin.exe makefile.rules obj-{0}/tmp.txt -s",
            "cmd2": "../../../pin -t ../ManualExamples/obj-{0}/inscount0.so -- ../Utils/obj-{0}/cp-pin.exe makefile.rules obj-{0}/tmp.txt -s",
            "threads": [1],
        },
    },
}

def runcmd(cmd, dir=None, env=None):
    """
    Run specified command in the specified directory under the specified enviroment
    @param  cmd: commanf to be executed.
    @param  dir: Directory to execute the command from. If not specified executed from the current directory.
    @param  env: Environment in which to run the test in. If not specified the current one will be used.
    @return: CompletedProcess instance. So the result of he execution can be queried.
    """
    sub = subprocess.run(cmd,
                         shell=True,
                         capture_output=True,
                         cwd=dir,
                         text=True,
                         env=env)
    # extract the delay result from standard output
    ret = extract_result(sub.stdout)
    if ret is None:
        raise AssertionError("Test without a formatted result")
    return ret


def extract_result(out):
    idx = out.find("Iteration delay")
    if -1 == idx:
        return None
    res = re.findall(r'\d+', out[idx:])
    if res.__len__() == 1:
        return int(res[0])
    return None


class Testset:
    """
    Helper class that will run the tests
    """
    def __init__(self, config, args):
        """
        Test ctor that grabs the test paramemters.
        Parameters are imported from the config and args object.
        args having higher priority over config
        @param config:
        @param args:
        """
        self._target = args.target
        self._dry = args.dry
        self._try = args.numtry
        self._duration = args.duration
        if args.cmd1 is not None or args.cmd1 is not None:
            # custom test
            self._threads = [1]
            self._tests = ["Custom"]
            self._modes = []
            if args.cmd1 is not None:
                self._modes.append("cmd1")
            if args.cmd2 is not None:
                self._modes.append("cmd2")
            self._cmd1 = args.cmd1
            self._cmd2 = args.cmd2
            return

       # thread number can either be taken from command parameter or test description in configuration
        self._threads = None
        if args.threads is not None:
            self._threads = [args.threads]
        # test name(s) can either be taken from command parameter or configuration
        self._tests = config[platform.system()].keys()
        if args.tests is not None:
            self._tests = args.tests
        if args.modes == 'both':
            self._modes = ["cmd1", "cmd2"]
        elif args.modes == 'cmd1':
            self._modes = ["cmd1"]
        else:
            self._modes = ["cmd2"]

    def execute_one(self, mode: "str", test: str, thread: int):
        if test == "Custom":
            # custom command is stored in self._cmd1 or self._cmd2
            command = getattr(self, "_" + mode)
        else:
            command = global_config[platform.system()][test][mode].format(self._target, thread, self._duration)
        logging.debug(command)
        if self._dry:
            return 0
        return runcmd(command)

    def execute_many(self, mode: str, test: str, thread: int):
        results = []
        for i in range(self._try):
            res = self.execute_one(mode, test, thread)
            results.append(res)
        return results

    def RunOnetest(self, mode: str, test: str):
        results = {}
        # If threads not specified, then it is taken from the test configuration
        threads = self._threads
        if threads is None:
            threads = global_config[platform.system()][test]["threads"]
        for nof_thread in threads:
            res = self.execute_many(mode, test, nof_thread)
            results[nof_thread] = {}
            results[nof_thread]["result"] = res

            # remove min and max from the result list
            tmp_res = res.copy()
            tmp_res.remove(max(tmp_res))
            tmp_res.remove(min(tmp_res))

            # compute avgs and stddev
            results[nof_thread]["avg"] = round(statistics.mean(tmp_res), 1)
            results[nof_thread]["stddev"] = round(statistics.pstdev(tmp_res), 2)
        return results

    def Runtests(self, mode: str):
        results = {}
        for test in self._tests:
            results[test] = self.RunOnetest(mode, test)
        return results

    def Runall(self):
        for mode in self._modes:
            res = self.Runtests(mode)
            logging.debug(json.dumps(res, indent=4))
            logging.info(f"{mode} mode results:")
            format_results(res)

def format_results(results: dict):
    for test in results.keys():
        logging.info(f"Test={test: <15} ThreadNum    Avg(usec)    Stddev")
        for thrnum in results[test].keys():
            avg = results[test][thrnum]["avg"]
            stddev = results[test][thrnum]["stddev"]
            logging.info(f"                     {thrnum: >9} {avg: >12} {stddev: >9}")
        logging.info("")


def init_log(debug):
    logger = logging.getLogger()
    if debug:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)
    chandler = logging.StreamHandler(sys.stdout)
    logger.addHandler(chandler)


def start(args):
    testset = Testset(global_config, args)
    testset.Runall()


def check(args):
    # when cmd1 or cmd2 are specified parameters tests and threads are not relevant
    # specifying them is not an error but we issue a warning
    if args.cmd1 is not None or args.cmd2 is not None:
        if args.tests is not None:
            logging.info(f"tests parameter: {args.tests} is ignored since custom test was specified")
        if args.threads is not None:
            logging.info(f"threads parameter: {args.threadss} is ignored since custom test was specified")

def Main():
    parser = argparse.ArgumentParser(description="Pin performance test script", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--dry", action="store_true", default=False, help="No execution just dump commands")
    parser.add_argument("--debug", help="debug level log", action="store_true")
    parser.add_argument("--modes", action="store", default='both', choices=['cmd1', 'cmd2', 'both'], help="run pint or/and native tests")
    parser.add_argument("--duration", action="store", type=int, default=5, help="A single test duration time")
    parser.add_argument("--threads", action="store", type=int, help="Set the number of threads for a single test, by default they are selected according to the configuration object")
    parser.add_argument("--numtry", action="store", type=int, default=5, help="Number of tries to evaluate a single test")
    parser.add_argument("--tests", action="append", choices=["Read", "Write", "QueryProcess", "Allocate", "Execdelay", "ReadUIO", "WriteUIO", "Big", "Inline"],
                        help="Set the test name, by default they are all selected")
    parser.add_argument("--target", action="store", default='ia32', choices=['ia32', 'intel64'], help="binary target")
    parser.add_argument("--cmd1", action="store", default=None, help="Specifies first command, when specified then threads and tests options are irrelevant (optional)")
    parser.add_argument("--cmd2", action="store", default=None, help="Specifies second command, when specified then threads and tests options are irrelevant (optional)")
    args = parser.parse_args()
    init_log(args.debug)
    check(args)
    return start(args)


if __name__ == "__main__":
    sys.exit(Main())
