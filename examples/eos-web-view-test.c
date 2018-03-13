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
on_button_clicked (GtkWidget *button)
{
  g_message ("%s", __func__);
}

gboolean
on_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  g_message ("%s %s %dx%d", __func__, G_OBJECT_TYPE_NAME (widget),
             (gint)event->x, (gint)event->y);
  return FALSE;
}

gboolean
on_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  g_message ("%s %s %dx%d", __func__, G_OBJECT_TYPE_NAME (widget),
             (gint)event->x, (gint)event->y);
  return FALSE;
}

gboolean
on_enter_notify_event (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  g_message ("%s %s %dx%d", __func__, G_OBJECT_TYPE_NAME (widget),
             (gint)event->x, (gint)event->y);
  return FALSE;
}

gboolean
on_leave_notify_event (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  g_message ("%s %s %dx%d", __func__, G_OBJECT_TYPE_NAME (widget),
             (gint)event->x, (gint)event->y);
  return FALSE;
}

gboolean
on_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  g_message ("%s %s %dx%d", __func__, G_OBJECT_TYPE_NAME (widget),
             (gint)event->x, (gint)event->y);
  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *webview, *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  webview = eos_web_view_new ();

  gtk_container_add (GTK_CONTAINER (window), webview);
  gtk_widget_show_all (window);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview),
                             "<html>"
                             "  <h1>EosWebview Test</h1>"
                             "   <canvas class=\"EosWebViewChild\" id=\"button\"></canvas>"
                             "</html>",
                             "file://");

  while (webkit_web_view_is_loading (WEBKIT_WEB_VIEW (webview)))
    gtk_main_iteration_do (FALSE);

  button = gtk_button_new_with_label ("A button inside an EosWebView");
  g_signal_connect (button, "clicked", G_CALLBACK (on_button_clicked), NULL);
  g_signal_connect (button, "enter-notify-event", G_CALLBACK (on_enter_notify_event), NULL);
  g_signal_connect (button, "leave-notify-event", G_CALLBACK (on_leave_notify_event), NULL);
  g_signal_connect (button, "button-press-event", G_CALLBACK (on_button_press_event), NULL);
  g_signal_connect (button, "button-release-event", G_CALLBACK (on_button_release_event), NULL);
  g_signal_connect (button, "motion-notify-event", G_CALLBACK (on_motion_notify_event), NULL);

  gtk_container_add (GTK_CONTAINER (webview), button);
  gtk_container_child_set (GTK_CONTAINER (webview), button,
                           "canvas-id", "button",
                          NULL);
  gtk_widget_show_all (button);

  gtk_main();

  return 0;
}
