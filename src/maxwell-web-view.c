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
  GdkWindow     *offscreen; /* child offscreen window */
  gchar         *id;        /* canvas-id child property */
  GtkAllocation  alloc;     /* canvas allocation in viewport coordinates */
} ChildData;

typedef struct
{
  GList      *children;  /* List of ChildData */
  GList      *pixbufs;   /* List of temp pixbufs to handle maxwell:// requests */
  gboolean    script_loaded;
} MaxwellWebViewPrivate;

enum
{
  CHILD_PROP_0,
  CHILD_PROP_CANVAS_ID,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_properties[N_CHILD_PROPERTIES];

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

  g_free (data->id);

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

MWV_DEFINE_CHILD_GETTER (id, const gchar *, !g_strcmp0 (data->id, id))
MWV_DEFINE_CHILD_GETTER (child, GtkWidget *, data->child == child)
MWV_DEFINE_CHILD_GETTER (offscreen, GdkWindow *, data->offscreen == offscreen)

static void
maxwell_web_view_init (MaxwellWebView *self)
{
}

static void
maxwell_web_view_dispose (GObject *object)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (object);

  g_list_free_full (priv->pixbufs, g_object_unref);
  priv->pixbufs = NULL;

  /* GtkContainer dispose will free children */
  G_OBJECT_CLASS (maxwell_web_view_parent_class)->dispose (object);
}

static void
on_maxwell_uri_scheme_request (WebKitURISchemeRequest *request,
                               gpointer                userdata)
{
  const gchar *path = webkit_uri_scheme_request_get_path (request);
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (userdata);
  GError *error = NULL;
  ChildData *data;

  if (path && *path == '/' && (data = get_child_data_by_id (priv, &path[1])))
    {
      const gchar *uri = webkit_uri_scheme_request_get_uri (request);
      GdkPixbuf *pixbuf = NULL;
      gchar *pixbuf_id;

      /*
       * maxwell:///id[?pixbuf]
       *
       * Where 'id' is the child id and 'pixbuf' is an optional GdkPixbuf
       * pointer encoded in base64
       *
       * GdkPixbuf are created in damage-event handler and pushed to priv->pixbufs
       * for us to consume if the uri includes the pixbuf id.
       * Otherwise if there is no specific image requested we return the whole
       * offscreen
       */
      if ((pixbuf_id = g_strstr_len (uri, -1, "?")))
        {
          guchar *image_data;
          gsize len;

          if ((image_data = g_base64_decode (&pixbuf_id[1], &len)) &&
              len == sizeof (GdkPixbuf *))
            {
              pixbuf = *((GdkPixbuf **)image_data);
              if (!g_list_find (priv->pixbufs, pixbuf))
                pixbuf = NULL;
            }

          g_free (image_data);
        }
      else
        {
          cairo_surface_t *surface;

          /* No pixbuf id, return the whole offscreen */
          surface = gdk_offscreen_window_get_surface (data->offscreen);
          pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                                data->alloc.width,
                                                data->alloc.height);

          priv->pixbufs = g_list_prepend (priv->pixbufs, pixbuf);
        }

      if (pixbuf &&
          gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB &&
          gdk_pixbuf_get_bits_per_sample (pixbuf) == 8 &&
          gdk_pixbuf_get_has_alpha (pixbuf))
        {
          const guint8 *pixels = gdk_pixbuf_read_pixels (pixbuf);
          GInputStream *stream = NULL;
          gsize len;

          len = gdk_pixbuf_get_height (pixbuf) *
                gdk_pixbuf_get_rowstride (pixbuf);

          stream = g_memory_input_stream_new_from_data (pixels, len, NULL);
          webkit_uri_scheme_request_finish (request, stream, len,
                                            "application/octet-stream");

          /* Add a week reference to free the Pixbuf when stream if finalized */
          g_object_weak_ref (G_OBJECT (stream),
                             (GWeakNotify) g_object_unref,
                             pixbuf);

          /* Pixbuf is no longer our responsibility, stream will take care
           * of freeing it when it's finalized
           */
          priv->pixbufs = g_list_remove (priv->pixbufs, pixbuf);

          g_object_unref (stream);
          return;
        }
      else if (pixbuf)
        {
          /* Pixbuf is not in RGBA format, ignore it */
          priv->pixbufs = g_list_remove (priv->pixbufs, pixbuf);
          g_object_unref (pixbuf);

          error = g_error_new (MAXWELL_ERROR, MAXWELL_ERROR_URI,
                               "Wrong image data format for %s",
                               uri);
        }
    }

  if (!error)
    error = g_error_new (MAXWELL_ERROR, MAXWELL_ERROR_URI,
                         "Could not find image data for %s",
                         webkit_uri_scheme_request_get_uri (request));

  webkit_uri_scheme_request_finish_error (request, error);
  g_error_free (error);
}

