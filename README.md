modplay
=======
A command-line [IT/XM/S3M/MOD](https://en.wikipedia.org/wiki/Module_file)
player based on [DUMB](http://dumb.sourceforge.net/index.php?page=about) and
[PortAudio](http://www.portaudio.com/).

Installation
------------
1. Install DUMB 0.9.3 or 1.0.0 (**not** 2.0.x) and PortAudio.
2. Edit the Makefile if you wish.
3. Run `make && make install`.

Usage
-----
    Usage: modplay [OPTION]... FILE

    Play an IT/XM/S3M/MOD file.

    Options:
      -c, --channels 2           1 or 2 for mono or stereo
      -f, --fadeout 0.0          post-loop fadeout in seconds
      -i, --interpolation cubic  none, linear, or cubic
      -l, --loops 1              number of loops to play
      -o, --output FILE          render to WAV file instead
      -v, --volume 1.0           playback volume factor
      -h, --help                 print this message and exit
          --version              print version and exit
