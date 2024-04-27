/*
 * Copyright 2024 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "xb-version.h"

/**
 * xb_version_string:
 *
 * Gets the XMLb installed runtime version.
 *
 * Returns: a version number, e.g. "0.3.19"
 *
 * Since: 0.3.19
 **/
const gchar *
xb_version_string(void)
{
	return G_STRINGIFY(XMLB_MAJOR_VERSION) "." G_STRINGIFY(XMLB_MINOR_VERSION) "." G_STRINGIFY(
	    XMLB_MICRO_VERSION);
}
