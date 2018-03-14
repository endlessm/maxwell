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

function update_positions () {
    let children = window.eos_web_view.children;

    for (var id in children) {
        let canvas = children[id];
        let rect = canvas.getBoundingClientRect();
        let x = rect.x;
        let y = rect.y;

        /* Bail if position did not changed */
        if (canvas.eos_position_x === x && canvas.eos_position_y === y)
            return;

        /* Update position in cache */
        canvas.eos_position_x = x;
        canvas.eos_position_y = y;

        /* Update position in EosWebView */
        window.webkit.messageHandlers.position.postMessage({ id: id, x: x, y: y });
    }
}

window.addEventListener("scroll", update_positions, false);
window.addEventListener("resize", update_positions, false);

window.addEventListener('load', () => {
    var elements = document.getElementsByClassName('EosWebViewChild');

    /* Initialize all EknWebViewCanvas elements */
    for (let i = 0, len = elements.length; i < len; i++) {
        let canvas = elements[i];

        if (!canvas.id || canvas.tagName !== 'CANVAS')
            continue;

        /* Keep a reference in a hash table for quick access */
        eos_web_view.children[canvas.id] = canvas;
    }

    update_positions();

    window.webkit.messageHandlers.allocate.postMessage({});
});

/* Semi public function to update widget surface */
window.eos_web_view.update_canvas = function (id, width, height) {
    let canvas = eos_web_view.children[id];

    /* Resize canvas */
    canvas.width = width;
    canvas.height = height;

    /* Unfortunatelly WebGL does not support texture_from_pixmap */

    /* Get image data */
    let xhr = new XMLHttpRequest();
    xhr.open('GET', 'eosimagedata:///'+id, false);
    xhr.responseType = 'arraybuffer';

    try {
        xhr.send();
    } catch (error) {
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
