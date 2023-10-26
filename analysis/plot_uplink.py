#!/usr/bin/python3

import subprocess
import os
import sys
import argparse
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.ticker as tick
import math
from cycler import cycler
import numpy as np



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
    "leaf_spine_128_100G_OS2": 104000,  # 2-tier
    "fat_k4_100G_OS2": 153000, # 3-tier -> core 400G
}

C = [
    'xkcd:grass green',
    'xkcd:blue',
    'xkcd:purple',
    'xkcd:orange',
    'xkcd:teal',
    'xkcd:brick red',
    'xkcd:black',
    'xkcd:brown',
    'xkcd:grey',
]

LS = [
    'solid',
    'dashed',
    'dotted',
    'dashdot'
]

M = [
    'o',
    's',
    'x',
    'v',
    'D'
]

H = [
    '//',
    'o',
    '***',
    'x',
    'xxx',
]


def getCdfFromArray(data_arr):
    v_sorted = np.sort(data_arr)
    p = 1. * np.arange(len(data_arr)) / (len(data_arr) - 1)

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
    return od

def setup():
    """Called before every plot_ function"""

    def lcm(a, b):
        return abs(a*b) // math.gcd(a, b)

    def a(c1, c2):
        """Add cyclers with lcm."""
        l = lcm(len(c1), len(c2))
        c1 = c1 * (l//len(c1))
        c2 = c2 * (l//len(c2))
        return c1 + c2

    def add(*cyclers):
        s = None
        for c in cyclers:
            if s is None:
                s = c
            else:
                s = a(s, c)
        return s

    plt.rc('axes', prop_cycle=(add(cycler(color=C),
                                   cycler(linestyle=LS),
                                   cycler(marker=M))))
    plt.rc('lines', markersize=5)
    plt.rc('legend', handlelength=3, handleheight=1.5, labelspacing=0.25)
    plt.rcParams["font.family"] = "sans"
    plt.rcParams["font.size"] = 10
    plt.rcParams['pdf.fonttype'] = 42
    plt.rcParams['ps.fonttype'] = 42


def getFilePath():
    dir_path = os.path.dirname(os.path.realpath(__file__))
    print("File directory: {}".format(dir_path))
    return dir_path

def get_pctl(a, p):
	i = int(len(a) * p)
	return a[i]

def size2str(steps):
    result = []
    for step in steps:
        if step < 10000:
            result.append("{:.1f}K".format(step / 1000))
        elif step < 1000000:
            result.append("{:.0f}K".format(step / 1000))
        else:
            result.append("{:.1f}M".format(step / 1000000))

    return result


def main():
    parser = argparse.ArgumentParser(description='Plotting FCT of results')
    parser.add_argument('-sT', dest='time_limit_begin', action='store', type=int, default=2005000000, help="only consider flows that finish after T, default=2005000000 ns")
    parser.add_argument('-fT', dest='time_limit_end', action='store', type=int, default=2100000000, help="only consider flows that finish before T, default=2100000000 ns")
    
    time_interval = 100000 # 100us
    args = parser.parse_args()
    time_start = int(args.time_limit_begin)
    time_end = int(args.time_limit_end)
    
    file_dir = getFilePath()
    fig_dir = file_dir + "/figures"
    output_dir = file_dir + "/../mix/output"
    history_filename = file_dir + "/../mix/.history"

    # read history file
    map_key_to_id = dict()

    with open(history_filename, "r") as f:
        for line in f.readlines():
            for topo in topo2bdp.keys():
                if topo in line:
                    parsed_line = line.replace("\n", "").split(',')
                    config_id = parsed_line[1]
                    cc_mode = cc_modes[int(parsed_line[2])]
                    lb_mode = lb_modes[int(parsed_line[3])]
                    encoded_fc = (int(parsed_line[9]), int(parsed_line[10]))
                    if encoded_fc == (0, 1):
                        flow_control = "IRN"
                    elif encoded_fc == (1, 0):
                        flow_control = "Lossless"
                    else:
                        continue
                    topo = parsed_line[13]
                    netload = parsed_line[16]
                    key = (topo, netload, flow_control)
                    if key not in map_key_to_id:
                        map_key_to_id[key] = [[config_id, lb_mode]]
                    else:
                        map_key_to_id[key].append([config_id, lb_mode])

    for k, v in map_key_to_id.items():
        ################## Uplink CDF plotting ##################
        fig = plt.figure(figsize=(5, 3))
        ax = fig.add_subplot(111)
        fig.tight_layout()

        ax.set_xlabel("Throughput Imbalance (MAX-MIN)/AVG (%)", fontsize=11.5)
        ax.set_ylabel("CDF", fontsize=11.5)
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.yaxis.set_ticks_position('left')
        ax.xaxis.set_ticks_position('bottom')
        
        lbmode_order = ["fecmp", "conga", "letflow", "conweave"]
        for tgt_lbmode in lbmode_order:
            for vv in v:
                config_id = vv[0]
                lb_mode = vv[1]

                if lb_mode == tgt_lbmode:
                    # plotting
                    filename_uplink = output_dir + "/{id}/{id}_out_uplink.txt".format(id=config_id)
                    port_list = set()

                    with open(filename_uplink, "r") as f:
                        # parsing the results: (switch) -> timestamp
                        
                        history_data = {}
                        diff_data = {}
                        last_ts = 0
                        for line in f.readlines():
                            parsed_line = line.replace("\n", "").split(",")
                            now_ts = int(parsed_line[0])
                            now_swid = int(parsed_line[1])
                            now_portid = int(parsed_line[2])
                            now_val = int(parsed_line[3])
                            
                            if now_ts < time_start or now_ts > time_end:
                                continue

                            if last_ts == 0:
                                last_ts = now_ts
                            elif last_ts + time_interval <= now_ts:
                                last_ts = now_ts
                            elif last_ts == now_ts:
                                pass
                            else:
                                continue


                            key = (now_swid, now_portid)
                            port_list.add(now_portid)

                            if key not in history_data:
                                history_data[key] = now_val
                            else:
                                if key not in diff_data:
                                    diff_data[key] = [now_val - history_data[key]]
                                else:
                                    diff_data[key].append(now_val - history_data[key])
                                history_data[key] = now_val
                        
                        # clustering with switch_id
                        switch_diff_data = {}
                        for kkk, vvv in diff_data.items():
                            switch_id, port_id = kkk
                            if switch_id not in switch_diff_data:
                                switch_diff_data[switch_id] = [vvv]
                            else:
                                switch_diff_data[switch_id].append(vvv)
                            
                        ts_data_arr = []
                        for switch_id, vvvv in switch_diff_data.items():
                            v_t = np.array(vvvv).T.tolist()
                            for vec in v_t:
                                if np.average(vec) == 0:
                                    continue
                                val = (np.max(vec) - np.min(vec)) / np.average(vec) * 100
                                ts_data_arr.append(val)

                        cdf_ts_data_arr = getCdfFromArray(ts_data_arr)
                        
                        ax.plot([x[0] for x in cdf_ts_data_arr],
                                [x[3] for x in cdf_ts_data_arr],
                                markersize=0,
                                linewidth=3.0,
                                label="{}".format(lb_mode))
        
        ax.legend(frameon=False, fontsize=12, facecolor='white')
        
        ax.grid(which='minor', alpha=0.2)
        ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.pdf".format("CDF_UPLINK_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()
            



        # ################## P99 plotting ##################
        # fig = plt.figure(figsize=(4, 4))
        # ax = fig.add_subplot(111)
        # fig.tight_layout()

        # ax.set_xlabel("Flow Size (Bytes)", fontsize=11.5)
        # ax.set_ylabel("p99 FCT Slowdown", fontsize=11.5)

        # ax.spines['top'].set_visible(False)
        # ax.spines['right'].set_visible(False)
        # ax.yaxis.set_ticks_position('left')
        # ax.xaxis.set_ticks_position('bottom')
        
        # xvals = [i for i in range(STEP, 100 + STEP, STEP)]

        # lbmode_order = ["fecmp", "conga", "letflow", "conweave"]
        # for tgt_lbmode in lbmode_order:
        #     for vv in v:
        #         config_id = vv[0]
        #         lb_mode = vv[1]

        #         if lb_mode == tgt_lbmode:
        #             # plotting
        #             fct_slowdown = output_dir + "/{id}/{id}_out_fct.txt".format(id=config_id)
        #             result = get_steps_from_raw(fct_slowdown, int(time_start), int(time_end), STEP)
                    
        #             ax.plot(xvals,
        #                 result["p99"],
        #                 markersize=1.0,
        #                 linewidth=3.0,
        #                 label="{}".format(lb_mode))
                
        # ax.legend(bbox_to_anchor=(0.0, 1.2), loc="upper left", borderaxespad=0,
        #         frameon=False, fontsize=12, facecolor='white', ncol=2,
        #         labelspacing=0.4, columnspacing=0.8)
        
        # ax.tick_params(axis="x", rotation=40)
        # ax.set_xticks(([0] + xvals)[::2])
        # ax.set_xticklabels(([0] + size2str(result["size"]))[::2], fontsize=10.5)
        # ax.set_ylim(bottom=1)
        # # ax.set_yscale("log")

        # fig.tight_layout()
        # ax.grid(which='minor', alpha=0.2)
        # ax.grid(which='major', alpha=0.5)
        # fig_filename = fig_dir + "/{}.pdf".format("P99_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        # print(fig_filename)
        # plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        # plt.close()
            

    


    



if __name__=="__main__":
    setup()
    main()
