# -*- coding: iso-8859-1 -*-
# -----------------------------------------------------------------------------
# display - Interface to the display code
# -----------------------------------------------------------------------------
# $Id$
#
# -----------------------------------------------------------------------------
# kaa-display - X11/SDL Display module
# Copyright (C) 2005 Dirk Meyer, Jason Tackaberry
#
# First Edition: Dirk Meyer <dmeyer@tzi.de>
# Maintainer:    Dirk Meyer <dmeyer@tzi.de>
#
# Please see the file doc/CREDITS for a complete list of authors.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MER-
# CHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# -----------------------------------------------------------------------------

# python imports
import weakref
import time

# the display module
import _Display

# pygame interface (only one function)
image_to_surface = _Display.image_to_surface

# default X11 display
_default_x11_display = None


class X11Display(object):
    XEVENT_MOTION_NOTIFY = 6
    XEVENT_EXPOSE = 12
    XEVENT_BUTTON_PRESS = 4
    XEVENT_KEY_PRESS = 2
    XEVENT_CONFIGURE_NOTIFY = 22

    XEVENT_WINDOW_EVENTS = (6, 12, 4, 2, 22)

    def __init__(self, dispname = ""):
        self._display = _Display.X11Display(dispname)
        self._windows = {}

    def handle_events(self):
        window_events = {}
        for event, args in self._display.handle_events():
            wid = 0
            if event in X11Display.XEVENT_WINDOW_EVENTS:
                wid = args[0]
            if wid:
                if wid not in window_events:
                    window_events[wid] = []
                window_events[wid].append((event, args[1:]))

        for wid, events in window_events.items():
            assert(wid in self._windows)
            window = self._windows[wid]()
            window.handle_events(events)


    def __getattr__(self, attr):
        if attr in ("socket,"):
            return getattr(self._display, attr)

        return getattr(super(X11Display, self), attr)

def _get_display(display):
    if not display:
        global _default_x11_display
        if not _default_x11_display:
            _default_x11_display = X11Display()
        display = _default_x11_display

    assert(type(display) == X11Display)
    return display



class X11Window(object):
    def __init__(self, display = None, window = None, **kwargs):
        display = _get_display(display)
        if window:
            self._window = window
        else:
            assert("size" in kwargs and "title" in kwargs)
            self._window = _Display.X11Window(display._display,
                                              kwargs["size"], kwargs["title"])

        self._display = display
        display._windows[self._window.ptr] = weakref.ref(self)
        self._last_mousemove_time = 0
        self._cursor_hide_timeout = -1

        # FIXME: Use Signal class for callbacks eventually.  This is temporary.
        self.input_callback = None

    def get_display(self):
        return self._display

    def show(self):
        self._window.show()

    def hide(self):
        self._window.hide()

    def render_imlib2_image(self, i, dst_pos = (0, 0), src_pos = (0, 0),
                            size = (-1, -1), dither = True, blend = False):
        return _Display.render_imlib2_image(self._window, i._image, dst_pos, \
                                            src_pos, size, dither, blend)

    def handle_events(self, events):
        for event, args in events:
            if event == X11Display.XEVENT_MOTION_NOTIFY:
                self._last_mousemove_time = time.time()
                if self._cursor_hide_timeout != 0:
                    self.set_cursor_visible(True)
            elif event == X11Display.XEVENT_KEY_PRESS and self.input_callback:
                self.input_callback(args[0])

        # FIXME: this needs to go on a timer -- need to hook into notifier or
        # something?
        if self._last_mousemove_time and \
               time.time() - self._last_mousemove_time > \
               self._cursor_hide_timeout:
            self._last_mousemove_time = 0
            if self._cursor_hide_timeout >= 0:
                self.set_cursor_visible(False)


    def move(self, pos):
        self._window.set_geometry(pos, (-1, -1))

    def resize(self, size):
        self._window.set_geometry((-1, -1), size)

    def set_geometry(self, pos, size):
        self._window.set_geometry(pos, size)

    def get_geometry(self):
        return self._window.get_geometry()

    def set_cursor_visible(self, visible):
        self._window.set_cursor_visible(visible)

    def set_cursor_hide_timeout(self, timeout):
        self._cursor_hide_timeout = timeout



class EvasWindow(X11Window):
    def __init__(self, engine, display = None, size = (640, 480),
                 title = "Evas", **kwargs):
        import kaa.evas

        if engine == "software_x11":
            f = _Display.new_evas_software_x11
        elif engine == "gl_x11" and hasattr(_Display, "new_evas_gl_x11"):
            f = _Display.new_evas_gl_x11
        else:
            raise ValueError, "Unsupported engine: " + engine

        display = _get_display(display)
        self._evas = kaa.evas.new()
        window = f(self._evas._evas, display._display, size = size,
                   title = title)
        self._evas.output_size_set(size)
        self._evas.viewport_set((0, 0), size)
        super(EvasWindow, self).__init__(display, window)


    def handle_events(self, events):
        needs_render = False
        for event, args in events:
            if event == X11Display.XEVENT_EXPOSE:
                self._evas.damage_rectangle_add((args[0], args[1]))
                needs_render = True
            elif event == X11Display.XEVENT_CONFIGURE_NOTIFY:
                if args[1] != self._evas.output_size_get():
                    # This doesn't act right for gl.
                    self._evas.output_size_set(args[1])
                    self._evas.viewport_set((0, 0), args[1])
                    needs_render = True

        super(EvasWindow, self).handle_events(events)

        if needs_render:
            self._evas.render()


    def get_evas(self):
        return self._evas