/*
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include "polkitsubject.h"
#include "polkitunixprocess.h"
#include "polkitunixsession.h"
#include "polkitsystembusname.h"
#include "polkiterror.h"
#include "polkitprivate.h"

/**
 * SECTION:polkitsubject
 * @title: PolkitSubject
 * @short_description: Type for representing subjects
 *
 * #PolkitSubject is an abstract type for representing one or more
 * processes.
 */

static void
base_init (gpointer g_iface)
{
}

GType
polkit_subject_get_type (void)
{
  static GType iface_type = 0;

  if (iface_type == 0)
    {
      static const GTypeInfo info =
      {
        sizeof (PolkitSubjectIface),
        base_init,              /* base_init      */
        NULL,                   /* base_finalize  */
        NULL,                   /* class_init     */
        NULL,                   /* class_finalize */
        NULL,                   /* class_data     */
        0,                      /* instance_size  */
        0,                      /* n_preallocs    */
        NULL,                   /* instance_init  */
        NULL                    /* value_table    */
      };

      iface_type = g_type_register_static (G_TYPE_INTERFACE, "PolkitSubject", &info, 0);

      g_type_interface_add_prerequisite (iface_type, G_TYPE_OBJECT);
    }

  return iface_type;
}

/**
 * polkit_subject_hash:
 * @subject: A #PolkitSubject.
 *
 * Gets a hash code for @subject that can be used with e.g. g_hash_table_new().
 *
 * Returns: A hash code.
 */
guint
polkit_subject_hash (PolkitSubject *subject)
{
  return POLKIT_SUBJECT_GET_IFACE (subject)->hash (subject);
}

/**
 * polkit_subject_equal:
 * @a: A #PolkitSubject.
 * @b: A #PolkitSubject.
 *
 * Checks if @a and @b are equal, ie. represent the same subject.
 *
 * This function can be used in e.g. g_hash_table_new().
 *
 * Returns: %TRUE if @a and @b are equal, %FALSE otherwise.
 */
gboolean
polkit_subject_equal (PolkitSubject *a,
                      PolkitSubject *b)
{
  if (!g_type_is_a (G_TYPE_FROM_INSTANCE (a), G_TYPE_FROM_INSTANCE (b)))
    return FALSE;

  return POLKIT_SUBJECT_GET_IFACE (a)->equal (a, b);
}

/**
 * polkit_subject_to_string:
 * @subject: A #PolkitSubject.
 *
 * Serializes @subject to a string that can be used in
 * polkit_subject_from_string().
 *
 * Returns: A string representing @subject. Free with g_free().
 */
gchar *
polkit_subject_to_string (PolkitSubject *subject)
{
  return POLKIT_SUBJECT_GET_IFACE (subject)->to_string (subject);
}

/**
 * polkit_subject_exists:
 * @subject: A #PolkitSubject.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously checks if @subject exists.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call polkit_subject_exists_finish() to get the result of the
 * operation.
 **/
void
polkit_subject_exists (PolkitSubject       *subject,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  POLKIT_SUBJECT_GET_IFACE (subject)->exists (subject,
                                              cancellable,
                                              callback,
                                              user_data);
}

/**
 * polkit_subject_exists_finish:
 * @subject: A #PolkitSubject.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to polkit_subject_exists().
 * @error: Return location for error or %NULL.
 *
 * Finishes checking whether a subject exists.
 *
 * Returns: %TRUE if the subject exists, %FALSE if not or @error is set.
 */
gboolean
polkit_subject_exists_finish (PolkitSubject   *subject,
                              GAsyncResult    *res,
                              GError         **error)
{
  return POLKIT_SUBJECT_GET_IFACE (subject)->exists_finish (subject,
                                                            res,
                                                            error);
}

/**
 * polkit_subject_exists_sync:
 * @subject: A #PolkitSubject.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Checks if @subject exists.
 *
 * This is a synchronous blocking call, see polkit_subject_exists()
 * for the asynchronous version.
 *
 * Returns: %TRUE if the subject exists, %FALSE if not or @error is set.
 */
gboolean
polkit_subject_exists_sync   (PolkitSubject  *subject,
                              GCancellable   *cancellable,
                              GError        **error)
{
  return POLKIT_SUBJECT_GET_IFACE (subject)->exists_sync (subject,
                                                          cancellable,
                                                          error);
}

/**
 * polkit_subject_from_string:
 * @str: A string obtained from polkit_subject_to_string().
 * @error: Return location for error.
 *
 * Creates an object from @str that implements the #PolkitSubject
 * interface.
 *
 * Returns: A #PolkitSubject or %NULL if @error is set. Free with
 * g_object_unref().
 */
PolkitSubject *
polkit_subject_from_string  (const gchar   *str,
                             GError       **error)
{
  PolkitSubject *subject;
  guint64 val;
  gchar *endptr;

  g_return_val_if_fail (str != NULL, NULL);

  /* TODO: we could do something with VFuncs like in g_icon_from_string() */

  subject = NULL;

  if (g_str_has_prefix (str, "unix-process:"))
    {
      val = g_ascii_strtoull (str + sizeof "unix-process:" - 1,
                              &endptr,
                              10);
      if (*endptr == '\0')
        {
          subject = polkit_unix_process_new ((gint) val);
          if (polkit_unix_process_get_start_time (POLKIT_UNIX_PROCESS (subject)) == 0)
            {
              g_object_unref (subject);
              subject = NULL;
              g_set_error (error,
                           POLKIT_ERROR,
                           POLKIT_ERROR_FAILED,
                           "No process with pid %" G_GUINT64_FORMAT,
                           val);
            }
        }
    }
  else if (g_str_has_prefix (str, "unix-session:"))
    {
      subject = polkit_unix_session_new (str + sizeof "unix-session:" - 1);
    }
  else if (g_str_has_prefix (str, "system-bus-name:"))
    {
      subject = polkit_system_bus_name_new (str + sizeof "system-bus-name:" - 1);
    }

  if (subject == NULL && (error != NULL && *error == NULL))
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_FAILED,
                   "Malformed subject string '%s'",
                   str);
    }


  return subject;
}

