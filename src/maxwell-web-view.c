/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * maxwell-web-view.c
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
#include "js-utils.h"

struct _MaxwellWebView
{
  GtkOffscreenWindow parent;
};

typedef struct
{
  GtkWidget     *child;
  GdkWindow     *offscreen;   /* child offscreen window */
  GtkRequisition minimum;     /* child minimum size */
  GtkAllocation  alloc;       /* canvas allocation in viewport coordinates */
  gboolean       dom_size;    /* use DOM allocation size */
  GCancellable  *cancellable; /* JavaScript cancellable for this child */
} ChildData;

typedef struct
{
  guint      id;              /* Unique id */
  GdkPixbuf *pixbuf;          /* A Pixbuf */
} PixbufData;

typedef struct
{
  GList        *children;     /* List of ChildData */
  GQueue       *pixbufs;      /* List of PixbufData to handle maxwell:// requests */
  guint         pixbuf_count; /* Pixbuf counter, used as id */
  GCancellable *cancellable;  /* Global JavaScript cancellable */
} MaxwellWebViewPrivate;

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CANVAS_ID,

  N_CHILD_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (MaxwellWebView, maxwell_web_view, WEBKIT_TYPE_WEB_VIEW)

#define RESOURCES_PATH "/com/endlessm/maxwell"
#define MAXWELL_WEB_VIEW_PRIVATE(d) ((MaxwellWebViewPrivate *) maxwell_web_view_get_instance_private((MaxwellWebView*)d))

static ChildData *
maxwell_web_view_child_new (GtkWidget *child)
{
  ChildData *data = g_slice_new0 (ChildData);

  data->child = g_object_ref_sink (child);

  return data;
}

static void
maxwell_web_view_child_free (ChildData *data)
{
  if (data == NULL)
    return;

  if (data->offscreen && data->child)
    gtk_widget_unregister_window (gtk_widget_get_parent (data->child),
                                  data->offscreen);

  g_clear_pointer (&data->offscreen, gdk_window_destroy);

  g_clear_object (&data->child);

  g_slice_free (ChildData, data);
}

#define MWV_DEFINE_CHILD_GETTER(prop, type, cmpstmt) \
ChildData * \
get_child_data_by_##prop (MaxwellWebViewPrivate *priv, type prop) \
{ \
  GList *l;\
  for (l = priv->children; l; l = g_list_next (l)) \
    { \
      ChildData *data = l->data; \
      if (cmpstmt) \
        return data; \
    } \
  return NULL; \
}

MWV_DEFINE_CHILD_GETTER (id, const gchar *, !g_strcmp0 (gtk_widget_get_name (data->child), id))
MWV_DEFINE_CHILD_GETTER (child, GtkWidget *, data->child == child)
MWV_DEFINE_CHILD_GETTER (offscreen, GdkWindow *, data->offscreen == offscreen)

static PixbufData *
maxwell_web_view_pixbuf_new (GdkPixbuf *pixbuf, guint id)
{
  PixbufData *data = g_slice_new0 (PixbufData);

  data->id = id;
  data->pixbuf = pixbuf;

  return data;
}

static void
maxwell_web_view_pixbuf_free (PixbufData *data)
{
  if (data == NULL)
    return;

  g_clear_object (&data->pixbuf);

  g_slice_free (PixbufData, data);
}

PixbufData *
get_pixbuf_data (MaxwellWebViewPrivate *priv, guint id)
{
  GList *l;

  if (!priv->pixbufs)
    return NULL;

  for (l = priv->pixbufs->head; l; l = g_list_next (l))
    {
      PixbufData *data = l->data;
      if (data->id == id)
        return data;
    }
  return NULL;
}

static void
maxwell_web_view_init (MaxwellWebView *self)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (self);

  priv->pixbufs = g_queue_new ();
}

static void
children_cancellable_cancel (MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  GList *l;

  /* Cancel all children specific JS operations */
  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

      g_cancellable_cancel (data->cancellable);
      g_clear_object (&data->cancellable);
    }
}

static void
maxwell_web_view_dispose (GObject *object)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (object);

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  children_cancellable_cancel (MAXWELL_WEB_VIEW (object));

  if (priv->pixbufs)
    {
      g_queue_free_full (priv->pixbufs,(GDestroyNotify) maxwell_web_view_pixbuf_free);
      priv->pixbufs = NULL;
    }

  /* GtkContainer dispose will free children */
  G_OBJECT_CLASS (maxwell_web_view_parent_class)->dispose (object);
}

