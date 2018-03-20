/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* maxwell.h
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

#ifndef MAXWELL_H
#define MAXWELL_H

#include "maxwell-web-view.h"

G_BEGIN_DECLS

#define MAXWELL_INSIDE
# include "maxwell-version.h"
#undef MAXWELL_INSIDE

#define MAXWELL_ERROR (maxwell_error_quark ())

GQuark maxwell_error_quark (void);

G_END_DECLS

#endif /* MAXWELL_H */


