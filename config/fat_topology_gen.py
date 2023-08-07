k_fat = 12
oversubscript = 2 # over-subscription between ToR uplink - downlink
link_rate = 100 # Gbps
link_latency = 1000 # ns


assert(k_fat % 2 == 0)
print("Fat K : {}".format(k_fat))

n_core = int(k_fat / 2 * k_fat / 2)
n_pod = k_fat
print("Number of Core: {}".format(n_core))
print("Number of pods: {}".format(n_pod))

n_agg_per_pod = int(k_fat / 2)
print("Number of Agg per pod: {}, total: {}".format(n_agg_per_pod, n_agg_per_pod * k_fat))

n_tor_per_pod = int(k_fat / 2)
print("Number of ToR per pod: {}, total: {}".format(n_tor_per_pod, n_tor_per_pod * k_fat))

n_server_per_pod = int(k_fat / 2 * k_fat / 2 * oversubscript)
n_server_per_tor = int(k_fat / 2 * oversubscript)
print("Number of servers per ToR: {} (oversubscript:{})".format(n_server_per_tor, oversubscript))
print("Number of servers per pod: {}, total: {}".format(n_server_per_pod, n_server_per_pod * k_fat))


n_server_total = n_server_per_pod * k_fat
n_tor_total = n_tor_per_pod * k_fat
n_agg_total = n_tor_per_pod * k_fat
n_core_total = n_core

i_server = 0
i_tor = n_server_total
i_agg = n_server_total + n_tor_total
i_core = n_server_total + n_tor_total + n_agg_total


num_link = 0
filename = "fat_k{}_{}G_OS{}.txt".format(k_fat, link_rate, oversubscript)
with open(filename, "w") as f:

    for p in range(n_tor_total):
        for i in range(n_server_per_tor):
            id_server = p * n_server_per_tor + i
            id_tor = i_tor + p
            # print("{} {} {}Gbps {}ns 0.000000".format(id_server, id_tor, link_rate, link_latency))
            f.write("{} {} {}Gbps {}ns 0.000000\n".format(id_server, id_tor, link_rate, link_latency))
            num_link += 1

    for i in range(n_pod):
        for j in range(n_tor_per_pod):
            for l in range(n_agg_per_pod):
                id_tor = i_tor + i * n_tor_per_pod + j
                id_agg = i_agg + i * n_tor_per_pod + l
                # print("{} {} {}Gbps {}ns 0.000000".format(id_tor, id_agg, link_rate, link_latency))
                f.write("{} {} {}Gbps {}ns 0.000000\n".format(id_tor, id_agg, link_rate, link_latency))
                num_link += 1


    n_jump = int(k_fat / 2)
    for i in range(n_pod):
        for j in range(n_agg_per_pod):
            for l in range(int(k_fat / 2)):
                id_agg = i_agg + i * n_agg_per_pod + j
                id_core = i_core + j * n_jump + l
                # print("{} {} {}Gbps {}ns 0.000000".format(id_agg, id_core, link_rate, link_latency))
                f.write("{} {} {}Gbps {}ns 0.000000\n".format(id_agg, id_core, link_rate, link_latency))
                num_link += 1

def line_prepender(filename, line):
    with open(filename, "r+") as f:
        content = f.read()
        f.seek(0, 0)
        f.write(line.rstrip('\r\n') + '\n' + content)

num_total_node = n_server_total + n_tor_total + n_agg_total + n_core_total
num_total_switch = n_tor_total + n_agg_total + n_core_total
id_switch_all = ""
# second line
for i in range(num_total_switch):
    if i == num_total_switch - 1:
        id_switch_all += "{}\n".format(i + n_server_total)
    else:
        id_switch_all += "{} ".format(i + n_server_total)
line_prepender(filename, id_switch_all)

# first line
line_prepender(filename, "{} {} {}".format(num_total_node, num_total_switch, num_link))


# First line: total node #, switch node #, link #
# Second line: switch node IDs...
# src0 dst0 rate delay error_rate
# src1 dst1 rate delay error_rate




filename_trace = "fat_k{}_trace.txt".format(k_fat, link_rate, oversubscript)
with open(filename_trace, "w") as f:
    f.write("{}\n".format(n_server_total))

    for i in range(n_server_total):
        if i == n_server_total - 1:
            id_switch_all += "{}\n".format(i)
        else:
            id_switch_all += "{} ".format(i)




# Fat K : 4
# Number of Core: 4
# Number of pods: 4
# Number of Agg per pod: 2, total: 8
# Number of ToR per pod: 2, total: 8
# Number of servers per ToR: 4 (oversubscript:2)
# Number of servers per pod: 8, total: 32


# Fat K : 8
# Number of Core: 16
# Number of pods: 8
# Number of Agg per pod: 4, total: 32
# Number of ToR per pod: 4, total: 32
# Number of servers per ToR: 8 (oversubscript:2)
# Number of servers per pod: 32, total: 256


# Fat K : 16
# Number of Core: 64
# Number of pods: 16
# Number of Agg per pod: 8, total: 128
# Number of ToR per pod: 8, total: 128
# Number of servers per ToR: 16 (oversubscript:2)
# Number of servers per pod: 128, total: 2048