static void
on_maxwell_uri_scheme_request (WebKitURISchemeRequest *request,
                               gpointer                userdata)
{
  WebKitWebView *webview = webkit_uri_scheme_request_get_web_view (request);
  MaxwellWebViewPrivate *priv;
  GError *error = NULL;
  const gchar *path;
  PixbufData *data;
  guint id;

  /* Context can be shared with others WebView */
  if (!MAXWELL_IS_WEB_VIEW (webview))
    {
      error = g_error_new (WEBKIT_NETWORK_ERROR,
                           WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL,
                           "maxwell:// Unkown protocol");
      webkit_uri_scheme_request_finish_error (request, error);
      g_error_free (error);
      return;
    }

  priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  path = webkit_uri_scheme_request_get_path (request);

  /*
   * maxwell:///pixbuf_id
   *
   * Where 'pixbuf_id' is a GdkPixbuf pointer encoded in base64
   *
   * GdkPixbuf are created in damage-event handler and pushed to priv->pixbufs
   * for us to consume.
   */
  if (path && *path == '/' &&
      (id = g_ascii_strtoll (&path[1], NULL, 10)) &&
      (data = get_pixbuf_data (priv, id)))
    {
      const guint8 *pixels = gdk_pixbuf_read_pixels (data->pixbuf);
      GInputStream *stream = NULL;
      gsize len;

      len = gdk_pixbuf_get_height (data->pixbuf) *
            gdk_pixbuf_get_rowstride (data->pixbuf);

      stream = g_memory_input_stream_new_from_data (pixels, len, NULL);
      webkit_uri_scheme_request_finish (request, stream, len,
                                        "application/octet-stream");

      /* Add a week reference to free the Pixbuf when stream if finalized */
      g_object_weak_ref (G_OBJECT (stream),
                         (GWeakNotify) maxwell_web_view_pixbuf_free,
                         data);

      /* Pixbuf is no longer our responsibility, stream will take care
       * of freeing it when it's finalized
       */
      g_queue_remove (priv->pixbufs, data);

      g_object_unref (stream);
      return;
    }

  if (!error)
    error = g_error_new (MAXWELL_ERROR, MAXWELL_ERROR_URI,
                         "Could not find image data for %s",
                         webkit_uri_scheme_request_get_uri (request));

  webkit_uri_scheme_request_finish_error (request, error);
  g_error_free (error);
}

static void
handle_script_message_children_move_resize (WebKitUserContentManager *manager,
                                            WebKitJavascriptResult   *result,
                                            MaxwellWebView           *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  JSGlobalContextRef context = webkit_javascript_result_get_global_context (result);
  JSValueRef value = webkit_javascript_result_get_value (result);
  JSObjectRef array;
  JSValueRef val;
  gint i = 0;

  if (!JSValueIsArray (context, value))
    {
      g_warning ("Error running javascript: unexpected return value");
      return;
    }

  array = JSValueToObject (context, value, NULL);

  while ((val = JSObjectGetPropertyAtIndex (context, array, i, NULL)) &&
         JSValueIsObject (context, val))
    {
      JSObjectRef obj = JSValueToObject (context, val, NULL);
      gchar *child_id = _js_object_get_string (context, obj, "id");
      ChildData *data = get_child_data_by_id (priv, child_id);

      if (data)
        {
          gint w = _js_object_get_number (context, obj, "width");
          gint h = _js_object_get_number (context, obj, "height");

          data->alloc.x = _js_object_get_number (context, obj, "x");
          data->alloc.y = _js_object_get_number (context, obj, "y");

          if (w && h && (data->alloc.width != w || data->alloc.height != h))
            {
              data->dom_size = TRUE;
              data->alloc.width = w;
              data->alloc.height = h;
              gtk_widget_queue_resize (data->child);
            }
        }

      g_free (child_id);
      i++;
    }
}

