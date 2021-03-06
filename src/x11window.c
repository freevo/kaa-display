/*
 * ----------------------------------------------------------------------------
 * x11window.c
 * ----------------------------------------------------------------------------
 * $Id$
 *
 * ----------------------------------------------------------------------------
 * kaa.display - Generic Display Module
 * Copyright (C) 2005, 2006 Dirk Meyer, Jason Tackaberry
 *
 * First Edition: Jason Tackaberry <tack@sault.org>
 * Maintainer:    Jason Tackaberry <tack@sault.org>
 *
 * Please see the file AUTHORS for a complete list of authors.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version
 * 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ----------------------------------------------------------------------------
 */
#include "config.h"
#include <Python.h>
#include "x11window.h"
#include "x11display.h"
#include "structmember.h"

void _make_invisible_cursor(X11Window_PyObject *win);
Visual *find_argb_visual (Display *dpy, int scr);

int _ewmh_set_hint(X11Window_PyObject *o, char *type, long *data, int ndata)
{
    int res, i;
    XEvent ev;

    memset(&ev, 0, sizeof(ev));

    XLockDisplay(o->display);
    XUngrabPointer(o->display, CurrentTime);
    ev.xclient.type = ClientMessage;
    ev.xclient.send_event = True;
    ev.xclient.message_type = XInternAtom(o->display, type, False);
    ev.xclient.window = o->window;
    ev.xclient.format = 32;

    for (i = 0; i < ndata; i++)
        ev.xclient.data.l[i] = (long)data[i];
    res = XSendEvent(o->display, DefaultRootWindow(o->display), False,
                    SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XSync(o->display, False);
    XUnlockDisplay(o->display);

    return res;
}

static int
X11Window_PyObject__clear(X11Window_PyObject *self)
{
    return 0;
}


static int
X11Window_PyObject__traverse(X11Window_PyObject *self, visitproc visit,
                             void *arg)
{
    int ret;
    if (self->display_pyobject) {
        ret = visit(self->display_pyobject, arg);
        if (ret != 0)
            return ret;
    }
    return 0;
}

PyObject *
X11Window_PyObject__new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    X11Window_PyObject *self, *py_parent;
    X11Display_PyObject *display;
    Window parent;
    Window root;
    Visual *visual;    
    int w, h, screen, argb=0, depth, window_events=1, mouse_events=1, key_events=1, input_only=0;
    long evmask = 0;
    char *window_title = NULL;
    XSetWindowAttributes attr;
    unsigned long wmask;    

    self = (X11Window_PyObject *)type->tp_alloc(type, 0);
    if (!args)
        // args is NULL it means we're being called from __wrap()
        return (PyObject *)self;

    if (!PyArg_ParseTuple(args, "O!(ii)", &X11Display_PyObject_Type, &display, &w, &h))
        return NULL;

    py_parent = (X11Window_PyObject *)PyDict_GetItemString(kwargs, "parent");
    if (PyMapping_HasKeyString(kwargs, "title"))
        window_title = PyString_AsString(PyDict_GetItemString(kwargs, "title"));

    if (PyMapping_HasKeyString(kwargs, "argb"))
        argb = PyInt_AsLong(PyDict_GetItemString(kwargs, "argb"));

    if (PyMapping_HasKeyString(kwargs, "window_events"))
        window_events = PyInt_AsLong(PyDict_GetItemString(kwargs, "window_events"));

    if (PyMapping_HasKeyString(kwargs, "mouse_events"))
        mouse_events = PyInt_AsLong(PyDict_GetItemString(kwargs, "mouse_events"));

    if (PyMapping_HasKeyString(kwargs, "key_events"))
        key_events = PyInt_AsLong(PyDict_GetItemString(kwargs, "key_events"));

    if (PyMapping_HasKeyString(kwargs, "input_only"))
        input_only = PyInt_AsLong(PyDict_GetItemString(kwargs, "input_only"));

    self->display_pyobject = (PyObject *)display;
    self->display = display->display;

    if (py_parent)
        parent = py_parent->window;
    else
        parent = DefaultRootWindow(self->display);

    if (window_events)
        evmask = ExposureMask | StructureNotifyMask | FocusChangeMask;
   
    if (mouse_events)
        evmask |= ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    
    if (key_events)
        evmask |= KeyPressMask | KeyReleaseMask;

    XLockDisplay(self->display);

    if (PyMapping_HasKeyString(kwargs, "window")) {
        x_error_trap_push();
        self->window = (Window)PyLong_AsUnsignedLong(PyDict_GetItemString(kwargs, "window"));
        XSelectInput(self->display, self->window, evmask);
        XSync(self->display, False);

        if (x_error_trap_pop(False) == BadAccess) {
            // Input select failed, retry without button masks.
            int error;
            evmask &= ~(ButtonPressMask | ButtonReleaseMask);
            x_error_trap_push();
            XSelectInput(self->display, self->window, evmask);
            XSync(self->display, False);
            error = x_error_trap_pop(False);

            fprintf(stderr, "kaa.display warning: Couldn't select %s events for "
                            "external window; %s signals will not work.\n",
                    error ? "any" : "button", error ? "window" : "button");
        }
        self->owner = Py_False;
    } else {
        screen = DefaultScreen(self->display);
#ifdef HAVE_X11_COMPOSITE        
        if (argb == 1)
        {
            depth = 32;
            root = RootWindow(self->display, screen);
            visual = find_argb_visual(self->display, screen);
            attr.colormap = XCreateColormap (self->display, root, visual, AllocNone);
            wmask = CWEventMask | CWBackPixel | CWBorderPixel | CWColormap;
        }
        else
#endif  
        {
            depth = DefaultDepth(self->display, screen);
            visual = DefaultVisual(self->display, screen);
            attr.colormap = DefaultColormap(self->display, screen);
            wmask = CWBackingStore | CWColormap | CWBackPixmap | CWWinGravity |
                            CWBitGravity | CWEventMask | CWOverrideRedirect;
        }

        attr.backing_store = NotUseful;
        attr.border_pixel = 0;
        attr.background_pixmap = None;
        attr.event_mask = evmask;
        attr.bit_gravity = StaticGravity;
        attr.win_gravity = StaticGravity;
        attr.override_redirect = False;

        x_error_trap_push();
        if (input_only){
                attr.event_mask = evmask & ~ExposureMask;
                self->window = XCreateWindow(self->display, parent, 0, 0,
                                w, h, 0, 0, InputOnly, NULL,
                                CWWinGravity | CWEventMask, &attr);
        } else {
                self->window = XCreateWindow(self->display, parent, 0, 0,
                                w, h, 0, depth, InputOutput, visual,
                                wmask, &attr);
        }
        XSync(self->display, False);

        if (x_error_trap_pop(True) != Success)
            goto fail;

        if (window_title) {
            x_error_trap_push();
            XStoreName(self->display, self->window, window_title);
            x_error_trap_pop(False);
        }
        self->owner = Py_True;
    }

    self->wid = PyLong_FromUnsignedLong(self->window);
    Py_INCREF(self->owner);
    // Needed to handle event for window close via window manager
    x_error_trap_push();
    XSetWMProtocols(self->display, self->window, &display->wmDeleteMessage, 1);
    x_error_trap_pop(False);
    XUnlockDisplay(self->display);
    Py_INCREF(display);
    return (PyObject *)self;

fail:
    XUnlockDisplay(display->display);
    type->tp_free((PyObject *)self);
    return NULL;
}