GVariant *
polkit_subject_to_gvariant (PolkitSubject *subject)
{
  GVariantBuilder builder;
  GVariant *dict;
  GVariant *ret;
  const gchar *kind;

  kind = "";

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  if (POLKIT_IS_UNIX_PROCESS (subject))
    {
      kind = "unix-process";
      g_variant_builder_add (&builder, "{sv}", "pid",
                             g_variant_new_uint32 (polkit_unix_process_get_pid (POLKIT_UNIX_PROCESS (subject))));
      g_variant_builder_add (&builder, "{sv}", "start-time",
                             g_variant_new_uint64 (polkit_unix_process_get_start_time (POLKIT_UNIX_PROCESS (subject))));
    }
  else if (POLKIT_IS_UNIX_SESSION (subject))
    {
      kind = "unix-session";
      g_variant_builder_add (&builder, "{sv}", "session-id",
                             g_variant_new_string (polkit_unix_session_get_session_id (POLKIT_UNIX_SESSION (subject))));
    }
  else if (POLKIT_IS_SYSTEM_BUS_NAME (subject))
    {
      kind = "system-bus-name";
      g_variant_builder_add (&builder, "{sv}", "name",
                             g_variant_new_string (polkit_system_bus_name_get_name (POLKIT_SYSTEM_BUS_NAME (subject))));
    }
  else
    {
      g_warning ("Unknown class %s implementing PolkitSubject", g_type_name (G_TYPE_FROM_INSTANCE (subject)));
    }

  dict = g_variant_builder_end (&builder);
  ret = g_variant_new ("(s@a{sv})", kind, dict);
  return ret;
}

static GVariant *
lookup_asv (GVariant            *dict,
            const gchar         *given_key,
            const GVariantType  *given_type,
            GError             **error)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;
  GVariant *ret;

  ret = NULL;

  g_variant_iter_init (&iter, dict);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, given_key) == 0)
        {
          if (!g_variant_is_of_type (value, given_type))
            {
              gchar *type_string;
              type_string = g_variant_type_dup_string (given_type);
              g_set_error (error,
                           POLKIT_ERROR,
                           POLKIT_ERROR_FAILED,
                           "Value for key `%s' found but is of type %s and type %s was expected",
                           given_key,
                           g_variant_get_type_string (value),
                           type_string);
              g_free (type_string);
              goto out;
            }
          ret = value;
          goto out;
        }
      g_variant_unref (value);
    }

 out:
  if (ret == NULL)
    {
      gchar *type_string;
      type_string = g_variant_type_dup_string (given_type);
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_FAILED,
                   "Didn't find value for key `%s' of type %s",
                   given_key,
                   type_string);
      g_free (type_string);
    }

  return ret;
}

PolkitSubject *
polkit_subject_new_for_gvariant (GVariant  *variant,
                                 GError    **error)
{
  PolkitSubject *ret;
  const gchar *kind;
  GVariant *details_gvariant;

  ret = NULL;

  g_variant_get (variant,
                 "(&s@a{sv})",
                 &kind,
                 &details_gvariant);

  if (g_strcmp0 (kind, "unix-process") == 0)
    {
      GVariant *v;
      guint32 pid;
      guint64 start_time;

      v = lookup_asv (details_gvariant, "pid", G_VARIANT_TYPE_UINT32, error);
      if (v == NULL)
        {
          g_prefix_error (error, "Error parsing unix-process subject: ");
          goto out;
        }
      pid = g_variant_get_uint32 (v);
      g_variant_unref (v);

      v = lookup_asv (details_gvariant, "start-time", G_VARIANT_TYPE_UINT64, error);
      if (v == NULL)
        {
          g_prefix_error (error, "Error parsing unix-process subject: ");
          goto out;
        }
      start_time = g_variant_get_uint64 (v);
      g_variant_unref (v);

      ret = polkit_unix_process_new_full (pid, start_time);
    }
  else if (g_strcmp0 (kind, "unix-session") == 0)
    {
      GVariant *v;
      const gchar *session_id;

      v = lookup_asv (details_gvariant, "session-id", G_VARIANT_TYPE_STRING, error);
      if (v == NULL)
        {
          g_prefix_error (error, "Error parsing unix-session subject: ");
          goto out;
        }
      session_id = g_variant_get_string (v, NULL);
      ret = polkit_unix_session_new (session_id);
      g_variant_unref (v);
    }
  else if (g_strcmp0 (kind, "system-bus-name") == 0)
    {
      GVariant *v;
      const gchar *name;

      v = lookup_asv (details_gvariant, "name", G_VARIANT_TYPE_STRING, error);
      if (v == NULL)
        {
          g_prefix_error (error, "Error parsing system-bus-name subject: ");
          goto out;
        }
      name = g_variant_get_string (v, NULL);
      if (!g_dbus_is_unique_name (name))
        {
          g_set_error (error,
                       POLKIT_ERROR,
                       POLKIT_ERROR_FAILED,
                       "Error parsing system-bus-name subject: `%s' is not a valid unique name",
                       name);
          goto out;
        }
      ret = polkit_system_bus_name_new (name);
      g_variant_unref (v);
    }
  else
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_FAILED,
                   "Unknown subject of kind `%s'",
                   kind);
    }

 out:
  g_variant_unref (details_gvariant);
  return ret;
}
