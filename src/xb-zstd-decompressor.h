/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define XB_TYPE_ZSTD_DECOMPRESSOR (xb_zstd_decompressor_get_type())
G_DECLARE_FINAL_TYPE(XbZstdDecompressor, xb_zstd_decompressor, XB, ZSTD_DECOMPRESSOR, GObject)

XbZstdDecompressor *
xb_zstd_decompressor_new(void);

G_END_DECLS
