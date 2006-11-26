/*
 * ----------------------------------------------------------------------------
 * imlib2.c - Imlib2 based X11 display
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

PyTypeObject *Image_PyObject_Type;

#ifdef USE_IMLIB2_X11

#include "imlib2.h"
Imlib_Image *(*imlib_image_from_pyobject)(PyObject *pyimg);

#include <X11/Xlib.h>
#include "x11window.h"
#include "x11display.h"

PyObject *render_imlib2_image(PyObject *self, PyObject *args)
{
    X11Window_PyObject *window;
    PyObject *pyimg;
    Imlib_Image *img;
    XWindowAttributes attrs;
    int dst_x = 0, dst_y = 0, src_x = 0, src_y = 0,
        w = -1, h = -1, img_w, img_h, dither = 1, blend = 0;

    if (!Image_PyObject_Type) {
        PyErr_Format(PyExc_SystemError, "kaa-imlib2 not installed.");
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "O!O!|(ii)(ii)(ii)ii",
                &X11Window_PyObject_Type, &window,
                Image_PyObject_Type, &pyimg,
                &dst_x, &dst_y, &src_x, &src_y, &w, &h,
                &dither, &blend))
        return NULL;

    img = imlib_image_from_pyobject(pyimg);
    imlib_context_set_image(img);
    img_w = imlib_image_get_width();
    img_h = imlib_image_get_height();

    if (w == -1) w = img_w;
    if (h == -1) h = img_h;

    XGetWindowAttributes(window->display, window->window, &attrs);
    imlib_context_set_display(window->display);
    imlib_context_set_visual(attrs.visual);
    imlib_context_set_colormap(attrs.colormap);
    imlib_context_set_drawable(window->window);

    imlib_context_set_dither(dither);
    imlib_context_set_blend(blend);
    if (src_x == 0 && src_y == 0 && w == img_w && h == img_h)
        imlib_render_image_on_drawable(dst_x, dst_y);
    else
        imlib_render_image_part_on_drawable_at_size(src_x, src_y, w, h, dst_x,
                                dst_y, w, h);

    Py_INCREF(Py_None);
    return Py_None;
}
#else

PyObject *render_imlib2_image(PyObject *self, PyObject *args)
{
    PyErr_Format(PyExc_SystemError, "kaa-display compiled without imlib2 display support.");
    return NULL;
}

#endif
