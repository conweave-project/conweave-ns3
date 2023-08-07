#!/usr/bin/python3
import sys
import os
import subprocess
import signal
import json
import collections
from argparse import ArgumentParser

class Config:
    config_idx = 0
    def __init__(self, cmdline_name, script_param_name, type, default, choices=None):
        self.cmd_n = cmdline_name
        self.scr_n = script_param_name
        self.type = type
        self.default = default
        self.choices = choices
        self.uniq_name = "arg%d" % Config.config_idx
        Config.config_idx += 1

    def add_to_parser(self, parser: ArgumentParser):
        if self.choices is None:
            parser.add_argument("--{}".format(self.cmd_n), dest=self.uniq_name, type=self.type, default=self.default)
        else:
            parser.add_argument("--{}".format(self.cmd_n), dest=self.uniq_name, type=self.type, default=self.default, choices=self.choices)

configs = [
    Config("expID", "", str, ""),
    Config("tlt", "ENABLE_TLT", int, 1, (0, 1)),
    Config("pfc", "ENABLE_PFC", int, 0, (0, 1)),
    Config("irn", "ENABLE_IRN", int, 0, (0, 1)),
    Config("tlp", "ENABLE_TLP", int, 0, (0, 1)),
    Config("dctcp", "ENABLE_DCTCP", int, 1, (0, 1)),
    Config("numFlow", ["TCP_FLOW_TOTAL", "NUM_BG_FLOWS"], int, 10000),  # number of background flows.
    Config("sizeFgFlow", ["DCTCP_INCAST_SIZE", "INCAST_FLOW_SIZE"], int, 16000), #previously incast
    Config("jumbo", "JUMBO_PACKET", int, 0),
    Config("linkLoad", "LOAD", float, 0.4),
    Config("tltth", "TLT_MAXBYTES_UIP", int, 400000),
    Config("opt", "OPTIMIZE", int, 1056),
    Config("ratioFgFlow", "FOREGROUND_RATIO", float, 0.1),
    Config("minRto", "MIN_RTO", float, 4),
    Config("workload", ["TCP_FLOW_FILE", "HPCC_WORKLOAD"], str, "/ns-3.19/workloads/DCTCP_CDF.txt"),
    Config("FgFlowPerHost", "FOREGROUND_INCAST_FLOW_PER_HOST", int, 4),
    Config("staticRto", "USE_STATIC_RTO", int, 0, (0, 1)),
    Config("ccMode", "CC_MODE", int, 3),
    Config("irnNoBdpfc", "IRN_NO_BDPFC", int, 0, (0, 1)),
    Config("tcpInitialCwnd", "TCP_INITIAL_CWND", int, 10),
    Config("seed", "RANDOM_SEED", int, 1),
]

class GracefulKiller:
  def __init__(self, child_pid):
    signal.signal(signal.SIGINT, self.exit_gracefully)
    signal.signal(signal.SIGTERM, self.exit_gracefully)
    self.child_pid = child_pid

  def exit_gracefully(self, signum, frame):
    os.kill(self.child_pid, signal.SIGKILL)


class ConfigGenerator:
    def __init__(self):
        self.parser = ArgumentParser()

        # These config will not be changed...
        # Config("incastSender", "DCTCP_SENDER_CNT", int, 31),
        # self.parser.add_argument("--flows", dest="flows", default=0) #flows per sender
        # self.parser.add_argument("--ascii", dest="ascii")
        # self.parser.add_argument("--pcap", dest="pcap")
        # self.parser.add_argument("--tltth", dest="tltth")
        # self.parser.add_argument("--optrdma", dest="opt_rdma")
        # self.parser.add_argument("--flowfile", dest="flowfile")
        for config in configs:
            config.add_to_parser(self.parser)
        
        self.parser.add_argument("-o", "--out", dest="out")
        self.parser.add_argument("-i", "--in", dest="input")
        self.parser.add_argument("--dry-run", action="store_true")

    def generate(self):
        (options, _) = self.parser.parse_known_args()
        setting = []
        template_name = "template.config"
        if options.input:
            template_name = options.input

        print("Using %s as template" %template_name, file=sys.stderr)
        with open(template_name, "r") as f:
            setting = f.readlines()

        def replace_setting (keyword, value):
            for i in range(len(setting)):
                if setting[i].startswith(keyword) and value is not None:
                    setting[i] = "{} {}\n".format(keyword, value)
                    break
        def __replace_setting (keyword, value):
            if value:
                replace_setting(keyword, value)
                
        self.arg_dict = {}
        for config in configs:
            if isinstance(config.scr_n, collections.abc.Sequence) and not isinstance(config.scr_n, str):
                ## multiple settings
                for scr_n in config.scr_n:
                    replace_setting(scr_n, getattr(options, config.uniq_name))
            else:
                if config.scr_n != "":
                    replace_setting(config.scr_n, getattr(options, config.uniq_name))
            self.arg_dict[config.cmd_n] = getattr(options, config.uniq_name)


        print("==========================ARG")
        print(json.dumps(self.arg_dict))
        print("=============================")

        if options.dry_run:
            print("".join(setting))
            print(json.dumps(self.arg_dict), file=sys.stderr)
            sys.exit(0)

        if options.out:
            with open(options.out, "w") as f:
                f.write("".join(setting))

        return "".join(setting)


def generate_script():
    '''
    Generate script and return the path of the script.
    '''
    argvs = sys.argv[1:]
    newfn = ["bgfg"]
    fn_tmp = ''
    for itm in argvs:
        if '--' in itm:
            if fn_tmp != '': newfn.append(fn_tmp)
            fn_tmp = itm.replace('--', '').replace('-', '').replace('totalflow', 'nfl').replace('incastsize', 'incsz').replace('tcpflowfile', 'flfl')
        else:
            if '/' in itm:
                fn_tmp += str(itm.split('/')[-1].split('.')[0])
            else:
                fn_tmp += str(itm)

    if fn_tmp is not None: newfn.append(fn_tmp)
    fn_p = "_".join(newfn)  
    fn = fn_p + ".txt"

    config_gen = ConfigGenerator()
    config_txt = config_gen.generate()

    with open(fn, 'w') as f:
        f.write(config_txt)
    with open('auto_alias', 'w') as f:
        f.write(fn_p)

    
    return fn


if __name__ == '__main__':
    print("Running TCP-flavored TLT ns-3 simulator...")
    script_path = generate_script()
    my_env = os.environ.copy()
    my_env["LD_LIBRARY_PATH"] = os.path.join(os.curdir, "build")
    os.system("ulimit -c unlimited")
    cmdline = [os.path.join(os.curdir,"build/scratch/hpcc-realistic-workload-bgfg"), script_path]
    proc = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=my_env)
    killer = GracefulKiller(proc.pid)
    (stdoutdata, stderrdata) = proc.communicate()
    
    # report = {
    #     'retcode': proc.returncode,
    #     'stdout': stdoutdata.decode('utf-8'),
    #     'stderr': stderrdata.decode('utf-8')
    # }
    
    print(stdoutdata.decode('utf-8'), end='', file=sys.stdout)
    print(stderrdata.decode('utf-8'), end='', file=sys.stderr)
    sys.exit(proc.returncode)