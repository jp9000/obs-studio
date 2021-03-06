#include <obs-module.h>
#include <libvlc.h>

#ifdef _MSC_VER
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <libvlc_media.h>
#include <libvlc_events.h>
#include <libvlc_media_list.h>
#include <libvlc_media_player.h>
#include <libvlc_media_list_player.h>

extern libvlc_instance_t *libvlc;
extern uint64_t time_start;

extern bool load_libvlc(void);

/* libvlc core */
typedef libvlc_instance_t *(*LIBVLC_NEW)(int argc, const char *const *argv);
typedef void (*LIBVLC_RELEASE)(libvlc_instance_t *p_instance);
typedef int64_t (*LIBVLC_CLOCK)(void);
typedef int (*LIBVLC_EVENT_ATTACH)(libvlc_event_manager_t *p_event_manager,
				   libvlc_event_type_t i_event_type,
				   libvlc_callback_t f_callback,
				   void *user_data);

/* libvlc media */
typedef libvlc_media_t *(*LIBVLC_MEDIA_NEW_PATH)(libvlc_instance_t *p_instance,
						 const char *path);
typedef libvlc_media_t *(*LIBVLC_MEDIA_NEW_LOCATION)(
	libvlc_instance_t *p_instance, const char *location);
typedef void (*LIBVLC_MEDIA_ADD_OPTION)(libvlc_media_t *p_md,
					const char *options);
typedef void (*LIBVLC_MEDIA_RETAIN)(libvlc_media_t *p_md);
typedef void (*LIBVLC_MEDIA_RELEASE)(libvlc_media_t *p_md);
typedef char *(*LIBVLC_MEDIA_GET_META)(libvlc_media_t *p_md,
				       libvlc_meta_t e_meta);

/* libvlc media player */
typedef libvlc_media_player_t *(*LIBVLC_MEDIA_PLAYER_NEW)(
	libvlc_instance_t *p_libvlc);
typedef libvlc_media_player_t *(*LIBVLC_MEDIA_PLAYER_NEW_FROM_MEDIA)(
	libvlc_media_t *p_md);
typedef void (*LIBVLC_MEDIA_PLAYER_RELEASE)(libvlc_media_player_t *p_mi);
typedef void (*LIBVLC_VIDEO_SET_CALLBACKS)(libvlc_media_player_t *mp,
					   libvlc_video_lock_cb lock,
					   libvlc_video_unlock_cb unlock,
					   libvlc_video_display_cb display,
					   void *opaque);
typedef void (*LIBVLC_VIDEO_SET_FORMAT_CALLBACKS)(
	libvlc_media_player_t *mp, libvlc_video_format_cb setup,
	libvlc_video_cleanup_cb cleanup);
typedef void (*LIBVLC_AUDIO_SET_CALLBACKS)(
	libvlc_media_player_t *mp, libvlc_audio_play_cb play,
	libvlc_audio_pause_cb pause, libvlc_audio_resume_cb resume,
	libvlc_audio_flush_cb flush, libvlc_audio_drain_cb drain, void *opaque);
typedef void (*LIBVLC_AUDIO_SET_FORMAT_CALLBACKS)(
	libvlc_media_player_t *mp, libvlc_audio_setup_cb setup,
	libvlc_audio_cleanup_cb cleanup);
typedef int (*LIBVLC_MEDIA_PLAYER_PLAY)(libvlc_media_player_t *p_mi);
typedef void (*LIBVLC_MEDIA_PLAYER_STOP)(libvlc_media_player_t *p_mi);
typedef libvlc_time_t (*LIBVLC_MEDIA_PLAYER_GET_TIME)(
	libvlc_media_player_t *p_mi);
typedef void (*LIBVLC_MEDIA_PLAYER_SET_TIME)(libvlc_media_player_t *p_mi,
					     libvlc_time_t i_time);
typedef int (*LIBVLC_VIDEO_GET_SIZE)(libvlc_media_player_t *p_mi, unsigned num,
				     unsigned *px, unsigned *py);
typedef libvlc_event_manager_t *(*LIBVLC_MEDIA_PLAYER_EVENT_MANAGER)(
	libvlc_media_player_t *p_mp);
typedef libvlc_state_t (*LIBVLC_MEDIA_PLAYER_GET_STATE)(
	libvlc_media_player_t *p_mi);
typedef libvlc_time_t (*LIBVLC_MEDIA_PLAYER_GET_LENGTH)(
	libvlc_media_player_t *p_mi);
typedef libvlc_media_t *(*LIBVLC_MEDIA_PLAYER_GET_MEDIA)(
	libvlc_media_player_t *p_mi);

/* libvlc media list */
typedef libvlc_media_list_t *(*LIBVLC_MEDIA_LIST_NEW)(
	libvlc_instance_t *p_instance);
