/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "protocol.h"

#define SERIALCOMM "9600/8n1"
/* 23ms is the longest interval between tokens. */
#define MAX_SCAN_TIME 25 * 1000

static const int32_t hwopts[] = {
	SR_CONF_CONN,
};

static const int32_t hwcaps[] = {
	SR_CONF_SOUNDLEVELMETER,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_CONTINUOUS,
	SR_CONF_DATALOG,
};


SR_PRIV struct sr_dev_driver cem_dt_885x_driver_info;
static struct sr_dev_driver *di = &cem_dt_885x_driver_info;


static int init(struct sr_context *sr_ctx)
{
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *scan(GSList *options)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	struct sr_dev_inst *sdi;
	struct sr_probe *probe;
	GSList *l, *devices;
	gint64 start;
	const char *conn;
	unsigned char c;

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		if (src->key == SR_CONF_CONN)
			conn = g_variant_get_string(src->data, NULL);
	}
	if (!conn)
		return NULL;

	if (!(serial = sr_serial_dev_inst_new(conn, SERIALCOMM)))
		return NULL;

	if (serial_open(serial, SERIAL_RDONLY | SERIAL_NONBLOCK) != SR_OK)
		return NULL;

	devices = NULL;
	drvc = di->priv;
	start = g_get_monotonic_time();
	while (g_get_monotonic_time() - start < MAX_SCAN_TIME) {
		if (serial_read(serial, &c, 1) == 1 && c == 0xa5) {
			/* Found one. */
			if (!(sdi = sr_dev_inst_new(0, SR_ST_INACTIVE, "CEM",
					"DT-885x", NULL)))
				return NULL;

			if (!(devc = g_try_malloc0(sizeof(struct dev_context)))) {
				sr_dbg("Device context malloc failed.");
				return NULL;
			}
			devc->cur_mqflags = 0;
			devc->recording = -1;

			if (!(sdi->conn = sr_serial_dev_inst_new(conn, SERIALCOMM)))
				return NULL;

			sdi->inst_type = SR_INST_SERIAL;
			sdi->priv = devc;
			sdi->driver = di;
			if (!(probe = sr_probe_new(0, SR_PROBE_ANALOG, TRUE, "SPL")))
				return NULL;
			sdi->probes = g_slist_append(sdi->probes, probe);
			drvc->instances = g_slist_append(drvc->instances, sdi);
			devices = g_slist_append(devices, sdi);
			break;
		}
		/* It takes about 1ms for a byte to come in. */
		g_usleep(1000);
	}

	serial_close(serial);

	return devices;
}

static GSList *dev_list(void)
{
	struct drv_context *drvc;

	drvc = di->priv;

	return drvc->instances;
}

static int dev_clear(void)
{
	return std_dev_clear(di, NULL);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return SR_ERR;

	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;
	if (serial && serial->fd != -1) {
		serial_close(serial);
		sdi->status = SR_ST_INACTIVE;
	}

	return SR_OK;
}

static int cleanup(void)
{
	return dev_clear();
}

static int config_get(int key, GVariant **data, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_DATALOG:
		*data = g_variant_new_boolean(cem_dt_885x_recording_get(sdi));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(int key, GVariant *data, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	uint64_t tmp_u64;
	int ret;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	if (!(devc = sdi->priv)) {
		sr_err("sdi->priv was NULL.");
		return SR_ERR_BUG;
	}

	ret = SR_OK;
	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		devc->limit_samples = tmp_u64;
		ret = SR_OK;
		break;
	case SR_CONF_DATALOG:
		if (g_variant_get_boolean(data)) {
			/* Start logging. */
			ret = cem_dt_885x_recording_set(sdi, TRUE);
		} else {
			/* Stop logging. */
			ret = cem_dt_885x_recording_set(sdi, FALSE);
		}
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi)
{
	int ret;

	(void)sdi;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				hwopts, ARRAY_SIZE(hwopts), sizeof(int32_t));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	if (!(devc = sdi->priv)) {
		sr_err("sdi->priv was NULL.");
		return SR_ERR_BUG;
	}

	devc->cb_data = cb_data;
	devc->state = ST_INIT;
	devc->num_samples = 0;
	devc->buf_len = 0;

	/* Send header packet to the session bus. */
	std_session_send_df_header(cb_data, LOG_PREFIX);

	/* Poll every 100ms, or whenever some data comes in. */
	serial = sdi->conn;
	sr_source_add(serial->fd, G_IO_IN, 150, cem_dt_885x_receive_data,
			(void *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	return std_dev_acquisition_stop_serial(sdi, cb_data, dev_close,
			sdi->conn, LOG_PREFIX);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver cem_dt_885x_driver_info = {
	.name = "cem-dt-885x",
	.longname = "CEM DT-885x",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
