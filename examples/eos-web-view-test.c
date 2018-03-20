/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * eos-web-view-test.c
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

#include "eos-web-view.h"

static void
on_button_clicked (GtkWidget *button, GtkLabel *label)
{
  g_message ("%s", __func__);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *webview;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *entry;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 640);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  webview = eos_web_view_new ();

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

  entry = gtk_entry_new ();

  button = gtk_button_new_with_label ("A GtkButton");
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  label = gtk_label_new ("a GtkLabel");
  gtk_container_add (GTK_CONTAINER (box), label);

  button = gtk_button_new_with_label ("Another button");
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  label = gtk_label_new ("another GtkLabel");

  eos_web_view_pack_child (EOS_WEB_VIEW (webview), label, "label");
  eos_web_view_pack_child (EOS_WEB_VIEW (webview), box, "box");
  eos_web_view_pack_child (EOS_WEB_VIEW (webview), entry, "entry");

  gtk_container_add (GTK_CONTAINER (window), webview);
  gtk_widget_show_all (window);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview),
                             "<html>"
                             "  <h1>EosWebview Test</h1>"
                             "Lorem ipsum dolor sit amet, consectetur adipiscing elit,<br>"
                             "  <h2>A GtkButton</h2>"
                             "   <canvas class=\"EosWebViewChild\" id=\"box\"></canvas>"
                             "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.<br>"
                             "  <h2>A HTML text input</h2>"
                             "  <input type=\"text\">"
                             "  <h2>A GtkEntry</h2>"
                             "   <canvas class=\"EosWebViewChild\" id=\"entry\"></canvas>"
                             "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.<br>"
                             "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.<br>"
                             "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.<br>"
                             "<br>"
                             "<canvas class=\"EosWebViewChild\" id=\"label\"></canvas>"
                             "Lorem ipsum dolor sit amet, consectetur adipiscing elit,<br>"
                             "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.<br>"
                             "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.<br>"
                             "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.<br>"
                             "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.<br>"
                             "<br>"
                             "</html>",
                             "file://");

  gtk_main();

  return 0;
}
