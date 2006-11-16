/*
 * ----------------------------------------------------------------------------
 * display.c
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

#include <Python.h>
#include "config.h"
#include "display.h"
#include "x11display.h"
#include "x11window.h"
#include "imlib2.h"
#include "evas.h"

PyMethodDef display_methods[] = {
    { "render_imlib2_image", (PyCFunction) render_imlib2_image, METH_VARARGS },
#ifdef USE_EVAS
    { "new_evas_software_x11", (PyCFunction) new_evas_software_x11,
      METH_VARARGS | METH_KEYWORDS },
#ifdef ENABLE_ENGINE_GL_X11
    { "new_evas_gl_x11", (PyCFunction) new_evas_gl_x11,
      METH_VARARGS | METH_KEYWORDS },
#endif
#endif
    { NULL }
};

void **get_module_api(char *module)
{
    PyObject *m, *c_api;
    void **ptrs;

    m = PyImport_ImportModule(module);
    if (m == NULL)
       return NULL;
    c_api = PyObject_GetAttrString(m, "_C_API");
    if (c_api == NULL || !PyCObject_Check(c_api))
        return NULL;
    ptrs = (void **)PyCObject_AsVoidPtr(c_api);
    Py_DECREF(c_api);
    return ptrs;
}

void init_X11(void)
{
    PyObject *m, *display_c_api;
    void **imlib2_api_ptrs, **evas_api_ptrs;
    static void *display_api_ptrs[3];

    m = Py_InitModule("_X11", display_methods);

    if (PyType_Ready(&X11Display_PyObject_Type) < 0)
        return;
    Py_INCREF(&X11Display_PyObject_Type);
    PyModule_AddObject(m, "X11Display", (PyObject *)&X11Display_PyObject_Type);

    if (PyType_Ready(&X11Window_PyObject_Type) < 0)
        return;
    Py_INCREF(&X11Window_PyObject_Type);
    PyModule_AddObject(m, "X11Window", (PyObject *)&X11Window_PyObject_Type);

    // Export display C API
    display_api_ptrs[0] = (void *)X11Window_PyObject__wrap;
    display_api_ptrs[1] = (void *)&X11Window_PyObject_Type;
    display_api_ptrs[2] = (void *)x11window_object_decompose;

    display_c_api = PyCObject_FromVoidPtr((void *)display_api_ptrs, NULL);
    PyModule_AddObject(m, "_C_API", display_c_api);

#ifdef USE_IMLIB2
    // Import kaa-imlib2's C api
    imlib2_api_ptrs = get_module_api("kaa.imlib2._Imlib2");
    if (imlib2_api_ptrs == NULL)
        return;
    imlib_image_from_pyobject = imlib2_api_ptrs[0];
    Image_PyObject_Type = imlib2_api_ptrs[1];
#else
    Image_PyObject_Type = NULL;
#endif

#ifdef USE_EVAS
    // Import kaa-evas's C api
    evas_api_ptrs = get_module_api("kaa.evas._evas");
    if (evas_api_ptrs == NULL) {
	// evas installed, but kaa.evas missing
	// FIXME: now with Evas_PyObject_Type == NULL we could
	// crash with a segfault when trying to use it. So we should
	// always import kaa.evas first before using this in C code
	// to get the exception in that case.
	PyErr_Clear();
	Evas_PyObject_Type = NULL;
    } else {
	evas_object_from_pyobject = evas_api_ptrs[0];
	Evas_PyObject_Type = evas_api_ptrs[1];
    }
#else
    Evas_PyObject_Type = NULL;
#endif

    if (!XInitThreads())
        PyErr_Format(PyExc_SystemError, "Unable to initialize X11 threads.");
}
