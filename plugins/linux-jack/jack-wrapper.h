/*
Copyright (C) 2015 by Bernd Buschinski <b.buschinski@gmail.com>

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
*/

#pragma once

#include <jack/jack.h>
#include <obs.h>
#include <util/threading.h>

struct jack_data {
	obs_source_t *source;

	/* user settings */
	char *device;
	uint_fast8_t channels;
	bool start_jack_server;

	/* server info */
	enum speaker_layout speakers;
	uint_fast32_t samples_per_sec;
	uint_fast32_t bytes_per_frame;

	jack_client_t *jack_client;
	jack_port_t **jack_ports;

	pthread_mutex_t jack_mutex;
};

/**
 * Initialize the jack client and register the ports
 */
int_fast32_t jack_init(struct jack_data *data);

/**
 * Destroys the jack client and unregisters the ports
 */
void deactivate_jack(struct jack_data *data);
