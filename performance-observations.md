
# Performance Observations

As of, roughly, commit `cab953640b536ffb`, the free memory is 1354B
Bringing the total number of pixels down to 8 (from 16) yields 1386B
Meaning that each stick costs 32 bytes of memory.
Makes sense... 4 bytes per channel, 8 pixels in a stick
So I shouldn't have any _memory_ problems adding one more

Implemented timing the patterns' inner loops
These are the number of milliseconds

pattern 0: 9127
pattern 1: 2833
pattern 2: 1
pattern 3: 1

Now let's try that trick to increase the speed of `analogRead`...

pattern 0: 8858 (269 ms)
pattern 1: 2780 (53 ms)
pattern 2: 1
pattern 3: 1
I don't know if it's worth it. Are the analog reads unstable?


---


To improve performance, I made a macro called `DEBUG_VALUES()`
that either expands to `debug_values()` or `true` depending on
the value of `DEBUG`. I wanted to avoid the function call overhead
of calling `debug_values()` if DEBUG is false in a way that isn't
ugly. It made no difference whatsoever. I don't know how

---

Testing performance/memory differences betwee DEBUG true and false

without
3355 time
1202B

with
3344 time
706B

---

Testing difference between using F and not using F

without
3355 time
1202B

with
1300B


3225
