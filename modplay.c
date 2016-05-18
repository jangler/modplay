#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dumb.h>
#include <portaudio.h>

#define SAMPLE_RATE 44100
#define VERSION "1.1.4"

// flags/args
char *argv0, *arg_filename, *output_filename;
float initial_volume = 1.0f, volume = 1.0f, fadeout = 0.0f;
int channels = 2, loops = 1;

// callback_data contains information needed in the rendering callback.
typedef struct {
	float delta;
	int sample_rate;
	DUH_SIGRENDERER *sr;
} callback_data;

typedef struct {
	char riff_id[4];
	unsigned int riff_size;
	char wave_id[4], fmt_id[4];
	unsigned int fmt_size;
	unsigned short format_tag, channels;
	unsigned int samples_per_second, bytes_per_second;
	unsigned short block_align, bits_per_sample;
	char data_id[4];
	unsigned int data_size;
} wav_header;

// usage prints usage information to stderr and exits with the given status.
void usage(int status) {
	fprintf(stderr, "Usage: %s [OPTION]... FILE\n\n", argv0);
	fprintf(stderr, "Play an IT/XM/S3M/MOD file.\n\n");
	fprintf(stderr, "Options:\n");
	char *options[] = {
		"  -c, --channels 2           1 or 2 for mono or stereo",
		"  -f, --fadeout 0.0          post-loop fadeout in seconds",
		"  -i, --interpolation cubic  none, linear, or cubic",
		"  -l, --loops 1              number of loops to play",
		"  -o, --output FILE          render to WAV file instead",
		"  -v, --volume 1.0           playback volume factor",
		"  -h, --help                 print this message and exit",
		"      --version              print version and exit",
		NULL,
	};
	int i = 0;
	while (options[i])
		fprintf(stderr, "%s\n", options[i++]);
	exit(status);
}

// parse_args sets flags based on command-line arguments or exits.
void parse_args(int argc, char *argv[]) {
	int i;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-c") == 0 ||
				strcmp(argv[i], "--channels") == 0) {
			if (++i >= argc)
				usage(2);
			channels = atoi(argv[i]);
			if (channels != 1 && channels != 2)
				usage(2);
		} else if (strcmp(argv[i], "-f") == 0 ||
				strcmp(argv[i], "--fadeout") == 0) {
			if (++i >= argc)
				usage(2);
			fadeout = atof(argv[i]);
			if (fadeout < 0)
				usage(2);
		} else if (strcmp(argv[i], "-i") == 0 ||
				strcmp(argv[i], "--interpolation") == 0) {
			if (++i >= argc)
				usage(2);

			if (strcmp(argv[i], "none") == 0)
				dumb_resampling_quality = DUMB_RQ_ALIASING;
			else if (strcmp(argv[i], "linear") == 0)
				dumb_resampling_quality = DUMB_RQ_LINEAR;
			else if (strcmp(argv[i], "cubic") == 0)
				dumb_resampling_quality = DUMB_RQ_CUBIC;
			else
				usage(2);
		} else if (strcmp(argv[i], "-l") == 0 ||
				strcmp(argv[i], "--loops") == 0) {
			if (++i >= argc)
				usage(2);
			loops = atoi(argv[i]);
		} else if (strcmp(argv[i], "-o") == 0 ||
				strcmp(argv[i], "--output") == 0) {
			if (++i >= argc)
				usage(2);
			output_filename = argv[i];
		} else if (strcmp(argv[i], "-v") == 0 ||
				strcmp(argv[i], "--volume") == 0) {
			if (++i >= argc)
				usage(2);
			initial_volume = volume = atof(argv[i]);
			if (volume <= 0)
				usage(2);
		} else if (strcmp(argv[i], "-h") == 0 ||
				strcmp(argv[i], "--help") == 0) {
			usage(0);
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("%s version %s\n", argv[0], VERSION);
			exit(0);
		} else if (strcmp(argv[i], "--") == 0) {
			// stop parsing flags
			if (++i == argc - 1) {
				arg_filename = argv[i];
				return;
			} else {
				usage(2);
			}
		} else if (strncmp(argv[i], "-", 1) == 0) {
			usage(2); // bad flag
		} else if (!arg_filename) {
			arg_filename = argv[i];
		} else {
			usage(2); // too many files
		}
	}

	if (!arg_filename) {
		usage(2);
	}
}

// dumb_load loads a module from the given filename or returns NULL.
DUH *dumb_load(const char *filename) {
	DUH *duh = dumb_load_it_quick(filename);
	if (!duh)
		duh = dumb_load_xm_quick(filename);
	if (!duh)
		duh = dumb_load_s3m_quick(filename);
	if (!duh)
		duh = dumb_load_mod_quick(filename);
	return duh;
}

// die prints a message to stderr and exits with nonzero status.
void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s: ", argv0);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

// pa_terminate wraps Pa_Terminate for use with atexit().
void pa_terminate() {
	Pa_Terminate();
}

// fade the volume based on the given frame count and sample rate.
void fade(int frames, int sample_rate) {
	if (fadeout)
		volume -= initial_volume * frames / sample_rate / fadeout;
	else
		volume = 0;
}

// callback is the portaudio rendering callback.
int callback(const void *input, void *output, unsigned long frames,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags flags, void *user_data) {
	(void) input, (void) time_info, (void) flags; // prevent warnings
	callback_data *cd = (callback_data*) user_data;
	long n = duh_render(cd->sr, 16, 0, volume, cd->delta, frames, output);
	if (loops <= 0)
		fade(frames, cd->sample_rate);
	if (volume <= 0 || (unsigned long) n < frames)
		return paComplete;
	return paContinue;
}

