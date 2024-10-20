# NS-3 Simulator for RDMA Network Load Balancing

This is a Github repository for the SIGCOMM'23 paper "[Network Load Balancing with In-network Reordering Support for RDMA](https://doi.org/10.1145/3603269.3604849)".

We describe how to run this repository either on docker or using your local machine with `ubuntu:20.04`. 


## Run with Docker

#### Docker Engine
For Ubuntu, following the installation guide [here](https://docs.docker.com/engine/install/ubuntu/) and make sure to apply the necessary post-install [steps](https://docs.docker.com/engine/install/linux-postinstall/).
Eventually, you should be able to launch the `hello-world` Docker container without the `sudo` command: `docker run hello-world`.

#### 0. Prerequisites
First, you do all these:

```shell
wget https://www.nsnam.org/releases/ns-allinone-3.19.tar.bz2
tar -xvf ns-allinone-3.19.tar.bz2
cd ns-allinone-3.19
rm -rf ns-3.19
git clone https://github.com/conweave-project/conweave-ns3.git ns-3.19
```

#### 1. Create a Dockerfile
Here, `ns-allinone-3.19` will be your root directory.

Create a Dockerfile at the root directory with the following:
```shell
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y gnuplot python python3 python3-pip build-essential libgtk-3-0 bzip2 wget git && rm -rf /var/lib/apt/lists/* && pip3 install numpy matplotlib cycler
WORKDIR /root
```

Then, you do this: 
```shell
docker build -t cw-sim:sigcomm23ae .
```

Once the container is built, do this from the root directory:
```shell
docker run -it -v $(pwd):/root cw-sim:sigcomm23ae bash -c "cd ns-3.19; ./waf configure --build-profile=optimized; ./waf"
```

This should build everything necessary for the simulator.

#### 2. Run
One can always just run the container: 
```shell
docker run -it --name cw-sim -v $(pwd):/root cw-sim:sigcomm23ae 
cd ns-3.19;
./autorun.sh
```

That will run `0.1 second` simulation of 8 experiments which are a part of Figure 12 and 13 in the paper.
In the script, you can easily change the network load (e.g., `50%`), runtime (e.g., `0.1s`), or topology (e.g., `leaf-spine`).
To plot the FCT graph, see below or refer to the script `./analysis/plot_fct.py`.
To plot the Queue Usage graph, see below or refer to the script `./analysis/plot_queue.py`.

:exclamation: **To run processes in background**, use the commands:
```shell
docker run -dit --name cw-sim -v $(pwd):/root cw-sim:sigcomm23ae 
docker exec -it cw-sim /bin/bash

root@252578ceff68:~# cd ns-3.19/
root@252578ceff68:~/ns-3.19# ./autorun.sh
Running RDMA Network Load Balancing Simulations (leaf-spine topology)

----------------------------------
TOPOLOGY: leaf_spine_128_100G_OS2
NETWORK LOAD: 50
TIME: 0.1
----------------------------------

Run Lossless RDMA experiments...
Run IRN RDMA experiments...
Runing all in parallel. Check the processors running on background!
root@252578ceff68:~/ns-3.19# exit
exit
```

#### 3. Plot
You can easily plot the results using the following command:
```shell
python3 ./analysis/plot_fct.py
python3 ./analysis/plot_queue.py
python3 ./analysis/plot_uplink.py
```

See below for details of output results.




---

## Run NS-3 on Ubuntu 20.04
#### 0. Prerequisites
We tested the simulator on Ubuntu 20.04, but latest versions of Ubuntu should also work.
```shell
sudo apt install build-essential python3 libgtk-3-0 bzip2
```
For plotting, we use `numpy`, `matplotlib`, and `cycler` for python3:
```shell
python3 -m pip install numpy matplotlib cycler
```


#### 1. Configure & Build
```shell
wget https://www.nsnam.org/releases/ns-allinone-3.19.tar.bz2
tar -xvf ns-allinone-3.19.tar.bz2
cd ns-allinone-3.19
rm -rf ns-3.19
git clone https://github.com/conweave-project/conweave-ns3.git ns-3.19
cd ns-3.19
./waf configure --build-profile=optimized
./waf
```


#### 2. Simulation
##### Run
You can reproduce the simulation results of Figure 12 and 13 (FCT slowdown), Figure 16 (Queue usage per switch) by running the script:
```shell
./autorun.sh
```

In the script, you can easily change the network load (e.g., `50%`), runtime (e.g., `0.1s`), or topology (e.g., `leaf-spine`).
This takes a few hours, and requires 8 CPU cores and 10G RAM.
Note that we do not run `DRILL` since it takes too much time due to many out-of-order packets.


If you want to run the simulation individually, try this command:
```shell
python3 ./run.py --h
```

It first calls a traffic generator `./traffic_gen/traffic_gen.py` to create an input trace.
Then, it runs NS-3 simulation script `./scratch/network-load-balance.cc`. 
Lastly, it runs FCT analyzer `./fctAnalysis.py` and switch resource analyzer `./queueAnalysis.py`. 


##### Plot
You can easily plot the results using the following command:
```shell
python3 ./analysis/plot_fct.py
python3 ./analysis/plot_queue.py
python3 ./analysis/plot_uplink.py
```

The outcome figures are located at `./analysis/figures`. 
1. The script requires input parameters such as `-sT` and `-fT` which indicate the time window to analyze the fct result. 
By default, it assuems to use `0.1 second` runtime. 
2. `plot_fct.py` plots the Average and 99-percentile FCT result and give comparisons between frameworks. It excludes `5ms` of warm-up and `50ms` of cool-down period in measurements. You can control these numbers in `run.py`:
```python
fct_analysis_time_limit_begin = int(flowgen_start_time * 1e9) + int(0.005 * 1e9)  # warmup
fct_analysistime_limit_end = int(flowgen_stop_time * 1e9) + int(0.05 * 1e9)  # extra term
```
or, directly put parameters into `plot_fct.py`. Use `-h` for details. 
3. `plot_queue.py` plots the CDF of queue volume usage per switch for ConWeave. It excludes `5ms` of warm-up period, and cool-down period is not used as it would _underestimate_ the overhead. Similarly, you can control this number in `run.py`:
```python
queue_analysis_time_limit_begin = int(flowgen_start_time * 1e9) + int(0.005 * 1e9)  # warmup
queue_analysistime_limit_end = int(flowgen_stop_time * 1e9) # no extra term!!
```
or, directly put parameters into `plot_queue.py`. Use `-h` for details. 
4. `plot_uplink.py` plots the load balance efficiency with ToR uplink utility. By default, it captures uplink throughputs for every `100µs` and measure the variations. It excludes `5ms` of warm-up and `50ms` of cool-down period in measurements. 
Or, directly put parameters into `plot_uplink.py`. Use `-h` for details. 

##### Output
As well as above figures, other results are located at `./mix/output`, such as uplink usage (Figure 14), queue number usage per port (Figure 15), etc.

* At `./mix/output`, several raw data is stored such as 
  * Flow Completion Time (`XXX_out_fct.txt`), - Figure 12, 13
  * PFC generation (`XXX_out_pfc.txt`), 
  * Uplink's utility (`XXX_out_uplink.txt`), - Figure 14
  * Number of connections (`XXX_out_conn.txt`), 
  * Congestion Notification Packet (`XXX_out_cnp.txt`).
  * CDF of number of queues usage per egress port (`XXX_out_voq_per_dst_cdf.txt`). - Figure 15 
  * CDF of total queue memory overhead per switch (`XXX_out_voq_cdf.txt`). - Figure 16
  
* Each run of simulation creates a repository in `./mix/output` with simulation ID (10-digit number).
* Inside the folder, you can check the simulation config `config.txt` and output log `config.log`. 
* The output files include post-processed files such as CDF results.
* The history of simulations will be recorded in `./mix/.history`. 

##### Topology
To evaluate on fat-tree (K=8) topology, you can simply change the `TOPOLOGY` variable in `autorun.sh` to `fat_k8_100G_OS2`:
```shell
TOPOLOGY="leaf_spine_128_100G_OS2" # or, fat_k8_100G_OS2
```

##### Clean up
To clean all data of previous simulation results, you can run the command:
```shell
./cleanup.sh
```

#### ConWeave Parameters
We include ConWeave's parameter values into `./run.py` based on flow control model and topology.  


### Simulator Structure
Most implementations of network load balancing are located in the directory `./src/point-to-point/model`.

* `switch-node.h/cc`: Switching logic that includes a default multi-path routing protocol (e.g., ECMP) and DRILL.
* `switch-mmu.h/cc`: Ingress/egress admission control and PFC.
* `conga-routing.h/cc`: Conga routing protocol.
* `letflow-routing.h/cc`: Letflow routing protocol.
* `conweave-routing.h/cc`: ConWeave routing protocol.
* `conweave-voq.h/cc`: ConWeave in-network reordering buffer.
* `settings.h/cc`: Global variables for logging and debugging.
* `rdma-hw.h/cc`: RDMA-enable NIC behavior model.

<b> RNIC behavior model to out-of-order packet arrival </b>
As disussed in the paper, we observe that RNIC reacts to even a single out-of-order packet sensitively by sending CNP packet.
However, existing RDMA-NS3 simulator (HPCC, DCQCN, TLT-RDMA, etc) did not account for this.
In this simulator, we implemented that behavior in `rdma-hw.cc`.


## Citation
If you find this repository useful in your research, please consider citing:
```
@inproceedings{song2023conweave,
  title={Network Load Balancing with In-network Reordering Support for RDMA},
  author={Song, Cha Hwan and Khooi, Xin Zhe and Joshi, Raj and Choi, Inho and Li, Jialin and Chan, Mun Choon},
  booktitle={Proceedings of SIGCOMM},
  year={2023}
}
```

## Credit
This code repository is based on [https://github.com/alibaba-edu/High-Precision-Congestion-Control](https://github.com/alibaba-edu/High-Precision-Congestion-Control) for Mellanox Connect-X based RDMA-enabled NIC implementation, and [https://github.com/kaist-ina/ns3-tlt-rdma-public.git](https://github.com/kaist-ina/ns3-tlt-rdma-public.git) for Broadcom switch's shared buffer model and IRN implementation.

```
MIT License

Copyright (c) 2023 National University of Singapore

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