static void
child_allocate (MaxwellWebView *webview, ChildData *data)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  GtkRequisition *minimum = &data->minimum;
  GtkAllocation alloc;

  gtk_widget_get_preferred_size (data->child, minimum, NULL);

  if (data->dom_size)
    {
      gtk_widget_get_allocation (data->child, &alloc);

      if (alloc.width == data->alloc.width && alloc.height == data->alloc.height)
        return;

      if (data->alloc.width <= 0)
        data->alloc.width = minimum->width;

      if (data->alloc.height <= 0)
        data->alloc.height = minimum->height;

      alloc.width = data->alloc.width;
      alloc.height = data->alloc.height;
    }
  else
    {
      if (minimum->width == data->alloc.width && minimum->height == data->alloc.height)
        return;

      alloc.width = minimum->width;
      alloc.height = minimum->height;

      data->alloc.width = alloc.width;
      data->alloc.height = alloc.height;
    }

  alloc.x = alloc.y = 0;
  gtk_widget_size_allocate (data->child, &alloc);

  if (data->offscreen)
    {
      gdk_window_resize (data->offscreen, alloc.width, alloc.height);

      if (!priv->cancellable)
        return;

      /* Cancel any pending resize or draw operation and start over */
      g_cancellable_cancel (data->cancellable);
      g_clear_object (&data->cancellable);
      data->cancellable = g_cancellable_new ();

      js_run_printf (webview, data->cancellable,
                     "maxwell.child_resize ('%s', %d, %d, %d, %d);\n",
                     gtk_widget_get_name (data->child),
                     alloc.width, alloc.height,
                     minimum->width, minimum->height);
    }
}

static void
handle_script_message_children_init (WebKitUserContentManager *manager,
                                     WebKitJavascriptResult   *result,
                                     MaxwellWebView           *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  JSGlobalContextRef context = webkit_javascript_result_get_global_context (result);
  JSValueRef value = webkit_javascript_result_get_value (result);
  JSObjectRef array;
  GString *script;
  JSValueRef val;
  gint i = 0;

  if (!JSValueIsArray (context, value))
    {
      g_warning ("Error running javascript: unexpected return value");
      return;
    }

  array = JSValueToObject (context, value, NULL);
  script = g_string_new ("");

  while ((val = JSObjectGetPropertyAtIndex (context, array, i, NULL)) &&
         JSValueIsObject (context, val))
    {
      JSObjectRef obj = JSValueToObject (context, val, NULL);
      gchar *id = _js_object_get_string (context, obj, "id");
      ChildData *data = get_child_data_by_id (priv, id);

      if (data && data->offscreen && data->alloc.width && data->alloc.height)
        {
          /* Collect children to initialize */
          if (gtk_widget_get_visible (data->child))
            {
              g_string_append_printf (script,
                                      "maxwell.child_set_visible ('%s', true);\n",
                                      id);

              /* Dont force allocation for widgets that must honor DOM tree size
               * Just show it and let the DOM tree mutate and trigger a resize
               */
              if (!_js_object_get_number (context, obj, "use_dom_size"))
                g_string_append_printf (script,
                                        "maxwell.child_resize ('%s', %d, %d, %d, %d);\n",
                                        id,
                                        data->alloc.width, data->alloc.height,
                                        data->minimum.width, data->minimum.height);

            }
          else
            {
              g_string_append_printf (script,
                                      "maxwell.child_set_visible ('%s', false);\n",
                                      id);
            }
        }
      g_free (id);
      i++;
    }

  /* Initialize all children at once */
  js_run_string (webview, priv->cancellable, script);
  g_string_free (script, TRUE);
}

