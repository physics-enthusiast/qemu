#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
trace/generated-events.c
"""

__author__     = "Lluís Vilanova <vilanova@ac.upc.edu>"
__copyright__  = "Copyright 2012-2016, Lluís Vilanova <vilanova@ac.upc.edu>"
__license__    = "GPL version 2 or (at your option) any later version"

__maintainer__ = "Stefan Hajnoczi"
__email__      = "stefanha@linux.vnet.ibm.com"


from tracetool import out


def generate(events, backend):
    out('/* This file is autogenerated by tracetool, do not edit. */',
        '',
        '#include "qemu/osdep.h"',
        '#include "trace.h"',
        '#include "trace/generated-events.h"',
        '#include "trace/control.h"',
        '')

    out('uint16_t dstate[TRACE_EVENT_COUNT];')
    out('bool dstate_init[TRACE_EVENT_COUNT];')

    out('static TraceEvent trace_events[TRACE_EVENT_COUNT] = {')

    for e in events:
        if "vcpu" in e.properties:
            vcpu_id = "TRACE_VCPU_" + e.name.upper()
        else:
            vcpu_id = "(size_t)-1";
        out('    { .id = %(id)s, .vcpu_id = %(vcpu_id)s,'
            ' .name = \"%(name)s\",'
            ' .sstate = %(sstate)s },',
            id = "TRACE_" + e.name.upper(),
            vcpu_id = vcpu_id,
            name = e.name,
            sstate = "TRACE_%s_ENABLED" % e.name.upper())

    out('};',
        '')

    out('void trace_register_events(void)',
        '{',
        '    trace_event_register_group(trace_events, TRACE_EVENT_COUNT, dstate, dstate_init);',
        '}',
        'trace_init(trace_register_events)')
