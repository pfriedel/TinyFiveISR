This is really rightfully just a branch of
http://github.com/pfriedel/TinyFiveCircle - the major difference is the drawing
routine happens in an interrupt instead of on demand.  It's easier to manage but
introduces some of its own quirks.

I can't guarantee that the SConstruct will work for your environment, but the
.ino should still compile under the Arduino environment, just with -Os instead
of -O3.  It still works, but the refresh rate isn't quite what I want in -Os.

--Patrick
