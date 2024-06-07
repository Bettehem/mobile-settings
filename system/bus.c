/*
 * Copyright Cedric Bellegarde <cedric.bellegarde@adishatz.org>
 */

#include <gio/gio.h>

#include "bus.h"
#include "../common/utils.h"

#define ADISHATZ_DBUS_NAME "org.adishatz.MobileSettings"
#define ADISHATZ_DBUS_PATH "/org/adishatz/MobileSettings"

struct _BusPrivate {
    GDBusConnection *adishatz_connection;

    GDBusNodeInfo *adishatz_introspection_data;
    guint adishatz_owner_id;
};

G_DEFINE_TYPE_WITH_CODE (Bus, bus, G_TYPE_OBJECT,
    G_ADD_PRIVATE (Bus))


static void
set_setting (Bus         *self,
             const gchar *setting,
             GVariant    *variant)
{
    g_autofree gchar *value = NULL;
    
    if (g_strcmp0 (setting, "touchpanel-double-tap") == 0) {
        gboolean double_tap;
        
        g_variant_get (variant, "b", &double_tap);
        
        value = g_strdup_printf ("%d", double_tap);
        
        write_to_file ("/sys/touchpanel/double_tap", value);
    }
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    Bus *self = user_data;

    if (g_strcmp0 (method_name, "Set") == 0) {
        const gchar *setting;
        g_autoptr(GVariant) value;

        g_variant_get (parameters, "(&sv)", &setting, &value);
        
        set_setting (self, setting, value);
        
        g_dbus_method_invocation_return_value (
            invocation, NULL
        );
    }
}


static const GDBusInterfaceVTable adishatz_interface_vtable = {
    handle_method_call,
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    Bus *self = user_data;
    guint registration_id;

    registration_id = g_dbus_connection_register_object (
        connection,
        ADISHATZ_DBUS_PATH,
        self->priv->adishatz_introspection_data->interfaces[0],
        &adishatz_interface_vtable,
        user_data,
        NULL,
        NULL
    );

    self->priv->adishatz_connection = g_object_ref (connection);

    g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    g_error ("Cannot own D-Bus name. Verify installation: %s\n", name);
}

static GDBusNodeInfo *
bus_init_path (const gchar *dbus_name,
               const gchar *xml,
               guint       *owner_id,
               gpointer     user_data)
{
    Bus *self = user_data;
    GBytes *bytes;
    GDBusNodeInfo *introspection_data;

    bytes = g_resources_lookup_data (
        xml,
        G_RESOURCE_LOOKUP_FLAGS_NONE,
        NULL
    );

    introspection_data = g_dbus_node_info_new_for_xml (
        g_bytes_get_data (bytes, NULL),
        NULL
    );
    g_bytes_unref (bytes);

    g_assert (introspection_data != NULL);

    *owner_id = g_bus_own_name (
        G_BUS_TYPE_SYSTEM,
        dbus_name,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        on_bus_acquired,
        on_name_acquired,
        on_name_lost,
        self,
        NULL
    );

    return introspection_data;
}

static void
bus_dispose (GObject *bus)
{
    Bus *self = BUS (bus);

    if (self->priv->adishatz_owner_id != 0) {
        g_bus_unown_name (self->priv->adishatz_owner_id);
    }

    g_clear_pointer (
      &self->priv->adishatz_introspection_data, g_dbus_node_info_unref
    );

    g_clear_object (&self->priv->adishatz_connection);

    G_OBJECT_CLASS (bus_parent_class)->dispose (bus);
}

static void
bus_finalize (GObject *bus)
{
    G_OBJECT_CLASS (bus_parent_class)->finalize (bus);
}

static void
bus_class_init (BusClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = bus_dispose;
    object_class->finalize = bus_finalize;
}

static void
bus_init (Bus *self)
{
    self->priv = bus_get_instance_private (self);

    self->priv->adishatz_introspection_data = bus_init_path (
        ADISHATZ_DBUS_NAME,
        "/org/adishatz/MobileSettings/org.adishatz.MobileSettings.xml",
        &self->priv->adishatz_owner_id,
        self
    );
}

/**
 * bus_new:
 * 
 * Creates a new #Bus
 *
 * Returns: (transfer full): a new #Bus
 *
 **/
GObject *
bus_new (void)
{
    GObject *bus;

    bus = g_object_new (
        TYPE_BUS,
        NULL
    );

    return bus;
}