static int
X11Window_PyObject__init(X11Window_PyObject *self, PyObject *args,
                         PyObject *kwds)
{
    return 0;
}

void
X11Window_PyObject__dealloc(X11Window_PyObject * self)
{
    if (self->window) {
        x_error_trap_push();
        XLockDisplay(self->display);
        if (self->owner == Py_True)
            XDestroyWindow(self->display, self->window);
        Py_XDECREF(self->wid);
        if (self->invisible_cursor)
            XFreeCursor(self->display, self->invisible_cursor);
        XUnlockDisplay(self->display);
        x_error_trap_pop(False);
    }
    Py_DECREF(self->owner);
    Py_XDECREF(self->display_pyobject);
    X11Window_PyObject__clear(self);
    self->ob_type->tp_free((PyObject*)self);
}


PyObject *
X11Window_PyObject__show(X11Window_PyObject * self, PyObject * args)
{
    int raise;
    if (!PyArg_ParseTuple(args, "i", &raise))
        return NULL;

    XLockDisplay(self->display);
    if (raise)
        XMapRaised(self->display, self->window);
    else
        XMapWindow(self->display, self->window);
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return Py_INCREF(Py_None), Py_None;
}

PyObject *
X11Window_PyObject__hide(X11Window_PyObject * self, PyObject * args)
{
    XLockDisplay(self->display);
    XUnmapWindow(self->display, self->window);
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return Py_INCREF(Py_None), Py_None;
}

