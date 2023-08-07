#!/usr/bin/python3

import subprocess
import os
import time
import numpy as np
from datetime import datetime
import sys
import os
import argparse
from datetime import date
import glob


# LB/CC mode matching
cc_modes = {
    1: "dcqcn",
    3: "hp",
    7: "timely",
    8: "dctcp",
}
lb_modes = {
    0: "fecmp",
    2: "drill",
    3: "conga",
    6: "letflow",
    9: "conweave",
}
topo2bdp = {
    "leaf_spine_128_100G_OS2": 104000,  # 2-tier -> all 100G
    "fat_k8_100G_OS2": 156000,  # 3-tier -> all 100G
}

def get_cdf(v: list):        
    # calculate cdf
    v_sorted = np.sort(v)
    p = 1. * np.arange(len(v)) / (len(v) - 1)
    od = []
    bkt = [0,0,0,0]
    n_accum = 0
    for i in range(len(v_sorted)):
        key = v_sorted[i]
        n_accum += 1
        if bkt[0] == key:
            bkt[1] += 1
            bkt[2] = n_accum
            bkt[3] = p[i]
        else:
            od.append(bkt)
            bkt = [0,0,0,0]
            bkt[0] = key
            bkt[1] = 1
            bkt[2] = n_accum
            bkt[3] = p[i]
    if od[-1][0] != bkt[0]:
        od.append(bkt)
    od.pop(0)

    ret = ""
    for bkt in od:
        var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
        ret += var
        
    return ret



def get_queue_per_switch_info_from_raw(filename, time_limit_start, time_limit_end, monitoring_interval, cdf_flag=True):

    # get number of ToR switches
    num_switch = 0
    set_switch = set()
    with open(filename, "r") as f:
        for line in f.readlines():
            parsed_line = line.replace("\n", "").split(",")
            if len(parsed_line) != 4:
                continue
            set_switch.add(int(parsed_line[1]))
    num_switch = len(set_switch)
    print("Number of ToR switches: {}".format(num_switch))
    assert(num_switch != 0)

    # start calculating percentiles
    nSample = int((float(time_limit_end) - float(time_limit_start)) / float(monitoring_interval) * num_switch) # 10us sampling interval
    result = {"nQueue": [], "nPkt": [], "nSample": nSample} 
    with open(filename, "r") as f:
        for line in f.readlines():
            parsed_line = line.replace("\n", "").split(",")
            if len(parsed_line) != 4:
                continue

            timestamp = int(parsed_line[0])
            if timestamp < time_limit_start or timestamp > time_limit_end:
                continue

            nQueue = int(parsed_line[2])
            nPkt = int(parsed_line[3])

            result["nQueue"].append(nQueue)
            result["nPkt"].append(nPkt)

    print("-> Total sample: {}, non-empty sample: {}".format(nSample, len(result["nQueue"])))
    result["nQueue"] += [0] * int(nSample - len(result["nQueue"]))
    result["nPkt"] += [0] * int(nSample - len(result["nPkt"]))
    assert(len(result["nQueue"]) == len(result["nPkt"]))


    result_stat = {"nQueue": [], "nPkt": [], "nSample": nSample} 
    ### Processing to get Avg, p50, p95, p99, p999, p9999, MAX
    result_stat["nQueue"] += [sum(result["nQueue"]) / len(result["nQueue"]),
                        int(np.percentile(result["nQueue"], 50)), 
                        int(np.percentile(result["nQueue"], 95)), 
                        int(np.percentile(result["nQueue"], 99)), 
                        int(np.percentile(result["nQueue"], 99.9)), 
                        int(np.percentile(result["nQueue"], 99.99)), 
                        np.max(result["nQueue"])]

    result_stat["nPkt"] += [sum(result["nPkt"]) / len(result["nPkt"]),
                        int(np.percentile(result["nPkt"], 50)), 
                        int(np.percentile(result["nPkt"], 95)), 
                        int(np.percentile(result["nPkt"], 99)), 
                        int(np.percentile(result["nPkt"], 99.9)), 
                        int(np.percentile(result["nPkt"], 99.99)), 
                        np.max(result["nPkt"])]

    print("-> nQueue: {}".format(result_stat["nQueue"]))
    print("-> nPkt: {}".format(result_stat["nPkt"]))


    ### SAVE CDF FILE IF NEEDED
    if cdf_flag == True:
        cdf_outfile = filename.replace(".txt", "") + "_cdf.txt" 
        cdf_output = get_cdf(result["nPkt"])
        
        with open(cdf_outfile, "w") as fw:
            fw.write(cdf_output)

    return result, result_stat




