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

function throttle (func, limit) {
    let stamp, id;

    return () => {
        let self = this;
        let args = arguments;

        if (!stamp) {
            func.apply(self, args);
            stamp = Date.now();
            return;
        }

        clearTimeout(id);
        id = setTimeout(() => {
            let now = Date.now();
            if (now - stamp >= limit) {
                func.apply(self, args);
                stamp = now;
            }
        }, limit - (Date.now() - stamp));
    }
}

function update_positions () {
    let children = window.eos_web_view.children;
    let positions = new Array ();

    for (let id in children) {
        let child = children[id];
        let rect = child.getBoundingClientRect();
        let x = rect.x;
        let y = rect.y;

        /* Bail if position did not changed */
        if (child.eos_position_x === x && child.eos_position_y === y)
            continue;

        /* Update position in cache */
        child.eos_position_x = x;
        child.eos_position_y = y;

        /* Collect new positions */
        positions.push ({ id: id, x: x, y: y });
    }

    /* Update all positions in EosWebView at once to reduce messages */
    window.webkit.messageHandlers.update_positions.postMessage(positions);
    position_timeout_id = null;
}

/* Limit to 10Hz */
let update_positions_throttled = throttle(update_positions, 100);

/* We need to update widget positions on scroll and resize events */
window.addEventListener("scroll", update_positions_throttled, { passive: true });
window.addEventListener("resize", update_positions_throttled, { passive: true });

/* We also need to update it on any DOM change */
function document_mutation_handler (mutations) {
    let children = new Array ();

    for (var mutation of mutations) {
        if (mutation.type !== 'childList')
            continue;

        for (let i = 0, len = mutation.addedNodes.length; i < len; i++) {
            let child = mutation.addedNodes[i];

            if (!child.id || child.tagName !== 'CANVAS' ||
                !child.classList.contains('EosWebViewChild'))
                continue;

            /* Save original display value */
            child.eos_display_value = child.style.display;

            /* Hide all widgets by default */
            child.style.display = 'none';

            /* And set no size (canvas default is 300x150) */
            child.width = 0;
            child.height = 0;

            /* Keep a reference in a hash table for quick access */
            eos_web_view.children[child.id] = child;

            /* Collect children to allocate */
            children.push(child.id);
        }
    }

    /* Allocate GtkWidget, canvas size will change the first time
     * child_draw() is called and we actually have something to show
     */
    window.webkit.messageHandlers.children_allocate.postMessage(children);

    /* Extra paranoid, update positions if anything changes in the DOM tree!
     * ideally it would be nice to directly observe BoundingClientRect changes.
     */
    update_positions_throttled();
};

/* Main DOM observer */
let observer = new MutationObserver(document_mutation_handler);
observer.observe(document, {
    childList: true,
    subtree: true,
    attributes: true
});

/* Semi Public API */

/* Main entry point */
window.eos_web_view = {}

/* List of children */
window.eos_web_view.children = {}

/* child_resize()
 */
window.eos_web_view.child_resize = function (id, width, height) {
    let child = eos_web_view.children[id];

    if (!child)
        return;

    /* Resize canvas keeping the old contents */
    if (child.width !== width || child.height !== height) {
        let ctx = child.getContext('2d');
        var old_image = ctx.getImageData(0, 0, child.width, child.height);

        child.width = width;
        child.height = height;

        ctx.putImageData(old_image, 0, 0);

        old_image = null;
    }
}

/* child_draw()
 *
 * Draw child canvas with eosimagedata:///canvasid image
 *
 * Unfortunatelly WebGL does not support texture_from_pixmap but we might be able
 * to use GL to implement this function if we can get the context from WebKit
 * itself
 */
window.eos_web_view.child_draw = function (id, image_id, x, y, width, height) {
    let child = eos_web_view.children[id];

    if (!child)
        return;

    let ctx = child.getContext('2d');

    /* Get image data */
    let xhr = new XMLHttpRequest();
    let uri = 'eosimagedata:///' + id;
    if (image_id)
        uri += '?' + image_id;
    xhr.open('GET', uri, false);
    xhr.responseType = 'arraybuffer';

    try {
        xhr.send();
    } catch (error) {
        return;
    }

    /* Update child canvas */
    if (xhr.response) {
        let data = new Uint8ClampedArray(xhr.response);
        let image = new ImageData(data, width, height);

        ctx.putImageData(image, x, y);

        /* Help GC */
        data = null;
        image = null;
    }

    xhr = null;
}

/* child_set_visible()
 *
 * Show/hide widget element
 */
window.eos_web_view.child_set_visible = function (id, visible) {
    let child = eos_web_view.children[id];

    if (child)
        child.style.display = (visible) ? child.eos_display_value : 'none';
}

/* child_init()
 *
 */
window.eos_web_view.child_init = function (id, width, height, visible) {
    let child = eos_web_view.children[id];

    if (!child)
        return;

    child.width = width;
    child.height = height;
    window.eos_web_view.child_draw (id, null, 0, 0, width, height);

    if (visible)
        window.eos_web_view.child_set_visible (id, visible);
}

/* Signal EosWebView the script has finished loading */
window.webkit.messageHandlers.script_loaded.postMessage({});

})();
