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

    for (let id in children) {
        let canvas = children[id];
        let rect = canvas.getBoundingClientRect();
        let x = rect.x;
        let y = rect.y;

        /* Bail if position did not changed */
        if (canvas.eos_position_x === x && canvas.eos_position_y === y)
            continue;

        /* Update position in cache */
        canvas.eos_position_x = x;
        canvas.eos_position_y = y;

        /* Update position in EosWebView */
        window.webkit.messageHandlers.position.postMessage({ id: id, x: x, y: y });
    }
}

/* We need to update widget positions on scroll and resize events */
window.addEventListener("scroll", update_positions, { passive: true });
window.addEventListener("resize", update_positions, { passive: true });

/* We also need to update it on any DOM change */
function document_mutation_handler (mutationsList) {
    for (var mutation of mutationsList) {
        if (mutation.type === 'childList') {
            for (let i = 0, len = mutation.addedNodes.length; i < len; i++) {
                let canvas = mutation.addedNodes[i];

                if (!canvas.id || canvas.tagName !== 'CANVAS' ||
                    !canvas.classList.contains ('EosWebViewChild'))
                    continue;

                /* Keep a reference in a hash table for quick access */
                eos_web_view.children[canvas.id] = canvas;
                window.webkit.messageHandlers.allocate.postMessage(canvas.id);
            }
        }
    }

    /* Extra paranoid, update positions if anything changes in the DOM tree!
     * ideally it would be nice to directly observe BoundingClientRect changes.
     */
    update_positions ();
};

/* Main DOM observer */
let observer = new MutationObserver(document_mutation_handler);

observer.observe(document, {
    childList: true,
    subtree: true,
    attributes: true
});

/* Semi public function to update widget surface */
window.eos_web_view.update_canvas = function (id, width, height) {
    let canvas = eos_web_view.children[id];

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