PyObject *
X11Window_PyObject__raise(X11Window_PyObject * self, PyObject * args)
{
    XLockDisplay(self->display);
    XRaiseWindow(self->display, self->window);
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return Py_INCREF(Py_None), Py_None;
}


PyObject *
X11Window_PyObject__lower(X11Window_PyObject * self, PyObject * args)
{
    XLockDisplay(self->display);
    XLowerWindow(self->display, self->window);
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return Py_INCREF(Py_None), Py_None;
}

PyObject *
X11Window_PyObject__set_geometry(X11Window_PyObject * self, PyObject * args)
{
    int x, y;
    unsigned int w, h;
    if (!PyArg_ParseTuple(args, "(ii)(ii)", &x, &y, &w, &h))
        return NULL;

    XLockDisplay(self->display);
    if (x != -1 && w != -1)
        XMoveResizeWindow(self->display, self->window, x, y, w, h);
    else if (x != -1)
        XMoveWindow(self->display, self->window, x, y);
    else if (w != -1)
        XResizeWindow(self->display, self->window, w, h);
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return Py_INCREF(Py_None), Py_None;
}

PyObject *
X11Window_PyObject__set_cursor_visible(X11Window_PyObject *self, PyObject *args)
{
    int visible;
    if (!PyArg_ParseTuple(args, "i", &visible))
        return NULL;

    XLockDisplay(self->display);
    if (!visible) {
        if (!self->invisible_cursor)
            _make_invisible_cursor(self);
        XDefineCursor(self->display, self->window, self->invisible_cursor);
    } else
        XUndefineCursor(self->display, self->window);
    XUnlockDisplay(self->display);

    return Py_INCREF(Py_None), Py_None;
}

PyObject *
X11Window_PyObject__get_geometry(X11Window_PyObject * self, PyObject * args)
{
    int absolute;
    if (!PyArg_ParseTuple(args, "i", &absolute))
        return NULL;

    XWindowAttributes attrs, parent_attrs;
    XLockDisplay(self->display);
    x_error_trap_push();
    XGetWindowAttributes(self->display, self->window, &attrs);
    if (x_error_trap_pop(True) != Success) {
        XUnlockDisplay(self->display);
        return NULL;
    }

    if (absolute) {
        Window w = self->window, root, parent = 0, *children;
        unsigned int n_children;
        while (parent != root) {
            XQueryTree(self->display, w, &root, &parent, &children, &n_children);
            XGetWindowAttributes(self->display, parent, &parent_attrs);
            attrs.x += parent_attrs.x;
            attrs.y += parent_attrs.y;
            w = parent;
        }
    }
    XUnlockDisplay(self->display);
    return Py_BuildValue("((ii)(ii))", attrs.x, attrs.y, attrs.width,
                         attrs.height);
}

PyObject *
X11Window_PyObject__set_fullscreen(X11Window_PyObject *self, PyObject *args)
{
    int fs;
    long data[2];

    if (!PyArg_ParseTuple(args, "i", &fs))
        return NULL;

    data[0] = (long)(fs ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE);
    data[1] = (long)XInternAtom(self->display, "_NET_WM_STATE_FULLSCREEN", False);
    return PyBool_FromLong(_ewmh_set_hint(self, "_NET_WM_STATE", (long *)&data, 2));
}

