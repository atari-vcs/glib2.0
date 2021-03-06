/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <locale.h>
#include <gio/gio.h>

#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <dbus/dbus.h>

/* ---------------------------------------------------------------------------------------------------- */

static void
hexdump (const guchar *str, gsize len)
{
  const guchar *data = (const guchar *) str;
  guint n, m;

  for (n = 0; n < len; n += 16)
    {
      g_printerr ("%04x: ", n);

      for (m = n; m < n + 16; m++)
        {
          if (m > n && (m%4) == 0)
            g_printerr (" ");
          if (m < len)
            g_printerr ("%02x ", data[m]);
          else
            g_printerr ("   ");
        }

      g_printerr ("   ");

      for (m = n; m < len && m < n + 16; m++)
        g_printerr ("%c", g_ascii_isprint (data[m]) ? data[m] : '.');

      g_printerr ("\n");
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
append_gv_to_dbus_iter (DBusMessageIter  *iter,
                        GVariant         *value,
                        GError          **error)
{
  const GVariantType *type;

  type = g_variant_get_type (value);
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      dbus_bool_t v = g_variant_get_boolean (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BOOLEAN, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
    {
      guint8 v = g_variant_get_byte (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BYTE, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
    {
      gint16 v = g_variant_get_int16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT16, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    {
      guint16 v = g_variant_get_uint16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT16, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    {
      gint32 v = g_variant_get_int32 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT32, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    {
      guint32 v = g_variant_get_uint32 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT32, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
    {
      gint64 v = g_variant_get_int64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT64, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
    {
      guint64 v = g_variant_get_uint64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT64, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    {
      gdouble v = g_variant_get_double (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_DOUBLE, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_OBJECT_PATH, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_SIGNATURE, &v);
    }
  else if (g_variant_type_is_variant (type))
    {
      DBusMessageIter sub;
      GVariant *child;

      child = g_variant_get_child_value (value, 0);
      dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT,
                                        g_variant_get_type_string (child),
                                        &sub);
      if (!append_gv_to_dbus_iter (&sub, child, error))
        {
            g_variant_unref (child);
            goto fail;
        }
      dbus_message_iter_close_container (iter, &sub);
      g_variant_unref (child);
    }
  else if (g_variant_type_is_array (type))
    {
      DBusMessageIter dbus_iter;
      const gchar *type_string;
      GVariantIter gv_iter;
      GVariant *item;

      type_string = g_variant_get_type_string (value);
      type_string++; /* skip the 'a' */

      dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY,
                                        type_string, &dbus_iter);
      g_variant_iter_init (&gv_iter, value);

      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&dbus_iter, item, error))
            {
              goto fail;
            }
        }

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else if (g_variant_type_is_tuple (type))
    {
      DBusMessageIter dbus_iter;
      GVariantIter gv_iter;
      GVariant *item;

      dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT,
                                        NULL, &dbus_iter);
      g_variant_iter_init (&gv_iter, value);

      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&dbus_iter, item, error))
            goto fail;
        }

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else if (g_variant_type_is_dict_entry (type))
    {
      DBusMessageIter dbus_iter;
      GVariant *key, *val;

      dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY,
                                        NULL, &dbus_iter);
      key = g_variant_get_child_value (value, 0);
      if (!append_gv_to_dbus_iter (&dbus_iter, key, error))
        {
          g_variant_unref (key);
          goto fail;
        }
      g_variant_unref (key);

      val = g_variant_get_child_value (value, 1);
      if (!append_gv_to_dbus_iter (&dbus_iter, val, error))
        {
          g_variant_unref (val);
          goto fail;
        }
      g_variant_unref (val);

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Error serializing GVariant with type-string '%s' to a D-Bus message",
                   g_variant_get_type_string (value));
      goto fail;
    }

  return TRUE;

 fail:
  return FALSE;
}

static gboolean
append_gv_to_dbus_message (DBusMessage  *message,
                           GVariant     *value,
                           GError      **error)
{
  gboolean ret;
  guint n;

  ret = FALSE;

  if (value != NULL)
    {
      DBusMessageIter iter;
      GVariantIter gv_iter;
      GVariant *item;

      dbus_message_iter_init_append (message, &iter);

      g_variant_iter_init (&gv_iter, value);
      n = 0;
      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&iter, item, error))
            {
              g_prefix_error (error,
                              "Error encoding in-arg %d: ",
                              n);
              goto out;
            }
          n++;
        }
    }

  ret = TRUE;

 out:
  return ret;
}

