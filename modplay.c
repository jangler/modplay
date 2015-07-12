#include <stdarg.h>
#include <stdlib.h>

#include <dumb.h>
#include <portaudio.h>

#define SAMPLE_RATE (44100)

const int delta = 65536.0f / SAMPLE_RATE;

int n_channels = 2; // stereo

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

int callback(const void *input, void *output, unsigned long frames,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags flags, void *user_data) {
	(void) input, (void) time_info, (void) flags; // prevent warnings
	DUH_SIGRENDERER *sr = (DUH_SIGRENDERER*) user_data;
	duh_render(sr, 16, 1, 1.0f, delta, frames, output);
	return 0;
}

// pa_terminate wraps Pa_Terminate for use with atexit().
void pa_terminate() {

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

	// init portaudio
	PaError err = Pa_Initialize();
	if (err != paNoError)
		die("%s: could not init PortAudio: %s\n", argv[0],
				Pa_GetErrorText(err));
	atexit(&pa_terminate);

	// open and start output stream
	PaStream *stream;
	DUH_SIGRENDERER *sr = duh_start_sigrenderer(duh, 0, n_channels, 0);
	err = Pa_OpenDefaultStream(&stream, 0, n_channels, paInt16, SAMPLE_RATE,
			paFramesPerBufferUnspecified, callback, sr);
	if (err != paNoError) {
		duh_end_sigrenderer(sr);
		die("%s: could not open default stream: %s\n", argv[0],
				Pa_GetErrorText(err));
	}
	if ((err = Pa_StartStream(stream)) != paNoError) {
		duh_end_sigrenderer(sr);
		die("%s: could not start stream: %s\n", argv[0],
				Pa_GetErrorText(err));
	}

	// clean up
	Pa_CloseStream(stream);
	duh_end_sigrenderer(sr);

	return 0;
}
