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
Usage: modplay <file>
```

To-do
-----

-	Terminate automatically if end of module is reached
-	Provide flags for mono/stereo, resampling algorithm, volume, loop
-	Write man page
