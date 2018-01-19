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

#ifndef LIBSIGROK_HARDWARE_FREQUNECYCOUNTER_PROTOCOL_H
#define LIBSIGROK_HARDWARE_FREQUNECYCOUNTER_PROTOCOL_H

#include <libusb.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"
#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "protocol_wrappers.h"


#define FREQCOUNT_TIMEOUT_MS 2000
#define FREQCOUNT_BUFSIZE 256
#define SERIALCOMM "9600/8n1/dtr=1/rts=0"

#define LOG_PREFIX "frequnecycounter"


char *CONN[];
char *send_buf;
uint64_t request_delay;


struct dmm_info {
	struct sr_dev_driver di;
	const char *vendor;
	const char *device;
	uint32_t baudrate;
	int packet_size;
	gboolean (*packet_valid)(const uint8_t *);
	int (*packet_parse)(const uint8_t *, float *,
			    struct sr_datafeed_analog *, void *);
	void (*dmm_details)(struct sr_datafeed_analog *, void *);
	gsize info_size;
};

#define CHUNK_SIZE		8

#define DMM_BUFSIZE		256

/** Private, per-device-instance driver context. */
struct frequency_counter_context
{
	struct sr_sw_limits limits;

	gboolean first_run;
    char gate_time;
	uint8_t protocol_buf[DMM_BUFSIZE];
	uint8_t bufoffset;
	uint8_t buflen;

	uint64_t limit_samples;
	uint64_t limit_msec;

	const void *model_config;
	void *model_state;
	double cur_samplerate;


	struct sr_channel_group **analog_groups;
	struct sr_channel_group **digital_groups;

	GSList *enabled_channels;
	GSList *current_channel;
	uint64_t num_frames;

	uint64_t frame_limit;

	gboolean data_pending;
};

struct scope_state {
	struct analog_channel_state *analog_states;
	gboolean *digital_states;
	gboolean *pod_states;

	int timebase;
	int gatetime;
	float horiz_triggerpos;

	int trigger_source;
	int trigger_slope;
	uint64_t sample_rate;
	uint32_t samples_per_frame;
};

struct scope_config {

	const uint8_t analog_channels;
	const uint8_t digital_channels;
	const uint8_t pods;

	const char *(*analog_names)[];
	const char *(*digital_names)[];

	const char *(*coupling_options)[];
	const uint8_t num_coupling_options;

	const char *(*trigger_sources)[];
	const uint8_t num_trigger_sources;

	const uint8_t num_xdivs;
	const uint8_t num_ydivs;
};


//struct dev_context {
//	const void *model_config;
//	void *model_state;
//
//	struct sr_channel_group **analog_groups;
//	struct sr_channel_group **digital_groups;
//
//	struct sr_sw_limits limits;
//
//	GSList *enabled_channels;
//	GSList *current_channel;
//	uint64_t num_frames;
//
//	uint64_t frame_limit;
//
//	//char receive_buffer[RECEIVE_BUFFER_SIZE];
//	gboolean data_pending;
//};

SR_PRIV int sample_rate_query(const struct sr_dev_inst *sdi);
SR_PRIV int freqcount_receive_data(int fd, int revents, void *cb_data);
SR_PRIV int send_data(const struct sr_dev_inst *sdi);
SR_PRIV int open_serial(const struct sr_dev_inst *sdi);

#endif