#define EWV_DEFINE_MSG_HANDLER(manager, name, object) \
  g_signal_connect_object (manager, "script-message-received::maxwell_"#name,\
                           G_CALLBACK (handle_script_message_##name),\
                           object, 0);\
  webkit_user_content_manager_register_script_message_handler (manager, "maxwell_"#name);

static void
maxwell_web_view_constructed (GObject *object)
{
  WebKitWebView *webview = WEBKIT_WEB_VIEW (object);
  WebKitUserContentManager *content_manager;
  WebKitSecurityManager *security_manager;
  WebKitWebContext *context;
  WebKitUserScript *script;
  GBytes *script_source;

  G_OBJECT_CLASS (maxwell_web_view_parent_class)->constructed (object);

  /* Give XmlHttpRequest a chance to work */
  g_object_set (webkit_web_view_get_settings (webview),
                "allow-universal-access-from-file-urls", TRUE,
                "allow-file-access-from-file-urls", TRUE,
                "enable-write-console-messages-to-stdout", TRUE,
                NULL);

  /* Install custom URI scheme to inject image buffers */
  context = webkit_web_view_get_context (webview);
  security_manager = webkit_web_context_get_security_manager (context);
  webkit_security_manager_register_uri_scheme_as_cors_enabled (security_manager,
                                                               "maxwell");
  webkit_web_context_register_uri_scheme (context, "maxwell",
                                          on_maxwell_uri_scheme_request,
                                          NULL, NULL);

  /* Add script */
  content_manager = webkit_web_view_get_user_content_manager (webview);
  script_source = g_resources_lookup_data (RESOURCES_PATH"/maxwell-web-view.js",
                                           G_RESOURCE_LOOKUP_FLAGS_NONE,
                                           NULL);
  script = webkit_user_script_new (g_bytes_get_data (script_source, NULL),
                                   WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                   WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                   NULL, NULL);
  webkit_user_content_manager_add_script (content_manager, script);

  /* Init canvas elements added to the DOM */
  EWV_DEFINE_MSG_HANDLER (content_manager, children_init, webview);

  /* Handle children position changes */
  EWV_DEFINE_MSG_HANDLER (content_manager, children_move_resize, webview);

  webkit_user_script_unref (script);
  g_bytes_unref (script_source);
}

static void
offscreen_to_parent (GdkWindow      *offscreen_window,
                     double          offscreen_x,
                     double          offscreen_y,
                     double         *parent_x,
                     double         *parent_y,
                     MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  ChildData *data = get_child_data_by_offscreen (priv, offscreen_window);

  if (data)
    {
      *parent_x = offscreen_x + data->alloc.x;
      *parent_y = offscreen_y + data->alloc.y;
    }
  else
    {
      *parent_x = offscreen_x;
      *parent_y = offscreen_y;
    }
}

static void
offscreen_from_parent (GdkWindow      *window,
                       double          parent_x,
                       double          parent_y,
                       double         *offscreen_x,
                       double         *offscreen_y,
                       MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  ChildData *data = get_child_data_by_offscreen (priv, window);

  if (data)
    {
      *offscreen_x = parent_x - data->alloc.x;
      *offscreen_y = parent_y - data->alloc.y;
    }
  else
    {
      *offscreen_x = parent_x;
      *offscreen_y = parent_y;
    }
}

static GdkWindow *
pick_offscreen_child (GdkWindow      *offscreen_window,
                      double          widget_x,
                      double          widget_y,
                      MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  GList *l;

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;
      GtkAllocation *alloc = &data->alloc;

      if (widget_x >= alloc->x && widget_x <= alloc->x + alloc->width &&
          widget_y >= alloc->y && widget_y <= alloc->y + alloc->height)
        return data->offscreen;
    }

  return NULL;
}

static void
ensure_offscreen (GtkWidget *webview, ChildData *data)
{
  GdkScreen *screen = gtk_widget_get_screen (webview);
  GdkWindowAttr attributes;
  gboolean mapped, realized;

  if (gtk_widget_get_name (data->child) == NULL || data->offscreen)
    return;

  attributes.width = data->alloc.width;
  attributes.height = data->alloc.height;
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.event_mask = gtk_widget_get_events (data->child) | GDK_EXPOSURE_MASK;
  attributes.visual = gdk_screen_get_rgba_visual (screen);
  attributes.wclass = GDK_INPUT_OUTPUT;

  data->offscreen = gdk_window_new (gdk_screen_get_root_window (screen),
                                    &attributes,
                                    GDK_WA_VISUAL);

  if ((mapped = gtk_widget_get_mapped (data->child)))
    gtk_widget_unmap (data->child);

  if ((realized = gtk_widget_get_realized (data->child)))
    gtk_widget_unrealize (data->child);

  gtk_widget_register_window (webview, data->offscreen);
  gtk_widget_set_parent_window (data->child, data->offscreen);

  gdk_offscreen_window_set_embedder (data->offscreen,
                                     gtk_widget_get_window (webview));
  g_signal_connect_object (data->offscreen, "to-embedder",
                           G_CALLBACK (offscreen_to_parent),
                           webview, 0);
  g_signal_connect_object (data->offscreen, "from-embedder",
                           G_CALLBACK (offscreen_from_parent),
                           webview, 0);

  gdk_window_show (data->offscreen);

  if (realized)
    gtk_widget_realize (data->child);

  if (mapped)
    gtk_widget_map (data->child);
}

static void
maxwell_web_view_realize (GtkWidget *widget)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  GString *script = NULL;
  GList *l;

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->realize (widget);

  g_signal_connect_object (gtk_widget_get_window (widget),
                           "pick-embedded-child",
                           G_CALLBACK (pick_offscreen_child),
                           widget,
                           0);

  if (priv->cancellable)
    script = g_string_new ("");

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

      ensure_offscreen (widget, data);

      if (script)
        g_string_append_printf (script, "maxwell.child_set_visible ('%s', %s);\n",
                                gtk_widget_get_name (data->child),
                                gtk_widget_get_visible (data->child) ?
                                  "true" : "false");
    }

  if (script)
    {
      js_run_string (widget, priv->cancellable, script);
      g_string_free (script, TRUE);
    }
}

