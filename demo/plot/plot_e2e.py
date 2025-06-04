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
app_base_dir = './../log/app/log'
prior_non_e2e_base_dir = './../log/prior-non-e2e/log'

markerList = ['P', '^', 'd', '*']

list_schemes = [3, 1, 2] # 0 for Ours
list_scheme_names = ["oursApp", "CoGNN", "GraphSC"]
list_scheme_formal_names = ["RingSG App", "CoGNN", "GraphSC"]
list_net_conds = [(4000, 1)]
list_num_parts = [8]
# list_scales = [12, 13, 14, 15, 16] # 2 ^ n
# list_scale_names = ["$2^{17}$", "$2^{18}$", "$2^{19}$", "$2^{20}$", "$2^{21}$"]

list_scales = [19] # 2 ^ n
list_scale_names = ["$2^{24}$"]
if isSmallest:
    list_scales = [10]
    list_scale_names = ["$2^{15}$"]

list_algs = [0, 1] # 0 for CC
list_alg_names = ["CC", "SP"]
iterations = 10
colors = ["#4B5F66", "#E39A56", "#BA5B59"]

# Traverse the log files (OGA ENABLED)

def parse_app_log(data: dict, net_cond):
    for executable_id, executable in enumerate(list_schemes):
        if executable_id < 1:
            base_path = app_base_dir
        else:
            base_path = prior_non_e2e_base_dir
        executable_name = list_scheme_names[executable_id]
        num_parts = list_num_parts[0]
        for scale_index in range(len(list_scales)):
            scale = list_scales[scale_index]
            scale_name = list_scale_names[scale_index]
            for alg_id, alg in enumerate(list_algs):     
                alg_name = list_alg_names[alg_id]
                # Initialize data structure for this algorithm if not already done
                if alg_name not in data:
                    data[alg_name] = {}
                if executable not in data[alg_name]:
                    data[alg_name][executable_name] = {'scale': [], 'prot-invoke duration': [], 'prot-invoke communication': [], 'total duration': [], 'total communication': [], 'result-extract duration': [], 'result-extract communication': []}
                
                # Read the log file
                with open(os.path.join(base_path, "executable_" + str(executable), "net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]), "num_parts_" + str(num_parts), "scale_" + str(scale), "alg_" + str(alg), "iters_" + str(iterations), "efficiency_0.log"), 'r') as f:
                    lines = f.readlines()
                    prot_duration = 0.0
                    prot_communication = 0.0
                    extract_duration = 0.0
                    duration = 0.0
                    communication = 0.0
                    for line in lines:
                        line = line.strip('\n')
                        if executable_name + ' took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            duration = float(segs[2])
                        elif executable_id < 1 and 'ProtocolInvocation took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            prot_duration = float(segs[2])
                        elif executable_id < 1 and 'ResultExtract took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            extract_duration = float(segs[2])
                        elif 'total:' in line:
                            communication_match = re.search(r'total: ([\d.]+)MB', line)
                            if communication_match:
                                communication = float(communication_match.group(1)) / 1024 # to GB
                        elif executable_id < 1 and 'prot-invoke:' in line:
                            communication_match = re.search(r'prot-invoke: ([\d.]+)MB', line)
                            if communication_match:
                                prot_communication = float(communication_match.group(1)) / 1024 # to GB                                               
                    
                    # Store the extracted data
                    data[alg_name][executable_name]['scale'].append(scale)
                    if executable_id < 1:
                        data[alg_name][executable_name]['total duration'].append(duration)
                        data[alg_name][executable_name]['total communication'].append(communication)
                        data[alg_name][executable_name]['prot-invoke duration'].append(prot_duration)
                        data[alg_name][executable_name]['prot-invoke communication'].append(prot_communication)
                        data[alg_name][executable_name]['result-extract duration'].append(extract_duration)
                        data[alg_name][executable_name]['result-extract communication'].append(communication - prot_communication)
                    else:
                        data[alg_name][executable_name]['prot-invoke duration'].append(duration)
                        data[alg_name][executable_name]['prot-invoke communication'].append(communication)

def dict_to_markdown_table(data):
    """
    Convert performance data dictionary to a Markdown table.
    
    Args:
        data (dict): Nested dictionary containing performance metrics.
    
    Returns:
        str: Markdown-formatted table string.
    """
    # Define task categories and their mappings to subtasks
    task_categories = {
        'CC': ('Detect Group Connection', {
            'prot-invoke duration': 'Protocol Invocation',
            'result-extract duration': 'Result Extraction',
            'total duration': 'Total'
        }),
        'SP': ('Trace Transfer Chain', {
            'prot-invoke duration': 'Protocol Invocation',
            'result-extract duration': 'Result Extraction',
            'total duration': 'Total'
        })
    }
    
    # Initialize Markdown table with headers
    markdown_table = "| Task Category | Subtask | oursApp | CoGNN | GraphSC |\n"
    markdown_table += "|---------------|---------|---------|-------|---------|\n"
    
    # Process each task category
    for category_key, (category_name, subtasks) in task_categories.items():
        if category_key not in data:
            continue
            
        # Process each subtask
        for metric_key, subtask_name in subtasks.items():
            # Start row with task category (only for the first subtask)
            category_cell = f"**{category_name}**" if metric_key == list(subtasks.keys())[0] else ""
            
            # Initialize row with category and subtask
            row = f"| {category_cell} | {subtask_name} | "
            
            # Add data for each model
            models = ['oursApp', 'CoGNN', 'GraphSC']
            model_data = []
            
            for model in models:
                if model in data[category_key] and metric_key in data[category_key][model]:
                    values = data[category_key][model][metric_key]
                    if values:
                        # Format value with placeholder for standard deviation
                        duration = values[0]
                        comm = data[category_key][model][metric_key.split(' ')[0] + ' communication'][0]
                        model_data.append(f"{duration:.4f} ({comm:.4f})")
                    else:
                        model_data.append("/")
                else:
                    model_data.append("/")
            
            # Complete the row
            row += " | ".join(model_data) + " |\n"
            markdown_table += row
    
    return markdown_table

# Initialize data structures to store the extracted information
app_data_lan = {}
parse_app_log(app_data_lan, list_net_conds[0])

print(">>>>TABLE 5:")
print("----")
print(dict_to_markdown_table(app_data_lan))
print("----")
