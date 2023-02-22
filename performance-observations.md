
# Performance Observations

As of, roughly, commit `cab953640b536ffb`, the free memory is 1354B
Bringing the total number of pixels down to 8 (from 16) yields 1386B
Meaning that each stick costs 32 bytes of memory.
Makes sense... 4 bytes per channel, 8 pixels in a stick
So I shouldn't have any _memory_ problems adding one more
