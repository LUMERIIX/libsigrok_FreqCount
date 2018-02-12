/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2018 lj <lj@lj>
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include "protocol.h"
#include "protocol_wrappers.h"
#include <glib.h>

#define BUF_MAX 8


    static const uint32_t drvopts[] = {
        SR_CONF_MULTIMETER,
    };

    static const uint32_t scanopts[] = {
        SR_CONF_CONN,
        SR_CONF_SERIALCOMM,
    };

    static const uint32_t devopts[] = {
        SR_CONF_MULTIMETER,
        SR_CONF_CONTINUOUS,
        SR_CONF_LIMIT_MSEC | SR_CONF_SET | SR_CONF_LIST,
        SR_CONF_TIMEBASE | SR_CONF_SET | SR_CONF_LIST,
        SR_CONF_DEVICE_MODE | SR_CONF_SET | SR_CONF_LIST,
        SR_CONF_DATA_SOURCE | SR_CONF_SET | SR_CONF_LIST,
        SR_CONF_LIMIT_SAMPLES | SR_CONF_SET | SR_CONF_GET,
        SR_CONF_SAMPLERATE | SR_CONF_SET | SR_CONF_GET,

    };


    static struct sr_dev_driver frequnecycounter_driver_info;

    const uint64_t timebases[4][2] = {
        /*miliseconds*/
        { 10, 1000 },
        { 100, 1000 },
        /* seconds */
        { 1, 1 },
        { 10, 1 },
    };

    static const uint64_t samplerates[] = {

        SR_HZ(1),
        SR_GHZ(1),
        SR_HZ(1),
        SR_KHZ(1),
    };


static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
    struct sr_dev_inst *sdi;
	struct frequency_counter_context *devc;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	GSList *l, *devices;
	const char *serialcomm, *conn;

	devices = NULL;
	conn = serialcomm = NULL;

	for (l = options; l; l = l->next)
    {
		src = l->data;
		switch (src->key)
		{
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

    serial = sr_serial_dev_inst_new(conn, SERIALCOMM);
    strcpy(CONN,conn);

	serial_open(serial, 1);

	serial_flush(serial);

    sdi = g_malloc0(sizeof(struct sr_dev_inst));
    sdi->status = SR_ST_INACTIVE;
    sdi->vendor = g_strdup("COMLAB");
    sdi->model = g_strdup("ONE");
    sdi->version = g_strdup("1.0.0");
    devc = g_malloc0(sizeof(struct frequency_counter_context));
    sr_sw_limits_init(&devc->limits);
    sdi->conn = serial;
    sdi->priv = devc;
    sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "P1");
    devices = g_slist_append(devices, sdi);

    if (!(devices))
		sr_serial_dev_inst_free(serial);

	return std_scan_complete(di, devices);
}

static int dev_clear(const struct sr_dev_driver *di)
{
	return std_dev_clear(di);

}

static int config_get(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
    int ret;//, cg_type;
	//unsigned int i;
	struct frequency_counter_context *devc;
	const struct scope_config *model;
	struct scope_state *state;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	model = devc->model_config;
	state = devc->model_state;


    switch (key)
    {
    case SR_CONF_TIMEBASE:
		*data = g_variant_new("(tt)",
				timebases[state->timebase][0],
				timebases[state->timebase][1]);
				sr_dbg("timebase: %f",data);
		ret = SR_OK;
		break;
    case SR_CONF_SAMPLERATE:
		*data = g_variant_new_double(devc->cur_samplerate);
		break;
    case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
    case SR_CONF_CONTINUOUS:
        sr_dbg("\n\n CONTINUOUS SAMPLING \n\n");
        break;
    default:
		ret = SR_ERR_NA;

	return ret;
    }
}


static GVariant *build_tuples(const uint64_t (*array)[][2], unsigned int n)
{
	unsigned int i;
	GVariant *rational[2];
	GVariantBuilder gvb;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < n; i++) {
		rational[0] = g_variant_new_uint64((*array)[i][0]);
		rational[1] = g_variant_new_uint64((*array)[i][1]);

		/* FIXME: Valgrind reports a memory leak here. */
		g_variant_builder_add_value(&gvb, g_variant_new_tuple(rational, 2));
	}

	return g_variant_builder_end(&gvb);
}

