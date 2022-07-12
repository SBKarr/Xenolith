/**
 Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLDefine.h"

#if LINUX

#include <dlfcn.h>
//#include <dbus/dbus.h>

typedef enum
{
  DBUS_BUS_SESSION,    /**< The login session bus */
  DBUS_BUS_SYSTEM,     /**< The systemwide bus */
  DBUS_BUS_STARTER     /**< The bus that started us, if any */
} DBusBusType;

struct DBusError
{
  const char *name;    /**< public error name field */
  const char *message; /**< public error message field */

  unsigned int dummy1 : 1; /**< placeholder */
  unsigned int dummy2 : 1; /**< placeholder */
  unsigned int dummy3 : 1; /**< placeholder */
  unsigned int dummy4 : 1; /**< placeholder */
  unsigned int dummy5 : 1; /**< placeholder */

  void *padding1; /**< placeholder */
};

typedef struct DBusMessageIter DBusMessageIter;

struct DBusMessageIter
{
  void *dummy1;         /**< Don't use this */
  void *dummy2;         /**< Don't use this */
  uint32_t dummy3; /**< Don't use this */
  int dummy4;           /**< Don't use this */
  int dummy5;           /**< Don't use this */
  int dummy6;           /**< Don't use this */
  int dummy7;           /**< Don't use this */
  int dummy8;           /**< Don't use this */
  int dummy9;           /**< Don't use this */
  int dummy10;          /**< Don't use this */
  int dummy11;          /**< Don't use this */
  int pad1;             /**< Don't use this */
  void *pad2;           /**< Don't use this */
  void *pad3;           /**< Don't use this */
};

typedef uint32_t  dbus_bool_t;
typedef struct DBusMessage DBusMessage;
typedef struct DBusConnection DBusConnection;

#define DBUS_TYPE_VARIANT       ((int) 'v')
#define DBUS_TYPE_STRING        ((int) 's')
#define DBUS_TYPE_INVALID       ((int) '\0')
#define DBUS_TYPE_INT32         ((int) 'i')
#define DBUS_TIMEOUT_USE_DEFAULT (-1)

namespace stappler::xenolith::platform {

struct DBusLibrary {
	DBusLibrary() {
		auto d = dlopen("libdbus-1.so", RTLD_LAZY);
		if (d) {
			if (open(d)) {
				handle = d;
			} else {
				dlclose(d);
			}
		}
	}

	~DBusLibrary() {
		if (handle) {
			dlclose(handle);
			handle = nullptr;
		}
	}

	bool open(void *d) {
		this->dbus_error_init =
				reinterpret_cast<decltype(this->dbus_error_init)>(dlsym(d, "dbus_error_init"));
		this->dbus_message_new_method_call =
				reinterpret_cast<decltype(this->dbus_message_new_method_call)>(dlsym(d, "dbus_message_new_method_call"));
		this->dbus_message_append_args =
				reinterpret_cast<decltype(this->dbus_message_append_args)>(dlsym(d, "dbus_message_append_args"));
		this->dbus_connection_send_with_reply_and_block =
				reinterpret_cast<decltype(this->dbus_connection_send_with_reply_and_block)>(dlsym(d, "dbus_connection_send_with_reply_and_block"));
		this->dbus_message_unref =
				reinterpret_cast<decltype(this->dbus_message_unref)>(dlsym(d, "dbus_message_unref"));
		this->dbus_error_is_set =
				reinterpret_cast<decltype(this->dbus_error_is_set)>(dlsym(d, "dbus_error_is_set"));
		this->dbus_message_iter_init =
				reinterpret_cast<decltype(this->dbus_message_iter_init)>(dlsym(d, "dbus_message_iter_init"));
		this->dbus_message_iter_recurse =
				reinterpret_cast<decltype(this->dbus_message_iter_recurse)>(dlsym(d, "dbus_message_iter_recurse"));
		this->dbus_message_iter_get_arg_type =
				reinterpret_cast<decltype(this->dbus_message_iter_get_arg_type)>(dlsym(d, "dbus_message_iter_get_arg_type"));
		this->dbus_message_iter_get_basic =
				reinterpret_cast<decltype(this->dbus_message_iter_get_basic)>(dlsym(d, "dbus_message_iter_get_basic"));
		this->dbus_bus_get =
				reinterpret_cast<decltype(this->dbus_bus_get)>(dlsym(d, "dbus_bus_get"));

		return this->dbus_error_init
				&& this->dbus_message_new_method_call
				&& this->dbus_message_append_args
				&& this->dbus_connection_send_with_reply_and_block
				&& this->dbus_message_unref
				&& this->dbus_error_is_set
				&& this->dbus_message_iter_init
				&& this->dbus_message_iter_recurse
				&& this->dbus_message_iter_get_arg_type
				&& this->dbus_message_iter_get_basic
				&& this->dbus_bus_get;
	}

