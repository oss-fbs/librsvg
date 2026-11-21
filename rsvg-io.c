/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2002, 2003, 2004, 2005 Dom Lachowicz <cinamod@hotmail.com>
   Copyright (C) 2003, 2004, 2005 Caleb Moore <c.moore@student.unsw.edu.au>
   Copyright © 2011, 2012 Christian Persch
   Copyright (C) 2026 Randy Butler

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "config.h"

#include "rsvg-io.h"
#include "rsvg-private.h"

#include <string.h>

/* Copied from soup-request-data.c (LGPL2+):
 * Copyright (C) 2009, 2010 Red Hat, Inc.
 * Copyright (C) 2010 Igalia, S.L.
 * and from soup-uri.c:
 * Copyright 1999-2003 Ximian, Inc.
 */

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

static char *
uri_decoded_copy (const char *part, 
                  gsize length,
                  gsize *out_len)
{
    unsigned char *s, *d;
    char *decoded = g_strndup (part, length);

    s = d = (unsigned char *)decoded;
    do {
        if (*s == '%') {
            if (!g_ascii_isxdigit (s[1]) ||
                !g_ascii_isxdigit (s[2])) {
                *d++ = *s;
                continue;
            }
            *d++ = HEXCHAR (s);
            s += 2;
        } else {
            *d++ = *s;
        }
    } while (*s++);

    /* The loop copies the trailing NUL. Length excludes it so a
     * percent-encoded 0x00 (H2-6) is not cut by strlen. */
    if (out_len)
        *out_len = (gsize) (d - (unsigned char *) decoded) - 1;

    return decoded;
}

#define BASE64_INDICATOR     ";base64"
#define BASE64_INDICATOR_LEN (sizeof (";base64") - 1)

static char *
rsvg_decode_data_uri (const char *uri,
                      char **out_mime_type,
                      gsize *out_len,
                      gsize max_bytes,
                      GError **error)
{
    const char *comma, *start, *end;
    char *mime_type;
    char *data;
    gsize data_len;
    gboolean base64 = FALSE;

    g_assert (out_len != NULL);
    g_assert (strncmp (uri, "data:", 5) == 0);

    mime_type = NULL;
    start = uri + 5;
    comma = strchr (start, ',');

    if (comma && comma != start) {
        /* Deal with MIME type / params */
        /* Allow data:;base64,... (empty MIME, issue #548). `>` missed
         * that case because ";base64" sits flush against the comma. */
        if (comma >= start + BASE64_INDICATOR_LEN &&
            !g_ascii_strncasecmp (comma - BASE64_INDICATOR_LEN, BASE64_INDICATOR, BASE64_INDICATOR_LEN)) {
            end = comma - BASE64_INDICATOR_LEN;
            base64 = TRUE;
        } else {
            end = comma;
        }

        if (end != start) {
            mime_type = uri_decoded_copy (start, end - start, NULL);
        }
    }

    if (comma)
        start = comma + 1;

    if (*start) {
        gsize encoded_len = strlen (start);

        if (max_bytes == 0)
            max_bytes = RSVG_MAX_DATA_URI_BYTES;
        if (encoded_len > max_bytes) {
            g_free (mime_type);
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "data: URI is too large");
            *out_len = 0;
            return NULL;
        }

        data = uri_decoded_copy (start, encoded_len, &data_len);

        if (base64)
            data = (char *) g_base64_decode_inplace (data, &data_len);
    } else {
        data = NULL;
        data_len = 0;
    }

    if (out_mime_type)
        *out_mime_type = mime_type;
    else
        g_free (mime_type);

    *out_len = data_len;
    return data;
}

/* Reject before the full read (H2-1). Unknown size is allowed through;
 * the caller still checks the buffer length afterwards. */
static gboolean
rsvg_gfile_within_size_limit (GFile *file,
                              gsize max_bytes,
                              GCancellable *cancellable,
                              GError **error)
{
    GFileInfo *info;
    goffset size;

    if (file == NULL)
        return FALSE;
    if (max_bytes == 0)
        max_bytes = RSVG_MAX_DATA_URI_BYTES;

    info = g_file_query_info (file,
                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                              G_FILE_QUERY_INFO_NONE,
                              cancellable,
                              NULL);
    if (info == NULL)
        return TRUE;

    size = g_file_info_get_size (info);
    g_object_unref (info);

    if (size > 0 && (guint64) size > (guint64) max_bytes) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "URI is too large");
        return FALSE;
    }
    return TRUE;
}

static gboolean
rsvg_buffer_within_size_limit (gsize len, gsize max_bytes, GError **error)
{
    if (max_bytes == 0)
        max_bytes = RSVG_MAX_DATA_URI_BYTES;
    if (len > max_bytes) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "URI is too large");
        return FALSE;
    }
    return TRUE;
}

static char *
rsvg_acquire_file_data (const char *uri,
                        char **out_mime_type,
                        gsize *out_len,
                        gsize max_bytes,
                        GCancellable *cancellable,
                        GError **error)
{
    GFile *file;
    gchar *path, *data;
    gsize len;
    char *content_type;

    rsvg_return_val_if_fail (uri != NULL, NULL, error);
    g_assert (out_len != NULL);

    file = g_file_new_for_uri (uri);
    path = g_file_get_path (file);

    if (path == NULL) {
        g_object_unref (file);
        return NULL;
    }

    if (!rsvg_gfile_within_size_limit (file, max_bytes, cancellable, error)) {
        g_free (path);
        g_object_unref (file);
        return NULL;
    }

    if (!g_file_get_contents (path, &data, &len, error)) {
        g_free (path);
        g_object_unref (file);
        return NULL;
    }

    if (!rsvg_buffer_within_size_limit (len, max_bytes, error)) {
        g_free (data);
        g_free (path);
        g_object_unref (file);
        return NULL;
    }

    if (out_mime_type &&
        (content_type = g_content_type_guess (path, (guchar *) data, len, NULL))) {
        *out_mime_type = g_content_type_get_mime_type (content_type);
        g_free (content_type);
    }

    g_free (path);
    g_object_unref (file);

    *out_len = len;
    return data;
}

