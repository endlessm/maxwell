/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * maxwell-test.c
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

#include "maxwell.h"

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

  webview = maxwell_web_view_new ();

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_name (box, "box");
  gtk_container_add (GTK_CONTAINER (webview), box);

  button = gtk_button_new_with_label ("A GtkButton");
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  label = gtk_label_new ("a GtkLabel");
  gtk_container_add (GTK_CONTAINER (box), label);

  button = gtk_button_new_with_label ("Another button");
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  entry = gtk_entry_new ();
  gtk_widget_set_name (entry, "entry");
  gtk_container_add (GTK_CONTAINER (webview), entry);

  label = gtk_label_new ("another GtkLabel");
  gtk_widget_set_name (label, "label");
  gtk_container_add (GTK_CONTAINER (webview), label);

  gtk_container_add (GTK_CONTAINER (window), webview);
  gtk_widget_show_all (window);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview),
                             "<html>"
                             "  <h1>MaxwellWebview Test</h1>"
                             "Lorem ipsum dolor sit amet, consectetur adipiscing elit,<br>"
                             "  <h2>A GtkButton</h2>"
                             "   <canvas class=\"GtkWidget\" id=\"box\" style=\"width: 100%;\"></canvas>"
                             "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.<br>"
                             "  <h2>A HTML text input</h2>"
                             "  <input type=\"text\">"
                             "  <h2>A GtkEntry</h2>"
                             "   <canvas class=\"GtkWidget\" id=\"entry\"></canvas>"
                             "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.<br>"
                             "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.<br>"
                             "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.<br>"
                             "<br>"
                             "<canvas class=\"GtkWidget\" id=\"label\"></canvas>"
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