// loop_callback decrements the loop counter whenever the song loops.
int loop_callback(void *data) {
	(void) data; // prevent warning
	if (--loops <= 0 && !fadeout)
		volume = 0;
	return 0;
}

void init_sigrenderer(DUH_SIGRENDERER *sr) {
	DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
	dumb_it_set_loop_callback(itsr, &loop_callback, NULL);
	dumb_it_set_xm_speed_zero_callback(itsr, &dumb_it_callback_terminate,
			NULL);
}

// play a module using portaudio.
void play(DUH *duh) {
	// init portaudio, redirecting stderr output temporarily; a lot of junk
	// gets printed otherwise. this won't work on windows, but who cares?
	int old_stderr = dup(2);
	int new_stderr = open("/dev/null", O_WRONLY);
	dup2(new_stderr, 2);
	PaError err = Pa_Initialize();
	close(new_stderr);
	dup2(old_stderr, 2);
	close(old_stderr);
	if (err != paNoError)
		die("could not init PortAudio: %s\n", Pa_GetErrorText(err));
	atexit(&pa_terminate);

	// get default device info
	PaDeviceIndex index = Pa_GetDefaultOutputDevice();
	if (index == paNoDevice)
		die("could not get default output device\n");
	callback_data cd;
	const PaDeviceInfo *dev = Pa_GetDeviceInfo(index);
	cd.sample_rate = dev->defaultSampleRate;
	cd.delta = 65536.0f / cd.sample_rate;

	// open and start output stream
	PaStream *stream;
	cd.sr = duh_start_sigrenderer(duh, 0, channels, 0);
	err = Pa_OpenDefaultStream(&stream, 0, channels, paInt16,
			dev->defaultSampleRate, paFramesPerBufferUnspecified,
			callback, &cd);
	if (err != paNoError) {
		duh_end_sigrenderer(cd.sr);
		unload_duh(duh);
		die("could not open default stream: %s\n",
				Pa_GetErrorText(err));
	}
	if ((err = Pa_StartStream(stream)) != paNoError) {
		duh_end_sigrenderer(cd.sr);
		unload_duh(duh);
		die("could not start stream: %s\n", Pa_GetErrorText(err));
	}

	init_sigrenderer(cd.sr);

	// play
	while (Pa_IsStreamActive(stream) == 1)
		Pa_Sleep(100);

	// clean up
	Pa_CloseStream(stream);
	duh_end_sigrenderer(cd.sr);
}

void write_wav_header(unsigned int num_bytes, FILE *file, DUH *duh) {
	wav_header header = {
		.riff_id            = "RIFF",
		.riff_size          = 4 + 24 + 8 + num_bytes,
		.wave_id            = "WAVE",
		.fmt_id             = "fmt ",
		.fmt_size           = 16,
		.format_tag         = 1,
		.channels           = channels,
		.samples_per_second = SAMPLE_RATE,
		.bytes_per_second   = SAMPLE_RATE * 16 * channels,
		.block_align        = 16 * channels,
		.bits_per_sample    = 8 * 2,
		.data_id            = "data",
		.data_size          = num_bytes,
	};
	fseek(file, 0, SEEK_SET);
	if (fwrite(&header, sizeof(wav_header), 1, file) < 1) {
		unload_duh(duh);
		die("%s: %s\n", output_filename, strerror(errno));
	}
}

// render a chunk of audio data to a WAV file and return 0 iff done.
int render_chunk(callback_data *cd, long frames, short *buf, FILE* file,
		long *num_bytes) {
	long n = duh_render(cd->sr, 16, 0, volume, cd->delta, frames, buf);
	if (loops <= 0)
		fade(frames, cd -> sample_rate);
	*num_bytes += n * channels * 2;

	if (file) {
		long bytes = frames * channels;
		if ((long) fwrite(buf, sizeof(short), bytes, file) < bytes) {
			fprintf(stderr, "%s: %s\n", output_filename,
					strerror(errno));
			return 0;
		}
	}

	if (volume <= 0 || n < frames)
		return 0;
	return 1;
}

// write a module to a file.
void render(DUH *duh) {
	// open output file
	FILE *file = fopen(output_filename, "wb");
	if (!file) {
		unload_duh(duh);
		die("%s: %s\n", output_filename, strerror(errno));
	}

	// init renderer
	callback_data cd = {
		.sample_rate = SAMPLE_RATE,
		.delta = 65536.0f / SAMPLE_RATE,
		.sr = duh_start_sigrenderer(duh, 0, channels, 0),
	};
	init_sigrenderer(cd.sr);
	long frames = 512;
	short buf[frames * channels];

	// write file
	long num_bytes = 0;
	write_wav_header(num_bytes, file, duh); // dummy header write
	while (render_chunk(&cd, frames, buf, file, &num_bytes));
	write_wav_header(num_bytes, file, duh); // real header write

	// clean up
	duh_end_sigrenderer(cd.sr);
	if (fclose(file)) {
		unload_duh(duh);
		die("%s: %s\n", output_filename, strerror(errno));
	}
}

int main(int argc, char *argv[]) {
	argv0 = argv[0];
	parse_args(argc, argv);

	// init dumb
	dumb_register_stdfiles();
	atexit(&dumb_exit);
	DUH *duh = dumb_load(arg_filename);
	if (!duh)
		die("could not load module: %s\n", arg_filename);

	if (output_filename)
		render(duh);
	else
		play(duh);

	unload_duh(duh);

	return 0;
}
