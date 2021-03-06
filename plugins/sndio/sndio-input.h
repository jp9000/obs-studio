#ifndef OBS_SNDIO_INPUT_H
#define OBS_SNDIO_INPUT_H

struct sndio_thr_data {
	obs_source_t *source;
	struct obs_source_audio out;
	struct sio_hdl *hdl;
	struct sio_par par;
	int sock;
};

struct sndio_data {
	obs_source_t *source;
	pthread_attr_t attr;
	int sock;
};

#endif // OBS_SNDIO_INPUT_H