typedef void (*LIBVLC_MEDIA_LIST_RELEASE)(libvlc_media_list_t *p_ml);
typedef int (*LIBVLC_MEDIA_LIST_ADD_MEDIA)(libvlc_media_list_t *p_ml,
					   libvlc_media_t *p_md);
typedef void (*LIBVLC_MEDIA_LIST_LOCK)(libvlc_media_list_t *p_ml);
typedef void (*LIBVLC_MEDIA_LIST_UNLOCK)(libvlc_media_list_t *p_ml);
typedef libvlc_event_manager_t *(*LIBVLC_MEDIA_LIST_EVENT_MANAGER)(
	libvlc_media_list_t *p_ml);

/* libvlc media list player */
typedef libvlc_media_list_player_t *(*LIBVLC_MEDIA_LIST_PLAYER_NEW)(
	libvlc_instance_t *p_instance);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_RELEASE)(
	libvlc_media_list_player_t *p_mlp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_PLAY)(libvlc_media_list_player_t *p_mlp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_PAUSE)(
	libvlc_media_list_player_t *p_mlp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_STOP)(libvlc_media_list_player_t *p_mlp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_SET_MEDIA_PLAYER)(
	libvlc_media_list_player_t *p_mlp, libvlc_media_player_t *p_mp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_SET_MEDIA_LIST)(
	libvlc_media_list_player_t *p_mlp, libvlc_media_list_t *p_mlist);
typedef libvlc_event_manager_t *(*LIBVLC_MEDIA_LIST_PLAYER_EVENT_MANAGER)(
	libvlc_media_list_player_t *p_mlp);
typedef void (*LIBVLC_MEDIA_LIST_PLAYER_SET_PLAYBACK_MODE)(
	libvlc_media_list_player_t *p_mlp, libvlc_playback_mode_t e_mode);
typedef int (*LIBVLC_MEDIA_LIST_PLAYER_NEXT)(libvlc_media_list_player_t *p_mlp);
typedef int (*LIBVLC_MEDIA_LIST_PLAYER_PREVIOUS)(
	libvlc_media_list_player_t *p_mlp);

/* -------------------------------------------------------------------- */

/* libvlc core */
extern LIBVLC_NEW libvlc_new_;
extern LIBVLC_RELEASE libvlc_release_;
extern LIBVLC_CLOCK libvlc_clock_;
extern LIBVLC_EVENT_ATTACH libvlc_event_attach_;

/* libvlc media */
extern LIBVLC_MEDIA_NEW_PATH libvlc_media_new_path_;
extern LIBVLC_MEDIA_NEW_LOCATION libvlc_media_new_location_;
extern LIBVLC_MEDIA_ADD_OPTION libvlc_media_add_option_;
extern LIBVLC_MEDIA_RELEASE libvlc_media_release_;
extern LIBVLC_MEDIA_RETAIN libvlc_media_retain_;
extern LIBVLC_MEDIA_GET_META libvlc_media_get_meta_;

/* libvlc media player */
extern LIBVLC_MEDIA_PLAYER_NEW libvlc_media_player_new_;
extern LIBVLC_MEDIA_PLAYER_NEW_FROM_MEDIA libvlc_media_player_new_from_media_;
extern LIBVLC_MEDIA_PLAYER_RELEASE libvlc_media_player_release_;
extern LIBVLC_VIDEO_SET_CALLBACKS libvlc_video_set_callbacks_;
extern LIBVLC_VIDEO_SET_FORMAT_CALLBACKS libvlc_video_set_format_callbacks_;
extern LIBVLC_AUDIO_SET_CALLBACKS libvlc_audio_set_callbacks_;
extern LIBVLC_AUDIO_SET_FORMAT_CALLBACKS libvlc_audio_set_format_callbacks_;
extern LIBVLC_MEDIA_PLAYER_PLAY libvlc_media_player_play_;
extern LIBVLC_MEDIA_PLAYER_STOP libvlc_media_player_stop_;
extern LIBVLC_MEDIA_PLAYER_GET_TIME libvlc_media_player_get_time_;
extern LIBVLC_MEDIA_PLAYER_SET_TIME libvlc_media_player_set_time_;
extern LIBVLC_VIDEO_GET_SIZE libvlc_video_get_size_;
extern LIBVLC_MEDIA_PLAYER_EVENT_MANAGER libvlc_media_player_event_manager_;
extern LIBVLC_MEDIA_PLAYER_GET_STATE libvlc_media_player_get_state_;
extern LIBVLC_MEDIA_PLAYER_GET_LENGTH libvlc_media_player_get_length_;
extern LIBVLC_MEDIA_PLAYER_GET_MEDIA libvlc_media_player_get_media_;

/* libvlc media list */
extern LIBVLC_MEDIA_LIST_NEW libvlc_media_list_new_;
extern LIBVLC_MEDIA_LIST_RELEASE libvlc_media_list_release_;
extern LIBVLC_MEDIA_LIST_ADD_MEDIA libvlc_media_list_add_media_;
extern LIBVLC_MEDIA_LIST_LOCK libvlc_media_list_lock_;
extern LIBVLC_MEDIA_LIST_UNLOCK libvlc_media_list_unlock_;
extern LIBVLC_MEDIA_LIST_EVENT_MANAGER libvlc_media_list_event_manager_;

