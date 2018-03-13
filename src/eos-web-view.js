/*
 * eos-web-view.js
 *
 * Copyright (C) 2018 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author: Juan Pablo Ugarte <ugarte@endlessm.com>
 *
 */

(function() {

/* Semi Public entry point */
window.eos_web_view = {}

/* List of children */
window.eos_web_view.children = {}

var lastState = 0;

/* GdkModifierType */
var GDK_SHIFT_MASK    = 1 << 0;
var GDK_LOCK_MASK     = 1 << 1;
var GDK_CONTROL_MASK  = 1 << 2;
var GDK_MOD1_MASK     = 1 << 3;
var GDK_MOD2_MASK     = 1 << 4;
var GDK_MOD3_MASK     = 1 << 5;
var GDK_MOD4_MASK     = 1 << 6;
var GDK_MOD5_MASK     = 1 << 7;
var GDK_BUTTON1_MASK  = 1 << 8;
var GDK_BUTTON2_MASK  = 1 << 9;
var GDK_BUTTON3_MASK  = 1 << 10;
var GDK_BUTTON4_MASK  = 1 << 11;
var GDK_BUTTON5_MASK  = 1 << 12;
var GDK_SUPER_MASK    = 1 << 26;
var GDK_HYPER_MASK    = 1 << 27;
var GDK_META_MASK     = 1 << 28;
var GDK_RELEASE_MASK  = 1 << 30;

function getButtonMask (button) {
    if (button == 1)
        return GDK_BUTTON1_MASK;
    if (button == 2)
        return GDK_BUTTON2_MASK;
    if (button == 3)
        return GDK_BUTTON3_MASK;
    if (button == 4)
        return GDK_BUTTON4_MASK;
    if (button == 5)
        return GDK_BUTTON5_MASK;
    return 0;
}

function updateForEvent(event) {
    lastState &= ~(GDK_SHIFT_MASK|GDK_CONTROL_MASK|GDK_MOD1_MASK);
    if (event.shiftKey)
        lastState |= GDK_SHIFT_MASK;
    if (event.ctrlKey)
        lastState |= GDK_CONTROL_MASK;
    if (event.altKey)
        lastState |= GDK_MOD1_MASK;
}

/* GdkEventCrossing GDK_ENTER_NOTIFY  */
function on_canvas_mouse_over (event) {
    updateForEvent (event);

    window.webkit.messageHandlers[event.target.id + "_crossing"].postMessage({
        enter: true,
        time: event.timeStamp,
        state: lastState,
        x: event.offsetX,
        y: event.offsetY
    });
}

/* GdkEventCrossing GDK_LEAVE_NOTIFY */
function on_canvas_mouse_out (event) {
    updateForEvent (event);

    window.webkit.messageHandlers[event.target.id + "_crossing"].postMessage({
        enter: false,
        time: event.timeStamp,
        state: lastState,
        x: event.offsetX,
        y: event.offsetY
    });
}

/* GdkEventButton GDK_BUTTON_PRESS */
function on_canvas_mouse_down (event) {
    let button = event.button + 1;

    updateForEvent (event);
    lastState = lastState | getButtonMask (button);

    window.webkit.messageHandlers[event.target.id + "_button"].postMessage({
        press: true,
        button: button,
        time: event.timeStamp,
        state: lastState,
        x: event.offsetX,
        y: event.offsetY
    });
}

/* GdkEventButton GDK_BUTTON_RELEASE */
function on_canvas_mouse_up (event) {
    let button = event.button + 1;

    updateForEvent (event);
    lastState = lastState | getButtonMask (button);

    window.webkit.messageHandlers[event.target.id + "_button"].postMessage({
        press: false,
        button: button,
        time: event.timeStamp,
        state: lastState,
        x: event.offsetX,
        y: event.offsetY
    });
}

/* GdkEventMotion GDK_MOTION_NOTIFY */
function on_canvas_mouse_move (event) {
    updateForEvent (event);

    window.webkit.messageHandlers[event.target.id + "_motion"].postMessage({
        time: event.timeStamp,
        state: lastState,
        x: event.offsetX,
        y: event.offsetY
    });
}

window.addEventListener ('load', () => {
    var elements = document.getElementsByClassName('EosWebViewChild');

    /* Initialize all EknWebViewCanvas elements */
    for (let i = 0, n = elements.length; i < n; i++) {
        let canvas = elements[i];

        if (!canvas.id || canvas.tagName !== 'CANVAS')
            continue;

        /* Keep a reference in a hash table for quick access */
        eos_web_view.children[canvas.id] = canvas;

        /* Marshal events */
        canvas.onmouseover = on_canvas_mouse_over;
        canvas.onmouseout = on_canvas_mouse_out;
        canvas.onmousedown = on_canvas_mouse_down;
        canvas.onmouseup = on_canvas_mouse_up;
        canvas.onmousemove = on_canvas_mouse_move;
    }
});

/* Semi public function to update widget surface */
window.eos_web_view.update_canvas = function (id, width, height) {
    let canvas = eos_web_view.children[id];

    /* Resize canvas */
    canvas.width = width;
    canvas.height = height;

    /* TODO: can we use texture_from_pixmap with WebGL? */

    /* Get image data */
    let xhr = new XMLHttpRequest();
    xhr.open('GET', 'eosimagedata:///'+id, false);
    xhr.responseType = 'arraybuffer';
    xhr.send();

    /* Update canvas */
    if (xhr.response) {
        var ctx = canvas.getContext('2d');
        let data = new Uint8ClampedArray(xhr.response);
        let image = new ImageData(data, width, height);

        ctx.putImageData(image, 0, 0);

        /* Help GC */
        data = null;
        image = null;
    }

    xhr = null;

    /* Hint EknWebView we are done with image data */
    window.webkit.messageHandlers.update_canvas_done.postMessage({});
}

})();
