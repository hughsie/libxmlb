/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define XB_TYPE_LZMA_DECOMPRESSOR (xb_lzma_decompressor_get_type ())
G_DECLARE_FINAL_TYPE (XbLzmaDecompressor, xb_lzma_decompressor, XB, LZMA_DECOMPRESSOR, GObject)

XbLzmaDecompressor	*xb_lzma_decompressor_new	(void);

G_END_DECLS
