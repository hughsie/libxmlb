/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "xb-zstd-decompressor.h"

#include "config.h"

#include <gio/gio.h>
#include <zstd.h>

static void
xb_zstd_decompressor_iface_init(GConverterIface *iface);

struct _XbZstdDecompressor {
	GObject parent_instance;
	ZSTD_DStream *zstdstream;
};

G_DEFINE_TYPE_WITH_CODE(XbZstdDecompressor,
			xb_zstd_decompressor,
			G_TYPE_OBJECT,
			G_IMPLEMENT_INTERFACE(G_TYPE_CONVERTER, xb_zstd_decompressor_iface_init))

static void
xb_zstd_decompressor_finalize(GObject *object)
{
	XbZstdDecompressor *self = XB_ZSTD_DECOMPRESSOR(object);
	ZSTD_freeDStream(self->zstdstream);
	G_OBJECT_CLASS(xb_zstd_decompressor_parent_class)->finalize(object);
}

static void
xb_zstd_decompressor_init(XbZstdDecompressor *self)
{
}

static void
xb_zstd_decompressor_constructed(GObject *object)
{
	XbZstdDecompressor *self = XB_ZSTD_DECOMPRESSOR(object);
	self->zstdstream = ZSTD_createDStream();
}

static void
xb_zstd_decompressor_class_init(XbZstdDecompressorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = xb_zstd_decompressor_finalize;
	object_class->constructed = xb_zstd_decompressor_constructed;
}

XbZstdDecompressor *
xb_zstd_decompressor_new(void)
{
	return g_object_new(XB_TYPE_ZSTD_DECOMPRESSOR, NULL);
}

static void
xb_zstd_decompressor_reset(GConverter *converter)
{
	XbZstdDecompressor *self = XB_ZSTD_DECOMPRESSOR(converter);
	ZSTD_initDStream(self->zstdstream);
}

static GConverterResult
xb_zstd_decompressor_convert(GConverter *converter,
			     const void *inbuf,
			     gsize inbuf_size,
			     void *outbuf,
			     gsize outbuf_size,
			     GConverterFlags flags,
			     gsize *bytes_read,
			     gsize *bytes_written,
			     GError **error)
{
	XbZstdDecompressor *self = XB_ZSTD_DECOMPRESSOR(converter);
	ZSTD_outBuffer output = {
	    .dst = outbuf,
	    .size = outbuf_size,
	    .pos = 0,
	};
	ZSTD_inBuffer input = {
	    .src = inbuf,
	    .size = inbuf_size,
	    .pos = 0,
	};
	size_t res;

	res = ZSTD_decompressStream(self->zstdstream, &output, &input);
	if (res == 0)
		return G_CONVERTER_FINISHED;
	if (ZSTD_isError(res)) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_INVALID_DATA,
			    "cannot decompress data: %s",
			    ZSTD_getErrorName(res));
		return G_CONVERTER_ERROR;
	}
	*bytes_read = input.pos;
	*bytes_written = output.pos;
	return G_CONVERTER_CONVERTED;
}

static void
xb_zstd_decompressor_iface_init(GConverterIface *iface)
{
	iface->convert = xb_zstd_decompressor_convert;
	iface->reset = xb_zstd_decompressor_reset;
}
