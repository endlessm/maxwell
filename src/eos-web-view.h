/* eos-web-view.h
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

#ifndef EOS_WEB_VIEW_H
#define EOS_WEB_VIEW_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define EOS_WEB_VIEW_INSIDE
# include "eos-web-view-version.h"
#undef EOS_WEB_VIEW_INSIDE

#define EOS_TYPE_WEB_VIEW (eos_web_view_get_type ())
G_DECLARE_FINAL_TYPE (EosWebView, eos_web_view, EOS, WEB_VIEW, WebKitWebView)

GtkWidget     *eos_web_view_new ();

G_END_DECLS

#endif /* EOS_WEB_VIEW_H */
