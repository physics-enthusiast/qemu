/*
 * QLit literal qobject
 *
 * Copyright IBM, Corp. 2009
 * Copyright (c) 2013, 2015, 2017 Red Hat Inc.
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *  Markus Armbruster <armbru@redhat.com>
 *  Marc-André Lureau <marcandre.lureau@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "qapi/qmp/qlit.h"
#include "qapi/qmp/types.h"

typedef struct QListCompareHelper {
    int index;
    QLitObject *objs;
    bool result;
} QListCompareHelper;

static void compare_helper(QObject *obj, void *opaque)
{
    QListCompareHelper *helper = opaque;

    if (!helper->result) {
        return;
    }

    if (helper->objs[helper->index].type == QTYPE_NONE) {
        helper->result = false;
        return;
    }

    helper->result =
        qlit_equal_qobject(&helper->objs[helper->index++], obj);
}

static bool qlit_equal_qdict(const QLitObject *lhs, const QDict *qdict)
{
    QDict *dict = qdict_clone_shallow(qdict);
    bool success = false;
    int i;

    for (i = 0; lhs->value.qdict[i].key; i++) {
        QObject *obj = qdict_get(dict, lhs->value.qdict[i].key);

        if (!qlit_equal_qobject(&lhs->value.qdict[i].value, obj)) {
            goto end;
        }
        qdict_del(dict, lhs->value.qdict[i].key);
    }

    if (qdict_size(dict) != 0) {
        goto end;
    }

    success = true;

end:
    QDECREF(dict);
    return success;
}

static bool qlit_equal_qlist(const QLitObject *lhs, const QList *qlist)
{
    QListCompareHelper helper;
    int i;

    for (i = 0; lhs->value.qlist[i].type != QTYPE_NONE; i++) {
        continue;
    }

    if (qlist_size(qlist) != i) {
        return false;
    }

    helper.index = 0;
    helper.objs = lhs->value.qlist;
    helper.result = true;

    qlist_iter(qlist, compare_helper, &helper);

    return helper.result;
}

bool qlit_equal_qobject(const QLitObject *lhs, const QObject *rhs)
{
    int64_t val;

    if (!rhs || lhs->type != qobject_type(rhs)) {
        return false;
    }

    switch (lhs->type) {
    case QTYPE_QBOOL:
        return lhs->value.qbool == qbool_get_bool(qobject_to_qbool(rhs));
    case QTYPE_QNUM:
        val = qnum_get_int(qobject_to_qnum(rhs));
        return lhs->value.qnum == val;
    case QTYPE_QSTRING:
        return g_str_equal(lhs->value.qstr,
                           qstring_get_str(qobject_to_qstring(rhs)));
    case QTYPE_QDICT:
        return qlit_equal_qdict(lhs, qobject_to_qdict(rhs));
    case QTYPE_QLIST:
        return qlit_equal_qlist(lhs, qobject_to_qlist(rhs));
    case QTYPE_QNULL:
        return true;
    default:
        break;
    }

    return false;
}

QObject *qobject_from_qlit(const QLitObject *qlit)
{
    int i;

    switch (qlit->type) {
    case QTYPE_QNULL:
        return QOBJECT(qnull());
    case QTYPE_QNUM:
        return QOBJECT(qnum_from_int(qlit->value.qnum));
    case QTYPE_QSTRING:
        return QOBJECT(qstring_from_str(qlit->value.qstr));
    case QTYPE_QDICT: {
        QDict *qdict = qdict_new();
        for (i = 0; qlit->value.qdict[i].value.type != QTYPE_NONE; i++) {
            QLitDictEntry *e = &qlit->value.qdict[i];

            qdict_put_obj(qdict, e->key, qobject_from_qlit(&e->value));
        }
        return QOBJECT(qdict);
    }
    case QTYPE_QLIST: {
        QList *qlist = qlist_new();

        for (i = 0; qlit->value.qlist[i].type != QTYPE_NONE; i++) {
            qlist_append_obj(qlist, qobject_from_qlit(&qlit->value.qlist[i]));
        }
        return QOBJECT(qlist);
    }
    case QTYPE_QBOOL:
        return QOBJECT(qbool_from_bool(qlit->value.qbool));
    case QTYPE_NONE:
        assert(0);
    }

    return NULL;
}