static void
maxwell_web_view_unrealize (GtkWidget *widget)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  GList *l;

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

      if (!data->offscreen)
        continue;

      gtk_widget_unregister_window (widget, data->offscreen);
      g_clear_pointer (&data->offscreen, gdk_window_destroy);
    }

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->unrealize (widget);
}

static gboolean
maxwell_web_view_damage_event (GtkWidget *widget, GdkEventExpose *event)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  ChildData *data;

  /* Don't do anything if the support script did not finished loading */
  if (!priv->cancellable)
    return FALSE;

  data = get_child_data_by_offscreen (priv, event->window);

  if (data &&
      gtk_widget_get_name (data->child) &&
      gtk_widget_get_visible (data->child))
    {
      GdkPixbuf *pixbuf = gdk_pixbuf_get_from_window (data->offscreen,
                                                      event->area.x,
                                                      event->area.y,
                                                      event->area.width,
                                                      event->area.height);

      /* Double check format is what JavaScript ImageData expects */
      if (pixbuf &&
          gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB &&
          gdk_pixbuf_get_bits_per_sample (pixbuf) == 8 &&
          gdk_pixbuf_get_has_alpha (pixbuf))
        {
          guint id = ++priv->pixbuf_count;

          g_queue_push_tail (priv->pixbufs,
                             maxwell_web_view_pixbuf_new (pixbuf, id));

          js_run_printf (widget, data->cancellable,
                         "maxwell.child_draw ('%s', '%u', %d, %d, %d, %d);",
                         gtk_widget_get_name (data->child),
                         id,
                         event->area.x,
                         event->area.y,
                         event->area.width,
                         event->area.height);
        }
    }

  return FALSE;
}

static void
maxwell_web_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  MaxwellWebView *webview = MAXWELL_WEB_VIEW (widget);
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  GList *l;

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->size_allocate (widget, allocation);

  for (l = priv->children; l; l = g_list_next (l))
    child_allocate (webview, l->data);
}

static gboolean
maxwell_web_view_draw (GtkWidget *widget, cairo_t *cr)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  GdkWindow *window = gtk_widget_get_window (widget);
  GList *l;

  if (gtk_cairo_should_draw_window (cr, window))
    GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->draw (widget, cr);

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

        if (!data->offscreen ||
            !gtk_cairo_should_draw_window (cr, data->offscreen))
          continue;

        /* Clear offscreen window instead of rendering webview background */
        cairo_save (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint (cr);
        cairo_restore (cr);

        gtk_container_propagate_draw (GTK_CONTAINER (widget), data->child, cr);
    }

  return FALSE;
}

static void
child_update_visibility (MaxwellWebView *webview, GtkWidget *child)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  ChildData *data = get_child_data_by_child (priv, child);

  if (priv->cancellable && data && gtk_widget_get_name (data->child))
    js_run_printf (webview, priv->cancellable,
                   "maxwell.child_set_visible ('%s', %s);",
                   gtk_widget_get_name (data->child),
                   gtk_widget_get_visible (child) ? "true" : "false");
}

static void
on_child_visible_notify (GObject        *object,
                         GParamSpec     *pspec,
                         MaxwellWebView *webview)
{
  child_update_visibility (webview, GTK_WIDGET (object));
}

static void
on_child_name_notify (GObject        *object,
                      GParamSpec     *pspec,
                      MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  GtkWidget *child = GTK_WIDGET (object);
  const gchar *id = gtk_widget_get_name (child);
  ChildData *data = get_child_data_by_child (priv, child);
  GList *l;

  if (!data)
    return;

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *cdata = l->data;
      if (cdata != data &&
          g_strcmp0 (gtk_widget_get_name (cdata->child), id) == 0)
        {
          g_warning ("Widget's name '%s' is not unique", id);
          return;
        }
    }

  if (gtk_widget_get_realized (GTK_WIDGET (webview)))
    {
      ensure_offscreen (GTK_WIDGET (webview), data);
      child_update_visibility (webview, data->child);
    }
}

