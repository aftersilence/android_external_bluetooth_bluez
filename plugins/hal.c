/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <dbus/dbus.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "plugin.h"
#include "adapter.h"
#include "logging.h"
#include "dbus-hci.h"

static void formfactor_reply(DBusPendingCall *call, void *user_data)
{
	struct btd_adapter *adapter = user_data;
	const char *formfactor = NULL;
	DBusMessage *reply;
	uint8_t cls[3], minor = 0;
	int dd;

	reply = dbus_pending_call_steal_reply(call);

	if (dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &formfactor,
						DBUS_TYPE_INVALID) == FALSE) {
		error("Wrong formfactor arguments");
		return;
	}

	debug("Computer is classified as %s", formfactor);

	if (formfactor != NULL) {
		if (g_str_equal(formfactor, "laptop") == TRUE)
			minor |= (1 << 2) | (1 << 3);
		else if (g_str_equal(formfactor, "desktop") == TRUE)
			minor |= 1 << 2;
		else if (g_str_equal(formfactor, "server") == TRUE)
			minor |= 1 << 3;
		else if (g_str_equal(formfactor, "handheld") == TRUE)
			minor += 1 << 4;
	}

	if (adapter_get_class(adapter, cls) < 0)
		return;

	debug("Current device class is 0x%02x%02x%02x\n",
						cls[2], cls[1], cls[0]);

	dd = hci_open_dev(adapter_get_dev_id(adapter));
	if (dd < 0)
		return;

	/* Computer major class */
	debug("Setting 0x%06x for major/minor device class", (1 << 8) | minor);

	set_major_class(dd, cls, 0x01);
	set_minor_class(dd, cls, minor);

	hci_close_dev(dd);
}

static DBusConnection *connection;

static int hal_probe(struct btd_adapter *adapter)
{
	const char *property = "system.formfactor";
	DBusMessage *message;
	DBusPendingCall *call;

	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return -ENOMEM;

	message = dbus_message_new_method_call("org.freedesktop.Hal",
				"/org/freedesktop/Hal/devices/computer",
						"org.freedesktop.Hal.Device",
							"GetPropertyString");
	if (message == NULL) {
		error("Failed to create formfactor request");
		dbus_connection_unref(connection);
		return -ENOMEM;
	}

	dbus_message_append_args(message, DBUS_TYPE_STRING, &property,
							DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(connection, message,
						&call, -1) == FALSE) {
		error("Failed to send formfactor request");
		dbus_connection_unref(connection);
		return -EIO;
	}

	dbus_pending_call_set_notify(call, formfactor_reply, adapter, NULL);

	return 0;
}

static void hal_remove(struct btd_adapter *adapter)
{
	dbus_connection_unref(connection);
}

static struct btd_adapter_driver hal_driver = {
	.name	= "hal",
	.probe	= hal_probe,
	.remove	= hal_remove,
};

static int hal_init(void)
{
	return btd_register_adapter_driver(&hal_driver);
}

static void hal_exit(void)
{
	btd_unregister_adapter_driver(&hal_driver);
}

BLUETOOTH_PLUGIN_DEFINE("hal", hal_init, hal_exit)
