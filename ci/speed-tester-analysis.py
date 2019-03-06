import sys
import os
import json
import re

"""
get median value of tx pps
"""
def median(array):
    array = sorted(array)
    half, odd = divmod(len(array), 2)
    if odd:
        return array[half]
    else:
        return (array[half - 1] + array[half]) / 2.0

if(len(sys.argv) != 2):
    print("ERROR: Invalid arguments.")
    sys.exit(1)

with open(sys.argv[1], "r") as f:
    contents = f.read()

pattern = re.compile("TX pkts per second:\s*([0-9]*)")
data = [int(x) for x in pattern.findall(contents)]

summary = "Median TX pps for Speed Tester: %d" % median(data)
print(summary)