static void
maxwell_web_view_add (GtkContainer *container, GtkWidget *child)
{
  MaxwellWebViewPrivate *priv;

  g_return_if_fail (MAXWELL_IS_WEB_VIEW (container));
  priv = MAXWELL_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  g_signal_connect_object (child, "notify::visible",
                           G_CALLBACK (on_child_visible_notify),
                           container, 0);
  g_signal_connect_object (child, "notify::name",
                           G_CALLBACK (on_child_name_notify),
                           container, 0);

  gtk_widget_set_parent (child, GTK_WIDGET (container));

  priv->children = g_list_prepend (priv->children,
                                   maxwell_web_view_child_new (child));

  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
maxwell_web_view_remove (GtkContainer *container, GtkWidget *child)
{
  MaxwellWebViewPrivate *priv;
  ChildData *data;

  g_return_if_fail (MAXWELL_IS_WEB_VIEW (container));
  priv = MAXWELL_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (container));

  g_signal_handlers_disconnect_by_func (child, on_child_visible_notify, container);
  g_signal_handlers_disconnect_by_func (child, on_child_name_notify, container);

  if ((data = get_child_data_by_child (priv, child)))
    {
      priv->children = g_list_remove (priv->children, data);
      maxwell_web_view_child_free (data);
    }

  gtk_widget_unparent (child);
}

static void
maxwell_web_view_forall (GtkContainer *container,
                         gboolean      include_internals,
                         GtkCallback   callback,
                         gpointer      callback_data)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (container);
  GList *l;

  g_return_if_fail (callback != NULL);

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;
      (*callback) (data->child, callback_data);
    }
}

static gboolean
maxwell_web_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  /* Ignore button events from offscreen windows */
  if (gdk_offscreen_window_get_embedder (event->window) == gtk_widget_get_window (widget))
    return TRUE;

  return GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->button_press_event (widget, event);
}

static gboolean
maxwell_web_view_button_release_event (GtkWidget *widget, GdkEventButton *event)
{
  /* Ignore button events from offscreen windows */
  if (gdk_offscreen_window_get_embedder (event->window) == gtk_widget_get_window (widget))
    return TRUE;

  return GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->button_release_event (widget, event);
}

static void
maxwell_web_view_load_changed (WebKitWebView  *webview,
                               WebKitLoadEvent event)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);

  if (event != WEBKIT_LOAD_STARTED && event != WEBKIT_LOAD_FINISHED)
    return;

  /* Cancel all JS on load started */
  children_cancellable_cancel (MAXWELL_WEB_VIEW (webview));
  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  /* Create a new GCancellable object when document finished loading */
  if (event == WEBKIT_LOAD_FINISHED)
    priv->cancellable = g_cancellable_new ();
}

static void
maxwell_web_view_class_init (MaxwellWebViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  WebKitWebViewClass *web_view_class = WEBKIT_WEB_VIEW_CLASS (klass);

  object_class->dispose = maxwell_web_view_dispose;
  object_class->constructed = maxwell_web_view_constructed;

  widget_class->realize = maxwell_web_view_realize;
  widget_class->unrealize = maxwell_web_view_unrealize;
  widget_class->size_allocate = maxwell_web_view_size_allocate;
  widget_class->draw = maxwell_web_view_draw;
  widget_class->damage_event = maxwell_web_view_damage_event;

  widget_class->button_press_event = maxwell_web_view_button_press_event;
  widget_class->button_release_event = maxwell_web_view_button_release_event;

  container_class->add = maxwell_web_view_add;
  container_class->remove = maxwell_web_view_remove;
  container_class->forall = maxwell_web_view_forall;

  web_view_class->load_changed = maxwell_web_view_load_changed;
}

/* Public API */

/**
 * maxwell_web_view_new:
 *
 * Creates a new #MaxwellWebView.
 *
 * Returns: (transfer full): the newly created web view
 *
 */
GtkWidget *
maxwell_web_view_new (void)
{
  return g_object_new (MAXWELL_TYPE_WEB_VIEW, NULL);
}

