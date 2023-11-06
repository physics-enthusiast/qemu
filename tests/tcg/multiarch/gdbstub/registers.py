# Exercise the register functionality by exhaustively iterating
# through all supported registers on the system.
#
# This is launched via tests/guest-debug/run-test.py but you can also
# call it directly if using it for debugging/introspection:
#
# SPDX-License-Identifier: GPL-2.0-or-later

import gdb
import sys
import xml.etree.ElementTree as ET

initial_vlen = 0
failcount = 0

def report(cond, msg):
    "Report success/fail of test."
    if cond:
        print("PASS: %s" % (msg))
    else:
        print("FAIL: %s" % (msg))
        global failcount
        failcount += 1


def fetch_xml_regmap():
    """
    Iterate through the XML descriptions and validate.

    We check for any duplicate registers and report them. Return a
    reg_map hash containing the names, regnums and initial values of
    all registers.
    """

    # First check the XML descriptions we have sent. Most arches
    # support XML but a few of the ancient ones don't in which case we
    # need to gracefully fail.

    try:
        xml = gdb.execute("maint print xml-tdesc", False, True)
    except (gdb.error):
        print("SKIP: target does not support XML")
        return None

    total_regs = 0
    reg_map = {}
    frame = gdb.selected_frame()

    tree = ET.fromstring(xml)
    for f in tree.findall("feature"):
        name = f.attrib["name"]
        regs = f.findall("reg")

        total = len(regs)
        total_regs += total
        base = int(regs[0].attrib["regnum"])
        top = int(regs[-1].attrib["regnum"])

        print(f"feature: {name} has {total} registers from {base} to {top}")

        for r in regs:
            name = r.attrib["name"]
            regnum = int(r.attrib["regnum"])
            try:
                value = frame.read_register(name)
            except ValueError:
                report(False, f"failed to read reg: {name}")

            entry = { "name": name, "initial": value, "regnum": regnum }

            if name in reg_map:
                report(False, f"duplicate register {entry} vs {reg_map[name]}")
                continue

            reg_map[name] = entry

    # Validate we match
    report(total_regs == len(reg_map.keys()),
           f"counted all {total_regs} registers in XML")

    return reg_map

def crosscheck_remote_xml(reg_map):
    """
    Cross-check the list of remote-registers with the XML info.
    """

    remote = gdb.execute("maint print remote-registers", False, True)
    r_regs = remote.split("\n")

    total_regs = len(reg_map.keys())
    total_r_regs = 0

    for r in r_regs:
        fields = r.split()
        # Some of the registers reported here are "pseudo" registers that
        # gdb invents based on actual registers so we need to filter them
        # out.
        if len(fields) == 8:
            r_name = fields[0]
            r_regnum = int(fields[6])

            # check in the XML
            try:
                x_reg = reg_map[r_name]
            except KeyError:
                report(False, f"{r_name} not in XML description")
                continue

            x_reg["seen"] = True
            x_regnum = x_reg["regnum"]
            if r_regnum != x_regnum:
                report(False, f"{r_name} {r_regnum} == {x_regnum} (xml)")
            else:
                total_r_regs += 1

    # Just print a mismatch in totals as gdb will filter out 64 bit
    # registers on a 32 bit machine. Also print what is missing to
    # help with debug.
    if total_regs != total_r_regs:
        print(f"xml-tdesc ({total_regs}) and remote-registers ({total_r_regs}) do not agree")

        for x_key in reg_map.keys():
            x_reg = reg_map[x_key]
            if "seen" not in x_reg:
                print(f"{x_reg} wasn't seen in remote-registers")

def complete_and_diff(reg_map):
    """
    Let the program run to (almost) completion and then iterate
    through all the registers we know about and report which ones have
    changed.
    """
    # Let the program get to the end and we can check what changed
    b = gdb.Breakpoint("_exit")
    if b.pending: # workaround Microblaze weirdness
        b.delete()
        gdb.Breakpoint("_Exit")

    gdb.execute("continue")

    frame = gdb.selected_frame()
    changed = 0

    for e in reg_map.values():
        name = e["name"]
        old_val = e["initial"]

        try:
            new_val = frame.read_register(name)
        except:
            report(False, f"failed to read {name} at end of run")
            continue

        if new_val != old_val:
            print(f"{name} changes from {old_val} to {new_val}")
            changed += 1

    # as long as something changed we can be confident its working
    report(changed > 0, f"{changed} registers were changed")


def run_test():
    "Run through the tests"

    reg_map = fetch_xml_regmap()

    if reg_map is not None:
        crosscheck_remote_xml(reg_map)
        complete_and_diff(reg_map)


#
# This runs as the script it sourced (via -x, via run-test.py)
#
try:
    inferior = gdb.selected_inferior()
    arch = inferior.architecture()
    print("ATTACHED: %s" % arch.name())
except (gdb.error, AttributeError):
    print("SKIPPING (not connected)", file=sys.stderr)
    exit(0)

if gdb.parse_and_eval('$pc') == 0:
    print("SKIP: PC not set")
    exit(0)

try:
    run_test()
except (gdb.error):
    print ("GDB Exception: %s" % (sys.exc_info()[0]))
    failcount += 1
    pass

print("All tests complete: %d failures" % failcount)
exit(failcount)
