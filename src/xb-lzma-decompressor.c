/*
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2009 Shaun McCance <shaunm@gnome.org>
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "xb-lzma-decompressor.h"

#include "config.h"

#include <errno.h>
#include <gio/gio.h>
#include <lzma.h>
#include <string.h>

static void
xb_lzma_decompressor_iface_init(GConverterIface *iface);

struct _XbLzmaDecompressor {
	GObject parent_instance;
	lzma_stream lzmastream;
};

G_DEFINE_TYPE_WITH_CODE(XbLzmaDecompressor,
			xb_lzma_decompressor,
			G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE(G_TYPE_CONVERTER, xb_lzma_decompressor_iface_init))

static void
xb_lzma_decompressor_finalize(GObject *object)
{
	XbLzmaDecompressor *self = XB_LZMA_DECOMPRESSOR(object);
	lzma_end(&self->lzmastream);
	G_OBJECT_CLASS(xb_lzma_decompressor_parent_class)->finalize(object);
}

static void
xb_lzma_decompressor_init(XbLzmaDecompressor *self)
{
}

static void
xb_lzma_decompressor_constructed(GObject *object)
{
	XbLzmaDecompressor *self = XB_LZMA_DECOMPRESSOR(object);
	lzma_stream tmp = LZMA_STREAM_INIT;
	lzma_ret res;

	self->lzmastream = tmp;
	res = lzma_auto_decoder(&self->lzmastream, SIZE_MAX, 0);
	if (res == LZMA_MEM_ERROR)
		g_error("XbLzmaDecompressor: Not enough memory for lzma use");
	if (res == LZMA_OPTIONS_ERROR)
		g_error("XbLzmaDecompressor: Unsupported flags");
	if (res != LZMA_OK)
		g_error("XbLzmaDecompressor: Unexpected lzma error");
}

static void
xb_lzma_decompressor_class_init(XbLzmaDecompressorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = xb_lzma_decompressor_finalize;
	object_class->constructed = xb_lzma_decompressor_constructed;
}

XbLzmaDecompressor *
xb_lzma_decompressor_new(void)
{
	return g_object_new(XB_TYPE_LZMA_DECOMPRESSOR, NULL);
}

static void
xb_lzma_decompressor_reset(GConverter *converter)
{
	XbLzmaDecompressor *self = XB_LZMA_DECOMPRESSOR(converter);
	lzma_ret res;

	/* lzma doesn't have a reset function.  Ending and reiniting
	 * might do the trick.  But this is untested.  If reset matters
	 * to you, test this.
	 */
	lzma_end(&self->lzmastream);
	res = lzma_code(&self->lzmastream, LZMA_RUN);
	if (res == LZMA_MEM_ERROR)
		g_error("XbLzmaDecompressor: Not enough memory for lzma use");
	if (res != LZMA_OK)
		g_error("XbLzmaDecompressor: Unexpected lzma error");
}

static GConverterResult
xb_lzma_decompressor_convert(GConverter *converter,
			     const void *inbuf,
			     gsize inbuf_size,
			     void *outbuf,
			     gsize outbuf_size,
			     GConverterFlags flags,
			     gsize *bytes_read,
			     gsize *bytes_written,
			     GError **error)
{
	XbLzmaDecompressor *self = XB_LZMA_DECOMPRESSOR(converter);
	lzma_ret res;

	self->lzmastream.next_in = (void *)inbuf;
	self->lzmastream.avail_in = inbuf_size;
	self->lzmastream.next_out = outbuf;
	self->lzmastream.avail_out = outbuf_size;

	res = lzma_code(&self->lzmastream, LZMA_RUN);
	if (res == LZMA_DATA_ERROR) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "Invalid compressed data");
		return G_CONVERTER_ERROR;
	}
	if (res == LZMA_MEM_ERROR) {
		g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not enough memory");
		return G_CONVERTER_ERROR;
	}
	if (res == LZMA_OK || res == LZMA_STREAM_END) {
		*bytes_read = inbuf_size - self->lzmastream.avail_in;
		*bytes_written = outbuf_size - self->lzmastream.avail_out;
		if (res == LZMA_STREAM_END)
			return G_CONVERTER_FINISHED;
		return G_CONVERTER_CONVERTED;
	}

	g_assert_not_reached();
}

static void
xb_lzma_decompressor_iface_init(GConverterIface *iface)
{
	iface->convert = xb_lzma_decompressor_convert;
	iface->reset = xb_lzma_decompressor_reset;
}