static void
print_gv_dbus_message (GVariant *value)
{
  DBusMessage *message;
  char *blob;
  int blob_len;
  GError *error;

  message = dbus_message_new (DBUS_MESSAGE_TYPE_METHOD_CALL);
  dbus_message_set_serial (message, 0x41);
  dbus_message_set_path (message, "/foo/bar");
  dbus_message_set_member (message, "Member");

  error = NULL;
  if (!append_gv_to_dbus_message (message, value, &error))
    {
      g_printerr ("Error printing GVariant as DBusMessage: %s", error->message);
      g_error_free (error);
      goto out;
    }

  dbus_message_marshal (message, &blob, &blob_len);
  g_printerr ("\n");
  hexdump ((guchar *) blob, blob_len);
 out:
  dbus_message_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
dbus_1_message_append (GString *s,
                       guint indent,
                       DBusMessageIter *iter)
{
  gint arg_type;
  DBusMessageIter sub;

  g_string_append_printf (s, "%*s", indent, "");

  arg_type = dbus_message_iter_get_arg_type (iter);
  switch (arg_type)
    {
     case DBUS_TYPE_BOOLEAN:
      {
        dbus_bool_t value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "bool: %s\n", value ? "true" : "false");
        break;
      }

     case DBUS_TYPE_BYTE:
      {
        guchar value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "byte: 0x%02x\n", (guint) value);
        break;
      }

     case DBUS_TYPE_INT16:
      {
        gint16 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int16: %" G_GINT16_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT16:
      {
        guint16 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint16: %" G_GUINT16_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_INT32:
      {
        gint32 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int32: %" G_GINT32_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT32:
      {
        guint32 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint32: %" G_GUINT32_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_INT64:
      {
        gint64 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int64: %" G_GINT64_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT64:
      {
        guint64 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint64: %" G_GUINT64_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_DOUBLE:
      {
        gdouble value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "double: %f\n", value);
        break;
      }

     case DBUS_TYPE_STRING:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "string: '%s'\n", value);
        break;
      }

     case DBUS_TYPE_OBJECT_PATH:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "object_path: '%s'\n", value);
        break;
      }

     case DBUS_TYPE_SIGNATURE:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "signature: '%s'\n", value);
        break;
      }

#ifdef DBUS_TYPE_UNIX_FD
    case DBUS_TYPE_UNIX_FD:
      {
        /* unfortunately there's currently no way to get just the
         * protocol value, since dbus_message_iter_get_basic() wants
         * to be 'helpful' and dup the fd for the user...
         */
        g_string_append (s, "unix-fd: (not extracted)\n");
        break;
      }