def get_queue_per_dst_info_from_raw(filename, time_limit_start, time_limit_end, monitoring_interval, cdf_flag=True):
    
    # get number of hosts from "topology config"
    nHost = 0
    filename_parsed = filename.split("/")
    filename_parsed[-1] = "config.txt"
    filename_deparsed = "/".join(filename_parsed)
    topology_file = ""
    with open(filename_deparsed, "r") as f:
        for line in f.readlines():
            if "TOPOLOGY_FILE" in line:
                topology_file = line.replace("\n", "").split(" ")[-1]
    with open(topology_file, "r") as f:
        first_line = f.readline()
        parsed_line = first_line.replace("\n", "").split(" ")
        nHost = int(parsed_line[0]) - int(parsed_line[1])

    print("Number of Servers: {}".format(nHost))
    assert(nHost != 0)


    nSample = int((time_limit_end - time_limit_start) / monitoring_interval * nHost) # 10us sampling interval

    result = {"nQueue": [], "nPkt": [], "nSample": nSample} 
    with open(filename, "r") as f:
        for line in f.readlines():
            parsed_line = line.replace("\n", "").split(",")
            timestamp = int(parsed_line[0])
            if timestamp < time_limit_start or timestamp > time_limit_end:
                continue

            nQueue = int(parsed_line[2])
            nPkt = int(parsed_line[3])

            result["nQueue"].append(nQueue)
            result["nPkt"].append(nPkt)
    
    print("-> Total sample: {}, non-empty sample: {}".format(nSample, len(result["nQueue"])))
    result["nQueue"] += [0] * int(nSample - len(result["nQueue"]))
    result["nPkt"] += [0] * int(nSample - len(result["nPkt"]))
    assert(len(result["nQueue"]) == len(result["nPkt"]))


    result_stat = {"nQueue": [], "nPkt": [], "nSample": nSample} 
    ### Processing to get Avg, p50, p95, p99, p999, p9999, MAX
    result_stat["nQueue"] += [sum(result["nQueue"]) / len(result["nQueue"]),
                        int(np.percentile(result["nQueue"], 50)), 
                        int(np.percentile(result["nQueue"], 95)), 
                        int(np.percentile(result["nQueue"], 99)), 
                        int(np.percentile(result["nQueue"], 99.9)), 
                        int(np.percentile(result["nQueue"], 99.99)), 
                        np.max(result["nQueue"])]

    result_stat["nPkt"] += [sum(result["nPkt"]) / len(result["nPkt"]),
                        int(np.percentile(result["nPkt"], 50)), 
                        int(np.percentile(result["nPkt"], 95)), 
                        int(np.percentile(result["nPkt"], 99)), 
                        int(np.percentile(result["nPkt"], 99.9)), 
                        int(np.percentile(result["nPkt"], 99.99)), 
                        np.max(result["nPkt"])]

    print("-> nQueue: {}".format(result_stat["nQueue"]))
    print("-> nPkt: {}".format(result_stat["nPkt"]))


    ### SAVE CDF FILE IF NEEDED
    if cdf_flag == True:
        cdf_outfile = filename.replace(".txt", "") + "_cdf.txt" 
        cdf_output = get_cdf(result["nQueue"])
        
        with open(cdf_outfile, "w") as fw:
            fw.write(cdf_output)

    return result, result_stat






if __name__=="__main__":
    parser = argparse.ArgumentParser(description='')
    parser.add_argument('-id', '--id', dest='id', required=True, action='store', help="traceId")
    parser.add_argument('-dir', '--dir', dest='dir', default='.', action='store', help="directory of run.py file, default='.'")
    parser.add_argument('-fdir', '--fdir', dest='fdir', default='mix', action='store', help="folder that the output files are located, default=mix")
    parser.add_argument('-sT', dest='time_limit_begin', action='store', type=int, default=2005000000, help="only consider flows that finish after T, default=2.005*10^9 ns")
    parser.add_argument('-fT', dest='time_limit_end', action='store', type=int, default=100000000000, help="only consider flows that finish before T, default=100 * 10^9 ns")
    parser.add_argument('-mT', dest='monitoring_interval', action='store', type=int, default=10000, help="monitoring interval, default 10us (10000)")
    args = parser.parse_args()
    config_ID = int(args.id)
    dirname = args.dir
    fdirname = args.fdir
    monitoringInterval = int(args.monitoring_interval)

    # time interval to consider
    time_limit_start = args.time_limit_begin
    time_limit_end = args.time_limit_end

    ### per-switch queue usage
    output_queue_total = dirname + "/" + fdirname + "/output/{id}/{id}_out_voq.txt".format(id=config_ID)
    get_queue_per_switch_info_from_raw(output_queue_total, time_limit_start, time_limit_end, monitoringInterval, cdf_flag=True)
    print("Finished queue usage per switch analysis!")

    ### per-destination queue usage
    output_queue_per_dst = dirname + "/" + fdirname + "/output/{id}/{id}_out_voq_per_dst.txt".format(id=config_ID)
    get_queue_per_dst_info_from_raw(output_queue_per_dst, time_limit_start, time_limit_end, monitoringInterval, cdf_flag=True)
    print("Finished queue usage per dst analysis!")

#### Example code:
# python3 queueAnalysis.py -id 720730903 -dir /home/mason/ns-conweave-3.29/ns-3.29 -fdir mix -bdp 156000 -sT 2000000000 -fT 2100000000