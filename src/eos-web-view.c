/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * eos-web-view.c
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
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>

struct _EosWebView
{
  GtkOffscreenWindow parent;
};

typedef struct
{
  GList      *children;     /* List of GtkWidget */
  GHashTable *offscreens;   /* List of GtkOffscreenWindow wrappers for children with a canvas-id */
  GdkPixbuf  *pixbuf;       /* Temporal copy of offscreen window while handling eosdataimage:// */
} EosWebViewPrivate;

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CANVAS_ID,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_properties[N_CHILD_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (EosWebView, eos_web_view, WEBKIT_TYPE_WEB_VIEW);

#define RESOURCES_PATH "/com/endlessm/eos-web-view"
#define EOS_WEB_VIEW_PRIVATE(d) ((EosWebViewPrivate *) eos_web_view_get_instance_private((EosWebView*)d))

static void
eos_web_view_wrap_child (EosWebView *webview, GtkWidget *child, const gchar *id);

static void
eos_web_view_init (EosWebView *self)
{
  EosWebViewPrivate *priv = EOS_WEB_VIEW_PRIVATE (self);

  priv->children   = NULL;
  priv->pixbuf     = NULL;
  priv->offscreens = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            g_object_unref);
}

static void
eos_web_view_dispose (GObject *object)
{
  EosWebViewPrivate *priv = EOS_WEB_VIEW_PRIVATE (object);
  GList *l;

  g_clear_pointer (&priv->offscreens, g_hash_table_unref);
  g_clear_object (&priv->pixbuf);

  for (l = priv->children; l; l = g_list_next (l))
    gtk_widget_destroy (gtk_widget_get_parent (l->data));

  g_clear_pointer (&priv->children, g_list_free);

  G_OBJECT_CLASS (eos_web_view_parent_class)->dispose (object);
}

static void
on_image_data_uri_scheme_request (WebKitURISchemeRequest *request,
                                  gpointer                user_data)
{
  const gchar *path = webkit_uri_scheme_request_get_path (request);
  EosWebViewPrivate *priv = EOS_WEB_VIEW_PRIVATE (user_data);
  GtkOffscreenWindow *offscreen;

  if (path && *path == '/' &&
      (offscreen = g_hash_table_lookup (priv->offscreens, &path[1])))
    {
      g_clear_object (&priv->pixbuf);
      priv->pixbuf = gtk_offscreen_window_get_pixbuf (offscreen);

      if (gdk_pixbuf_get_colorspace (priv->pixbuf) == GDK_COLORSPACE_RGB &&
          gdk_pixbuf_get_bits_per_sample (priv->pixbuf) == 8 &&
          gdk_pixbuf_get_has_alpha (priv->pixbuf))
        {
          GInputStream *stream = NULL;
          gsize stream_length;

          stream_length = gdk_pixbuf_get_height (priv->pixbuf) *
                          gdk_pixbuf_get_rowstride (priv->pixbuf);

          stream = g_memory_input_stream_new_from_data (gdk_pixbuf_read_pixels (priv->pixbuf),
                                                        stream_length, NULL);
          webkit_uri_scheme_request_finish (request,
                                            stream,
                                            stream_length,
                                            "application/octet-stream");
          g_object_unref (stream);
          return;
        }
    }

  webkit_uri_scheme_request_finish_error (request, NULL);
}