static GInputStream *
rsvg_acquire_gvfs_stream (const char *uri, 
                          const char *base_uri, 
                          char **out_mime_type,
                          GCancellable *cancellable,
                          GError **error)
{
    GFile *base, *file;
    GFileInputStream *stream;
    GError *err = NULL;

    file = g_file_new_for_uri (uri);

    stream = g_file_read (file, cancellable, &err);
    g_object_unref (file);

    if (stream == NULL &&
        g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
        g_clear_error (&err);

        base = g_file_new_for_uri (base_uri);
        file = g_file_resolve_relative_path (base, uri);
        g_object_unref (base);

        stream = g_file_read (file, cancellable, &err);
        g_object_unref (file);
    }

    if (stream == NULL) {
        g_propagate_error (error, err);
        return NULL;
    }

    if (out_mime_type) {
        GFileInfo *file_info;
        const char *content_type;

        file_info = g_file_input_stream_query_info (stream, 
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    cancellable,
                                                    NULL /* error */);
        if (file_info &&
            (content_type = g_file_info_get_content_type (file_info)))
            *out_mime_type = g_content_type_get_mime_type (content_type);
        else
            *out_mime_type = NULL;

        if (file_info)
            g_object_unref (file_info);
    }

    return G_INPUT_STREAM (stream);
}

static char *
rsvg_acquire_gvfs_data (const char *uri,
                        const char *base_uri,
                        char **out_mime_type,
                        gsize *out_len,
                        gsize max_bytes,
                        GCancellable *cancellable,
                        GError **error)
{
    GFile *base, *file;
    GError *err;
    char *data;
    gsize len;
    char *content_type;
    gboolean res;

    file = g_file_new_for_uri (uri);

    err = NULL;
    data = NULL;
    if (!rsvg_gfile_within_size_limit (file, max_bytes, cancellable, &err)) {
        g_object_unref (file);
        g_propagate_error (error, err);
        return NULL;
    }
    if (!(res = g_file_load_contents (file, cancellable, &data, &len, NULL, &err)) &&
        g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
        base_uri != NULL) {
        g_clear_error (&err);
        g_object_unref (file);

        base = g_file_new_for_uri (base_uri);
        file = g_file_resolve_relative_path (base, uri);
        g_object_unref (base);

        if (!rsvg_gfile_within_size_limit (file, max_bytes, cancellable, &err)) {
            g_object_unref (file);
            g_propagate_error (error, err);
            return NULL;
        }
        res = g_file_load_contents (file, cancellable, &data, &len, NULL, &err);
    }

    g_object_unref (file);

    if (err) {
        g_propagate_error (error, err);
        return NULL;
    }

    if (!rsvg_buffer_within_size_limit (len, max_bytes, error)) {
        g_free (data);
        return NULL;
    }

    if (out_mime_type &&
        (content_type = g_content_type_guess (uri, (guchar *) data, len, NULL))) {
        *out_mime_type = g_content_type_get_mime_type (content_type);
        g_free (content_type);
    }

    *out_len = len;
    return data;
}

char *
_rsvg_io_acquire_data (const char *href, 
                       const char *base_uri, 
                       char **mime_type,
                       gsize *len,
                       gsize max_data_uri_bytes,
                       GCancellable *cancellable,
                       GError **error)
{
    char *data;
    gsize llen;

    if (!(href && *href)) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Invalid URI");
        return NULL;
    }

    if (!len)
        len = &llen;

    if (strncmp (href, "data:", 5) == 0)
      return rsvg_decode_data_uri (href, mime_type, len, max_data_uri_bytes, error);

    if ((data = rsvg_acquire_file_data (href, mime_type, len, max_data_uri_bytes, cancellable, NULL)))
      return data;

    if ((data = rsvg_acquire_gvfs_data (href, base_uri, mime_type, len, max_data_uri_bytes, cancellable, error)))
      return data;

    return NULL;
}

GInputStream *
_rsvg_io_acquire_stream (const char *href, 
                         const char *base_uri, 
                         char **mime_type,
                         gsize max_data_uri_bytes,
                         GCancellable *cancellable,
                         GError **error)
{
    GInputStream *stream;
    char *data;
    gsize len;

    if (!(href && *href)) {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Invalid URI");
        return NULL;
    }

    if (strncmp (href, "data:", 5) == 0) {
        if (!(data = rsvg_decode_data_uri (href, mime_type, &len, max_data_uri_bytes, error)))
            return NULL;

        return g_memory_input_stream_new_from_data (data, len, (GDestroyNotify) g_free);
    }

    if ((data = rsvg_acquire_file_data (href, mime_type, &len, max_data_uri_bytes, cancellable, NULL)))
      return g_memory_input_stream_new_from_data (data, len, (GDestroyNotify) g_free);

    if ((stream = rsvg_acquire_gvfs_stream (href, base_uri, mime_type, cancellable, error)))
      return stream;

    return NULL;
}
