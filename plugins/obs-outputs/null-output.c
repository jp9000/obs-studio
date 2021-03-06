/******************************************************************************
    Copyright (C) 2017 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <util/threading.h>
#include <obs-module.h>

struct null_output {
	obs_output_t *output;

	pthread_t stop_thread;
	bool stop_thread_active;
};

static const char *null_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Null Encoding Output";
}

static void *null_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct null_output *context = bzalloc(sizeof(*context));
	context->output = output;
	UNUSED_PARAMETER(settings);
	return context;
}

static void null_output_destroy(void *data)
{
	struct null_output *context = data;
	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);
	bfree(context);
}

static bool null_output_start(void *data)
{
	struct null_output *context = data;

	if (!obs_output_can_begin_data_capture(context->output, 0))
		return false;
	if (!obs_output_initialize_encoders(context->output, 0))
		return false;

	if (context->stop_thread_active)
		pthread_join(context->stop_thread, NULL);

	obs_output_begin_data_capture(context->output, 0);
	return true;
}

static void *stop_thread(void *data)
{
	struct null_output *context = data;
	obs_output_end_data_capture(context->output);
	context->stop_thread_active = false;
	return NULL;
}

static void null_output_stop(void *data, uint64_t ts)
{
	struct null_output *context = data;
	UNUSED_PARAMETER(ts);

	context->stop_thread_active = pthread_create(&context->stop_thread,
						     NULL, stop_thread,
						     data) == 0;
}

static void null_output_data(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

struct obs_output_info null_output_info = {
	.id = "null_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED,
	.get_name = null_output_getname,
	.create = null_output_create,
	.destroy = null_output_destroy,
	.start = null_output_start,
	.stop = null_output_stop,
	.encoded_packet = null_output_data,
};