static void
eos_web_view_set_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EosWebViewPrivate *priv;

  g_return_if_fail (EOS_IS_WEB_VIEW (container));
  priv = EOS_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (g_list_find (priv->children, child));

  switch (property_id)
    {
      case CHILD_PROP_CANVAS_ID:
        eos_web_view_wrap_child (EOS_WEB_VIEW (container),
                                 child,
                                 g_value_get_string (value));
      break;
      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
eos_web_view_get_child_property (GtkContainer *container,
                                 GtkWidget    *child,
                                 guint         property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  EosWebViewPrivate *priv;

  g_return_if_fail (EOS_IS_WEB_VIEW (container));
  priv = EOS_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (g_list_find (priv->children, child));

  switch (property_id)
    {
      case CHILD_PROP_CANVAS_ID:
        g_value_set_string (value, g_object_get_data (G_OBJECT (child), "EosWebViewId"));
      break;
      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
eos_web_view_constructed (GObject *object)
{
  WebKitWebView *webview = WEBKIT_WEB_VIEW (object);
  WebKitUserContentManager *content_manager;
  WebKitSecurityManager *security_manager;
  WebKitWebContext *context;
  WebKitUserScript *script;
  GBytes *script_source;

  G_OBJECT_CLASS (eos_web_view_parent_class)->constructed (object);

  /* Give XMlHttpRequest a chance to work */
  g_object_set (webkit_web_view_get_settings (webview),
                "allow-universal-access-from-file-urls", TRUE,
                "allow-file-access-from-file-urls", TRUE,
                "enable-write-console-messages-to-stdout", TRUE,
                NULL);

  /* Install custom URI scheme to inject image buffers */
  context = webkit_web_view_get_context (webview);
  security_manager = webkit_web_context_get_security_manager (context);
  webkit_security_manager_register_uri_scheme_as_cors_enabled (security_manager,
                                                               "eosimagedata");
  webkit_web_context_register_uri_scheme (context, "eosimagedata",
                                          on_image_data_uri_scheme_request,
                                          object, NULL);

  /* Add script */
  content_manager = webkit_web_view_get_user_content_manager (webview);
  script_source = g_resources_lookup_data (RESOURCES_PATH"/eos-web-view.js",
                                           G_RESOURCE_LOOKUP_FLAGS_NONE,
                                           NULL);
  script = webkit_user_script_new (g_bytes_get_data (script_source, NULL),
                                   WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                   WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                   NULL, NULL);
  webkit_user_content_manager_add_script (content_manager, script);
}

static void
eos_web_view_add (GtkContainer *container, GtkWidget *child)
{
  EosWebViewPrivate *priv;

  g_return_if_fail (EOS_IS_WEB_VIEW (container));
  priv = EOS_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  priv->children = g_list_prepend (priv->children, child);
}

static void
eos_web_view_remove (GtkContainer *container, GtkWidget *child)
{
  EosWebViewPrivate *priv;
  GtkOffscreenWindow *offscreen;
  const gchar *id;

  g_return_if_fail (EOS_IS_WEB_VIEW (container));
  priv = EOS_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (g_list_find (priv->children, child));

  priv->children = g_list_remove (priv->children, child);

  id = g_object_get_data (G_OBJECT (child), "EosWebViewId");
  offscreen = g_hash_table_lookup (priv->offscreens, id);

  g_hash_table_remove (priv->offscreens, id);
  gtk_widget_destroy (GTK_WIDGET (offscreen));
}

static void
eos_web_view_class_init (EosWebViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = eos_web_view_dispose;
  object_class->constructed = eos_web_view_constructed;

  container_class->add = eos_web_view_add;
  container_class->remove = eos_web_view_remove;
  container_class->set_child_property = eos_web_view_set_child_property;
  container_class->get_child_property = eos_web_view_get_child_property;

  child_properties[CHILD_PROP_CANVAS_ID] =
    g_param_spec_string ("canvas-id",
                         "Canvas Id",
                         "The HTML canvas element id where to embedded child",
                         NULL,
                         G_PARAM_READWRITE);

  gtk_container_class_install_child_properties (container_class,
                                                N_CHILD_PROPERTIES,
                                                child_properties);
}

static void
run_javascrip_finish_handler (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
    WebKitJavascriptResult *js_result;
    GError *error = NULL;

    js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object),
                                                       result,
                                                       &error);
    if (!js_result)
      {
        g_warning ("Error running javascript: %s", error->message);
        g_error_free (error);
        return;
      }
}

static void
run_javascript (EosWebView *webview, const gchar *format, ...)
{
  va_list args;
  gchar *script;

  va_start (args, format);
  script = g_strdup_vprintf (format, args);
  va_end (args);

  webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (webview), script, NULL,
                                  run_javascrip_finish_handler,
                                  NULL);
  g_free (script);
}

static void
on_child_visible_notify (GObject    *object,
                         GParamSpec *pspec,
                         EosWebView *webview)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidget *parent = gtk_widget_get_parent (widget);

  run_javascript (webview,
                  "if (window.hasOwnProperty ('eos_web_view'))"
                  "  eos_web_view.children.%s.style.visibility = '%s';",
                  g_object_get_data (G_OBJECT (parent), "EosWebViewId"),
                  gtk_widget_get_visible (widget) ? "visible" : "hidden");
}

static gboolean
on_offscreen_configure_event (GtkWidget         *offscreen,
                              GdkEventConfigure *event,
                              EosWebView        *webview)
{
  g_message ("%s %dx%d %dx%d", __func__,
             gtk_widget_get_allocated_width (offscreen),
             gtk_widget_get_allocated_height (offscreen),
             event->width,
             event->height);

  return FALSE;
}

static gboolean
on_offscreen_damage_event (GtkWidget      *offscreen,
                           GdkEventExpose *event,
                           EosWebView     *webview)
{
  gint w = gtk_widget_get_allocated_width (offscreen);
  gint h = gtk_widget_get_allocated_height (offscreen);

  /* TODO: update only the parts that actually changed */
  run_javascript (webview,
                  "if (window.hasOwnProperty ('eos_web_view'))"
                  "  eos_web_view.update_canvas('%s', %d, %d);",
                  g_object_get_data (G_OBJECT (offscreen), "EosWebViewId"),
                  w, h);

  return FALSE;
}

