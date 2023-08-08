# NS-3 Simulator for RDMA Network Load Balancing

This is a Github repository for the SIGCOMM'23 paper "[Network Load Balancing with In-network Reordering Support for RDMA](https://doi.org/10.1145/3603269.3604849)".

## Guide Using Docker

#### Docker Engine
For Ubuntu, following the installation guide [here](https://docs.docker.com/engine/install/ubuntu/) and make sure to apply the necessary post-install [steps](https://docs.docker.com/engine/install/linux-postinstall/).

Eventually, you should be able to launch the `hello-world` Docker container without the `sudo` command: `docker run hello-world`.

### 0. Prerequisites
First, you do all these:

```shell
wget https://www.nsnam.org/releases/ns-allinone-3.29.tar.bz2
tar -xvf ns-allinone-3.29.tar.bz2
cd ns-allinone-3.29
rm -rf ns-3.29
git clone https://github.com/conweave-project/conweave-ns3.git ns-3.29
```

### 1. Create a Dockerfile
Here, `ns-allinone-3.29` will be your root directory.

Create a Dockerfile at the root directory with the following:
```shell
FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y gnuplot python python3 python3-pip build-essential libgtk-3-0 bzip2 wget git && rm -rf /var/lib/apt/lists/* && pip3 install install numpy matplotlib cycler
WORKDIR /root
```

Then, you do this: 
```shell
docker build -t cw-sim:sigcomm23ae .
```

Once the container is built, do this from the root directory:
```shell
docker run -it -v $(pwd):/root cw-sim:sigcomm23ae bash -c "cd ns-3.29; ./waf configure --build-profile=optimized; ./waf"
```

This should build everything necessary for the simulator.

### 2. Run
One can always just run the container: 
```shell
docker run -it -v $(pwd):/root cw-sim:sigcomm23ae
```

Then, manually `cd ns-3.29`, and do `./autorun.sh` step by step following your README.
That will run `0.1 second` simulation of 8 experiments which are a part of Figure 12 and 13 in the paper.
In the script, you can easily change the network load (e.g., `50%`), runtime (e.g., `0.1s`), or topology (e.g., `leaf-spine`).
To plot the FCT graph, see below or refer to the script `./analysis/plot_fct.py`.



## Guide To Run NS-3 Locally
### 0. Prerequisites
We tested the simulator on Ubuntu 20.04, but latest versions of Ubuntu should also work.
```shell
sudo apt install build-essential python3 libgtk-3-0 bzip2
```
For plotting, we use `numpy`, `matplotlib`, and `cycler` for python3:
```shell
python3 -m pip install numpy matplotlib cycler
```


### 1. Configure & Build
```shell
wget https://www.nsnam.org/releases/ns-allinone-3.29.tar.bz2
tar -xvf ns-allinone-3.29.tar.bz2
cd ns-allinone-3.29
rm -rf ns-3.29
git clone https://github.com/conweave-project/conweave-ns3.git ns-3.29
cd ns-3.29
./waf configure --build-profile=optimized
./waf
```


### 2. Simulation
#### Run
You can reproduce the simulation results of Figure 12 and 13 (FCT slowdown) by running the script:
```shell
./autorun.sh
```

In the script, you can easily change the network load (e.g., `50%`), runtime (e.g., `0.1s`), or topology (e.g., `leaf-spine`).
This takes a few hours, and requires 8 CPU cores and 10G RAM.


If you want to run the simulation individually, try this command:
```shell
python3 ./run.py --h
```

It first calls a traffic generator `./traffic_gen/traffic_gen.py` to create an input trace.
Then, it runs NS-3 simulation script `./scratch/network-load-balance.cc`. 
Lastly, it runs FCT analyzer `./fctAnalysis.py` and switch resource analyzer `./queueAnalysis.py`. 


#### Plot
You can easily plot the results using the following command:
```shell
python3 ./analysis/plot_fct.py
```

The result figures are located at `./analysis/figures`. 
The script requires input parameters such as `-sT` and `-fT` which indicate the time window to analyze the fct result. 
By default, it assuems to use `0.1 second` runtime. 

#### Clean up
To clean all data of previous simulation results, you can run the command:
```shell
./cleanup.sh
```

#### Others
* At `./mix/output`, several raw data is stored such as (1) FCT, (2) PFC, (3) uplink's utility, (3) number of connections, and (5) CNP.

* Each run of simulation creates a repository in `./mix/output` with simulation ID (10-digit number).

* Inside the folder, you can check the simulation config `config.txt` and output log `config.log`. 

* The history of simulations will be recorded in `./mix/.history`. 


#### ConWeave Parameter
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
```shell
TBD
```

## Credit
This code repository is based on [https://github.com/alibaba-edu/High-Precision-Congestion-Control](https://github.com/alibaba-edu/High-Precision-Congestion-Control) for Mellanox Connect-X based RDMA-enabled NIC implementation, and [https://github.com/kaist-ina/ns3-tlt-rdma-public.git](https://github.com/kaist-ina/ns3-tlt-rdma-public.git) for Broadcom switch's shared buffer model and IRN implementation.



<!-- 
## NS-3 Simulator Structure

## Dependencies

### Ubuntu20.04 LTS on Docker
'''
docker run --name llvm_u2004 -dt ubuntu:20.04
docker ps
docker run --rm -it ubuntu:20.04 /bin/bash
docker exec -it ubuntu_20.04 /bin/bash 

adduser {username}
usermod -aG sudo {username}
apt update; apt upgrade
apt install sudo

su {username}
sudo groupadd docker
sudo gpasswd -a $USER docker

sudo apt update && sudo apt upgrade
sudo apt-get install git gcc g++ python python3 python-dev mercurial python-setuptools autoconf cvs bzr unrar cmake gdb valgrind uncrustify tcpdump automake -y
apt-get install python3-pip


sudo apt install openssh-server
sudo systemctl enable ssh
sudo systemctl start ssh (it not working, try service ssh start)
'''


### Python3
'''
python3 -m pip install numpy plotly==5.10.0 pandas
python3 -m pip install -U kaleido
''' -->