static int config_set(uint32_t key, GVariant *data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
    int ret, cg_type;
	unsigned int i;
    char* ChannelSelect;
    char* RefChannelSelect;
	struct frequency_counter_context *devc;      //
	struct scope_state *state;
	uint64_t p, q;
    uint64_t TimebaseSel;
	gboolean update_sample_rate;
    char gatetime[30];
	ret = SR_ERR_NA;

	devc = sdi->priv;

switch (key) {
	case SR_CONF_TIMEBASE:
		g_variant_get(data, "(tt)", &p, &q);
		*send_buf = 0x00;

		for (i = 0; i < ARRAY_SIZE(timebases); i++) {
			if (p != timebases[i][0] ||
					q != timebases[i][1])
				continue;
			g_ascii_formatd(gatetime, sizeof(gatetime),
					"%E", (float) p / q);
			update_sample_rate = TRUE;
			if(strcmp(gatetime,"1.000000E+01")==0) {
                send_buf[0] |= 0x0C;
                request_delay = 10000;}
            else if(strcmp(gatetime,"1.000000E+00")==0) {
                send_buf[0] |= 0x08;
                request_delay = 1000; }
            else if(strcmp(gatetime,"1.000000E-01")==0) {
                send_buf[0] |= 0x04;
                request_delay = 100;}
            else if(strcmp(gatetime,"1.000000E-02")==0) {
                send_buf[0] |= 0x00;
                request_delay = 10; }

		}
		break;
    case SR_CONF_LIMIT_MSEC:
        TimebaseSel = g_variant_get_uint64(data);
        if(TimebaseSel == 5){
            send_buf[0] = ((send_buf[0] & 0xF3)|0x0C);
            request_delay = 10000;}
        else if (TimebaseSel == 4){
            send_buf[0] = ((send_buf[0] & 0xF3)|0x08);
            request_delay = 1000;}
        else if (TimebaseSel == 3){
            send_buf[0] = ((send_buf[0] & 0xF3)|0x04);
            request_delay = 100;}
        else {
            send_buf[0] = ((send_buf[0] & 0xF3));
            request_delay = 10;}

        break;
    case SR_CONF_CONTINUOUS:
        break;

    case SR_CONF_DEVICE_MODE:
        RefChannelSelect = g_variant_get_string(data, NULL);
        if(strcmp(RefChannelSelect,"Int.Ref")==0) {
            send_buf[0] = ((send_buf[0] & 0xFE)|0x00);
        }
        else if(strcmp(RefChannelSelect,"Ext.Ref")==0) {
            send_buf[0] = ((send_buf[0] & 0xFE)|0x01);
        }
        sr_dbg("Channel Select:%d",ChannelSelect);
        //send_buf[0] |= ChannelSelect;
        break;

    case SR_CONF_DATA_SOURCE:
        ChannelSelect = g_variant_get_string(data, NULL);
        if(strcmp(ChannelSelect,"CHA")==0) {
            send_buf[0] = ((send_buf[0] & 0xFD)|0x00);
        }
        else if(strcmp(ChannelSelect,"CHB")==0) {
            send_buf[0] = ((send_buf[0] & 0xFD)|0x02);
        }
        sr_dbg("Channel Select:%d",ChannelSelect);
        //send_buf[0] |= ChannelSelect;
        break;

    case SR_CONF_LIMIT_SAMPLES:
		devc->limit_msec = 0;
		sr_dbg("Select SAMPLES");
		devc->limit_samples = g_variant_get_uint64(data);
		break;

        case SR_CONF_SAMPLERATE:
            sr_dbg("Select SAMPLERATE");
		devc->cur_samplerate = g_variant_get_uint64(data);
		break;

    default:
		send_buf[0] = send_buf[0];
		break;
        }

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
    (void)sdi;
	(void)cg;

    GVariant *gvar;
	GVariantBuilder gvb;

    struct frequency_counter_context *devc = NULL;
	const struct scope_config *model = NULL;

	if (key == SR_CONF_SCAN_OPTIONS)
    {
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
		return SR_OK;
	}

	/* If sdi is NULL, nothing except SR_CONF_DEVICE_OPTIONS can be provided. */
	if (key == SR_CONF_DEVICE_OPTIONS && !sdi)
    {
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				drvopts, ARRAY_SIZE(drvopts), sizeof(uint32_t));
		return SR_OK;
	}

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	model = devc->model_config;

	/*
	 * If cg is NULL, only the SR_CONF_DEVICE_OPTIONS that are not
	 * specific to a channel group must be returned.
	 */
	if (!cg)
    {
		switch (key)
		{
		case SR_CONF_DEVICE_OPTIONS:
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
					devopts, ARRAY_SIZE(devopts), sizeof(uint32_t));
			return SR_OK;
		case SR_CONF_TIMEBASE:
			*data = build_tuples(&timebases, ARRAY_SIZE(timebases));
			return SR_OK;
        case SR_CONF_SCAN_OPTIONS:
            *data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
            return SR_OK;
        case SR_CONF_DEVICE_MODE:
            //*data = device_modes;
        case SR_CONF_SAMPLERATE:
			g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
			gvar = g_variant_new_fixed_array(G_VARIANT_TYPE("t"), samplerates,
					ARRAY_SIZE(samplerates), sizeof(uint64_t));
			g_variant_builder_add(&gvb, "{sv}", "samplerate-steps", gvar);
			*data = g_variant_builder_end(&gvb);
			break;
		default:
			return SR_ERR_NA;
		}
	}


    return SR_OK;
}


static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
    struct frequency_counter_context *devc;
	struct sr_serial_dev_inst *serial;
	sr_dbg("ACQUISATION");

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;

	sr_sw_limits_acquisition_start(&devc->limits);

    send_data(sdi);

	/* Poll every 50ms, or whenever some data comes in. */
    serial = sdi->conn;
        serial_source_add(sdi->session, serial, G_IO_IN, 50,
    			freqcount_receive_data, (struct sr_dev_inst *)sdi);      //


    std_session_send_df_header(sdi);

	return SR_OK;
}


static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
    struct sr_serial_dev_inst *serial;
	sr_dbg("Stopping acquisition.");
	std_session_send_df_end(sdi);
	sr_session_source_remove(sdi->session, -1);
	serial_close(serial);

	return SR_OK;
}

static struct sr_dev_driver frequnecycounter_driver_info = {
	.name = "frequnecycounter",
	.longname = "frequencycounter",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(frequnecycounter_driver_info);