static gchar *
js_value_get_string (JSGlobalContextRef context, JSValueRef value)
{
  if (JSValueIsString (context, value))
    {
      JSStringRef js_str = JSValueToStringCopy (context, value, NULL);
      gsize length = JSStringGetMaximumUTF8CStringSize (js_str);
      gchar *str = g_malloc (length);

      JSStringGetUTF8CString (js_str, str, length);
      JSStringRelease (js_str);

      return str;
    }
  return NULL;
}

static JSValueRef
js_object_get_property (JSGlobalContextRef context,
                        JSObjectRef        object,
                        char              *property)
{
  JSStringRef pname = JSStringCreateWithUTF8CString (property);
  JSValueRef retval;

  retval = JSObjectGetProperty (context, object, pname, NULL);

  JSStringRelease (pname);

  return retval;
}

static void
eos_web_view_do_event (GtkWidget *offscreen, GdkEvent *event)
{
  GtkWidget *widget = gtk_bin_get_child (GTK_BIN (offscreen));
  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkSeat *seat = gdk_display_get_default_seat (display);

  event->any.window = g_object_ref (gtk_widget_get_window (widget));
  gdk_event_set_device (event, gdk_seat_get_pointer (seat));
  gdk_event_set_screen (event, gtk_widget_get_screen (widget));

  gtk_widget_event (widget, event);
}

static void
handle_script_message_crossing (WebKitUserContentManager *manager,
                                WebKitJavascriptResult   *result,
                                GtkWidget                *offscreen)
{
  JSGlobalContextRef context = webkit_javascript_result_get_global_context (result);
  JSValueRef value = webkit_javascript_result_get_value (result);

  if (JSValueIsObject (context, value))
    {
      JSObjectRef obj = JSValueToObject (context, value, NULL);
      JSValueRef enter = js_object_get_property (context, obj, "enter");
      JSValueRef time = js_object_get_property (context, obj, "time");
      JSValueRef state = js_object_get_property (context, obj, "state");
      JSValueRef x = js_object_get_property (context, obj, "x");
      JSValueRef y = js_object_get_property (context, obj, "y");
      GdkEventCrossing *e;
      GdkEvent *event;

      event = gdk_event_new (JSValueToBoolean (context, enter) ?
                             GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);

      e = (GdkEventCrossing *)event;
      e->send_event = TRUE;
      e->subwindow = NULL;
      e->time = JSValueToNumber (context, time, NULL);
      e->x = JSValueToNumber (context, x, NULL);
      e->y = JSValueToNumber (context, y, NULL);
      e->x_root = e->x;
      e->y_root = e->y;
      e->mode = GDK_CROSSING_NORMAL;
      e->detail = GDK_NOTIFY_ANCESTOR;
      e->focus = TRUE;
      e->state = JSValueToNumber (context, state, NULL);

      eos_web_view_do_event (offscreen, event);
      gdk_event_free (event);
    }
  else
    {
      g_warning ("Error running javascript: unexpected return value");
    }
}

static void
handle_script_message_button (WebKitUserContentManager *manager,
                              WebKitJavascriptResult   *result,
                              GtkWidget                *offscreen)
{
  JSGlobalContextRef context = webkit_javascript_result_get_global_context (result);
  JSValueRef value = webkit_javascript_result_get_value (result);

  if (JSValueIsObject (context, value))
    {
      JSObjectRef obj = JSValueToObject (context, value, NULL);
      JSValueRef press = js_object_get_property (context, obj, "press");
      JSValueRef time = js_object_get_property (context, obj, "time");
      JSValueRef state = js_object_get_property (context, obj, "state");
      JSValueRef button = js_object_get_property (context, obj, "button");
      JSValueRef x = js_object_get_property (context, obj, "x");
      JSValueRef y = js_object_get_property (context, obj, "y");
      GdkEventButton *e;
      GdkEvent *event;

      event = gdk_event_new (JSValueToBoolean (context, press) ?
                             GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);

      e = (GdkEventButton *)event;
      e->send_event = TRUE;
      e->time = JSValueToNumber (context, time, NULL);
      e->x = JSValueToNumber (context, x, NULL);
      e->y = JSValueToNumber (context, y, NULL);
      e->axes = NULL;
      e->state = JSValueToNumber (context, state, NULL);
      e->button = JSValueToNumber (context, button, NULL);
      e->x_root = e->x;
      e->y_root = e->y;

      eos_web_view_do_event (offscreen, event);
      gdk_event_free (event);
    }
  else
    {
      g_warning ("Error running javascript: unexpected return value");
    }
}

