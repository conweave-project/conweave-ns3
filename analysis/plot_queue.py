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
    parser = argparse.ArgumentParser(description='Plotting Queue Usage of results')
    args = parser.parse_args()
    
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

                    # filter only for ConWeave
                    if lb_mode != "conweave":
                        continue

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
        ################## Queue Storage ##################
        fig = plt.figure(figsize=(4, 4))
        ax = fig.add_subplot(111)
        ax.set_xlabel("Memory Usage (Packets)", fontsize=11.5)
        ax.set_ylabel("CDF", fontsize=11.5)
        
        for vv in v:
            config_id = vv[0]
            lb_mode = vv[1]
            filename_voq_volume = output_dir + "/{id}/{id}_out_voq_cdf.txt".format(id=config_id)
            data_voq_volume = {"x": [], "y": []}
            with open(filename_voq_volume, "r") as f:
                for line in f.readlines():
                    parsed_line = line.replace("\n","").split(" ")
                    data_voq_volume["x"].append(float(parsed_line[0]))
                    data_voq_volume["y"].append(float(parsed_line[3])) 

            ax.plot(data_voq_volume["x"],
                    data_voq_volume["y"],
                    markersize=0,
                    linewidth=3.0,
                    label="{}".format(lb_mode))
            
        ax.legend(bbox_to_anchor=(0.0, 1.2), loc="upper left", borderaxespad=0,
            frameon=False, fontsize=12, facecolor='white', ncol=2,
            labelspacing=0.4, columnspacing=0.8)
        
        fig.tight_layout()
        ax.grid(which='minor', alpha=0.2)
        ax.grid(which='major', alpha=0.5)
        fig_filename = fig_dir + "/{}.pdf".format("CDF_QUEUE_TOPO_{}_LOAD_{}_FC_{}".format(k[0], k[1], k[2]))
        print(fig_filename)
        plt.savefig(fig_filename, transparent=False, bbox_inches='tight')
        plt.close()
            



if __name__=="__main__":
    setup()
    main()
