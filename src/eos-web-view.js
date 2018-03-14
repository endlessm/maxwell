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

function update_root_position (id) {
    let canvas = window.eos_web_view.children[id];
    let e = canvas;
    let root_x = 0;
    let root_y = 0;

    while (e) {
        if (e.tagName == "BODY") {
            let scroll_x = e.scrollLeft || document.documentElement.scrollLeft;
            let scroll_y = e.scrollTop || document.documentElement.scrollTop;
            root_x += e.offsetLeft - scroll_x + e.clientLeft;
            root_y += e.offsetTop - scroll_y + e.clientTop;
        } else {
            root_x += e.offsetLeft - e.scrollLeft + e.clientLeft;
            root_y += e.offsetTop - e.scrollTop + e.clientTop;
        }
      e = e.offsetParent;
    }

    /* Bail if position did not changed */
    if (canvas.hasOwnProperty ('eos_position')) {
        let old_pos = canvas.eos_position;
        if (old_pos && old_pos.x === root_x && old_pos.y === root_y)
            return;
    }

    /* Update position in cache */
    canvas.eos_position = { x: root_x, y: root_y };

    /* Update position in EosWebView */
    window.webkit.messageHandlers.position.postMessage({ id: id, x: root_x, y: root_y });
}

function update_positions () {
    for (var id in window.eos_web_view.children)
        update_root_position (id);
}

window.addEventListener("scroll", update_positions, false);
window.addEventListener("resize", update_positions, false);

window.addEventListener('load', () => {
    var elements = document.getElementsByClassName('EosWebViewChild');
    let allocate = false;

    /* Initialize all EknWebViewCanvas elements */
    for (let i = 0, len = elements.length; i < len; i++) {
        let canvas = elements[i];

        if (!canvas.id || canvas.tagName !== 'CANVAS')
            continue;

        /* Keep a reference in a hash table for quick access */
        eos_web_view.children[canvas.id] = canvas;

        update_root_position(canvas.id);
        allocate = true;
    }

    if (allocate)
        window.webkit.messageHandlers.allocate.postMessage({});
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

    try {
        xhr.send();
    } catch (error) {
        console.error (error);
        return;
    }

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
