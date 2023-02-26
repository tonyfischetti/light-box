#!/usr/bin/python3

# adapted from:
#   https://gist.github.com/hexagon5un/3df734ad08d8dc8d9ace0491ef97cc58


import math

def gamma(nsteps, gamma):
    gammaedUp = [math.pow(x, gamma) for x in range(nsteps)]
    return [x/max(gammaedUp) for x in gammaedUp]

def rounder(topValue, gammas):
    return [min(topValue, round(x*topValue)) for x in gammas]


if __name__ == "__main__":
    myGamma = .5
    steps = 256
    with open("gamma.h", "w") as output:
        output.write("// gamma: {}\n".format(myGamma))
        output.write("const byte PROGMEM gamma_xlate[] = {\n    ")
        for index, value in enumerate(rounder(255, gamma(steps, myGamma))):
            output.write("{}".format(value))
            if (index!=255):
                output.write(",")
                if (value < 10):
                    output.write("  ")
                elif (value < 100):
                    output.write(" ")
            if ((index+1) % 16 == 0):
                output.write("\n")
                if (index!=255):
                    output.write("    ")
        output.write("};\n")

