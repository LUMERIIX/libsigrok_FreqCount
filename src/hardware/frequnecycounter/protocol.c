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
#include <config.h>

#include <math.h>

#define BUF_MAX 50


void parse_serial (char *byte_buff, int byte_num)
{
    int i =0;
    for(i=0;i<byte_num;i++)
    {
        sr_dbg("Received Data:0x%x",0l+byte_buff[i]);
    }
    printf("\n");
}

SR_PRIV int open_serial(const struct sr_dev_inst *sdi)
{

    const char *conn, *serialcomm;
    conn = serialcomm = NULL;

    struct sr_serial_dev_inst *serial;
	serial = sr_serial_dev_inst_new(conn, serialcomm);

	serial_open(serial, SERIAL_RDWR);

	serial_flush(serial);

    return SR_OK;
}

SR_PRIV int send_data(const struct sr_dev_inst *sdi)
{

    struct sr_serial_dev_inst *serial;
    serial = sr_serial_dev_inst_new(CONN, SERIALCOMM);

	serial_open(serial, 1);

	serial_flush(serial);

	if (serial_write_blocking(serial, 0/*send_buf*/, 10,
				serial_timeout(serial, 10)) < 0) {
			sr_err("Unable to send identification request.");
			return SR_ERR;
		}
    sr_dbg("send");

    g_usleep(400*1000);


    serial_close(serial);
	return SR_OK;
}



SR_PRIV int freqcount_receive_data(int fd, int revents, void *cb_data)
{
    struct frequency_counter_context *devc;
    struct sr_serial_dev_inst *serial;
	char outBuffer[1] = {0};
	char inBuffer[1] = {0};
	uint32_t i = 0;
	struct sr_datafeed_analog analog;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	struct sr_datafeed_packet packet;
	struct sr_dev_inst *sdi;

	float double_buf = 0;
	uint8_t ByteRequest = 0;
	uint64_t MeasCount = 0;
	uint64_t RefCount = 0;


	(void)fd;
	(void)revents;

	sdi = cb_data;

	sr_analog_init(&analog, &encoding, &meaning, &spec, 2);


    devc = sdi->priv;
    serial = sr_serial_dev_inst_new(CONN, SERIALCOMM);

	sr_dbg("Sending request: ");

	serial_open(serial, 1);

    serial_flush(serial);

    sr_dbg("sendbuf: 0x%x",0l+send_buf[0]);
    send_buf[0] |= 0x80;       //enable 8A
    sr_dbg("sendbuf: 0x%x",0l+send_buf[0]);
    uint32_t l = 0;

    for (i = 0; i < devc->limit_samples;)
    {
        //g_usleep((request_delay*1000) + (request_delay*1000/4));
        sp_blocking_write(serial->data,send_buf,sizeof(send_buf),1);
        //g_usleep(2000*1000);
        sr_dbg("Send Start");

        for(l = 0; l < 4;l++)
        {
            outBuffer[0] = ByteRequest;
            sp_nonblocking_write(serial->data,outBuffer,sizeof(outBuffer));
            sp_blocking_read_next(serial->data,inBuffer,sizeof(inBuffer),100);
            parse_serial(inBuffer,sizeof(inBuffer));
            MeasCount |= ((0xFF&((unsigned int)inBuffer[0]))<<(24-(8*l)));
            ByteRequest = ByteRequest+0x10;
            sr_dbg("Reading MeasCount");
        }

        for(l = 0; l < 4;l++)
        {
            outBuffer[0] = ByteRequest;
            sp_nonblocking_write(serial->data,outBuffer,sizeof(outBuffer));
            sp_blocking_read_next(serial->data,inBuffer,sizeof(inBuffer),100);
            parse_serial(inBuffer,sizeof(inBuffer));
            RefCount |= ((0xFF&((unsigned int)inBuffer[0]))<<(24-(8*l)));
            ByteRequest = ByteRequest+0x10;
            sr_dbg("Reading RefCount");
            sr_dbg("RefCount:%x",RefCount);
        }
        ByteRequest = 0x00;

        double_buf = (((9999999.9 / RefCount)*MeasCount));
        if((send_buf[0]&0x02) == 0x00 ){
            double_buf = double_buf * 8;}
        i++;


        analog.meaning->mq = SR_MQ_FREQUENCY;
        analog.meaning->unit = SR_UNIT_HERTZ;
        analog.meaning->channels = sdi->channels;
        analog.num_samples = 1;
        analog.data = &double_buf;//float_buf;
        packet.type = SR_DF_ANALOG;
        packet.payload = &analog;
        sr_session_send(sdi, &packet);
        if(devc->limit_samples == 65536)
        {
            i = 0;
        }
        l = 0;
    }

    if (sr_sw_limits_check(&devc->limits))
        sdi->driver->dev_acquisition_stop(sdi);

    sdi->driver->dev_acquisition_stop(sdi);
    return SR_OK;
}

SR_PRIV int sample_rate_query(const struct sr_dev_inst *sdi)
{
	struct frequency_counter_context *devc;
	struct scope_state *state;
	float tmp_float;

	devc = sdi->priv;
	state = devc->model_state;
	/*
	 * No need to find an active channel to query the sample rate:
	 * querying any channel will do, so we use channel 1 all the time.
	 */
	if (dlm_analog_chan_srate_get(sdi->conn, 1, &tmp_float) != SR_OK)
		return SR_ERR;

	state->sample_rate = tmp_float;

	return SR_OK;
}
