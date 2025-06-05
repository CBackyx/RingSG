import subprocess
import sys
import os
from threading import Thread
import psutil
import time
import pandas as pd
import shutil
import argparse

executable_root_path = "./../"
data_root_path = "./data/"
ferret_ot_data_root_path = "./ot-data/"
result_root_path = "./result/"
log_root_path = "./log/"
config_root_path = "./config/"
preprocess_root_path = "./preprocess/"
comm_root_path = "./comm/"
# iterations = 120
iterations = 4
defaultNumParts = 2
doPreprocess = False
defaultBandWidth = 400 #Mbps
defaultLatency = 1 #ms
isCluster = True
defaultInterRatio = 0.4
defaultScaler = 0.2
bandwidthList = [200, 400, 1000, 4000]
latencyList = [0.15, 1, 10, 20]

demoCommand = False

def my_makedir(path):
    if not os.path.isdir(path):
        os.makedirs(path)

def delete_and_create_dir(dir_path):
    # check if the directory already exists
    if os.path.exists(dir_path):
        # delete the existing directory
        shutil.rmtree(dir_path)
    # create a new directory
    os.makedirs(dir_path)

# print(process.stdout.read().decode('utf-8'))

# python batch_Deckard.py > test_case/batch_similarity.txt

# ./sssp-ss -t 5 -g 5 -i 4 -m 2 -p 1 ./../data/test.dat ./../data/test5p.part ./../data/result_4.txt 0

processList = []

def setup_network(bandwidth, latency):
    # Clean first
    curProc = subprocess.Popen(["sudo", "bash", "./scripts/clean_network.sh"])
    curProc.wait()
    # Setup network
    curProc = subprocess.Popen(["sudo", "bash", "./scripts/setup_network.sh", str(bandwidth), str(latency)])
    curProc.wait()

def clean_network():
    curProc = subprocess.Popen(["sudo", "bash", "./scripts/clean_network.sh"])
    curProc.wait()

nsList = ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J"]

def get_size(bytes):
    """
    Returns size of bytes in a nice format
    """
    # for unit in ['', 'K', 'M', 'G', 'T', 'P']:
    #     if bytes < 1024:
    #         return f"{bytes:.2f}{unit}B"
    #     bytes /= 1024
    bytes /= 1024
    bytes /= 1024
    return f"{bytes:.2f}MB"

def commStart():
    io = psutil.net_io_counters(pernic=True)
    return io

def commEnd(io, doPreprocess, scaler, tag=""):
    # get the network I/O stats again per interface 
    io_2 = psutil.net_io_counters(pernic=True)
    # initialize the data to gather (a list of dicts)
    data = []
    for iface, iface_io in io.items():
        # new - old stats gets us the speed
        upload, download = io_2[iface].bytes_sent - iface_io.bytes_sent, io_2[iface].bytes_recv - iface_io.bytes_recv
        data.append({
            "iface": iface, "Download": get_size(download),
            "Upload": get_size(upload),
        })
    # construct a Pandas DataFrame to print stats in a cool tabular style
    df = pd.DataFrame(data)
    # sort values per column, feel free to change the column
    df.sort_values("Download", inplace=True, ascending=False)
    # clear the screen based on your OS
    # os.system("cls") if "nt" in os.name else os.system("clear")
    # print the stats
    # print(df.to_string())
    my_makedir(comm_root_path)
    with open(comm_root_path + str(doPreprocess) + "preprocess_" + str(scaler) + "scaler" + tag + ".comm", "w", encoding='utf-8') as ofile:
        ofile.write(df.to_string())

def commMeaStart():
    io = psutil.net_io_counters(pernic=True)
    return io

def commMeaEnd(io, comm_file_path=""):
    # get the network I/O stats again per interface 
    io_2 = psutil.net_io_counters(pernic=True)
    # initialize the data to gather (a list of dicts)
    data = []
    for iface, iface_io in io.items():
        # new - old stats gets us the speed
        upload, download = io_2[iface].bytes_sent - iface_io.bytes_sent, io_2[iface].bytes_recv - iface_io.bytes_recv
        data.append({
            "iface": iface, "Download": get_size(download),
            "Upload": get_size(upload),
        })
    # construct a Pandas DataFrame to print stats in a cool tabular style
    df = pd.DataFrame(data)
    # sort values per column, feel free to change the column
    df.sort_values("Download", inplace=True, ascending=False)
    # clear the screen based on your OS
    # os.system("cls") if "nt" in os.name else os.system("clear")
    # print the stats
    # print(df.to_string())
    with open(comm_file_path, "w", encoding='utf-8') as ofile:
        ofile.write(df.to_string())

def set_root_paths(application):
    global executable_root_path, data_root_path, ferret_ot_data_root_path, result_root_path, log_root_path, config_root_path, preprocess_root_path, comm_root_path
    data_root_path = f"./{str(application)}/data/"
    result_root_path = f"./{str(application)}/result/"
    log_root_path = f"./{str(application)}/log/"
    config_root_path = "./config/"
    comm_root_path = f"./{str(application)}/comm/"

