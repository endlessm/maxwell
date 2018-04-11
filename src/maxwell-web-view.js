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
    let positions = null;

    for (let i = 0, len = children.length; i < len; i++) {
        let child = children[i];
        let child_rect = child.maxwell.rect;
        let rect = child.getBoundingClientRect();

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

            /* Save original display value */
            child.maxwell_display_value = child.style.display;

            /* Hide all widgets by default */
            child.style.display = 'none';

            /* Make sure canvas content do not get stretched when the style size changes */
            child.style.objectFit = 'none';

            /* Make sure content position is at start */
            child.style.objectPosition = '0px 0px';

            /* And set no size (canvas default is 300x150) */
            child.width = 0;
            child.height = 0;

            /* Setup child data */
            child.maxwell = {
                dom_width: (child.style.width && child.style.width !== 'auto') || false,
                dom_height: (child.style.height && child.style.height !== 'auto') || false,
            };

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

/* Get image data */
function get_image (id, image_id, width, height) {
    let xhr = new XMLHttpRequest();
    let uri = 'maxwell:///' + id;

    if (image_id)
        uri += '?' + image_id;

    xhr.open('GET', uri, false);
    xhr.responseType = 'arraybuffer';

    try {
        xhr.send();
    } catch (error) {
        return null;
    }

    if (xhr.response) {
        let data = new Uint8ClampedArray(xhr.response);
        return new ImageData(data, width, height);
    }

    return null;
}

/* Semi Public API */

/* Main entry point */
window.maxwell = {};

/* child_resize()
 */
window.maxwell.child_resize = function (id, width, height, minWidth, minHeight) {
    let child = children_hash[id];

    if (!child || (child.width === width && child.height === height))
        return;

    /* Get image data first */
    let image = get_image(id, null, width, height);

    /* Resize canvas to the actual widget allocation */
    child.width = width;
    child.height = height;

    /* Minimum size as returned by gtk_widget_get_preferred_size() */
    child.style.minWidth = minWidth + 'px';
    child.style.minHeight = minHeight + 'px';

    /* Force DOM tree to honor sizes from GTK */
    if (!child.maxwell.dom_width)
        child.style.width = minWidth + 'px';

    if (!child.maxwell.dom_height)
        child.style.height = minHeight + 'px';

    /* Update contents ASAP */
    if (image) {
        let ctx = child.getContext('2d');
        ctx.globalCompositeOperation = "copy";
        ctx.putImageData(image, 0, 0);
    }
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
    let image = get_image(id, image_id, width, height);

    /* Update contents */
    if (image) {
        let ctx = child.getContext('2d');
        ctx.globalCompositeOperation = "copy";
        ctx.putImageData(image, x, y);
    }
}

/* child_set_visible()
 *
 * Show/hide widget element
 */
window.maxwell.child_set_visible = function (id, visible) {
    let child = children_hash[id];

    if (child)
        child.style.display = (visible) ? child.maxwell_display_value : 'none';
}

})();
