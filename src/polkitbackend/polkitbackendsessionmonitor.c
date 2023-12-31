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

#include "config.h"
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <glib/gstdio.h>

#include <polkit/polkit.h>
#include "polkitbackendsessionmonitor.h"

#define CKDB_PATH "/var/run/ConsoleKit/database"

/**
 * SECTION:polkitbackendsessionmonitor
 * @title: PolkitBackendSessionMonitor
 * @short_description: Monitor sessions
 *
 * The #PolkitBackendSessionMonitor class is a utility class to track and monitor sessions.
 */

struct _PolkitBackendSessionMonitor
{
  GObject parent_instance;

  GDBusConnection *system_bus;

  GKeyFile *database;
  GFileMonitor *database_monitor;
};

struct _PolkitBackendSessionMonitorClass
{
  GObjectClass parent_class;

  void (*changed) (PolkitBackendSessionMonitor *monitor);
};


enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (PolkitBackendSessionMonitor, polkit_backend_session_monitor, G_TYPE_OBJECT);

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
reload_database (PolkitBackendSessionMonitor  *monitor,
                 GError                      **error)
{
  gboolean ret;

  ret = FALSE;

  monitor->database = g_key_file_new ();
  if (!g_key_file_load_from_file (monitor->database,
                                  CKDB_PATH,
                                  G_KEY_FILE_NONE,
                                  error))
    {
      g_key_file_free (monitor->database);
      monitor->database = NULL;
      goto out;
    }

  ret = TRUE;

 out:
  return ret;
}

static gboolean
ensure_database (PolkitBackendSessionMonitor  *monitor,
                 GError                      **error)
{
  gboolean ret;

  if (monitor->database != NULL)
    {
      ret = TRUE;
      goto out;
    }

  ret = reload_database (monitor, error);

 out:
  return ret;
}

static void
on_file_monitor_changed (GFileMonitor     *file_monitor,
                         GFile            *file,
                         GFile            *other_file,
                         GFileMonitorEvent event_type,
                         gpointer          user_data)
{
  PolkitBackendSessionMonitor *monitor = POLKIT_BACKEND_SESSION_MONITOR (user_data);

  /* throw away cache */
  if (monitor->database != NULL)
    {
      g_key_file_free (monitor->database);
      monitor->database = NULL;
    }
  g_signal_emit (monitor, signals[CHANGED_SIGNAL], 0);
}

static void
polkit_backend_session_monitor_init (PolkitBackendSessionMonitor *monitor)
{
  GError *error;
  GFile *file;

  error = NULL;
  monitor->system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (monitor->system_bus == NULL)
    {
      g_printerr ("Error getting system bus: %s", error->message);
      g_error_free (error);
    }

  error = NULL;
  if (!ensure_database (monitor, &error))
    {
      g_printerr ("Error loading " CKDB_PATH ": %s", error->message);
      g_error_free (error);
    }

  error = NULL;
  file = g_file_new_for_path (CKDB_PATH);
  monitor->database_monitor = g_file_monitor_file (file,
                                                   G_FILE_MONITOR_NONE,
                                                   NULL,
                                                   &error);
  g_object_unref (file);
  if (monitor->database_monitor == NULL)
    {
      g_printerr ("Error monitoring " CKDB_PATH ": %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_signal_connect (monitor->database_monitor,
                        "changed",
                        G_CALLBACK (on_file_monitor_changed),
                        monitor);
    }
}