static void
handle_script_message_update_positions (WebKitUserContentManager *manager,
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
          data->alloc.x = _js_object_get_number (context, obj, "x");
          data->alloc.y = _js_object_get_number (context, obj, "y");
        }

      g_free (child_id);
      i++;
    }
}

static gboolean
child_allocate (ChildData *data)
{
  GtkRequisition natural;
  GtkAllocation alloc;

  gtk_widget_get_preferred_size (data->child, NULL, &natural);

  if (natural.width == data->alloc.width && natural.height == data->alloc.height)
    return FALSE;

  alloc.x = 0;
  alloc.y = 0;
  alloc.height = natural.height;
  alloc.width = natural.width;

  /* Update canvas alloc size */
  data->alloc.width = natural.width;
  data->alloc.height = natural.height;

  gtk_widget_size_allocate (data->child, &alloc);

  if (data->offscreen)
    gdk_window_resize (data->offscreen, natural.width, natural.height);

  return TRUE;
}

static void
child_update_visibility (MaxwellWebView *webview, GtkWidget *child)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  ChildData *data = get_child_data_by_child (priv, child);

  if (priv->script_loaded && data && data->id)
    js_run_printf (webview, "maxwell.child_set_visible ('%s', %s);",
                   data->id,
                   gtk_widget_get_visible (child) ? "true" : "false");
}

static void
handle_script_message_children_allocate (WebKitUserContentManager *manager,
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
         JSValueIsString (context, val))
    {
      gchar *id = _js_get_string (context, val);
      ChildData *data = get_child_data_by_id (priv, id);

      if (data)
        {
          /* TODO: check if we can remove allocating children here */
          child_allocate (data);

          /* Collect children to initialize */
          g_string_append_printf (script, "maxwell.child_init ('%s', %d, %d, %s);\n",
                                  data->id, data->alloc.width, data->alloc.height,
                                  gtk_widget_get_visible (data->child) ? "true" : "false");
        }
      g_free (id);
      i++;
    }

  /* Initialize all children at once */
  js_run_string (webview, script);
  g_string_free (script, TRUE);
}

static void
handle_script_message_script_loaded (WebKitUserContentManager *manager,
                                     WebKitJavascriptResult   *result,
                                     MaxwellWebView           *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);
  priv->script_loaded = TRUE;
}

static void
maxwell_web_view_set_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  g_return_if_fail (MAXWELL_IS_WEB_VIEW (container));
  g_return_if_fail (GTK_IS_WIDGET (child));

  switch (property_id)
    {
      case CHILD_PROP_CANVAS_ID:
        {
          MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (container);
          const gchar *id = g_value_get_string (value);
          ChildData *data;

          if (get_child_data_by_id (priv, id))
            {
              g_warning ("Canvas id '%s' is not unique", id);
              return;
            }

          if ((data = get_child_data_by_child (priv, child)))
            {
              g_free (data->id);
              data->id = g_strdup (id);
            }
        }
      break;
      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
maxwell_web_view_get_child_property (GtkContainer *container,
                                     GtkWidget    *child,
                                     guint         property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec)
{
  g_return_if_fail (MAXWELL_IS_WEB_VIEW (container));
  g_return_if_fail (GTK_IS_WIDGET (child));

  switch (property_id)
    {
      case CHILD_PROP_CANVAS_ID:
        {
          MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (container);
          ChildData *data = get_child_data_by_child (priv, child);
          g_value_set_string (value, (data) ? data->id : NULL);
        }
      break;
      default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
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
                                          object, NULL);

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

  /* Signal script has been loaded */
  EWV_DEFINE_MSG_HANDLER (content_manager, script_loaded, webview);

  /* Handle children position changes */
  EWV_DEFINE_MSG_HANDLER (content_manager, update_positions, webview);

  /* Allocate new canvas added to the DOM */
  EWV_DEFINE_MSG_HANDLER (content_manager, children_allocate, webview);

  webkit_user_script_unref (script);
  g_bytes_unref (script_source);
}

static void
on_child_visible_notify (GObject        *object,
                         GParamSpec     *pspec,
                         MaxwellWebView *webview)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (webview);

  if (priv->script_loaded)
    child_update_visibility (webview, GTK_WIDGET (object));
}