	void *handle = nullptr;

	void (*dbus_error_init) (DBusError *error) = nullptr;
	DBusMessage* (*dbus_message_new_method_call) (const char *bus_name, const char *path, const char *iface, const char *method) = nullptr;
	dbus_bool_t (*dbus_message_append_args) (DBusMessage *message, int first_arg_type, ...) = nullptr;
	DBusMessage* (*dbus_connection_send_with_reply_and_block) (DBusConnection *connection,
			DBusMessage *message, int timeout_milliseconds, DBusError *error) = nullptr;
	void (*dbus_message_unref) (DBusMessage *message) = nullptr;
	dbus_bool_t (*dbus_error_is_set) (const DBusError *error) = nullptr;
	dbus_bool_t (*dbus_message_iter_init) (DBusMessage *message, DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_recurse) (DBusMessageIter *iter, DBusMessageIter *sub) = nullptr;
	int (*dbus_message_iter_get_arg_type) (DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_get_basic) (DBusMessageIter *iter, void *value) = nullptr;
	DBusConnection* (*dbus_bus_get) (DBusBusType type, DBusError *error) = nullptr;
};

static DBusMessage* get_setting_sync(DBusLibrary *lib, DBusConnection *const connection, const char *key, const char *value) {
	DBusError error;
	dbus_bool_t success;
	DBusMessage *message;
	DBusMessage *reply;

	lib->dbus_error_init(&error);

	message = lib->dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "Read");

	success = lib->dbus_message_append_args(message, DBUS_TYPE_STRING, &key, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);

	if (!success)
		return NULL;

	reply = lib->dbus_connection_send_with_reply_and_block(connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);

	lib->dbus_message_unref(message);

	if (lib->dbus_error_is_set(&error))
		return NULL;

	return reply;
}

static bool parse_type(DBusLibrary *lib, DBusMessage *const reply, const int type, void *value) {
	DBusMessageIter iter[3];

	lib->dbus_message_iter_init(reply, &iter[0]);
	if (lib->dbus_message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT)
		return false;

	lib->dbus_message_iter_recurse(&iter[0], &iter[1]);
	if (lib->dbus_message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT)
		return false;

	lib->dbus_message_iter_recurse(&iter[1], &iter[2]);
	if (lib->dbus_message_iter_get_arg_type(&iter[2]) != type)
		return false;

	lib->dbus_message_iter_get_basic(&iter[2], value);

	return true;
}

static bool get_cursor_settings(char **theme, int *size) {
	static const char name[] = "org.gnome.desktop.interface";
	static const char key_theme[] = "cursor-theme";
	static const char key_size[] = "cursor-size";

	DBusLibrary lib;
	if (lib.handle) {
		do {
			DBusError error;
			DBusConnection *connection;
			DBusMessage *reply;
			const char *value_theme = NULL;

			lib.dbus_error_init(&error);
			connection = lib.dbus_bus_get(DBUS_BUS_SESSION, &error);

			if (lib.dbus_error_is_set(&error))
				break;

			reply = get_setting_sync(&lib, connection, name, key_theme);
			if (!reply)
				break;

			if (!parse_type(&lib, reply, DBUS_TYPE_STRING, &value_theme)) {
				lib.dbus_message_unref(reply);
				break;
			}

			*theme = strdup(value_theme);

			lib.dbus_message_unref(reply);

			reply = get_setting_sync(&lib, connection, name, key_size);
			if (!reply)
				break;

			if (!parse_type(&lib, reply, DBUS_TYPE_INT32, size)) {
				lib.dbus_message_unref(reply);
				break;
			}

			lib.dbus_message_unref(reply);
			return true;
		} while (0);
	}

	char *env_xtheme;
	char *env_xsize;

	env_xtheme = getenv("XCURSOR_THEME");
	if (env_xtheme != NULL)
		*theme = strdup(env_xtheme);

	env_xsize = getenv("XCURSOR_SIZE");
	if (env_xsize != NULL)
		*size = atoi(env_xsize);

	return env_xtheme != NULL && env_xsize != NULL;
}

}

#endif