PyObject *
X11Window_PyObject__set_transient_for_hint(X11Window_PyObject *self, PyObject *args)
{
    int win_id, transient;

    if (!PyArg_ParseTuple(args, "ii", &win_id, &transient))
        return NULL;

    XLockDisplay(self->display);
    XUngrabPointer(self->display, CurrentTime);
    if (!transient)
    {
        XDeleteProperty(self->display, self->window, XA_WM_TRANSIENT_FOR);
    } else
    {
        if (!win_id)
        {
            win_id = DefaultRootWindow(self->display);
        }
        XSetTransientForHint(self->display, self->window, win_id);
    }
    XSync(self->display, False);
    XUnlockDisplay(self->display);
    return PyBool_FromLong((long) transient);
}

PyObject *
X11Window_PyObject__get_visible(X11Window_PyObject * self, PyObject * args)
{
    XWindowAttributes attrs;
    XLockDisplay(self->display);
    XGetWindowAttributes(self->display, self->window, &attrs);
    XUnlockDisplay(self->display);
    return Py_BuildValue("i", attrs.map_state == IsViewable);
}

PyObject *
X11Window_PyObject__focus(X11Window_PyObject * self, PyObject * args)
{
    XLockDisplay(self->display);
    XSetInputFocus(self->display, self->window, RevertToParent, CurrentTime);
    XUnlockDisplay(self->display);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
X11Window_PyObject__set_title(X11Window_PyObject * self, PyObject * args)
{
    char *title;
    if (!PyArg_ParseTuple(args, "s", &title))
        return NULL;
    XLockDisplay(self->display);
    XStoreName(self->display, self->window, title);
    XUnlockDisplay(self->display);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
X11Window_PyObject__get_title(X11Window_PyObject * self, PyObject * args)
{
    char *title;
    PyObject *pytitle;

    XLockDisplay(self->display);
    XFetchName(self->display, self->window, &title);
    XUnlockDisplay(self->display);
    Py_INCREF(Py_None);

    pytitle = Py_BuildValue("s", title);
    XFree(title);
    return pytitle;
}

static void
_walk_children(Display *display, Window window, PyObject *pychildren, 
               int scr_w, int scr_h, int x, int y,
               int recursive, int visible_only, int titled_only)
{
    Window root, parent, *children;
    unsigned int n_children, i;
    XWindowAttributes attrs;
    char *title;

    XQueryTree(display, window, &root, &parent, &children, &n_children);

    for (i = 0; i < n_children; i++) {
        int child_x = x, child_y = y;
        if (visible_only) {
            XGetWindowAttributes(display, children[i], &attrs);
            // Make x, y absolute for this child.
            child_x = x + attrs.x;
            child_y = y + attrs.y;

            if (attrs.map_state != IsViewable || (child_y + attrs.height < 0 || child_y > scr_h) ||
                (child_x + attrs.width < 0 || child_x > scr_w))
                continue;
        }
        if (titled_only) {
            XFetchName(display, children[i], &title);
            if (title)
                XFree(title);
        }

        if (!titled_only || title) {
            PyObject *wid = Py_BuildValue("k", children[i]);
            PyList_Append(pychildren, wid);
            Py_DECREF(wid);
        }

        if (recursive)
            _walk_children(display, children[i], pychildren, scr_w, scr_h, 
                           child_x, child_y, recursive, visible_only, titled_only);
    }
}


PyObject *
X11Window_PyObject__get_children(X11Window_PyObject * self, PyObject * args)
{
    int recursive, visible_only, titled_only;
    PyObject *pychildren;
    XWindowAttributes attrs;

    if (!PyArg_ParseTuple(args, "iii", &recursive, &visible_only, &titled_only))
        return NULL;

    // Fetch screen size
    XGetWindowAttributes(self->display, self->window, &attrs);
    pychildren = PyList_New(0);
    XLockDisplay(self->display);
    _walk_children(self->display, self->window, pychildren, attrs.screen->width, attrs.screen->height, 
                   attrs.x, attrs.y, recursive, visible_only, titled_only);
    XUnlockDisplay(self->display);

    return pychildren;
}

PyObject *
X11Window_PyObject__get_parent(X11Window_PyObject * self, PyObject * args)
{
    Window root, parent, *children;
    unsigned int n_children;

    XLockDisplay(self->display);
    XQueryTree(self->display, self->window, &root, &parent, &children, &n_children);
    XUnlockDisplay(self->display);

    return Py_BuildValue("k", parent);
}

PyObject *
X11Window_PyObject__get_properties(X11Window_PyObject * self, PyObject * args)
{
    Atom *properties, type;
    int n_props, i, format;
    unsigned long n_items, bytes_left;
    char **property_names, *type_name;
    unsigned char *data;
    PyObject *list = PyList_New(0);

    XLockDisplay(self->display);
    x_error_trap_push();
    properties = XListProperties(self->display, self->window, &n_props);
    x_error_trap_pop(False);
    if (!properties) {
        XUnlockDisplay(self->display);
        return list;
    }

    // allocate a chunk of memory for atom values
    data = malloc(8192); 
    property_names = (char **)malloc(sizeof(char *) * n_props);
    XGetAtomNames(self->display, properties, n_props, property_names);

    // Iterate over all properties and make a list containing 5-tuples of:
    // (atom name, atom type, format, number of items, data)
    for (i = 0; i < n_props; i++) {
        PyObject *tuple = PyTuple_New(5), *pydata;
        int field_len = 1, n;
        XGetWindowProperty(self->display, self->window, properties[i], 0, 256, False, AnyPropertyType, 
                           &type, &format, &n_items, &bytes_left, &data);

        field_len = format == 16 ? sizeof(short) : sizeof(long);
        type_name = XGetAtomName(self->display, type);

        if (!strcmp(type_name, "ATOM")) {
            // For ATOM types, resolve atoms to their names.
            pydata = PyList_New(0);
            for (n = 0; n < n_items; n++, data += field_len) {
                char *atom_name = XGetAtomName(self->display, *((Atom *)data));
                PyObject *pyatom_name = PyString_FromString(atom_name);
                PyList_Append(pydata, pyatom_name);
                XFree(atom_name);
                Py_DECREF(pyatom_name);
            }
        } else {
            // For other types, just return the raw buffer and we will parse
            // it in python space.
            void *buffer_ptr;
            Py_ssize_t buffer_len;
            pydata = PyBuffer_New(n_items * field_len + bytes_left);
            PyObject_AsWriteBuffer(pydata, &buffer_ptr, &buffer_len);
            memmove(buffer_ptr, data, n_items * field_len);
            // TODO: if bytes_left > 0, need to fetch the rest.
        }

        PyTuple_SET_ITEM(tuple, 0, PyString_FromString(property_names[i]));
        PyTuple_SET_ITEM(tuple, 1, PyString_FromString(type_name));
        PyTuple_SET_ITEM(tuple, 2, PyLong_FromLong(format));
        PyTuple_SET_ITEM(tuple, 3, PyLong_FromLong(n_items));
        PyTuple_SET_ITEM(tuple, 4, pydata);

        PyList_Append(list, tuple);
        XFree(type_name);
        XFree(property_names[i]);
    }
    free(property_names);
    free(data);
    XFree(properties);
    XUnlockDisplay(self->display);
    return list;
}


PyObject *
X11Window_PyObject__set_shape_mask(X11Window_PyObject * self, PyObject * args)
{
    const char *data;
    Pixmap pix;
    int allocated_bit_buffer = 0, len, x, y, width, height;
    
    
    if (!PyArg_ParseTuple(args, "s#(ii)(ii)", &data, &len, &x, &y, &width, &height))
        return NULL;
    
    if ((width * height) == len) {
        int bit_count = 0, data_pos = 0, bit_buffer_pos = 0;
        char *bit_buffer = malloc( ((width * height) + 7) / 8);

        if (bit_buffer == NULL) {
            return NULL;
        }
        
        for (data_pos = 0; data_pos < len; data_pos ++) {
            bit_buffer[bit_buffer_pos] |= data[data_pos] << bit_count;
            bit_count ++;
            if (bit_count == 8) {
                bit_count = 0;
                bit_buffer_pos ++;
                bit_buffer[bit_buffer_pos] = 0;
            }
        }        
        data = bit_buffer;
        allocated_bit_buffer = 1;
    }
    
    XLockDisplay(self->display);
    // Construct a bitmap from the supplied data for passing to the XShape extension
    pix = XCreateBitmapFromData(self->display, self->window, data, width, height);
    if (pix != None) {
        XShapeCombineMask(self->display, self->window, ShapeBounding, x, y, pix, ShapeSet);
        XFreePixmap(self->display, pix);
    }
    XUnlockDisplay(self->display);
    
    if (allocated_bit_buffer) {
        free((void*)data);
    }
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
X11Window_PyObject__reset_shape_mask(X11Window_PyObject * self, PyObject * args)
{
    XLockDisplay(self->display);
    XShapeCombineMask(self->display, self->window, ShapeBounding, 0, 0, None, ShapeSet);
    XUnlockDisplay(self->display);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
X11Window_PyObject__set_decorated(X11Window_PyObject * self, PyObject * args)
{
    int decorated = 1;
    long data[1];
    Atom _NET_WM_WINDOW_TYPE;

    if (!PyArg_ParseTuple(args, "i", &decorated))
        return NULL;
        

    _NET_WM_WINDOW_TYPE = XInternAtom(self->display, "_NET_WM_WINDOW_TYPE", False);    
    
    if (decorated) {
        Atom _NET_WM_WINDOW_TYPE_NORMAL = XInternAtom(self->display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
        
        data[0] = (long) _NET_WM_WINDOW_TYPE_NORMAL;

    } else {
        Atom _NET_WM_WINDOW_TYPE_SPLASH = XInternAtom(self->display, "_NET_WM_WINDOW_TYPE_SPLASH", False);
        
        data[0] = (long) _NET_WM_WINDOW_TYPE_SPLASH;
    }
    
    XLockDisplay(self->display);
    XChangeProperty(self->display, self->window, _NET_WM_WINDOW_TYPE, XA_ATOM, 32, PropModeReplace, 
                    (unsigned char*)&data, 1);
    XUnlockDisplay(self->display);
    
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject *
X11Window_PyObject__draw_rectangle(X11Window_PyObject * self, PyObject * args)
{
    int x, y, width, height;
    unsigned long color;
    int depth, r, g, b;
    GC gc;

    if (!PyArg_ParseTuple(args, "(ii)(ii)(iii)", &x, &y, &width, &height, &r, &g, &b))
        return NULL;
 
    XLockDisplay(self->display);
    depth = DefaultDepth(self->display, DefaultScreen(self->display));
    if (depth == 16)
        color = ((r & 248) << 8) + ((g & 252) << 3) + ((b & 248) >> 3);
    else
        color = (r << 16) + (g << 8) + b;
    gc = XCreateGC(self->display, self->window, 0, 0);
    XSetForeground(self->display, gc, color);
    XFillRectangle(self->display, self->window, gc, x, y, width, height);
    XFreeGC(self->display, gc);
    XUnlockDisplay(self->display);

    Py_INCREF(Py_None);
    return Py_None;
}

PyMethodDef X11Window_PyObject_methods[] = {
    { "show", (PyCFunction)X11Window_PyObject__show, METH_VARARGS },
    { "hide", (PyCFunction)X11Window_PyObject__hide, METH_VARARGS },
    { "raise_", (PyCFunction)X11Window_PyObject__raise, METH_VARARGS },
    { "lower", (PyCFunction)X11Window_PyObject__lower, METH_VARARGS },
    { "set_geometry", (PyCFunction)X11Window_PyObject__set_geometry, METH_VARARGS },
    { "get_geometry", (PyCFunction)X11Window_PyObject__get_geometry, METH_VARARGS },
    { "set_cursor_visible", (PyCFunction)X11Window_PyObject__set_cursor_visible, METH_VARARGS },
    { "set_fullscreen", (PyCFunction)X11Window_PyObject__set_fullscreen, METH_VARARGS },
    { "set_transient_for", (PyCFunction)X11Window_PyObject__set_transient_for_hint, METH_VARARGS },
    { "get_visible", (PyCFunction)X11Window_PyObject__get_visible, METH_VARARGS },
    { "focus", (PyCFunction)X11Window_PyObject__focus, METH_VARARGS },
    { "set_title", (PyCFunction)X11Window_PyObject__set_title, METH_VARARGS },
    { "get_title", (PyCFunction)X11Window_PyObject__get_title, METH_VARARGS },
    { "get_children", (PyCFunction)X11Window_PyObject__get_children, METH_VARARGS },
    { "get_parent", (PyCFunction)X11Window_PyObject__get_parent, METH_VARARGS },
    { "get_properties", (PyCFunction)X11Window_PyObject__get_properties, METH_VARARGS },
    { "set_shape_mask", (PyCFunction)X11Window_PyObject__set_shape_mask, METH_VARARGS },
    { "reset_shape_mask", (PyCFunction)X11Window_PyObject__reset_shape_mask, METH_VARARGS },
    { "set_decorated", (PyCFunction)X11Window_PyObject__set_decorated, METH_VARARGS },
    { "draw_rectangle", (PyCFunction)X11Window_PyObject__draw_rectangle, METH_VARARGS },
    { NULL, NULL }
};


X11Window_PyObject *
X11Window_PyObject__wrap(PyObject *display, Window window)
{
    X11Window_PyObject *o;

    o = (X11Window_PyObject *)X11Window_PyObject__new(&X11Window_PyObject_Type,
                                                      NULL, NULL);

    o->display_pyobject = display;
    Py_INCREF(display);
    o->display = ((X11Display_PyObject *)display)->display;
    o->window = window;
    o->wid = PyLong_FromUnsignedLong(window);
    XLockDisplay(o->display);
    _make_invisible_cursor(o);
    XUnlockDisplay(o->display);
    return o;
}

void
_make_invisible_cursor(X11Window_PyObject *win)
{
    Pixmap pix;
    static char bits[] = {0, 0, 0, 0, 0, 0, 0};
    XColor cfg;

    // Construct an invisible cursor for mouse hiding.
    cfg.red = cfg.green = cfg.blue = 0;
    pix = XCreateBitmapFromData(win->display, win->window, bits, 1, 1);
    // Memory leak in Xlib: https://bugs.freedesktop.org/show_bug.cgi?id=3568
    win->invisible_cursor = XCreatePixmapCursor(win->display, pix, pix, &cfg,
                                                &cfg, 0, 0);
    XFreePixmap(win->display, pix);
}

// Exported _C_API function
int x11window_object_decompose(X11Window_PyObject *win, Window *window, Display **display)
{
    if (!win || !X11Window_PyObject_Check(win))
        return 0;

    if (window)
        *window = win->window;
    if (display)
        *display = win->display;

    return 1;
}

#ifdef HAVE_X11_COMPOSITE

Visual *find_argb_visual (Display *dpy, int scr)
{
    XVisualInfo		*xvi;
    XVisualInfo		template;
    int			nvi;
    int			i;
    XRenderPictFormat	*format;
    Visual		*visual;

    template.screen = scr;
    template.depth = 32;
    template.class = TrueColor;
    xvi = XGetVisualInfo (dpy, 
			  VisualScreenMask |
			  VisualDepthMask |
			  VisualClassMask,
			  &template,
			  &nvi);
    if (!xvi)
        return 0;
    
    visual = 0;
    for (i = 0; i < nvi; i++)
    {
        format = XRenderFindVisualFormat (dpy, xvi[i].visual);
        if (format->type == PictTypeDirect && format->direct.alphaMask)
        {
            visual = xvi[i].visual;
            break;
        }
    }

    XFree (xvi);
    return visual;
}
#endif

static PyMemberDef X11Window_PyObject_members[] = {
    {"wid", T_OBJECT_EX, offsetof(X11Window_PyObject, wid), 0, ""},
    {"owner", T_OBJECT_EX, offsetof(X11Window_PyObject, owner), 0, ""},
    {NULL}  /* Sentinel */
};


PyTypeObject X11Window_PyObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "X11Window",              /*tp_name*/
    sizeof(X11Window_PyObject),  /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)X11Window_PyObject__dealloc, /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
    "X11 Window Object",      /* tp_doc */
    (traverseproc)X11Window_PyObject__traverse,   /* tp_traverse */
    (inquiry)X11Window_PyObject__clear,           /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    X11Window_PyObject_methods,             /* tp_methods */
    X11Window_PyObject_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)X11Window_PyObject__init,      /* tp_init */
    0,                         /* tp_alloc */
    X11Window_PyObject__new,   /* tp_new */
};
