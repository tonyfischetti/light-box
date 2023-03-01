
# Ampere Observations

_Can I use two neopixel sticks with a power supply stick that
maxes out at 800 mA?_

---

(using a multimeter)


## one neopixel stick

### pattern 0
```
brightness            red         white
lowest brightness     2.2 mA      5.5 mA
12 o clock            16 mA       40 mA
3 o clock             38 mA       104 mA
full                  62 mA       180 mA
```

### pattern 1
```
brightness            red         cyan
lowest brightness     2.2 mA      2.2 mA
12 o clock            16.4 mA     29 mA
3 o clock             35 mA       69 mA
full                  62 mA       124 mA
```

### pattern 2
```
brightness            red         green      blue         white
lowest brightness     2.2 mA      2.2 mA     2.2 mA       2.2 mA
12 o clock            3.4 mA      3.4 mA     2.9 mA       4.7 mA
3 o clock             16.4 mA     14.8 mA    13.2 mA      39 mA
full                  63.3 mA     63.7 mA    63.3 mA      185 mA
```

### pattern 3
```
brightness            warm white
lowest brightness     2.6 mA
12 o clock            32.2 mA
3 o clock             72 mA
full                  128 mA
```

---

## two neopixel sticks

### pattern 0
```
brightness            red         white
lowest brightness     4.7 mA      5.4 mA
12 o clock            34.5 mA     94 mA
3 o clock             74 mA       > 200 mA
full                  125 nA      > 200 mA
```

### pattern 1
```
brightness            red         cyan
lowest brightness     5 mA        5 mA
12 o clock            33.4 mA     62 mA
3 o clock             70.4        137 mA
full                  125 mA      > 200 mA
```

### pattern (rgb)
```
brightness            red         green      blue         white
lowest brightness     4.8 mA      4.8 mA     4.8 mA       4.8 mA
12 o clock            7 mA        7 mA       6.1 mA       9 mA
3 o clock             32 mA       31 mA      26 mA        72 mA
full                  128 mA      127 mA     126 mA       > 200 mA
```

### pattern (warm white)
```
brightness            warm white
lowest brightness     5.5 mA
12 o clock            67 mA
3 o clock             148 mA
full                  > 200 mA
```

What should be the highest current (pattern 2 / white) didn't blow a .5
amp fuse, so.... we good?

R estimated that it should be no more than 350 mA
ratios suggest 310

The datasheet says the typical current is 18.5

(assuming that means full brightness warm white)

so that tracks

18.5 * 8 * 2

---

# After building it with 3

As it turns out, light box will get sick if you use 3 sticks at full
brightness warm white :(

Anddddd... I—just now—realized it doesn't happen with some chargers

With 9V chargers—ranging from 1A to 3A, it wasn't long, at all,
before it started bugging out (I say that because I don't think
it's Light Box).

HOWEVER... it's been going on 15 min with a charger thats
5V 8A.

So it's either (a) the other chargers were lying about how many
amps they push, or (b) [more likely?] the
[power supply](https://www.sparkfun.com/products/13032) I chose
(whose current limit is 800mA [and I have no reason not to believe
them]) didn't reach the current limit; it was bugging out because
it's regulating a 9V source down to 5V caused it to overheat

If that's true, a 7V charger (and at least 1A) should last
longer than the 9 volds. Maybe it won't even overheat at all!


I'm surprised it works with a 5V charger. I would think that there's
a voltage drop at the power supply's regulator.






The first was 9V 2A

Doesn't last long _at_ _all_


The second was 9V 1A

Which didn't last long but paradoxically a little longer than
the format


The third was 

