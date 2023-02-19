
# Ampere Observations

_Can I use two neopixel sticks with a power supply stick that
maxes out at 800 mA?_

---

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

### pattern 2
```
brightness            red         green      blue         white
lowest brightness     4.8 mA      4.8 mA     4.8 mA       4.8 mA
12 o clock            7 mA        7 mA       6.1 mA       9 mA
3 o clock             32 mA       31 mA      26 mA        72 mA
full                  128 mA      127 mA     126 mA       > 200 mA
```

### pattern 3
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