/* libvlc media list player */
extern LIBVLC_MEDIA_LIST_PLAYER_NEW libvlc_media_list_player_new_;
extern LIBVLC_MEDIA_LIST_PLAYER_RELEASE libvlc_media_list_player_release_;
extern LIBVLC_MEDIA_LIST_PLAYER_PLAY libvlc_media_list_player_play_;
extern LIBVLC_MEDIA_LIST_PLAYER_PAUSE libvlc_media_list_player_pause_;
extern LIBVLC_MEDIA_LIST_PLAYER_STOP libvlc_media_list_player_stop_;
extern LIBVLC_MEDIA_LIST_PLAYER_SET_MEDIA_PLAYER
	libvlc_media_list_player_set_media_player_;
extern LIBVLC_MEDIA_LIST_PLAYER_SET_MEDIA_LIST
	libvlc_media_list_player_set_media_list_;
extern LIBVLC_MEDIA_LIST_PLAYER_EVENT_MANAGER
	libvlc_media_list_player_event_manager_;
extern LIBVLC_MEDIA_LIST_PLAYER_SET_PLAYBACK_MODE
	libvlc_media_list_player_set_playback_mode_;
extern LIBVLC_MEDIA_LIST_PLAYER_NEXT libvlc_media_list_player_next_;
extern LIBVLC_MEDIA_LIST_PLAYER_PREVIOUS libvlc_media_list_player_previous_;

#define EXTENSIONS_AUDIO \
	"*.3ga;"         \
	"*.669;"         \
	"*.a52;"         \
	"*.aac;"         \
	"*.ac3;"         \
	"*.adt;"         \
	"*.adts;"        \
	"*.aif;"         \
	"*.aifc;"        \
	"*.aiff;"        \
	"*.amb;"         \
	"*.amr;"         \
	"*.aob;"         \
	"*.ape;"         \
	"*.au;"          \
	"*.awb;"         \
	"*.caf;"         \
	"*.dts;"         \
	"*.flac;"        \
	"*.it;"          \
	"*.kar;"         \
	"*.m4a;"         \
	"*.m4b;"         \
	"*.m4p;"         \
	"*.m5p;"         \
	"*.mid;"         \
	"*.mka;"         \
	"*.mlp;"         \
	"*.mod;"         \
	"*.mpa;"         \
	"*.mp1;"         \
	"*.mp2;"         \
	"*.mp3;"         \
	"*.mpc;"         \
	"*.mpga;"        \
	"*.mus;"         \
	"*.oga;"         \
	"*.ogg;"         \
	"*.oma;"         \
	"*.opus;"        \
	"*.qcp;"         \
	"*.ra;"          \
	"*.rmi;"         \
	"*.s3m;"         \
	"*.sid;"         \
	"*.spx;"         \
	"*.tak;"         \
	"*.thd;"         \
	"*.tta;"         \
	"*.voc;"         \
	"*.vqf;"         \
	"*.w64;"         \
	"*.wav;"         \
	"*.wma;"         \
	"*.wv;"          \
	"*.xa;"          \
	"*.xm"

#define EXTENSIONS_VIDEO                                                       \
	"*.3g2;*.3gp;*.3gp2;*.3gpp;*.amv;*.asf;*.avi;"                         \
	"*.bik;*.bin;*.crf;*.divx;*.drc;*.dv;*.evo;*.f4v;*.flv;*.gvi;*.gxf;"   \
	"*.iso;*.m1v;*.m2v;*.m2t;*.m2ts;*.m4v;*.mkv;*.mov;*.mp2;*.mp2v;*.mp4;" \
	"*.mp4v;*.mpe;*.mpeg;*.mpeg1;*.mpeg2;*.mpeg4;*.mpg;*.mpv2;*.mts;"      \
	"*.mtv;*.mxf;*.mxg;*.nsv;*.nuv;*.ogg;*.ogm;*.ogv;*.ogx;*.ps;*.rec;"    \
	"*.rm;*.rmvb;*.rpl;*.thp;*.tod;*.ts;*.tts;*.txd;*.vob;*.vro;*.webm;"   \
	"*.wm;*.wmv;*.wtv;*.xesc"

#define EXTENSIONS_PLAYLIST                           \
	"*.asx;*.b4s;*.cue;*.ifo;*.m3u;*.m3u8;*.pls;" \
	"*.ram;*.rar;*.sdp;*.vlc;*.xspf;*.wax;*.wvx;*.zip;*.conf"

#define EXTENSIONS_MEDIA \
	EXTENSIONS_VIDEO ";" EXTENSIONS_AUDIO ";" EXTENSIONS_PLAYLIST
