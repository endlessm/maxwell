/*
 * maxwell-web-view.js
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

let children = [];             /* List of children */
let children_hash = new Map(); /* Hash table of children */

function update_position_size () {
    let scale = window.devicePixelRatio;
    let positions = null;

    for (let i = 0, len = children.length; i < len; i++) {
        let child = children[i];
        let child_rect = child.maxwell.rect;
        let rect = child.getBoundingClientRect();

        /* FIXME: Setting CSS zoom property breaks getBoundingClientRect()
         *
         * X and Y are divided by zoom instead of multiplying width and height
         * by it.
         * Note that zoom is 1/scale so to get the original value we need to
         * add scroll then divide by scale, which is the same as multiplying by
         * zoom and finally subtract scroll.
         *
         * See: https://bugs.webkit.org/show_bug.cgi?id=185034
         */
        if (scale !== 1) {
            rect.x = ((rect.x + window.scrollX) / scale) - window.scrollX;
            rect.y = ((rect.y + window.scrollY) / scale) - window.scrollY;
            rect.width /= scale;
            rect.height /= scale;
        }

        /* Bail if position did not changed */
        if (child_rect &&
            child_rect.x === rect.x &&
            child_rect.y === rect.y &&
            child_rect.width === rect.width &&
            child_rect.height === rect.height)
            continue;

        /* Update position in cache */
        child.maxwell.rect = rect;

        /* Ensure array */
        if (!positions)
            positions = [];

        /* Collect new positions */
        positions.push({
            id: child.id,
            x: rect.x,
            y: rect.y,
            width: child.maxwell.dom_width ? rect.width : -1,
            height: child.maxwell.dom_height ? rect.height : -1
        });
    }

    /* Update all positions in MaxwellWebView at once to reduce messages */
    if (positions)
        window.webkit.messageHandlers.maxwell_children_move_resize.postMessage(positions);
}

/* We need to update widget positions on scroll and resize events */
window.addEventListener("scroll", update_position_size, { passive: true });
window.addEventListener("resize", update_position_size, { passive: true });

/* We also need to update it on any DOM change */
function document_mutation_handler (mutations) {
    let new_children = null;

    for (var mutation of mutations) {
        if (mutation.type !== 'childList')
            continue;

        for (let i = 0, len = mutation.addedNodes.length; i < len; i++) {
            let child = mutation.addedNodes[i];

            if (!child.id || child.tagName !== 'CANVAS' ||
                !child.classList.contains('GtkWidget'))
                continue;

            /* Setup child data */
            child.maxwell = {
                display_value: child.style.display,
                draw_requests: [],
                dom_width: (child.style.width && child.style.width !== 'auto') || false,
                dom_height: (child.style.height && child.style.height !== 'auto') || false,
            };

            /* Hide all widgets by default */
            child.style.display = 'none';

            /* Make sure canvas content do not get stretched when the style size changes */
            child.style.objectFit = 'none';

            /* Make sure content position is at start */
            child.style.objectPosition = 'left top';

            /* And set no size (canvas default is 300x150) */
            child.width = 0;
            child.height = 0;

            /* Keep a reference in a hash table for quick lookup */
            children_hash[child.id] = child;

            /* And another one in an array for quick iteration */
            children.push(child);

            /* Ensure array */
            if (!new_children)
                new_children = [];

            /* Collect children to allocate */
            new_children.push({
                id: child.id,
                use_dom_size: (child.maxwell.dom_width || child.maxwell.dom_height),
            });
        }
    }

    if (new_children)
        window.webkit.messageHandlers.maxwell_children_init.postMessage(new_children);

    /* Extra paranoid, update positions if anything changes in the DOM tree!
     * ideally it would be nice to directly observe BoundingClientRect changes.
     */
    update_position_size();
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
window.maxwell = {};

/* child_resize()
 */
window.maxwell.child_resize = function (id, width, height, minWidth, minHeight) {
    let scale = window.devicePixelRatio;
    let child = children_hash[id];

    /* On HiDPI we make the canvas bigger so the backing store matches the
     * display resolution and the set zoom CSS property acordingly so it will
     * be rendered at the right resolution.
     */
    if (scale !== 1) {
        width *= scale;
        height *= scale;
        minWidth *= scale;
        minHeight *= scale;
    }

    if (!child || (child.width === width && child.height === height))
        return;

    if (scale !== 1)
        child.style.zoom = 1/scale;

    /* Abort all pending draw request */
    child.maxwell.draw_requests.forEach((req) => { req.abort(); });
    child.maxwell.draw_requests = [];

    /* Resizing canvas clears it, so we need to save the image contents */
    let ctx = child.getContext('2d');
    let image = null;

    if (child.width && child.height)
        image = ctx.getImageData(0, 0, child.width, child.height);

    /* Resize canvas to the actual widget allocation */
    child.width = width;
    child.height = height;

    /* Repaint old image to avoid flickering */
    if (image) {
        ctx.globalCompositeOperation = "copy";
        ctx.putImageData(image, 0, 0);
    }

    /* Minimum size as returned by gtk_widget_get_preferred_size() */
    child.style.minWidth = minWidth + 'px';
    child.style.minHeight = minHeight + 'px';

    /* Force DOM tree to honor sizes from GTK */
    if (!child.maxwell.dom_width)
        child.style.width = minWidth + 'px';

    if (!child.maxwell.dom_height)
        child.style.height = minHeight + 'px';
}

function on_child_draw_load () {
    let child = this.maxwell.child;
    let requests = child.maxwell.draw_requests;
    let ctx = child.getContext('2d');
    let i, len;

    /* Remove request if failed, otherwise it will be removed once drawn */
    if (!this.response)
        requests.splice(requests.indexOf(this), 1);

    ctx.globalCompositeOperation = "copy";

    for (i = 0, len = requests.length; i < len && requests[i].response; i++) {
        let dReq = requests[i];
        let data = new Uint8ClampedArray(dReq.response);

        try {
            let scale = window.devicePixelRatio;
            let image = new ImageData(data, dReq.maxwell.width * scale, dReq.maxwell.height * scale);

            /* Update contents */
            ctx.putImageData(image, dReq.maxwell.x * scale, dReq.maxwell.y * scale);
        } catch (error) {
            console.log(error);
        }
    }

    /* Remove all request that were drawn */
    requests.splice(0, i);
}

/* child_draw()
 *
 * Draw child canvas with maxwell:///canvasid image
 *
 * Unfortunatelly WebGL does not support texture_from_pixmap but we might be able
 * to use GL to implement this function if we can get the context from WebKit
 * itself
 */
window.maxwell.child_draw = function (id, image_id, x, y, width, height) {
    let child = children_hash[id];

    if (!child)
        return;

    /* Get image data */
    let xhr = new XMLHttpRequest();

    xhr.open('GET', 'maxwell:///' + image_id);
    xhr.responseType = 'arraybuffer';
    xhr.addEventListener('load', on_child_draw_load);
    xhr.maxwell = { child, x, y, width, height };

    /* Add request to stack */
    child.maxwell.draw_requests.push(xhr);

    try {
        xhr.send();
    } catch (error) {
    }
}

/* child_set_visible()
 *
 * Show/hide widget element
 */
window.maxwell.child_set_visible = function (id, visible) {
    let child = children_hash[id];

    if (!child)
        return;

    child.style.display = (visible) ? child.maxwell.display_value : 'none';
}

})();
