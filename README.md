modplay
=======

A command-line [IT/XM/S3M/MOD](https://en.wikipedia.org/wiki/Module_file)
player based on [DUMB](http://dumb.sourceforge.net/index.php?page=about)
and [PortAudio](http://www.portaudio.com/).

Installation
------------

1.	Install DUMB and PortAudio.
2.	Edit the Makefile if you wish.
3.	Run `make && make install`.

Usage
-----

```
Usage: modplay [<option> ...] <file>

Play an IT/XM/S3M/MOD file.

Options:
  -c, --channels=2  1 or 2 for mono or stereo
  -v, --volume=1.0  playback volume factor
  -h, --help        print this message and exit
      --version     print version and exit
```

To-do
-----

-	Terminate automatically if end of module is reached
-	Provide flags for resampling algorithm, loop
-	Write man page
