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
base_dir = './../log/efficiency/log'
no_oga_base_dir = './../log/remove-oga/log'
no_oep_base_dir = './../log/remove-oep/log'

markerList = ['P', '^', 'd', '*']

list_schemes = [0, 1, 2] # 0 for Ours
list_scheme_names = ["ours", "CoGNN", "GraphSC"]
list_scheme_formal_names = ["RingSG", "CoGNN", "GraphSC"]
list_net_conds = [(4000, 1), (200, 10)]
list_num_parts = [8]
# list_scales = [12, 13, 14, 15, 16] # 2 ^ n
# list_scale_names = ["$2^{17}$", "$2^{18}$", "$2^{19}$", "$2^{20}$", "$2^{21}$"]
list_scales = [16] # 2 ^ n
list_scale_names = ["$2^{21}$"]
if isSmallest:
    list_scales = [10]
    list_scale_names = ["$2^{15}$"]
    
list_algs = [0, 1, 2] # 0 for CC
list_alg_names = ["CC", "SP", "PR"]
iterations = 5
colors = ["#4B5F66", "#E39A56", "#BA5B59"]

# Traverse the log files (OGA ENABLED)

def parse_log(data: dict, net_cond, base_path: str):
    for executable in list_schemes:
        num_parts = list_num_parts[0]
        for scale_index in range(len(list_scales)):
            scale = list_scales[scale_index]
            scale_name = list_scale_names[scale_index]
            for alg in list_algs:            
                # Initialize data structure for this algorithm if not already done
                if alg not in data:
                    data[alg] = {}
                if executable not in data[alg]:
                    data[alg][executable] = {'scale': [], 'duration': [], 'Scatter duration': [], 'Gather duration': [], 'communication': []}
                
                # Read the log file
                with open(os.path.join(base_path, "executable_" + str(executable), "net_cond_" + str(net_cond[0]) + "_" + str(net_cond[1]), "num_parts_" + str(num_parts), "scale_" + str(scale), "alg_" + str(alg), "iters_" + str(iterations), "efficiency_0.log"), 'r') as f:
                    lines = f.readlines()
                    duration = 0.0
                    Scatter_duration = 0.0
                    Gather_duration = 0.0
                    communication = 0.0
                    for line in lines:
                        line = line.strip('\n')
                        if list_scheme_names[executable] + ' took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            duration = float(segs[2])
                        elif 'Scatter took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            Scatter_duration = float(segs[2])
                        elif 'Gather took ' in line:
                            segs = line.split(" ")
                            segs = [seg for seg in segs if seg != ""]
                            Gather_duration = float(segs[2])
                        else:
                            communication_match = re.search(r'total: ([\d.]+)MB', line)
                            if communication_match:
                                communication = float(communication_match.group(1))                        
                    
                    # Store the extracted data
                    data[alg][executable]['scale'].append(scale)
                    data[alg][executable]['duration'].append(duration/iterations)
                    data[alg][executable]['Scatter duration'].append(Scatter_duration)
                    data[alg][executable]['Gather duration'].append(Gather_duration)
                    data[alg][executable]['communication'].append(communication/iterations)

# Initialize data structures to store the extracted information
oga_data_lan = {}
no_oga_data_lan = {}
no_oep_data_lan = {}

oga_data_wan = {}
no_oga_data_wan = {}
no_oep_data_wan = {}

parse_log(oga_data_lan, list_net_conds[0], base_path=base_dir)
parse_log(no_oga_data_lan, list_net_conds[0], base_path=no_oga_base_dir)
parse_log(no_oep_data_lan, list_net_conds[0], base_path=no_oep_base_dir)

parse_log(oga_data_wan, list_net_conds[1], base_path=base_dir)
parse_log(no_oga_data_wan, list_net_conds[1], base_path=no_oga_base_dir)
parse_log(no_oep_data_wan, list_net_conds[1], base_path=no_oep_base_dir)

# print("OGA ENABLED:")
# print("----")
# print(oga_data)
# print("----")
# print("OGA DISABLED:")
# print("----")
# print(no_oga_data)
# print("----")
# exit(-1)

def generate_table_main_body(data_mat):
    """
    Generate a formatted Markdown table with fixed headers from numerical data.
    
    Args:
        data_mat (list): 2D list of numerical data without headers. Each row corresponds to:
                         [sys_scatter, sys_gather, sys_total, cog_scatter, cog_gather, cog_total]
    
    Returns:
        str: Formatted Markdown table with headers and bold formatting.
    """
    # Fixed headers and row labels
    column_headers = [
        "RingSG Scatter", "RingSG Gather (OGA)", "RingSG Total", 
        "CoGNN Scatter (OGA)", "CoGNN Gather", "CoGNN Total"
    ]
    row_labels = ["CC", "SP", "PR"]
    
    # Format data into table rows with bolding
    table_data = []
    for i, row in enumerate(data_mat):
        if len(row) != 6:
            raise ValueError("Each data row must contain exactly 6 values.")
        
        formatted_row = [f"{val}" for val in row]
        
        # Bold sys_total and sys_gather columns
        formatted_row[1] = f"**{formatted_row[1]}**"  # sys_gather
        formatted_row[2] = f"**{formatted_row[2]}**"  # sys_total
        
        table_data.append([row_labels[i]] + formatted_row)
    
    # Generate Markdown table
    header_row = [""] + [f"**{h}**" for h in column_headers]
    markdown_table = [
        "| " + " | ".join(header_row) + " |",
        "| --- | " + " | ".join(["---:" for _ in range(len(column_headers))]) + " |"
    ]
    
    for row in table_data:
        markdown_table.append("| " + " | ".join(row) + " |")
    
    return "\n".join(markdown_table)

