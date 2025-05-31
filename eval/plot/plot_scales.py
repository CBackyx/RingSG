import os
import re
import matplotlib.pyplot as plt
import argparse

isSmallest = False
parser = argparse.ArgumentParser()
parser.add_argument('--smallest', action='store_true', help='Evaluate with smallest scale')
args = parser.parse_args()
isSmallest = args.smallest

# Define the base directory for the log files
base_dir = './../log/efficiency-scales/log'

markerList = ['P', '^', 'd', '*']

# Initialize data structures to store the extracted information
data = {}

list_schemes = [0, 1, 2] # 0 for Ours
list_scheme_names = ["ours", "CoGNN", "GraphSC"]
list_scheme_formal_names = ["RingSG", "CoGNN", "GraphSC"]
list_net_conds = [(4000, 1)]
list_num_parts = [8]
list_scales = [12, 13, 14, 15, 16] # 2 ^ n
list_scale_names = ["$2^{17}$", "$2^{18}$", "$2^{19}$", "$2^{20}$", "$2^{21}$"]
if isSmallest:
    list_scales = range(7, 12) # 2 ^ n
    list_scale_names = [f"$2^{{{x + 5}}}$" for x in list_scales]
list_algs = [0, 1, 2] # 0 for CC
list_alg_names = ["CC", "SP", "PR"]
iterations = 5
colors = ["#4B5F66", "#E39A56", "#BA5B59"]

# Traverse the log files
for executable in list_schemes:
    net_cond = list_net_conds[0]
    num_parts = list_num_parts[0]
    for scale_index in range(len(list_scales)):
        scale = list_scales[scale_index]
        scale_name = list_scale_names[scale_index]
        for alg in list_algs:            
            # Initialize data structure for this algorithm if not already done
            if alg not in data:
                data[alg] = {}
            if executable not in data[alg]:
                data[alg][executable] = {'scale': [], 'duration': [], 'communication': []}
            
            # Read the log file
            with open(os.path.join(base_dir, "executable_" + str(executable), "net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]), "num_parts_" + str(num_parts), "scale_" + str(scale), "alg_" + str(alg), "iters_" + str(iterations), "efficiency_0.log"), 'r') as f:
                lines = f.readlines()
                duration = 0.0
                communication = 0.0
                for line in lines:
                    line = line.strip('\n')
                    if list_scheme_names[executable] + ' took ' in line:
                        segs = line.split(" ")
                        segs = [seg for seg in segs if seg != ""]
                        duration = float(segs[2])
                    else:
                        communication_match = re.search(r'total: ([\d.]+)MB', line)
                        if communication_match:
                            communication = float(communication_match.group(1))                        
                
                # Store the extracted data
                data[alg][executable]['scale'].append(scale)
                data[alg][executable]['duration'].append(duration)
                data[alg][executable]['communication'].append(communication)

print(data)
# exit(-1)

# Plot the results
fig, axes = plt.subplots(3, 2, figsize=(8, 6))

for alg in list_algs:
    row = alg
    for executable in data[alg]:
        axes[row, 0].plot(data[alg][executable]['scale'], data[alg][executable]['duration'], markerList[executable], linewidth=1, ls='-', ms=8, color=colors[executable], label=f'{list_scheme_formal_names[executable]}')
        axes[row, 1].plot(data[alg][executable]['scale'], [x/1024 for x in data[alg][executable]['communication']], markerList[executable], linewidth=1, ls='-', ms=8, color=colors[executable], label=f'{list_scheme_formal_names[executable]}')
    print("Duration CoGNN/Ours = ", [x/y for x,y in zip(data[alg][1]['duration'], data[alg][0]['duration'])])
    print("Duration GraphSC/Ours = ", [x/y for x,y in zip(data[alg][2]['duration'], data[alg][0]['duration'])])
    print("Comm CoGNN/Ours = ", [x/y for x,y in zip(data[alg][1]['communication'], data[alg][0]['communication'])])
    print("Comm GraphSC/Ours = ", [x/y for x,y in zip(data[alg][2]['communication'], data[alg][0]['communication'])])
    
    axes[row, 0].set_title(f'Algorithm {list_alg_names[alg]} - Duration', fontsize=14)
    axes[row, 0].set_xlabel('Size of Global Graph', fontsize=12)
    axes[row, 0].set_xticks(list_scales, list_scale_names)
    axes[row, 0].set_ylabel('Running Time (s)', fontsize=12)
    axes[row, 0].legend(fontsize=11)
    axes[row, 0].tick_params(axis='both', which='major', labelsize=12)
    
    axes[row, 1].set_title(f'Algorithm {list_alg_names[alg]} - Communication', fontsize=14)
    axes[row, 1].set_xlabel('Size of Global Graph', fontsize=12)
    axes[row, 1].set_xticks(list_scales, list_scale_names)
    axes[row, 1].set_ylabel('Per-party Comm (GB)', fontsize=12)
    axes[row, 1].legend(fontsize=11)
    axes[row, 1].tick_params(axis='both', which='major', labelsize=12)

plt.tight_layout()
plt.savefig("fig/efficiency_scale.pdf")
plt.show()
