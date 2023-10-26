import subprocess
import argparse
import numpy as np

def get_pctl(a, p):
	i = int(len(a) * p)
	return a[i]

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

if __name__=="__main__":
	parser = argparse.ArgumentParser(description='')
	parser.add_argument('-id', '--id', dest='id', required=True, action='store', help="traceId")
	parser.add_argument('-dir', '--dir', dest='dir', default='.', action='store', help="directory of run.py file, default='.'")
	parser.add_argument('-fdir', '--fdir', dest='fdir', default='mix', action='store', help="folder that the output files are located, default=mix")
	parser.add_argument('-bdp', dest='bdp', action='store', required=True, help="1 BDP of this topology, default=104000 (100G with 2-tier)")
	parser.add_argument('-sT', dest='time_limit_begin', action='store', type=int, default=2005000000, help="only consider flows that finish after T, default=2.005*10^9 ns")
	parser.add_argument('-fT', dest='time_limit_end', action='store', type=int, default=100000000000, help="only consider flows that finish before T, default=100 * 10^9 ns")
	
	args = parser.parse_args()

	config_ID = int(args.id)
	dirname = args.dir
	fdirname = args.fdir
	OneBDP = int(args.bdp)
	step = 5
	res = [[i/100.] for i in range(0, 100, step)]

	output_fct = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct.txt".format(id=config_ID)
	output_fct_summary = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_summary.txt".format(id=config_ID)
	output_fct_small_absolute_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_small_absolute_cdf.txt".format(id=config_ID)
	output_fct_small_slowdown_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_small_slowdown_cdf.txt".format(id=config_ID)
	output_fct_large_absolute_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_large_absolute_cdf.txt".format(id=config_ID)
	output_fct_large_slowdown_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_large_slowdown_cdf.txt".format(id=config_ID)
	output_fct_all_slowdown_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_all_slowdown_cdf.txt".format(id=config_ID)
	output_fct_all_absolute_cdf = dirname + "/" + fdirname + "/output/{id}/{id}_out_fct_all_absolute_cdf.txt".format(id=config_ID)

	# time interval to consider
	time_limit_start = args.time_limit_begin
	time_limit_end = args.time_limit_end

	# read lines
	file = "%s"%(output_fct)
	cmd_absolute = "cat %s"%(file) + " | awk '{if ($6>" + "%d"%time_limit_start + " && $6+$7<" + "%d"%(time_limit_end) + ") {print $7/1000, $5} }' | sort -n -k 2"
	print(cmd_absolute)
	output_absolute = subprocess.check_output(cmd_absolute, shell=True)
	cmd_slowdown = "cat %s"%(file) + " | awk '{if ($6>" + "%d"%time_limit_start + " && $6+$7<" + "%d"%(time_limit_end) + ") {print $7/$8<1?1:$7/$8, $5} }' | sort -n -k 2"
	print(cmd_slowdown)
	output_slowdown = subprocess.check_output(cmd_slowdown, shell=True)


	with open(output_fct_summary, "w") as outfile_fct_summary:

		################
		### SLOWDOWN ###
		################

		outfile_fct_summary.write("SLOWDOWN")
		aa = output_slowdown.decode("utf-8").split('\n')[:-2]
		nn = len(aa)

		fct_bdp = []
		fct_over_bdp = []
		for x in aa:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i < OneBDP):
				fct_bdp.append(val)
			else:
				fct_over_bdp.append(val)
		
		# BRIEF INFORMATION (<1BDP, >1BDP)
		outfile_fct_summary.write("#1BDP={}Bytes\n".format(OneBDP))
		outfile_fct_summary.write("#{:5},{:5},{:5},{:6},{:6},{:6}\n".format("Category", "Avg", "50%", "95%", "99%", "99.9%"))
		outfile_fct_summary.write("{:5},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f}\n".format("<1BDP", np.average(fct_bdp), 
																						np.percentile(fct_bdp, 50),
																						np.percentile(fct_bdp, 95),
																						np.percentile(fct_bdp, 99),
																						np.percentile(fct_bdp, 99.9)))
		outfile_fct_summary.write("{:5},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f}\n".format(">1BDP", np.average(fct_over_bdp), 
																						np.percentile(fct_over_bdp, 50),
																						np.percentile(fct_over_bdp, 95),
																						np.percentile(fct_over_bdp, 99),
																						np.percentile(fct_over_bdp, 99.9)))
		outfile_fct_summary.write("#\n#\n#\n#\n#\n")

		# CDF of FCT
		res = [[i/100.] for i in range(0, 100, step)]
		for i in range(0,100,step):
			l = int(i * nn / 100)
			r = int((i+step) * nn / 100)
			fct_size = aa[l:r]
			fct_size = [[float(x.split(" ")[0]), int(x.split(" ")[1])] for x in fct_size]
			fct = sorted(map(lambda x: x[0], fct_size))
			
			res[int(i/step)].append(fct_size[-1][1]) # flow size
			
			res[int(i/step)].append(sum(fct) / len(fct)) # avg fct
			res[int(i/step)].append(get_pctl(fct, 0.5)) # mid fct
			res[int(i/step)].append(get_pctl(fct, 0.95)) # 95-pct fct
			res[int(i/step)].append(get_pctl(fct, 0.99)) # 99-pct fct
			res[int(i/step)].append(get_pctl(fct, 0.999)) # 99-pct fct
		
		outfile_fct_summary.write("#{:5} {:3}\t{:5} {:5} {:6} {:6} {:6}\n".format("CDF", "Size", "Avg", "50%", "95%", "99%", "99.9%"))
		for item in res:
			line = "#%.3f %3d"%(item[0] + step/100.0, item[1])
			i = 1
			line += "\t{:.3f} {:.3f} {:.3f} {:.3f} {:.3f}\n".format(item[i+1], item[i+2], item[i+3], item[i+4], item[i+5])
			outfile_fct_summary.write(line)


		outfile_fct_summary.write("#\n#\n#\n#\n#\n")


		################
		### ABSOLUTE ###
		################

		outfile_fct_summary.write("ABSOLUTE")
		a = output_absolute.decode("utf-8").split('\n')[:-2]
		n = len(a)
		
		fct_bdp = []
		fct_over_bdp = []
		for x in a:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i < OneBDP):
				fct_bdp.append(val)
			else:
				fct_over_bdp.append(val)
		
		# BRIEF INFORMATION (<1BDP, >1BDP)
		outfile_fct_summary.write("#1BDP={}Bytes\n".format(OneBDP))
		outfile_fct_summary.write("#{:5},{:5},{:5},{:6},{:6},{:6}\n".format("Category", "Avg", "50%", "95%", "99%", "99.9%"))
		outfile_fct_summary.write("{:5},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f}\n".format("<1BDP", np.average(fct_bdp), 
																						np.percentile(fct_bdp, 50),
																						np.percentile(fct_bdp, 95),
																						np.percentile(fct_bdp, 99),
																						np.percentile(fct_bdp, 99.9)))
		outfile_fct_summary.write("{:5},{:.3f},{:.3f},{:.3f},{:.3f},{:.3f}\n".format(">1BDP", np.average(fct_over_bdp), 
																						np.percentile(fct_over_bdp, 50),
																						np.percentile(fct_over_bdp, 95),
																						np.percentile(fct_over_bdp, 99),
																						np.percentile(fct_over_bdp, 99.9)))
		outfile_fct_summary.write("#\n#\n#\n#\n#\n")

		# CDF of FCT
		res = [[i/100.] for i in range(0, 100, step)]
		for i in range(0,100,step):
			l = int(i * n / 100)
			r = int((i+step) * n / 100)
			fct_size = a[l:r]
			fct_size = [[float(x.split(" ")[0]), int(x.split(" ")[1])] for x in fct_size]
			fct = sorted(map(lambda x: x[0], fct_size))
			
			res[int(i/step)].append(fct_size[-1][1]) # flow size
			
			res[int(i/step)].append(sum(fct) / len(fct)) # avg fct
			res[int(i/step)].append(get_pctl(fct, 0.5)) # mid fct
			res[int(i/step)].append(get_pctl(fct, 0.95)) # 95-pct fct
			res[int(i/step)].append(get_pctl(fct, 0.99)) # 99-pct fct
			res[int(i/step)].append(get_pctl(fct, 0.999)) # 99-pct fct
		
		outfile_fct_summary.write("#{:5},{:6},{:6},{:6},{:7},{:7},{:7} >> scale: {}\n".format("CDF", "Size", "Avg", "50%", "95%", "99%", "99.9%", "us-scale"))
		for item in res:
			line = "#%.3f %3d"%(item[0] + step/100.0, item[1])
			i = 1
			line += "\t{:.3f} {:.3f} {:.3f} {:.3f} {:.3f}\n".format(item[i+1], item[i+2], item[i+3], item[i+4], item[i+5])
			outfile_fct_summary.write(line)

		outfile_fct_summary.write("#\n#EOF")



	with open(output_fct_all_slowdown_cdf, "w") as outfile_fct_all_slowdown:
		# up to here, `output` should be a string of multiple lines, each line is: fct, size
		aa = output_slowdown.decode("utf-8").split('\n')[:-2]
		print("output_slowdown number:", len(aa))
		########################
		### SLOWDOWN CDF ALL ###
		########################
		fct_arr = [float(x.split(" ")[0]) for x in aa]
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_all_slowdown.write(var)

	with open(output_fct_small_slowdown_cdf, "w") as outfile_fct_small_slowdown:
		# up to here, `output` should be a string of multiple lines, each line is: fct, size
		aa = output_slowdown.decode("utf-8").split('\n')[:-2]
		##########################
		### SLOWDOWN CDF SMALL ###
		##########################
		fct_arr = []
		for x in aa:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i < OneBDP):
				fct_arr.append(val)
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_small_slowdown.write(var)

	with open(output_fct_large_slowdown_cdf, "w") as outfile_fct_large_slowdown:
		# up to here, `output` should be a string of multiple lines, each line is: fct, size
		aa = output_slowdown.decode("utf-8").split('\n')[:-2]
		##########################
		### SLOWDOWN CDF LARGE ###
		##########################
		fct_arr = []
		for x in aa:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i >= OneBDP):
				fct_arr.append(val)
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_large_slowdown.write(var)

	
	with open(output_fct_all_absolute_cdf, "w") as outfile_fct_all_absolute:
		a = output_absolute.decode("utf-8").split('\n')[:-2]
		print("output_absolute number:", len(a))
		########################
		### ABSOLUTE CDF ALL ###
		########################
		fct_arr = [float(x.split(" ")[0]) for x in a]
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_all_absolute.write(var)
		
	with open(output_fct_small_absolute_cdf, "w") as outfile_fct_small_absolute:
		a = output_absolute.decode("utf-8").split('\n')[:-2]
		##########################
		### ABSOLUTE CDF SMALL ###
		##########################
		fct_arr = []
		for x in a:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i < OneBDP):
				fct_arr.append(val)
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_small_absolute.write(var)


	with open(output_fct_large_absolute_cdf, "w") as outfile_fct_large_absolute:
		a = output_absolute.decode("utf-8").split('\n')[:-2]
		##########################
		### ABSOLUTE CDF LARGE ###
		##########################
		fct_arr = []
		for x in a:
			i = int(x.split(" ")[1])
			val = float(x.split(" ")[0])
			if (i >= OneBDP):
				fct_arr.append(val)
		fct_cdf = getCdfFromArray(fct_arr)
		for bkt in fct_cdf:
			var = str(bkt[0]) + " " + str(bkt[1]) + " " + str(bkt[2]) + " " + str(bkt[3]) + "\n"
			outfile_fct_large_absolute.write(var)