static void
maxwell_web_view_add (GtkContainer *container, GtkWidget *child)
{
  MaxwellWebViewPrivate *priv;
  ChildData *data;

  g_return_if_fail (MAXWELL_IS_WEB_VIEW (container));
  priv = MAXWELL_WEB_VIEW_PRIVATE (container);
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  data = maxwell_web_view_child_new (child);
  g_signal_connect_object (child, "notify::visible",
                           G_CALLBACK (on_child_visible_notify),
                           container, 0);

  gtk_widget_set_parent (child, GTK_WIDGET (container));

  priv->children = g_list_prepend (priv->children, data);
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

  if ((data = get_child_data_by_child (priv, child)))
    {
      priv->children = g_list_remove (priv->children, data);
      maxwell_web_view_child_free (data);
    }

  gtk_widget_unparent (child);
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

  if (data->id == NULL)
    return;

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = data->alloc.width;
  attributes.height = data->alloc.height;
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.event_mask = gtk_widget_get_events (data->child) | GDK_EXPOSURE_MASK;
  attributes.visual = gdk_screen_get_rgba_visual (screen);
  attributes.wclass = GDK_INPUT_OUTPUT;

  data->offscreen = gdk_window_new (gdk_screen_get_root_window (screen),
                                    &attributes,
                                    GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL);

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
}

static void
maxwell_web_view_realize (GtkWidget *widget)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  GList *l;

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->realize (widget);

  g_signal_connect_object (gtk_widget_get_window (widget),
                           "pick-embedded-child",
                           G_CALLBACK (pick_offscreen_child),
                           widget,
                           0);

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

      ensure_offscreen (widget, data);

      child_update_visibility (MAXWELL_WEB_VIEW (widget), data->child);
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

      child_update_visibility (MAXWELL_WEB_VIEW (widget), data->child);
    }

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->unrealize (widget);
}

static gboolean
maxwell_web_view_damage_event (GtkWidget *widget, GdkEventExpose *event)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  ChildData *data;

  /* Don't do anything if the support script did not finished loading */
  if (!priv->script_loaded)
    return FALSE;

  data = get_child_data_by_offscreen (priv, event->window);

  if (data && data->id && gtk_widget_get_visible (data->child))
    {
      cairo_surface_t *surface;
      GdkPixbuf *pixbuf;
      gchar *img_id;

      surface = gdk_offscreen_window_get_surface (data->offscreen);
      pixbuf = gdk_pixbuf_get_from_surface (surface,
                                            event->area.x,
                                            event->area.y,
                                            event->area.width,
                                            event->area.height);

      img_id = g_base64_encode ((const guchar *)&pixbuf, sizeof (GdkPixbuf *));
      priv->pixbufs = g_list_prepend (priv->pixbufs, pixbuf);

      js_run_printf (widget, "maxwell.child_draw ('%s', '%s', %d, %d, %d, %d);",
                     data->id,
                     img_id,
                     event->area.x,
                     event->area.y,
                     event->area.width,
                     event->area.height);
      g_free (img_id);
    }

  return FALSE;
}

static void
maxwell_web_view_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  MaxwellWebViewPrivate *priv = MAXWELL_WEB_VIEW_PRIVATE (widget);
  GString *script = g_string_new ("");
  GList *l;

  for (l = priv->children; l; l = g_list_next (l))
    {
      ChildData *data = l->data;

      if (child_allocate (data) && data->offscreen)
        g_string_append_printf (script, "maxwell.child_resize ('%s', %d, %d);\n",
                                data->id, data->alloc.width, data->alloc.height);
    }

  js_run_string (widget, script);
  g_string_free (script, TRUE);

  GTK_WIDGET_CLASS (maxwell_web_view_parent_class)->size_allocate (widget, allocation);
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

        if (!gtk_cairo_should_draw_window (cr, data->offscreen))
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
maxwell_web_view_class_init (MaxwellWebViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

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
  container_class->set_child_property = maxwell_web_view_set_child_property;
  container_class->get_child_property = maxwell_web_view_get_child_property;

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

/**
 * maxwell_web_view_pack_child:
 * @webview: a #MaxwellWebView
 * @child: a #GtkWidget
 * @id: a string identifying child
 *
 * Packs @child in @webview and binds it to <canvas class="GtkWidget" id="@id"/>
 * element
 *
 * This is the same as calling gtk_container_add() and setting "canvas-id"
 * packing property to @id.
 *
 */
void
maxwell_web_view_pack_child (MaxwellWebView  *webview,
                             GtkWidget       *child,
                             const gchar     *id)
{
  g_return_if_fail (MAXWELL_IS_WEB_VIEW (webview));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (id != NULL);
  g_return_if_fail (*id != '\0');

  gtk_container_add (GTK_CONTAINER (webview), child);
  gtk_container_child_set (GTK_CONTAINER (webview), child,
                           "canvas-id", id,
                           NULL);
}
