#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dumb.h>
#include <portaudio.h>

#define VERSION ("0.0.0")

// flags/args
char *arg_filename;
float volume = 1.0f;
int channels = 2;

// callback_data contains information needed in the rendering callback.
struct callback_data {
	float delta;
	DUH_SIGRENDERER *sr;
};

// die prints a message to stderr and exits with nonzero status.
void die(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

// usage prints usage information to stderr and exits with the given status.
void usage(char *argv0, int status) {
	fprintf(stderr, "Usage: %s [<option> ...] <file>\n\n", argv0);
	fprintf(stderr, "Play an IT/XM/S3M/MOD file.\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -c, --channels=2  1 or 2 for mono or stereo\n");
	fprintf(stderr, "  -v, --volume=1.0  playback volume factor\n");
	fprintf(stderr, "  -h, --help        print this message and exit\n");
	fprintf(stderr, "      --version     print version and exit\n");
	exit(status);
}

// parse_args sets flags based on command-line arguments or exits.
void parse_args(int argc, char *argv[]) {
	int i;
	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-c") == 0 ||
				strcmp(argv[i], "--channels") == 0) {
			if (++i >= argc)
				usage(argv[0], 1);
			channels = atoi(argv[i]);
			if (channels != 1 && channels != 2)
				usage(argv[0], 1);
		} else if (strcmp(argv[i], "-v") == 0 ||
				strcmp(argv[i], "--volume") == 0) {
			if (++i >= argc)
				usage(argv[0], 1);
			volume = atof(argv[i]);
			if (volume < 0)
				usage(argv[0], 1);
		} else if (strcmp(argv[i], "-h") == 0 ||
				strcmp(argv[i], "--help") == 0) {
			usage(argv[0], 0);
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("%s version %s\n", argv[0], VERSION);
			exit(1);
		} else if (strcmp(argv[i], "--") == 0) {
			if (++i == argc - 1) {
				arg_filename = argv[i];
				return;
			} else {
				usage(argv[0], 1);
			}
		} else if (strncmp(argv[i], "-", 1) == 0) {
			usage(argv[0], 1); // bad flag
		} else if (!arg_filename) {
			arg_filename = argv[i];
		} else {
			usage(argv[0], 1); // too many files
		}
	}

	if (!arg_filename) {
		usage(argv[0], 1); // no file
	}
}

// dumb_load loads a module from the given filename or returns NULL.
DUH *dumb_load(const char *filename) {
	DUMBFILE *df = dumbfile_open(filename);
	if (!df)
		return NULL;

	DUH *duh = dumb_read_it_quick(df);
	if (!duh)
		duh = dumb_read_xm_quick(df);
	if (!duh)
		duh = dumb_read_s3m_quick(df);
	if (!duh)
		duh = dumb_read_mod_quick(df);
	dumbfile_close(df);

	return duh;
}

// callback is the portaudio rendering callback.
int callback(const void *input, void *output, unsigned long frames,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags flags, void *user_data) {
	(void) input, (void) time_info, (void) flags; // prevent warnings
	struct callback_data *cd = (struct callback_data *) user_data;
	duh_render(cd->sr, 16, 0, volume, cd->delta, frames, output);
	return 0;
}

// pa_terminate wraps Pa_Terminate for use with atexit().
void pa_terminate() {
	Pa_Terminate();
}

int main(int argc, char *argv[]) {
	parse_args(argc, argv);

	// init dumb
	dumb_register_stdfiles();
	atexit(&dumb_exit);
	DUH *duh = dumb_load(arg_filename);
	if (!duh)
		die("%s: could not load module: %s\n", argv[0], arg_filename);

	// init portaudio, redirecting stderr output temporarily; a lot of
	// junk gets printed otherwise. this won't work on windows, but who
	// cares?
	int old_stderr = dup(2), new_stderr = open("/dev/null", O_WRONLY);
	dup2(new_stderr, 2);
	PaError err = Pa_Initialize();
	close(new_stderr);
	dup2(old_stderr, 2);
	close(old_stderr);
	if (err != paNoError)
		die("%s: could not init PortAudio: %s\n", argv[0],
				Pa_GetErrorText(err));
	atexit(&pa_terminate);

	// get default device info
	PaDeviceIndex index = Pa_GetDefaultOutputDevice();
	if (index == paNoDevice)
		die("%s: could not get default output device\n", argv[0]);
	struct callback_data cd;
	const PaDeviceInfo *dev = Pa_GetDeviceInfo(index);
	cd.delta = 65536.0f / dev->defaultSampleRate;

	// open and start output stream
	PaStream *stream;
	cd.sr = duh_start_sigrenderer(duh, 0, channels, 0);
	err = Pa_OpenDefaultStream(&stream, 0, channels, paInt16,
			dev->defaultSampleRate, paFramesPerBufferUnspecified,
			callback, &cd);
	if (err != paNoError) {
		duh_end_sigrenderer(cd.sr);
		unload_duh(duh);
		die("%s: could not open default stream: %s\n", argv[0],
				Pa_GetErrorText(err));
	}
	if ((err = Pa_StartStream(stream)) != paNoError) {
		duh_end_sigrenderer(cd.sr);
		unload_duh(duh);
		die("%s: could not start stream: %s\n", argv[0],
				Pa_GetErrorText(err));
	}

	// play
	while (Pa_IsStreamActive(stream) == 1) {
		Pa_Sleep(100);
	}

	// clean up
	Pa_CloseStream(stream);
	duh_end_sigrenderer(cd.sr);
	unload_duh(duh);

	return 0;
}