def generate_table_appendix(data_mat):
    """
    Generate a formatted Markdown table with fixed headers from numerical data.
    
    Args:
        data_mat (list): 2D list of numerical data without headers. Each row corresponds to:
                         [sys_scatter, sys_gather, sys_total, cog_scatter, cog_gather, cog_total]
    
    Returns:
        str: Formatted Markdown table with headers and bold formatting.
    """
    # Fixed headers and row labels
    column_headers = [
        "RingSG OEP", "RingSG OGA", "RingSG the Rest (Merge + share redistribution/conversion)", "RingSG Total",
        "CoGNN OEP", "CoGNN OGA", "CoGNN the Rest (Merge + share redistribution)", "CoGNN Total"
    ]
    row_labels = ["CC", "SP", "PR"]
    
    # Format data into table rows with bolding
    table_data = []
    for i, row in enumerate(data_mat):
        if len(row) != 8:
            raise ValueError("Each data row must contain exactly 8 values.")
        
        formatted_row = [f"{val}" for val in row]
        
        # Bold sys_total and sys_gather columns
        formatted_row[1] = f"**{formatted_row[1]}**"  # sys_gather
        formatted_row[2] = f"**{formatted_row[2]}**"  # sys_total
        
        table_data.append([row_labels[i]] + formatted_row)
    
    # Generate Markdown table
    header_row = [""] + [f"**{h}**" for h in column_headers]
    markdown_table = [
        "| " + " | ".join(header_row) + " |",
        "| --- | " + " | ".join(["---:" for _ in range(len(column_headers))]) + " |"
    ]
    
    for row in table_data:
        markdown_table.append("| " + " | ".join(row) + " |")
    
    return "\n".join(markdown_table)

def data_format_print_main_body(oga_data: dict, no_oga_data: dict):
    data_mat = []
    for alg in list_algs:
        data_mat.append([])
        for executable in list_schemes[:2]:
            if executable == 0:
                data_mat[-1].append(oga_data[alg][executable]['Scatter duration'][0])
                data_mat[-1].append(f"{oga_data[alg][executable]['Gather duration'][0]} ({oga_data[alg][executable]['Gather duration'][0] - no_oga_data[alg][executable]['Gather duration'][0]})")
                data_mat[-1].append(oga_data[alg][executable]['duration'][0])
            else:
                data_mat[-1].append(f"{oga_data[alg][executable]['Scatter duration'][0]} ({oga_data[alg][executable]['Scatter duration'][0] - no_oga_data[alg][executable]['Scatter duration'][0]})")
                data_mat[-1].append(oga_data[alg][executable]['Gather duration'][0])
                data_mat[-1].append(oga_data[alg][executable]['duration'][0])                

    data_table = generate_table_main_body(data_mat)
    return data_table

def data_format_print_appendix(oga_data: dict, no_oga_data: dict, no_oep_data: dict):
    data_mat = []
    for alg in list_algs:
        data_mat.append([])
        for executable in list_schemes[:2]:
            if executable == 0:
                oep_du = oga_data[alg][executable]['Gather duration'][0] + oga_data[alg][executable]['Scatter duration'][0] - (no_oep_data[alg][executable]['Gather duration'][0] + no_oep_data[alg][executable]['Scatter duration'][0])
                oga_du = oga_data[alg][executable]['Gather duration'][0] - no_oga_data[alg][executable]['Gather duration'][0]
                other_du = oga_data[alg][executable]['Gather duration'][0] + oga_data[alg][executable]['Scatter duration'][0] - oep_du - oga_du
                data_mat[-1] += [oep_du, oga_du, other_du, oga_data[alg][executable]['duration'][0]]
            else:
                oep_du = oga_data[alg][executable]['Gather duration'][0] + oga_data[alg][executable]['Scatter duration'][0] - (no_oep_data[alg][executable]['Gather duration'][0] + no_oep_data[alg][executable]['Scatter duration'][0])
                oga_du = oga_data[alg][executable]['Scatter duration'][0] - no_oga_data[alg][executable]['Scatter duration'][0]
                other_du = oga_data[alg][executable]['Gather duration'][0] + oga_data[alg][executable]['Scatter duration'][0] - oep_du - oga_du
                data_mat[-1] += [oep_du, oga_du, other_du, oga_data[alg][executable]['duration'][0]]       

    data_table = generate_table_appendix(data_mat)
    return data_table

print(">>>>TABLE 3:")
print("----")
print(data_format_print_main_body(oga_data_lan, no_oga_data_lan))
print("----")

print(">>>>TABLE 4:")
print("----")
print(data_format_print_main_body(oga_data_wan, no_oga_data_wan))
print("----")

print(">>>>TABLE 6:")
print("----")
print(data_format_print_appendix(oga_data_lan, no_oga_data_lan, no_oep_data_lan))
print("----")

print(">>>>TABLE 7:")
print("----")
print(data_format_print_appendix(oga_data_wan, no_oga_data_wan, no_oep_data_wan))
print("----")
