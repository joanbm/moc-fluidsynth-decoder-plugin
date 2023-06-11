/*
 * MOC - music on console
 * Copyright (C) 2004 Damian Pietras <daper@daper.net>
 *
 * FluidSynth-plugin Copyright (C) 2023 Joan Bruguera <joanbrugueram@gmail.com>
 * Based on libTiMidity-plugin, Copyright (C) 2007 Hendrik Iben <hiben@tzi.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(STANDALONE)
#define HAVE_STDBOOL_H 1
#define HAVE_VAR_ATTRIBUTE_UNUSED 1
#endif

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fluidsynth.h>
#ifdef HAVE_SMF
#include <smf.h>
#endif

#define DEBUG

#include "common.h"
#include "io.h"
#include "decoder.h"
#include "log.h"
#include "files.h"
#include "options.h"

struct fluidsynth_data
{
	fluid_synth_t *synth;
	fluid_player_t *player;
#ifdef HAVE_SMF
	smf_t *smf;
#endif
	struct decoder_error error;
};

static char *soundfont = NULL;
static int rate = 44100;
static fluid_settings_t *settings = NULL;
static fluid_synth_t *available_synth = NULL;
static pthread_mutex_t available_synth_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool have_available_synth_delayed_reclaim_thread = false;
static pthread_t available_synth_delayed_reclaim_thread;

void *available_synth_delayed_reclaim(void *void_data ATTR_UNUSED)
{
	// Grace period during which, if the user loads another MIDI file,
	// the thread will be cancelled and no cleanup will happen
	sleep(300);

	// Now we are commited to clean up; ignore cancelation requests
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &(int){0});

	pthread_mutex_lock(&available_synth_mutex);
	if (available_synth)
	{
		delete_fluid_synth(available_synth);
		available_synth = NULL;
	}
	pthread_mutex_unlock(&available_synth_mutex);

	return NULL;
}

static fluid_synth_t *acquire_available_synth()
{
	pthread_mutex_lock(&available_synth_mutex);

	if (have_available_synth_delayed_reclaim_thread)
	{
		pthread_cancel(available_synth_delayed_reclaim_thread);
		have_available_synth_delayed_reclaim_thread = false;
	}

	fluid_synth_t *acquired_synth = available_synth;
	available_synth = NULL;

	pthread_mutex_unlock(&available_synth_mutex);

	return acquired_synth;
}

static bool donate_available_synth(fluid_synth_t *synth)
{
	pthread_mutex_lock(&available_synth_mutex);

	bool success = available_synth == NULL;
	if (success)
	{
		available_synth = synth;

		have_available_synth_delayed_reclaim_thread =
		    pthread_create(&available_synth_delayed_reclaim_thread,
				   NULL, available_synth_delayed_reclaim,
				   NULL) == 0;
	}

	pthread_mutex_unlock(&available_synth_mutex);
	return success;
}

static fluid_synth_t *create_or_recycle_synth()
{
	fluid_synth_t *recycled_synth = acquire_available_synth();
	return recycled_synth != NULL ? recycled_synth
				      : new_fluid_synth(settings);
}

static void return_or_delete_synth(fluid_synth_t *synth)
{
	if (available_synth == NULL) // Double-checked locking optimization
	{
		// Reset the synthesizer to avoid mixing up between MIDIs
		fluid_synth_system_reset(synth);
		// Also necessary even after a reset to avoid carryover... why?
		fluid_synth_all_sounds_off(synth, -1);

		if (donate_available_synth(synth))
		{
			return;
		}
	}

	delete_fluid_synth(synth);
}

static void free_fluidsynth_data(struct fluidsynth_data *data)
{
	if (data->player)
	{
		delete_fluid_player(data->player);
		data->player = NULL;
	}
	if (data->synth)
	{
		return_or_delete_synth(data->synth);
		data->synth = NULL;
	}
#ifdef HAVE_SMF
	if (data->smf)
	{
		smf_delete(data->smf);
		data->smf = NULL;
	}
#endif
}

static int set_or_update_soundfont(fluid_synth_t *synth)
{
	if (soundfont == NULL)
	{
		return FLUID_OK;
	}

	// If there's already a SoundFont, make sure it's the one we want
	if (fluid_synth_sfcount(synth) == 1)
	{
		fluid_sfont_t *font = fluid_synth_get_sfont(synth, 0);
		if (strcmp(fluid_sfont_get_name(font), soundfont) != 0)
		{
			fluid_synth_sfunload(synth, fluid_sfont_get_id(font),
					     1);
		}
	}

	if (fluid_synth_sfcount(synth) == 0 &&
	    fluid_synth_sfload(synth, soundfont, 1) == FLUID_FAILED)
	{
		return FLUID_FAILED;
	}

	return FLUID_OK;
}

static struct fluidsynth_data *make_fluidsynth_data(const char *file)
{
	struct fluidsynth_data *data =
	    (struct fluidsynth_data *)xmalloc(sizeof(struct fluidsynth_data));
	data->synth = NULL;
	data->player = NULL;
#ifdef HAVE_SMF
	data->smf = NULL;
#endif
	decoder_error_init(&data->error);

	if (fluid_is_soundfont(file))
	{
		free(soundfont);
		soundfont = strdup(file);
		return data;
	}

	data->synth = create_or_recycle_synth();
	if (data->synth == NULL)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Failed to create the FluidSynth synth");
		free_fluidsynth_data(data);
		return data;
	}
	if (set_or_update_soundfont(data->synth) == FLUID_FAILED)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Can't load soundfont: %s", soundfont);
		free_fluidsynth_data(data);
		return data;
	}

	data->player = new_fluid_player(data->synth);
	if (data->player == NULL)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Failed to create the FluidSynth player");
		free_fluidsynth_data(data);
		return data;
	}

	if (fluid_player_add(data->player, file) == FLUID_FAILED)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Can't load midifile: %s", file);
		free_fluidsynth_data(data);
		return data;
	}

	if (fluid_player_play(data->player) == FLUID_FAILED)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Can't play midifile: %s", file);
		free_fluidsynth_data(data);
		return data;
	}

#ifdef HAVE_SMF
	data->smf = smf_load(file);
	if (data->smf == NULL)
	{
		decoder_error(&data->error, ERROR_FATAL, 0,
			      "Can't load midifile with libsmf: %s", file);
		free_fluidsynth_data(data);
		return data;
	}
#endif

	return data;
}

static void *fluidsynth_open(const char *file)
{
	return make_fluidsynth_data(file);
}

static void fluidsynth_close(void *void_data)
{
	struct fluidsynth_data *data = (struct fluidsynth_data *)void_data;

	free_fluidsynth_data(data);
	decoder_error_clear(&data->error);
	free(data);
}

static void fluidsynth_info(const char *file_name, struct file_tags *info,
			    const int tags_sel)
{
#ifdef HAVE_SMF
	smf_t *smf = smf_load(file_name);
	if (smf == NULL)
	{
		return;
	}

	if (tags_sel & TAGS_TIME)
	{
		info->time = (int)smf_get_length_seconds(smf);
		info->filled |= TAGS_TIME;
	}

	smf_delete(smf);
#else
	(void)file_name, (void)info, (void)tags_sel;
#endif
}

static int fluidsynth_seek(void *void_data, int sec)
{
#ifdef HAVE_SMF
	struct fluidsynth_data *data = (struct fluidsynth_data *)void_data;
	if (data->player == NULL || data->smf == NULL)
	{
		return 0;
	}

	assert(sec >= 0);

	float sec_clamp = MIN(sec, smf_get_length_seconds(data->smf));
	if (smf_seek_to_seconds(data->smf, sec_clamp) != 0)
	{
		return -1;
	}

	smf_event_t *event = smf_peek_next_event(data->smf);
	if (fluid_player_seek(data->player, event->time_pulses) == FLUID_FAILED)
	{
		return -1;
	}

	return (int)event->time_seconds;
#else
	(void)void_data, (void)sec;
	return -1;
#endif
}

static int fluidsynth_decode(void *void_data, char *buf, int buf_len,
			     struct sound_params *sound_params)
{
	struct fluidsynth_data *data = (struct fluidsynth_data *)void_data;
	if (data->player == NULL)
	{
		return 0;
	}

	sound_params->channels = 2;
	sound_params->rate = rate;
	sound_params->fmt = SFMT_S16 | SFMT_LE;

	if (fluid_player_get_status(data->player) != FLUID_PLAYER_PLAYING)
	{
		return 0;
	}

	if (fluid_synth_write_s16(data->synth,
				  buf_len / (2 * sound_params->channels), buf,
				  0, 2, buf, 1, 2) == FLUID_FAILED)
	{
		return 0;
	}

	return buf_len;
}

static int fluidsynth_get_bitrate(void *void_data ATTR_UNUSED)
{
	return -1;
}

static int fluidsynth_get_duration(void *void_data)
{
#ifdef HAVE_SMF
	struct fluidsynth_data *data = (struct fluidsynth_data *)void_data;
	return data->smf != NULL ? (int)smf_get_length_seconds(data->smf) : -1;
#else
	(void)void_data;
	return -1;
#endif
}

static void fluidsynth_get_name(const char *file ATTR_UNUSED, char buf[4])
{
	char *ext = ext_pos(file);
	if (!strcasecmp(ext, "MID"))
		strcpy(buf, "MID");
	else if (!strcasecmp(ext, "SF2"))
		strcpy(buf, "SF2");
	else if (!strcasecmp(ext, "SF3"))
		strcpy(buf, "SF3");
}

static int fluidsynth_our_format_ext(const char *ext)
{
	return !strcasecmp(ext, "MID") || !strcasecmp(ext, "SF2") ||
	       !strcasecmp(ext, "SF3");
}

static int fluidsynth_our_format_mime(const char *mime)
{
	return !strcasecmp(mime, "audio/midi") ||
	       !strncasecmp(mime, "audio/midi;", 11) ||
	       !strcasecmp(mime, "audio/x-sfbk") ||
	       !strncasecmp(mime, "audio/x-sfbk;", 13);
}

static void fluidsynth_get_error(void *prv_data, struct decoder_error *error)
{
	struct fluidsynth_data *data = (struct fluidsynth_data *)prv_data;

	decoder_error_copy(error, &data->error);
}

static void fluidsynth_init()
{
#ifdef STANDALONE
	soundfont = xstrdup(getenv("MOC_FLUIDSYNTH_SOUNDFONT"));
	rate = 44100;
#else
	soundfont = xstrdup(options_get_str("FluidSynth_SoundFont"));
	rate = options_get_int("FluidSynth_Rate");
#endif

	// Since we just decode into a buffer, we don't need audio drivers.
	// And also avoid https://github.com/FluidSynth/fluidsynth/issues/218
	static const char *EMPTY_AUDIO_DRIVERS[] = {NULL};
	if (fluid_audio_driver_register(EMPTY_AUDIO_DRIVERS) == FLUID_FAILED)
	{
		fatal("FluidSynth: Failed to register empty audio drivers");
	}

	settings = new_fluid_settings();
	if (settings == NULL)
	{
		fatal("FluidSynth: Failed to create settings");
	}

	if (fluid_settings_setnum(settings, "synth.sample-rate", rate) ==
	    FLUID_FAILED)
	{
		fatal("FluidSynth: Failed to set the sample date");
	}

	if (soundfont == NULL)
	{
		char *default_soundfont;
		if (fluid_settings_dupstr(settings, "synth.default-soundfont",
					  &default_soundfont) == FLUID_OK)
		{
			soundfont = default_soundfont;
		}
	}
}

static void fluidsynth_destroy()
{
	fluid_synth_t *recycled_synth = acquire_available_synth();
	if (recycled_synth != NULL)
	{
		delete_fluid_synth(recycled_synth);
	}

	free(soundfont);
	soundfont = NULL;

	delete_fluid_settings(settings);
	settings = NULL;
}

static struct decoder fluidsynth_decoder = {DECODER_API_VERSION,
					    fluidsynth_init,
					    fluidsynth_destroy,
					    fluidsynth_open,
					    NULL,
					    NULL,
					    fluidsynth_close,
					    fluidsynth_decode,
					    fluidsynth_seek,
					    fluidsynth_info,
					    fluidsynth_get_bitrate,
					    fluidsynth_get_duration,
					    fluidsynth_get_error,
					    fluidsynth_our_format_ext,
					    fluidsynth_our_format_mime,
					    fluidsynth_get_name,
					    NULL,
					    NULL,
					    NULL};

struct decoder *plugin_init()
{
	return &fluidsynth_decoder;
}
