/*
 * ----------------------------------------------------------------------------
 * x11display.c
 * ----------------------------------------------------------------------------
 * $Id$
 *
 * ----------------------------------------------------------------------------
 * kaa-display - X11/SDL Display module
 * Copyright (C) 2005 Dirk Meyer, Jason Tackaberry
 *
 * First Edition: Jason Tackaberry <tack@sault.org>
 * Maintainer:    Jason Tackaberry <tack@sault.org>
 *
 * Please see the file doc/CREDITS for a complete list of authors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MER-
 * CHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * ----------------------------------------------------------------------------
 */

#include <Python.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "x11display.h"
#include "structmember.h"

extern PyTypeObject X11Display_PyObject_Type;

PyObject *
X11Display_PyObject__new(PyTypeObject *type, PyObject * args,
                         PyObject * kwargs)
{
    X11Display_PyObject *self;
    Display *display;
    char *display_name;
    if (!PyArg_ParseTuple(args, "s", &display_name))
        return NULL;
    if (strlen(display_name) == 0)
        display_name = NULL;

    display = XOpenDisplay(display_name);
    if (!display) {
        PyErr_Format(PyExc_SystemError, "Unable to open X11 display.");
        return NULL;
    }

    self = (X11Display_PyObject *)type->tp_alloc(type, 0);
    self->display = display;
    return (PyObject *)self;
}

static int
X11Display_PyObject__init(X11Display_PyObject *self, PyObject *args,
                          PyObject *kwargs)
{
    self->socket = PyInt_FromLong( ConnectionNumber(self->display) );
    return 0;
}


void
X11Display_PyObject__dealloc(X11Display_PyObject * self)
{
    //printf("X11Display dealloc\n");
    if (self->display) {
        XCloseDisplay(self->display);
    }
    Py_XDECREF(self->socket);
    self->ob_type->tp_free((PyObject*)self);
}

PyObject *
X11Display_PyObject__handle_events(X11Display_PyObject * self, PyObject * args)
{
    PyObject *events = PyList_New(0), *o;
    XEvent ev;

    XSync(self->display, False);
    while (XPending(self->display)) {
        XNextEvent(self->display, &ev);
        if (ev.type == Expose) {
            o = Py_BuildValue("(i(i(ii)(ii)))", Expose, ev.xexpose.window,
                              ev.xexpose.x, ev.xexpose.y, ev.xexpose.width,
                              ev.xexpose.height);
            PyList_Append(events, o);
            Py_DECREF(o);
        }
        else if (ev.type == KeyPress) {
            o = Py_BuildValue("(i(ii))", KeyPress, ev.xkey.window,
                              ev.xkey.keycode);
            PyList_Append(events, o);
            Py_DECREF(o);
        }
        else if (ev.type == MotionNotify) {
            o = Py_BuildValue("(i(i(ii)(ii)))", MotionNotify,
                              ev.xmotion.window,
                    ev.xmotion.x, ev.xmotion.y,
                    ev.xmotion.x_root, ev.xmotion.y_root);
            PyList_Append(events, o);
            Py_DECREF(o);
        }
        else if (ev.type == ConfigureNotify) {
            o = Py_BuildValue("(i(i(ii)(ii)))", ConfigureNotify,
                              ev.xconfigure.window,
                    ev.xconfigure.x, ev.xconfigure.y,
                    ev.xconfigure.width, ev.xconfigure.height);
            PyList_Append(events, o);
            Py_DECREF(o);
        }
    }
    return events;
}

PyObject *
X11Display_PyObject__sync(X11Display_PyObject * self, PyObject * args)
{
    XSync(self->display, False);
    return Py_INCREF(Py_None), Py_None;
}



PyMethodDef X11Display_PyObject_methods[] = {
    { "handle_events", ( PyCFunction ) X11Display_PyObject__handle_events,
      METH_VARARGS },
    { "sync", ( PyCFunction ) X11Display_PyObject__sync, METH_VARARGS },
    { NULL, NULL }
};


static PyMemberDef X11Display_PyObject_members[] = {
    {"socket", T_OBJECT_EX, offsetof(X11Display_PyObject, socket), 0, ""},
    {NULL}  /* Sentinel */
};


PyTypeObject X11Display_PyObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "X11Display",              /*tp_name*/
    sizeof(X11Display_PyObject),  /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)X11Display_PyObject__dealloc, /* tp_dealloc */
    0,                         /*tp_print*/
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    PyObject_GenericGetAttr,   /*tp_getattro*/
    PyObject_GenericSetAttr,   /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "X11 Display Object",      /* tp_doc */
    0,   /* tp_traverse */
    0,           /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    X11Display_PyObject_methods,             /* tp_methods */
    X11Display_PyObject_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)X11Display_PyObject__init,      /* tp_init */
    0,                         /* tp_alloc */
    X11Display_PyObject__new,   /* tp_new */
};
