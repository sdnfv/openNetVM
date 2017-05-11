#!/usr/bin/python

import sys
import os.path

USAGE_INFO = "Usage: python csv-analysis.py <csv file name>.csv"
TIME_LABEL = "time (s)"
update_time = -1

def main():
    if len(sys.argv) != 2:
        print "Invalid number of arguments!"
        print USAGE_INFO
        sys.exit(-1)

    file_name = sys.argv[1]

    if not(os.path.isfile(file_name)):
        print "Invalid file path!"
        print USAGE_INFO
        sys.exit(-1)

    data_table = parse_data(file_name)

    analyses = []

    for label in data_table:
        analyses.append(analyze_data(data_table[label], label))

    print_output(analyses)

def print_output(analyses):
    for a in analyses:
        print a["label"]
        print "-------------------------"
        print "Packet Data (pps)"
        print "Min: " + str(a["min"])
        print "Max: " + str(a["max"])
        print "Avg: " + str(a["avg"])
        print "Median: " + str(a["median"])

        print ""

        print "Time Data (seconds)"
        print "Start Time: " + str(a["start"])
        if(a["end"] != -1):
            print "End Time: " + str(a["end"])
        else:
            print "End Time: Does not end!"

        print ""

def analyze_data(info, label):
    data = info["data"]

    analysis = {}
    analysis["label"] = label
    analysis["min"] = find_min(data)
    analysis["max"] = find_max(data)
    analysis["avg"] =  find_avg(data)
    analysis["median"] = find_median(data)
    analysis["start"] = find_start(data)
    analysis["end"] = find_end(data, analysis["start"])

    return analysis

def find_min(data):
    minimum = 1000000000
    for i in data:
        if i < minimum and i != -1:
            minimum = i

    return minimum

def find_max(data):
    maximum = data[0]
    for i in data:
        if i > maximum:
            maximum = i

    return maximum

def find_median(data):
    stripped = [item for item in data if item >= 0]
    stripped.sort()

    return stripped[len(stripped) / 2]


def find_avg(data):
    summation = 0
    count = 0
    for i in data:
        if i != -1:
            summation = summation + i
            count = count + 1

    return summation / count

def find_start(data):
    for i in range(0, len(data)):
        if(data[i] != -1):
            return i * update_time

    return -1

def find_end(data, start):
    for i in range(start, len(data)):
        if(data[i] == -1):
            return i * update_time

    return -1

def parse_data(file_name):

    table = {}

    with open(file_name) as f:
        lines = [line.rstrip("\n") for line in f.readlines()]

    time_index = -1

    titles = [x.strip() for x in lines[0].split(",")]
    for i in range(0, len(titles)):
        if(titles[i] != TIME_LABEL):
            create_bucket(table, titles[i], i)
        else:
            time_index = i

    lines.pop(0)

    global update_time
    update_time = int(lines[1][time_index]) - int(lines[0][time_index])

    for line in lines:
        data_arr = [int(x.strip()) for x in line.split(",")]
        for i in range(0, len(data_arr)):
            label = find_matching_label(i, table)
            if label is None:
                continue
            table[label]["data"].append(data_arr[i])

    return table

def find_matching_label(i, table):
    for label in table:
        if table[label]["index"] == i:
            return label

    return None

def create_bucket(table, title, index):
    obj = {}
    obj["data"] = []
    obj["index"] = index

    table[title] = obj

if __name__ == "__main__":
    main()
