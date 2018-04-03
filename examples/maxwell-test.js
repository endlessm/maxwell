/*
 * maxwell-test.js
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

const Gtk = imports.gi.Gtk;
const Maxwell = imports.gi.Maxwell;

Gtk.init(null);

let window = new Gtk.Window({ default_width: 640, default_height: 480 });

window.connect('delete-event', () => { Gtk.main_quit(); return true; });
let webview = new Maxwell.WebView ();

window.add(webview);

webview.load_html (`
<html>
  <h1>MaxwellWebview Test</h1>
Lorem ipsum dolor sit amet, consectetur adipiscing elit,<br>
  <h2>A GtkButton</h2>
   <canvas class="GtkWidget" id="button"></canvas>
Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.<br>
  <h2>A HTML text input</h2>
  <input type="text">
  <h2>A GtkEntry</h2>
   <canvas class="GtkWidget" id="entry"></canvas>
sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.<br>
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.<br>
Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.<br>
  <br>
Lorem ipsum dolor sit amet, consectetur adipiscing elit,<br>
sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.<br>
Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.<br>
Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.<br>
Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.
</html>`, 'file://');

let button = new Gtk.Button({ name: "button", label: 'A Gtk Button' });
let entry = new Gtk.Entry({ name: "entry" });

webview.add (button);
webview.add (entry);

window.show_all();

Gtk.main();

