import sys
import os
import json

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

if(len(sys.argv) != 5):
    print("ERROR: Invalid arguments.")
    sys.exit(1)

STATS_FILE = sys.argv[1]
STATS_NODE_NAME = sys.argv[2]
OUT_FILE = sys.argv[3]
AVG_SPEED = int(sys.argv[4]) 

contents = tuple(open(STATS_FILE, "r"))
data = [int(x.rstrip()) for x in contents if x.rstrip()]

median_speed = median(data)

results = {}
results['node'] = STATS_NODE_NAME
results['speed'] = median_speed

performance_rating = (median_speed / AVG_SPEED) * 100
if (performance_rating < 97):
    results['pass_performance_check'] = False
else:
    results['pass_performance_check'] = True
results['performance_rating'] = performance_rating
results['results_from'] = "[Results from %s]" % (STATS_NODE_NAME)
results['summary'] = "\n - Median TX pps for Pktgen: %d\n  Performance rating - %.2f%% (compared to %d average)" % (median_speed, performance_rating, AVG_SPEED)

with open(OUT_FILE, 'w') as outfile:
    json.dump(results, outfile)
