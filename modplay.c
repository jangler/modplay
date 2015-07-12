#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <dumb.h>
#include <portaudio.h>

// these defaults should be configurable via command-line arguments
int n_channels = 2;
float volume = 1.0f;

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
	// exit on incorrect usage
	if (argc < 2)
		die("Usage: %s <file>\n", argv[0]);

	// init dumb
	dumb_register_stdfiles();
	atexit(&dumb_exit);
	DUH *duh = dumb_load(argv[1]);
	if (!duh)
		die("%s: could not load module: %s\n", argv[0], argv[1]);

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
	cd.sr = duh_start_sigrenderer(duh, 0, n_channels, 0);
	err = Pa_OpenDefaultStream(&stream, 0, n_channels, paInt16,
			dev->defaultSampleRate, paFramesPerBufferUnspecified,
			callback, &cd);
	if (err != paNoError) {
		duh_end_sigrenderer(cd.sr);
		die("%s: could not open default stream: %s\n", argv[0],
				Pa_GetErrorText(err));
	}
	if ((err = Pa_StartStream(stream)) != paNoError) {
		duh_end_sigrenderer(cd.sr);
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

	return 0;
}