static void
polkit_backend_session_monitor_finalize (GObject *object)
{
  PolkitBackendSessionMonitor *monitor = POLKIT_BACKEND_SESSION_MONITOR (object);

  if (monitor->system_bus != NULL)
    g_object_unref (monitor->system_bus);

  if (monitor->database_monitor != NULL)
    g_object_unref (monitor->database_monitor);

  if (monitor->database != NULL)
    g_key_file_free (monitor->database);

  if (G_OBJECT_CLASS (polkit_backend_session_monitor_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (polkit_backend_session_monitor_parent_class)->finalize (object);
}

static void
polkit_backend_session_monitor_class_init (PolkitBackendSessionMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = polkit_backend_session_monitor_finalize;

  /**
   * PolkitBackendSessionMonitor::changed:
   * @monitor: A #PolkitBackendSessionMonitor
   *
   * Emitted when something changes.
   */
  signals[CHANGED_SIGNAL] = g_signal_new ("changed",
                                          POLKIT_BACKEND_TYPE_SESSION_MONITOR,
                                          G_SIGNAL_RUN_LAST,
                                          G_STRUCT_OFFSET (PolkitBackendSessionMonitorClass, changed),
                                          NULL,                   /* accumulator      */
                                          NULL,                   /* accumulator data */
                                          g_cclosure_marshal_VOID__VOID,
                                          G_TYPE_NONE,
                                          0);
}

PolkitBackendSessionMonitor *
polkit_backend_session_monitor_new (void)
{
  PolkitBackendSessionMonitor *monitor;

  monitor = POLKIT_BACKEND_SESSION_MONITOR (g_object_new (POLKIT_BACKEND_TYPE_SESSION_MONITOR, NULL));

  return monitor;
}

/* ---------------------------------------------------------------------------------------------------- */

GList *
polkit_backend_session_monitor_get_sessions (PolkitBackendSessionMonitor *monitor)
{
  /* TODO */
  return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * polkit_backend_session_monitor_get_user:
 * @monitor: A #PolkitBackendSessionMonitor.
 * @subject: A #PolkitSubject.
 * @error: Return location for error.
 *
 * Gets the user corresponding to @subject or %NULL if no user exists.
 *
 * Returns: %NULL if @error is set otherwise a #PolkitUnixUser that should be freed with g_object_unref().
 */
PolkitIdentity *
polkit_backend_session_monitor_get_user_for_subject (PolkitBackendSessionMonitor  *monitor,
                                                     PolkitSubject                *subject,
                                                     GError                      **error)
{
  PolkitIdentity *ret;
  GError *local_error;
  gchar *group;
  guint32 uid;

  ret = NULL;

  if (POLKIT_IS_UNIX_PROCESS (subject))
    {
      local_error = NULL;
      uid = polkit_unix_process_get_owner (POLKIT_UNIX_PROCESS (subject), &local_error);
      if (local_error != NULL)
        {
          g_propagate_prefixed_error (error, local_error, "Error getting user for process: ");
          g_error_free (local_error);
          goto out;
        }

      ret = polkit_unix_user_new (uid);
    }
  else if (POLKIT_IS_SYSTEM_BUS_NAME (subject))
    {
      GVariant *result;

      result = g_dbus_connection_call_sync (monitor->system_bus,
                                            "org.freedesktop.DBus",
                                            "/org/freedesktop/DBus",
                                            "org.freedesktop.DBus",
                                            "GetConnectionUnixUser",
                                            g_variant_new ("(s)", polkit_system_bus_name_get_name (POLKIT_SYSTEM_BUS_NAME (subject))),
                                            G_VARIANT_TYPE ("(u)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, /* timeout_msec */
                                            NULL, /* GCancellable */
                                            error);
      if (result == NULL)
        goto out;
      g_variant_get (result, "(u)", &uid);
      g_variant_unref (result);

      ret = polkit_unix_user_new (uid);
    }
  else if (POLKIT_IS_UNIX_SESSION (subject))
    {
      if (!ensure_database (monitor, error))
        {
          g_prefix_error (error, "Error getting user for session: Error ensuring CK database at " CKDB_PATH ": ");
          goto out;
        }

      group = g_strdup_printf ("Session %s", polkit_unix_session_get_session_id (POLKIT_UNIX_SESSION (subject)));
      local_error = NULL;
      uid = g_key_file_get_integer (monitor->database, group, "uid", &local_error);
      if (local_error != NULL)
        {
          g_propagate_prefixed_error (error, local_error, "Error getting uid using " CKDB_PATH ": ");
          g_free (group);
          goto out;
        }
      g_free (group);

      ret = polkit_unix_user_new (uid);
    }

 out:
  return ret;
}

/**
 * polkit_backend_session_monitor_get_session_for_subject:
 * @monitor: A #PolkitBackendSessionMonitor.
 * @subject: A #PolkitSubject.
 * @error: Return location for error.
 *
 * Gets the session corresponding to @subject or %NULL if no session exists.
 *
 * Returns: %NULL if @error is set otherwise a #PolkitUnixSession that should be freed with g_object_unref().
 */
PolkitSubject *
polkit_backend_session_monitor_get_session_for_subject (PolkitBackendSessionMonitor *monitor,
                                                        PolkitSubject               *subject,
                                                        GError                     **error)
{
  PolkitSubject *session;

  session = NULL;

  if (POLKIT_IS_UNIX_PROCESS (subject))
    {
      const gchar *session_id;
      GVariant *result;
      result = g_dbus_connection_call_sync (monitor->system_bus,
                                            "org.freedesktop.ConsoleKit",
                                            "/org/freedesktop/ConsoleKit/Manager",
                                            "org.freedesktop.ConsoleKit.Manager",
                                            "GetSessionForUnixProcess",
                                            g_variant_new ("(u)", polkit_unix_process_get_pid (POLKIT_UNIX_PROCESS (subject))),
                                            G_VARIANT_TYPE ("(o)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, /* timeout_msec */
                                            NULL, /* GCancellable */
                                            error);
      if (result == NULL)
        goto out;
      g_variant_get (result, "(&o)", &session_id);
      session = polkit_unix_session_new (session_id);
      g_variant_unref (result);
    }
  else if (POLKIT_IS_SYSTEM_BUS_NAME (subject))
    {
      guint32 pid;
      const gchar *session_id;
      GVariant *result;

      result = g_dbus_connection_call_sync (monitor->system_bus,
                                            "org.freedesktop.DBus",
                                            "/org/freedesktop/DBus",
                                            "org.freedesktop.DBus",
                                            "GetConnectionUnixProcessID",
                                            g_variant_new ("(s)", polkit_system_bus_name_get_name (POLKIT_SYSTEM_BUS_NAME (subject))),
                                            G_VARIANT_TYPE ("(u)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, /* timeout_msec */
                                            NULL, /* GCancellable */
                                            error);
      if (result == NULL)
        goto out;
      g_variant_get (result, "(u)", &pid);
      g_variant_unref (result);

      result = g_dbus_connection_call_sync (monitor->system_bus,
                                            "org.freedesktop.ConsoleKit",
                                            "/org/freedesktop/ConsoleKit/Manager",
                                            "org.freedesktop.ConsoleKit.Manager",
                                            "GetSessionForUnixProcess",
                                            g_variant_new ("(u)", pid),
                                            G_VARIANT_TYPE ("(o)"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1, /* timeout_msec */
                                            NULL, /* GCancellable */
                                            error);
      if (result == NULL)
        goto out;
      g_variant_get (result, "(&o)", &session_id);
      session = polkit_unix_session_new (session_id);
      g_variant_unref (result);
    }
  else
    {
      g_set_error (error,
                   POLKIT_ERROR,
                   POLKIT_ERROR_NOT_SUPPORTED,
                   "Cannot get user for subject of type %s",
                   g_type_name (G_TYPE_FROM_INSTANCE (subject)));
    }

 out:

  return session;
}

static gboolean
get_boolean (PolkitBackendSessionMonitor *monitor,
             PolkitSubject               *session,
             const gchar                 *key_name)
{
  gboolean ret;
  gchar *group;
  GError *error;

  ret = FALSE;

  group = g_strdup_printf ("Session %s", polkit_unix_session_get_session_id (POLKIT_UNIX_SESSION (session)));

  error = NULL;
  ret = g_key_file_get_boolean (monitor->database, group, key_name, &error);
  if (error != NULL)
    {
      g_printerr ("Error looking %s using " CKDB_PATH " for %s: %s\n",
                  key_name,
                  group,
                  error->message);
      g_error_free (error);
      goto out;
    }

 out:
  g_free (group);
  return ret;
}

gboolean
polkit_backend_session_monitor_is_session_local  (PolkitBackendSessionMonitor *monitor,
                                                  PolkitSubject               *session)
{
  return get_boolean (monitor, session, "is_local");
}


gboolean
polkit_backend_session_monitor_is_session_active (PolkitBackendSessionMonitor *monitor,
                                                  PolkitSubject               *session)
{
  return get_boolean (monitor, session, "is_active");
}