static void
handle_script_message_motion (WebKitUserContentManager *manager,
                              WebKitJavascriptResult   *result,
                              GtkWidget                *offscreen)
{
  JSGlobalContextRef context = webkit_javascript_result_get_global_context (result);
  JSValueRef value = webkit_javascript_result_get_value (result);

  if (JSValueIsObject (context, value))
    {
      JSObjectRef obj = JSValueToObject (context, value, NULL);
      JSValueRef time = js_object_get_property (context, obj, "time");
      JSValueRef state = js_object_get_property (context, obj, "state");
      JSValueRef x = js_object_get_property (context, obj, "x");
      JSValueRef y = js_object_get_property (context, obj, "y");
      GdkEventMotion *e;
      GdkEvent *event;

      event = gdk_event_new (GDK_MOTION_NOTIFY);

      e = (GdkEventMotion *)event;
      e->send_event = TRUE;
      e->time = JSValueToNumber (context, time, NULL);
      e->x = JSValueToNumber (context, x, NULL);
      e->y = JSValueToNumber (context, y, NULL);
      e->axes = NULL;
      e->state = JSValueToNumber (context, state, NULL);
      e->x_root = e->x;
      e->y_root = e->y;

      eos_web_view_do_event (offscreen, event);
      gdk_event_free (event);
    }
  else
    {
      g_warning ("Error running javascript: unexpected return value");
    }
}

static void
handle_script_message_update_canvas_done (WebKitUserContentManager *manager,
                                          WebKitJavascriptResult   *result,
                                          EosWebView               *webview)
{
  EosWebViewPrivate *priv = EOS_WEB_VIEW_PRIVATE (webview);
  g_clear_object (&priv->pixbuf);
}


static void
add_message_handler (WebKitUserContentManager *manager,
                     const gchar              *id,
                     const gchar              *name,
                     GCallback                 handler,
                     gpointer                  object_data)
{
  gchar *id_name, *signal;

  id_name = g_strconcat (id, "_", name, NULL);
  signal = g_strconcat ("script-message-received::", id_name, NULL);

  g_signal_connect_object (manager, signal, handler, object_data, 0);
  webkit_user_content_manager_register_script_message_handler (manager, id_name);

  g_free (id_name);
  g_free (signal);
}

static GtkWidget *
eos_web_view_offscreen_new (EosWebView  *webview,
                            GtkWidget   *child,
                            const gchar *id)
{
  GtkWidget *offscreen = gtk_offscreen_window_new ();
  WebKitUserContentManager *manager;

  manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW  (webview));

  g_object_set_data_full (G_OBJECT (offscreen), "EosWebViewId",
                          g_strdup (id), g_free);
  g_object_set_data_full (G_OBJECT (child), "EosWebViewId",
                          g_strdup (id), g_free);

  g_signal_connect_object (child, "notify::visible",
                           G_CALLBACK (on_child_visible_notify),
                           webview, 0);

  g_signal_connect_object (offscreen, "configure-event",
                           G_CALLBACK (on_offscreen_configure_event),
                           webview, 0);

  g_signal_connect_object (offscreen, "damage-event",
                           G_CALLBACK (on_offscreen_damage_event),
                           webview, 0);

  /* Add script message handlers to get events from canvas */
  add_message_handler (manager, id, "crossing",
                       G_CALLBACK (handle_script_message_crossing),
                       offscreen);
  add_message_handler (manager, id, "button",
                       G_CALLBACK (handle_script_message_button),
                       offscreen);
  add_message_handler (manager, id, "motion",
                       G_CALLBACK (handle_script_message_motion),
                       offscreen);

  /* Handler to free pixbuf imediately after updating canvas */
  g_signal_connect_object (manager, "script-message-received::update_canvas_done",
                           G_CALLBACK (handle_script_message_update_canvas_done),
                           webview, 0);
  webkit_user_content_manager_register_script_message_handler (manager, "update_canvas_done");

  /* Add widget to offscreen */
  gtk_container_add (GTK_CONTAINER (offscreen), child);
  gtk_container_set_focus_child (GTK_CONTAINER (offscreen), child);
  gtk_widget_show_all (offscreen);

  return offscreen;
}

static void
eos_web_view_wrap_child (EosWebView *webview, GtkWidget *child, const gchar *id)
{
  EosWebViewPrivate *priv;
  GtkWidget *offscreen;

  g_return_if_fail (EOS_IS_WEB_VIEW (webview));
  priv = EOS_WEB_VIEW_PRIVATE (webview);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (id != NULL && *id != '\0');

  /* Create new wrapper window */
  offscreen = eos_web_view_offscreen_new (webview, child, id);

  /* Insert child in hash table */
  g_hash_table_insert (priv->offscreens,
                       g_strdup (id),
                       g_object_ref (offscreen));
}


/* Public API */

GtkWidget *
eos_web_view_new ()
{
  return (GtkWidget *) g_object_new (EOS_TYPE_WEB_VIEW, NULL);
}

