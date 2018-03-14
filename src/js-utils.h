/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* js-utils.h
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

#ifndef JS_UTILS_H
#define JS_UTILS_H

#include <webkit2/webkit2.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>

G_BEGIN_DECLS

void        _js_run (WebKitWebView *webview,
                     const gchar   *format,
                     ...);

gchar      *_js_object_get_string (JSGlobalContextRef context,
                                   JSObjectRef        object,
                                   gchar             *property);
gdouble     _js_object_get_number (JSGlobalContextRef context,
                                   JSObjectRef        object,
                                   gchar             *property);
G_END_DECLS

#endif /* JS_UTILS_H */