#endif

     case DBUS_TYPE_VARIANT:
       g_string_append_printf (s, "variant:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_ARRAY:
       g_string_append_printf (s, "array:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_STRUCT:
       g_string_append_printf (s, "struct:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_DICT_ENTRY:
       g_string_append_printf (s, "dict_entry:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     default:
       g_printerr ("Error serializing D-Bus message to GVariant. Unsupported arg type '%c' (%d)",
                   arg_type,
                   arg_type);
       g_assert_not_reached ();
       break;
    }
}

static gchar *
dbus_1_message_print (DBusMessage *message)
{
  GString *s;
  guint n;
  DBusMessageIter iter;

  s = g_string_new (NULL);
  n = 0;
  dbus_message_iter_init (message, &iter);
  while (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_INVALID)
    {
      g_string_append_printf (s, "value %d: ", n);
      dbus_1_message_append (s, 2, &iter);
      dbus_message_iter_next (&iter);
      n++;
    }

  return g_string_free (s, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
get_body_signature (GVariant *value)
{
  const gchar *s;
  gsize len;
  gchar *ret;

  if (value == NULL)
    {
      ret = g_strdup ("");
      goto out;
    }

  s = g_variant_get_type_string (value);
  len = strlen (s);
  g_assert (len >= 2);

  ret = g_strndup (s + 1, len - 2);

 out:
  return ret;
}

/* If @value is floating, this assumes ownership. */
static gchar *
get_and_check_serialization (GVariant *value)
{
  guchar *blob;
  gsize blob_size;
  DBusMessage *dbus_1_message;
  GDBusMessage *message;
  GDBusMessage *recovered_message;
  GError *error;
  DBusError dbus_error;
  gchar *s = NULL;
  guint n;

  message = g_dbus_message_new ();
  g_dbus_message_set_body (message, value);
  g_dbus_message_set_message_type (message, G_DBUS_MESSAGE_TYPE_METHOD_CALL);
  g_dbus_message_set_serial (message, 0x41);
  s = get_body_signature (value);
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_PATH, g_variant_new_object_path ("/foo/bar"));
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_MEMBER, g_variant_new_string ("Member"));
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE, g_variant_new_signature (s));
  g_free (s);

  /* First check that the serialization to the D-Bus wire format is correct - do this for both byte orders */
  for (n = 0; n < 2; n++)
    {
      GDBusMessageByteOrder byte_order;
      switch (n)
        {
        case 0:
          byte_order = G_DBUS_MESSAGE_BYTE_ORDER_BIG_ENDIAN;
          break;
        case 1:
          byte_order = G_DBUS_MESSAGE_BYTE_ORDER_LITTLE_ENDIAN;
          break;
        case 2:
          g_assert_not_reached ();
          break;
        }
      g_dbus_message_set_byte_order (message, byte_order);

      error = NULL;
      blob = g_dbus_message_to_blob (message,
                                     &blob_size,
                                     G_DBUS_CAPABILITY_FLAGS_NONE,
                                     &error);
      g_assert_no_error (error);
      g_assert (blob != NULL);

      switch (byte_order)
        {
        case G_DBUS_MESSAGE_BYTE_ORDER_BIG_ENDIAN:
          g_assert_cmpint (blob[0], ==, 'B');
          break;
        case G_DBUS_MESSAGE_BYTE_ORDER_LITTLE_ENDIAN:
          g_assert_cmpint (blob[0], ==, 'l');
          break;
        }

      dbus_error_init (&dbus_error);
      dbus_1_message = dbus_message_demarshal ((char *) blob, blob_size, &dbus_error);
      if (dbus_error_is_set (&dbus_error))
        {
          g_printerr ("Error calling dbus_message_demarshal() on this blob: %s: %s\n",
                      dbus_error.name,
                      dbus_error.message);
          hexdump (blob, blob_size);
          dbus_error_free (&dbus_error);

          s = g_variant_print (value, TRUE);
          g_printerr ("\nThe blob was generated from the following GVariant value:\n%s\n\n", s);
          g_free (s);

          g_printerr ("If the blob was encoded using DBusMessageIter, the payload would have been:\n");
          print_gv_dbus_message (value);

          g_assert_not_reached ();
        }

      s = dbus_1_message_print (dbus_1_message);
      dbus_message_unref (dbus_1_message);

      /* Then serialize back and check that the body is identical */

      error = NULL;
      recovered_message = g_dbus_message_new_from_blob (blob,
                                                        blob_size,
                                                        G_DBUS_CAPABILITY_FLAGS_NONE,
                                                        &error);
      g_assert_no_error (error);
      g_assert (recovered_message != NULL);

      if (value == NULL)
        {
          g_assert (g_dbus_message_get_body (recovered_message) == NULL);
        }
      else
        {
          g_assert (g_dbus_message_get_body (recovered_message) != NULL);
          g_assert_cmpvariant (g_dbus_message_get_body (recovered_message), value);
        }
      g_object_unref (recovered_message);
      g_free (blob);
    }

  g_object_unref (message);

  return g_steal_pointer (&s);
}

/* If @value is floating, this assumes ownership. */
static void
check_serialization (GVariant *value,
                     const gchar *expected_dbus_1_output)
{
  gchar *s = get_and_check_serialization (value);
  g_assert_cmpstr (s, ==, expected_dbus_1_output);
  g_free (s);
}

static void
test_message_serialize_basic (void)
{
  check_serialization (NULL, "");

  check_serialization (g_variant_new ("(sogybnqiuxtd)",
                                      "this is a string",
                                      "/this/is/a/path",
                                      "sad",
                                      42,
                                      TRUE,
                                      -42,
                                      60000,
                                      -44,
                                      100000,
                                      -(G_GUINT64_CONSTANT(2)<<34),
                                      G_GUINT64_CONSTANT(0xffffffffffffffff),
                                      42.5),
                       "value 0:   string: 'this is a string'\n"
                       "value 1:   object_path: '/this/is/a/path'\n"
                       "value 2:   signature: 'sad'\n"
                       "value 3:   byte: 0x2a\n"
                       "value 4:   bool: true\n"
                       "value 5:   int16: -42\n"
                       "value 6:   uint16: 60000\n"
                       "value 7:   int32: -44\n"
                       "value 8:   uint32: 100000\n"
                       "value 9:   int64: -34359738368\n"
                       "value 10:   uint64: 18446744073709551615\n"
                       "value 11:   double: 42.500000\n");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_message_serialize_complex (void)
{
  GError *error;
  GVariant *value;
  guint i;
  gchar *serialization = NULL;

  error = NULL;

  value = g_variant_parse (G_VARIANT_TYPE ("(aia{ss})"),
                           "([1, 2, 3], {'one': 'white', 'two': 'black'})",
                           NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  check_serialization (value,
                       "value 0:   array:\n"
                       "    int32: 1\n"
                       "    int32: 2\n"
                       "    int32: 3\n"
                       "value 1:   array:\n"
                       "    dict_entry:\n"
                       "      string: 'one'\n"
                       "      string: 'white'\n"
                       "    dict_entry:\n"
                       "      string: 'two'\n"
                       "      string: 'black'\n");
  g_variant_unref (value);

  value = g_variant_parse (G_VARIANT_TYPE ("(sa{sv}as)"),
                           "('01234567890123456', {}, ['Something'])",
                           NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  check_serialization (value,
                       "value 0:   string: '01234567890123456'\n"
                       "value 1:   array:\n"
                       "value 2:   array:\n"
                       "    string: 'Something'\n");
  g_variant_unref (value);

  /* https://bugzilla.gnome.org/show_bug.cgi?id=621838 */
  check_serialization (g_variant_new_parsed ("(@aay [], {'cwd': <'/home/davidz/Hacking/glib/gio/tests'>})"),
                       "value 0:   array:\n"
                       "value 1:   array:\n"
                       "    dict_entry:\n"
                       "      string: 'cwd'\n"
                       "      variant:\n"
                       "        string: '/home/davidz/Hacking/glib/gio/tests'\n");

#ifdef DBUS_TYPE_UNIX_FD
  value = g_variant_parse (G_VARIANT_TYPE ("(hah)"),
                           "(42, [43, 44])",
                           NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  /* about (not extracted), see comment in DBUS_TYPE_UNIX_FD case in
   * dbus_1_message_append() above.
   */
  check_serialization (value,
                       "value 0:   unix-fd: (not extracted)\n"
                       "value 1:   array:\n"
                       "    unix-fd: (not extracted)\n"
                       "    unix-fd: (not extracted)\n");
  g_variant_unref (value);
#endif

  /* Deep nesting of variants (just below the recursion limit). */
  value = g_variant_new_string ("buried");
  for (i = 0; i < 64; i++)
    value = g_variant_new_variant (value);
  value = g_variant_new_tuple (&value, 1);

  serialization = get_and_check_serialization (value);
  g_assert_nonnull (serialization);
  g_assert_true (g_str_has_prefix (serialization,
                                   "value 0:   variant:\n"
                                   "    variant:\n"
                                   "      variant:\n"));
  g_free (serialization);

  /* Deep nesting of arrays and structs (just below the recursion limit).
   * See https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-marshaling-signature */
  value = g_variant_new_string ("hello");
  for (i = 0; i < 32; i++)
    value = g_variant_new_tuple (&value, 1);
  for (i = 0; i < 32; i++)
    value = g_variant_new_array (NULL, &value, 1);
  value = g_variant_new_tuple (&value, 1);

  serialization = get_and_check_serialization (value);
  g_assert_nonnull (serialization);
  g_assert_true (g_str_has_prefix (serialization,
                                   "value 0:   array:\n"
                                   "    array:\n"
                                   "      array:\n"));
  g_free (serialization);
}


/* ---------------------------------------------------------------------------------------------------- */

static void
replace (char       *blob,
         gsize       len,
	 const char *before,
	 const char *after)
{
  gsize i;
  gsize slen = strlen (before) + 1;

  g_assert_cmpuint (strlen (before), ==, strlen (after));
  g_assert_cmpuint (len, >=, slen);

  for (i = 0; i < (len - slen + 1); i++)
    {
      if (memcmp (blob + i, before, slen) == 0)
        memcpy (blob + i, after, slen);
    }
}

static void
test_message_serialize_invalid (void)
{
  guint n;

  /* Other things we could check (note that GDBus _does_ check for all
   * these things - we just don't have test-suit coverage for it)
   *
   *  - array exceeding 64 MiB (2^26 bytes) - unfortunately libdbus-1 checks
   *    this, e.g.
   *
   *      process 19620: arguments to dbus_message_iter_append_fixed_array() were incorrect,
   *      assertion "n_elements <= DBUS_MAXIMUM_ARRAY_LENGTH / _dbus_type_get_alignment (element_type)"
   *      failed in file dbus-message.c line 2344.
   *      This is normally a bug in some application using the D-Bus library.
   *      D-Bus not built with -rdynamic so unable to print a backtrace
   *      Aborted (core dumped)
   *
   *  - message exceeding 128 MiB (2^27 bytes)
   *
   *  - endianness, message type, flags, protocol version
   */

  for (n = 0; n < 3; n++)
    {
      GDBusMessage *message;
      GError *error;
      DBusMessage *dbus_message;
      char *blob;
      int blob_len;
      /* these are in pairs with matching length */
      const gchar *valid_utf8_str = "this is valid...";
      const gchar *invalid_utf8_str = "this is invalid\xff";
      const gchar *valid_signature = "a{sv}a{sv}a{sv}aiai";
      const gchar *invalid_signature = "not valid signature";
      const gchar *valid_object_path = "/this/is/a/valid/dbus/object/path";
      const gchar *invalid_object_path = "/this/is/not a valid object path!";

      dbus_message = dbus_message_new (DBUS_MESSAGE_TYPE_METHOD_CALL);
      dbus_message_set_serial (dbus_message, 0x41);
      dbus_message_set_path (dbus_message, "/foo/bar");
      dbus_message_set_member (dbus_message, "Member");
      switch (n)
        {
        case 0:
          /* invalid UTF-8 */
          dbus_message_append_args (dbus_message,
                                    DBUS_TYPE_STRING, &valid_utf8_str,
                                    DBUS_TYPE_INVALID);
          break;

        case 1:
          /* invalid object path */
          dbus_message_append_args (dbus_message,
                                    DBUS_TYPE_OBJECT_PATH, &valid_object_path,
                                    DBUS_TYPE_INVALID);
          break;

        case 2:
          /* invalid signature */
          dbus_message_append_args (dbus_message,
                                    DBUS_TYPE_SIGNATURE, &valid_signature,
                                    DBUS_TYPE_INVALID);
          break;

        default:
          g_assert_not_reached ();
          break;
        }
      dbus_message_marshal (dbus_message, &blob, &blob_len);
      /* hack up the message to be invalid by replacing each valid string
       * with its invalid counterpart */
      replace (blob, blob_len, valid_utf8_str, invalid_utf8_str);
      replace (blob, blob_len, valid_object_path, invalid_object_path);
      replace (blob, blob_len, valid_signature, invalid_signature);

      error = NULL;
      message = g_dbus_message_new_from_blob ((guchar *) blob,
                                              blob_len,
                                              G_DBUS_CAPABILITY_FLAGS_NONE,
                                              &error);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
      g_error_free (error);
      g_assert (message == NULL);

      dbus_free (blob);
      dbus_message_unref (dbus_message);
    }

}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_message_serialize_header_checks (void)
{
  GDBusMessage *message;
  GDBusMessage *reply;
  GError *error = NULL;
  guchar *blob;
  gsize blob_size;

  /*
   * check we can't serialize messages with INVALID type
   */
  message = g_dbus_message_new ();
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: type is INVALID");
  g_clear_error (&error);
  g_assert_null (blob);
  g_object_unref (message);

  /*
   * check that we can't serialize messages with SIGNATURE set to a non-signature-typed value
   */
  message = g_dbus_message_new_signal ("/the/path", "The.Interface", "TheMember");
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE, g_variant_new_boolean (FALSE));
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);

  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Signature header found but is not of type signature");
  g_assert_null (blob);

  g_clear_error (&error);
  g_clear_object (&message);

  /*
   * check we can't serialize signal messages with INTERFACE, PATH or MEMBER unset / set to reserved value
   */
  message = g_dbus_message_new_signal ("/the/path", "The.Interface", "TheMember");
  /* ----- */
  /* interface NULL => error */
  g_dbus_message_set_interface (message, NULL);
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: SIGNAL message: PATH, INTERFACE or MEMBER header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* interface reserved value => error */
  g_dbus_message_set_interface (message, "org.freedesktop.DBus.Local");
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: SIGNAL message: The INTERFACE header field is using the reserved value org.freedesktop.DBus.Local");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset interface */
  g_dbus_message_set_interface (message, "The.Interface");
  /* ----- */
  /* path NULL => error */
  g_dbus_message_set_path (message, NULL);
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: SIGNAL message: PATH, INTERFACE or MEMBER header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* path reserved value => error */
  g_dbus_message_set_path (message, "/org/freedesktop/DBus/Local");
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: SIGNAL message: The PATH header field is using the reserved value /org/freedesktop/DBus/Local");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset path */
  g_dbus_message_set_path (message, "/the/path");
  /* ----- */
  /* member NULL => error */
  g_dbus_message_set_member (message, NULL);
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: SIGNAL message: PATH, INTERFACE or MEMBER header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset member */
  g_dbus_message_set_member (message, "TheMember");
  /* ----- */
  /* done */
  g_object_unref (message);

  /*
   * check that we can't serialize method call messages with PATH or MEMBER unset
   */
  message = g_dbus_message_new_method_call (NULL, "/the/path", NULL, "TheMember");
  /* ----- */
  /* path NULL => error */
  g_dbus_message_set_path (message, NULL);
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: METHOD_CALL message: PATH or MEMBER header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset path */
  g_dbus_message_set_path (message, "/the/path");
  /* ----- */
  /* member NULL => error */
  g_dbus_message_set_member (message, NULL);
  blob = g_dbus_message_to_blob (message, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: METHOD_CALL message: PATH or MEMBER header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset member */
  g_dbus_message_set_member (message, "TheMember");
  /* ----- */
  /* done */
  g_object_unref (message);

  /*
   * check that we can't serialize method reply messages with REPLY_SERIAL unset
   */
  message = g_dbus_message_new_method_call (NULL, "/the/path", NULL, "TheMember");
  g_dbus_message_set_serial (message, 42);
  /* method reply */
  reply = g_dbus_message_new_method_reply (message);
  g_assert_cmpint (g_dbus_message_get_reply_serial (reply), ==, 42);
  g_dbus_message_set_header (reply, G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL, NULL);
  blob = g_dbus_message_to_blob (reply, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: METHOD_RETURN message: REPLY_SERIAL header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  g_object_unref (reply);
  /* method error - first nuke ERROR_NAME, then REPLY_SERIAL */
  reply = g_dbus_message_new_method_error (message, "Some.Error.Name", "the message");
  g_assert_cmpint (g_dbus_message_get_reply_serial (reply), ==, 42);
  /* nuke ERROR_NAME */
  g_dbus_message_set_error_name (reply, NULL);
  blob = g_dbus_message_to_blob (reply, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: ERROR message: REPLY_SERIAL or ERROR_NAME header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  /* reset ERROR_NAME */
  g_dbus_message_set_error_name (reply, "Some.Error.Name");
  /* nuke REPLY_SERIAL */
  g_dbus_message_set_header (reply, G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL, NULL);
  blob = g_dbus_message_to_blob (reply, &blob_size, G_DBUS_CAPABILITY_FLAGS_NONE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_cmpstr (error->message, ==, "Cannot serialize message: ERROR message: REPLY_SERIAL or ERROR_NAME header field is missing");
  g_clear_error (&error);
  g_assert_null (blob);
  g_object_unref (reply);
  g_object_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_message_parse_empty_arrays_of_arrays (void)
{
  GVariant *body;
  GError *error = NULL;

  g_test_bug ("673612");
  /* These three-element array of empty arrays were previously read back as a
   * two-element array of empty arrays, due to sometimes erroneously skipping
   * four bytes to align for the eight-byte-aligned grandchild types (x and
   * dict_entry).
   */
  body = g_variant_parse (G_VARIANT_TYPE ("(aaax)"),
      "([@aax [], [], []],)", NULL, NULL, &error);
  g_assert_no_error (error);
  check_serialization (body,
      "value 0:   array:\n"
      "    array:\n"
      "    array:\n"
      "    array:\n");
  g_variant_unref (body);

  body = g_variant_parse (G_VARIANT_TYPE ("(aaa{uu})"),
      "([@aa{uu} [], [], []],)", NULL, NULL, &error);
  g_assert_no_error (error);
  check_serialization (body,
      "value 0:   array:\n"
      "    array:\n"
      "    array:\n"
      "    array:\n");
  g_variant_unref (body);

  /* Due to the same bug, g_dbus_message_new_from_blob() would fail for this
   * message because it would try to read past the end of the string. Hence,
   * sending this to an application would make it fall off the bus. */
  body = g_variant_parse (G_VARIANT_TYPE ("(a(aa{sv}as))"),
      "([ ([], []),"
      "   ([], []),"
      "   ([], [])],)", NULL, NULL, &error);
  g_assert_no_error (error);
  check_serialization (body,
      "value 0:   array:\n"
      "    struct:\n"
      "      array:\n"
      "      array:\n"
      "    struct:\n"
      "      array:\n"
      "      array:\n"
      "    struct:\n"
      "      array:\n"
      "      array:\n");
  g_variant_unref (body);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_message_serialize_double_array (void)
{
  GVariantBuilder builder;
  GVariant *body;

  g_test_bug ("732754");

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("ad"));
  g_variant_builder_add (&builder, "d", (gdouble)0.0);
  g_variant_builder_add (&builder, "d", (gdouble)8.0);
  g_variant_builder_add (&builder, "d", (gdouble)22.0);
  g_variant_builder_add (&builder, "d", (gdouble)0.0);
  body = g_variant_new ("(@ad)", g_variant_builder_end (&builder));
  check_serialization (body,
      "value 0:   array:\n"
      "    double: 0.000000\n"
      "    double: 8.000000\n"
      "    double: 22.000000\n"
      "    double: 0.000000\n");
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid header in a D-Bus message (specifically, with a type
 * which doesn???t match what???s expected for the given header) is gracefully
 * handled with an error rather than a crash.
 * The set of bytes here come directly from fuzzer output. */
static void
test_message_parse_non_signature_header (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x04,  /* message type */
    0x0f,  /* message flags */
    0x01,  /* major protocol version */
    0x00, 0x00, 0x00, 0x00,  /* body length */
    0x00, 0x00, 0x00, 0xbc,  /* message serial */
    /* a{yv} of header fields:
     * (things start to be invalid below here) */
    0x02, 0x00, 0x00, 0x00,  /* array length (in bytes) */
      G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE, /* array key */
      /* Variant array value: */
      0x04, /* signature length */
      'd', 0x00, 0x00, 'F',  /* signature (invalid) */
      0x00,  /* nul terminator */
      /* (Variant array value payload missing) */
    /* (message body length missing) */
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid header in a D-Bus message (specifically, containing a
 * variant with an empty type signature) is gracefully handled with an error
 * rather than a crash. The set of bytes here come directly from fuzzer
 * output. */
static void
test_message_parse_empty_signature_header (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x20,  /* message type */
    0x20,  /* message flags */
    0x01,  /* major protocol version */
    0x20, 0x20, 0x20, 0x00,  /* body length (invalid) */
    0x20, 0x20, 0x20, 0x20,  /* message serial */
    /* a{yv} of header fields:
     * (things start to be even more invalid below here) */
    0x20, 0x20, 0x20, 0x00,  /* array length (in bytes) */
      0x20, /* array key */
      /* Variant array value: */
      0x00, /* signature length */
      0x00,  /* nul terminator */
      /* (Variant array value payload missing) */
    /* (message body length missing) */
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid header in a D-Bus message (specifically, containing a
 * variant with a type signature containing multiple complete types) is
 * gracefully handled with an error rather than a crash. The set of bytes here
 * come directly from fuzzer output. */
static void
test_message_parse_multiple_signature_header (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x20,  /* message type */
    0x20,  /* message flags */
    0x01,  /* major protocol version */
    0x20, 0x20, 0x20, 0x00,  /* body length (invalid) */
    0x20, 0x20, 0x20, 0x20,  /* message serial */
    /* a{yv} of header fields:
     * (things start to be even more invalid below here) */
    0x20, 0x20, 0x20, 0x00,  /* array length (in bytes) */
      0x20, /* array key */
      /* Variant array value: */
      0x02, /* signature length */
      'b', 'b',  /* two complete types */
      0x00,  /* nul terminator */
      /* (Variant array value payload missing) */
    /* (message body length missing) */
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid header in a D-Bus message (specifically, containing a
 * variant with a valid type signature that is too long to be a valid
 * #GVariantType due to exceeding the array nesting limits) is gracefully
 * handled with an error rather than a crash. The set of bytes here come
 * directly from fuzzer output. */
static void
test_message_parse_over_long_signature_header (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x20,  /* message type */
    0x20,  /* message flags */
    0x01,  /* major protocol version */
    0x20, 0x20, 0x20, 0x01,  /* body length (invalid) */
    0x20, 0x20, 0x20, 0x20,  /* message serial */
    /* a{yv} of header fields:
     * (things start to be even more invalid below here) */
    0x20, 0x00, 0x00, 0x00,  /* array length (in bytes) */
      0x08,  /* array key */
      /* Variant array value: */
      0x04,  /* signature length */
      'g', 0x00, 0x20, 0x20,  /* one complete type plus some rubbish */
      0x00,  /* nul terminator */
      /* (Variant array value payload) */
      /* Critically, this contains 128 nested ???a???s, which exceeds
       * %G_VARIANT_MAX_RECURSION_DEPTH. */
      0xec,
      'a', 'b', 'g', 'd', 'u', 'd', 'd', 'd', 'd', 'd', 'd', 'd',
      'd', 'd', 'd',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
      'v'
    /* (message body length missing) */
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid header in a D-Bus message (specifically, containing too
 * many levels of nested variant) is gracefully handled with an error rather
 * than a crash. The set of bytes here come almost directly from fuzzer output. */
static void
test_message_parse_deep_header_nesting (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x20,  /* message type */
    0x20,  /* message flags */
    0x01,  /* major protocol version */
    0x20, 0x20, 0x20, 0x00,  /* body length (invalid) */
    0x20, 0x20, 0x20, 0x20,  /* message serial */
    /* a{yv} of header fields:
     * (things start to be even more invalid below here) */
    0x20, 0x20, 0x20, 0x00,  /* array length (in bytes) */
      0x20,  /* array key (this is not currently a valid header field) */
      /* Variant array value: */
      0x01,  /* signature length */
      'v',  /* one complete type */
      0x00,  /* nul terminator */
      /* (Variant array value payload) */
      /* Critically, this contains 64 nested variants (minus two for the
       * ???arbitrary valid content??? below, but ignoring two for the `a{yv}`
       * above), which in total exceeds %G_DBUS_MAX_TYPE_DEPTH. */
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
      0x01, 'v', 0x00, 0x01, 'v', 0x00,
      /* Some arbitrary valid content inside the innermost variant: */
      0x01, 'y', 0x00, 0xcc,
    /* (message body length missing) */
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Test that an invalid body in a D-Bus message (specifically, containing too
 * many levels of nested variant) is gracefully handled with an error rather
 * than a crash. The set of bytes here are a modified version of the bytes from
 * test_message_parse_deep_header_nesting(). */
static void
test_message_parse_deep_body_nesting (void)
{
  const guint8 data[] = {
    'l',  /* little-endian byte order */
    0x20,  /* message type */
    0x20,  /* message flags */
    0x01,  /* major protocol version */
    0x20, 0x20, 0x20, 0x00,  /* body length (invalid) */
    0x20, 0x20, 0x20, 0x20,  /* message serial */
    /* a{yv} of header fields: */
    0x07, 0x00, 0x00, 0x00,  /* array length (in bytes) */
      0x08,  /* array key (signature field) */
      /* Variant array value: */
      0x01,  /* signature length */
      'g',  /* one complete type */
      0x00,  /* nul terminator */
      /* (Variant array value payload) */
      0x01, 'v', 0x00,
    /* End-of-header padding to reach an 8-byte boundary: */
    0x00,
    /* Message body: over 64 levels of nested variant, which is not valid: */
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00, 0x01, 'v', 0x00,
    /* Some arbitrary valid content inside the innermost variant: */
    0x01, 'y', 0x00, 0xcc,
  };
  gsize size = sizeof (data);
  GDBusMessage *message = NULL;
  GError *local_error = NULL;

  message = g_dbus_message_new_from_blob ((guchar *) data, size,
                                          G_DBUS_CAPABILITY_FLAGS_NONE,
                                          &local_error);
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (message);

  g_clear_error (&local_error);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_setenv ("LC_ALL", "C", TRUE);
  setlocale (LC_ALL, "C");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

  g_test_add_func ("/gdbus/message-serialize/basic",
                   test_message_serialize_basic);
  g_test_add_func ("/gdbus/message-serialize/complex",
                   test_message_serialize_complex);
  g_test_add_func ("/gdbus/message-serialize/invalid",
                   test_message_serialize_invalid);
  g_test_add_func ("/gdbus/message-serialize/header-checks",
                   test_message_serialize_header_checks);
  g_test_add_func ("/gdbus/message-serialize/double-array",
                   test_message_serialize_double_array);

  g_test_add_func ("/gdbus/message-parse/empty-arrays-of-arrays",
                   test_message_parse_empty_arrays_of_arrays);
  g_test_add_func ("/gdbus/message-parse/non-signature-header",
                   test_message_parse_non_signature_header);
  g_test_add_func ("/gdbus/message-parse/empty-signature-header",
                   test_message_parse_empty_signature_header);
  g_test_add_func ("/gdbus/message-parse/multiple-signature-header",
                   test_message_parse_multiple_signature_header);
  g_test_add_func ("/gdbus/message-parse/over-long-signature-header",
                   test_message_parse_over_long_signature_header);
  g_test_add_func ("/gdbus/message-parse/deep-header-nesting",
                   test_message_parse_deep_header_nesting);
  g_test_add_func ("/gdbus/message-parse/deep-body-nesting",
                   test_message_parse_deep_body_nesting);

  return g_test_run();
}
