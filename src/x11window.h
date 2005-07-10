/*
 * ----------------------------------------------------------------------------
 * x11window.h
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

#ifndef _X11WINDOW_H_
#define _X11WINDOW_H_
#include <X11/Xlib.h>

typedef struct {
    PyObject_HEAD

    PyObject *display_pyobject;
    Display *display;
    Window   window;
    Cursor   invisible_cursor;

    PyObject *ptr;
} X11Window_PyObject;

extern PyTypeObject X11Window_PyObject_Type;

X11Window_PyObject *X11Window_PyObject__wrap(PyObject *display, Window window);
#endif