def run_graph_processing_with_limited_cores(scheme, net_cond, num_parts, scale, avgDegree, interRatio, alg, iterations, ablation_tag='', max_retries=3):
    
    for attempt in range(max_retries):
        cur_bandwidth = net_cond[0]
        cur_latency = net_cond[1]
        if isCluster:
            setup_network(cur_bandwidth, cur_latency)

        log_path = log_root_path + "executable_" + str(scheme) + "/net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]) + "/num_parts_" + str(num_parts) + "/scale_" + str(scale) + "/avgDegree_" + str(avgDegree) + "/interRatio_" + str(interRatio) + "/alg_" + str(alg) + "/iters_" + str(iterations) + "/"
        my_makedir(log_path)

        executable_path = executable_root_path + "./out/build/linux/demo/demo" + ablation_tag
        processList = []
        start_io = commMeaStart()
        for i in range(num_parts):
            cmd = ["sudo", "ip", "netns", "exec", nsList[i]]
            cmd += ["taskset", "--cpu-list"]
            cmd += [str(i * 3) + "-" + str((i + 1) * 3 - 1)]
            cmd += [executable_path, str(scheme), str(num_parts), str(i), str(scale), str(avgDegree), str(interRatio), str(alg), str(iterations), str(os.path.abspath(log_path)), demoCommand]
            print(" ".join(cmd))
            log_f = open(log_path+"efficiency_"+str(i)+".log", 'w', encoding='utf-8')
            processList.append(subprocess.Popen(cmd, stdout=log_f))
        all_success = True
        for process in processList:
            process.wait()
            if process.returncode != 0 and process.returncode != 255:
                all_success = False
                print(f"Process {process} failed with return code {process.returncode}")
        
        if all_success:
            comm_log_path = comm_root_path + "executable_" + str(scheme) + "/net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]) + "/num_parts_" + str(num_parts) + "/scale_" + str(scale) + "/avgDegree_" + str(avgDegree) + "/interRatio_" + str(interRatio) + "/alg_" + str(alg) + "/iters_" + str(iterations) + "/"
            my_makedir(comm_log_path)
            commMeaEnd(start_io, comm_log_path + "comm")
            processList = []
            print(f"SUCCESS")
            return
        else:
            print(f"Retrying in 3 seconds...")
            time.sleep(3)

    print(f"Max retries ({max_retries}) exceeded. Giving up.")
    return False

def run_graph_processing_debug(scheme, net_cond, num_parts, scale, alg, iterations):
    cur_bandwidth = net_cond[0]
    cur_latency = net_cond[1]
    if isCluster:
        setup_network(cur_bandwidth, cur_latency)

    log_path = log_root_path + "executable_" + str(scheme) + "/net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]) + "/num_parts_" + str(num_parts) + "/scale_" + str(scale) + "/alg_" + str(alg) + "/iters_" + str(iterations) + "/"
    my_makedir(log_path)

    executable_path = executable_root_path + "./out/build/linux/eval/eval"
    processList = []
    start_io = commMeaStart()
    for i in range(num_parts):
        # if i == 0:
        #     continue
        cmd = ["sudo", "ip", "netns", "exec", nsList[i]]
        cmd += [executable_path, str(scheme), str(num_parts), str(i), str(scale), str(alg), str(iterations)]
        print(" ".join(cmd))
        log_f = open(log_path+"efficiency_"+str(i)+".log", 'w', encoding='utf-8')
        processList.append(subprocess.Popen(cmd, stdout=log_f))
    for process in processList:
        process.wait()
    comm_log_path = comm_root_path + "executable_" + str(scheme) + "/net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]) + "/num_parts_" + str(num_parts) + "/scale_" + str(scale) + "/alg_" + str(alg) + "/iters_" + str(iterations) + "/"
    my_makedir(comm_log_path)
    commMeaEnd(start_io, comm_log_path + "comm")
    processList = []

def demo():
    global iterations
    global isCluster

    isCluster = True

    set_root_paths("log/demo")

    print("##<------------>##")

    list_schemes = [0]
    list_net_conds = [(4000, 1)]
    list_num_parts = [8]
    list_scales = [3]
    list_avgDegrees = [3]
    list_interRatios = [0.6]
    list_algs = [0]
    iterations = 2

    for cur_scheme in list_schemes:
        for cur_net_cond in list_net_conds:
            for cur_num_parts  in list_num_parts:
                for cur_scale in list_scales:
                    for cur_avgDegree in list_avgDegrees:
                        for cur_interRatio in list_interRatios:
                            for cur_alg in list_algs:
                                run_graph_processing_with_limited_cores(cur_scheme, cur_net_cond, cur_num_parts, cur_scale, cur_avgDegree, cur_interRatio, cur_alg, iterations)

# Define the main function to parse command line arguments and call the appropriate functions
def main():

    global demoCommand

    parser = argparse.ArgumentParser(description='Demo RingSG for Connected Component Labeling')
    parser.add_argument('--demo', action='store_true', help='Demo')
    parser.add_argument('--command', type=str, help='Demo command')
    args = parser.parse_args()

    demoCommand = args.command

    if args.demo:
        demo()

if __name__ == "__main__":
    main()