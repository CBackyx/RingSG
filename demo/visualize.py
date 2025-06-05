import os
import re
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
import argparse
from typing import List, Dict, Tuple, Set

vertex_to_party = {}  # Map each vertex to its party
vertex_data = {}

# Set up plotting parameters
plt.rcParams.update({
    "figure.figsize": (12, 12),
    "font.family": "sans-serif",
    "font.size": 10
})

def parse_party_file(filename: str) -> Tuple[List[int], Dict[int, int], List[Tuple[int, int]]]:
    """
    Parse a party's graph file and extract vertices, data, and edges
    
    Args:
        filename: Path to the party's graph file
    
    Returns:
        (vertex_ids, vertex_data, all_edges)
    """
    vertex_ids = []
    vertex_data = {}
    all_edges = []
    current_section = None

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Detect section headers
            if line.startswith("# Vertices"):
                current_section = "vertices"
            elif line.startswith("# Incoming Edges") or line.startswith("# Outgoing Edges"):
                current_section = "edges"
            elif line.startswith("# From Party") or line.startswith("# To Party"):
                continue  # Skip party headers, we'll handle edges globally
            
            # Skip comments
            if line.startswith("#"):
                continue
            
            # Parse data based on section
            try:
                if current_section == "vertices":
                    vid, data = map(int, line.split())
                    vertex_ids.append(vid)
                    vertex_data[vid] = data
                elif current_section == "edges":
                    src, dst = map(int, line.split())
                    all_edges.append((src, dst))
            except ValueError:
                continue

    return vertex_ids, vertex_data, all_edges

def parse_updated_file(filename: str) -> Tuple[List[int], Dict[int, int], List[Tuple[int, int]]]:
    vertex_data = {}
    current_section = None

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Detect section headers
            if line.startswith("# Updated Vertex Data"):
                current_section = "vertices"
            
            # Skip comments
            if line.startswith("#"):
                continue
            
            # Parse data based on section
            try:
                if current_section == "vertices":
                    vid, data = map(int, line.split())
                    vertex_data[vid] = data
            except ValueError:
                continue

    return vertex_data

def get_party_id(filename: str) -> int:
    """Extract party ID from filename (e.g., party_3.txt -> 3)"""
    match = re.search(r'party_(\d+)\.txt', filename)
    return int(match.group(1)) if match else -1

def get_party_id_updated(filename: str) -> int:
    """Extract party ID from filename (e.g., party_3.txt -> 3)"""
    match = re.search(r'party_(\d+)_updated\.txt', filename)
    return int(match.group(1)) if match else -1

def assign_party_positions(parties: Dict[int, List[int]], center_radius: float = 1.0) -> Dict[int, Tuple[float, float]]:
    """
    Assign circular positions to each party's center
    Args:
        parties: {party_id: [vertex_ids]}
        center_radius: Radius for center of each party circle
    Returns:
        {party_id: (center_x, center_y)}
    """
    num_parties = len(parties)
    positions = {}
    
    for i, party_id in enumerate(sorted(parties.keys())):
        # Calculate angular position
        theta = 2 * np.pi * (i / num_parties)
        x = center_radius * np.cos(theta)
        y = center_radius * np.sin(theta)
        positions[party_id] = (x, y)
    
    return positions

def assign_vertices_in_party_circles(
    parties: Dict[int, List[int]], 
    party_centers: Dict[int, Tuple[float, float]],
    party_radius: float
) -> Dict[int, Tuple[float, float]]:
    """
    Assign positions to vertices within their respective party circles
    Args:
        parties: {party_id: [vertex_ids]}
        party_centers: {party_id: (center_x, center_y)}
        party_radius: Radius of each party's circle
    Returns:
        {vertex_id: (x, y)}
    """
    vertex_positions = {}
    
    for party_id, graph in parties.items():
        vertices = graph['vertices']
        center_x, center_y = party_centers[party_id]
        num_vertices = len(vertices)
        
        if num_vertices == 1:
            # Single vertex at the center
            vertex_positions[vertices[0]] = (center_x, center_y)
        else:
            # Distribute vertices evenly within the circle
            for i, vertex_id in enumerate(vertices):
                theta = 2 * np.pi * (i / num_vertices)
                x = center_x + 0.8 * party_radius * 0.8 * np.cos(theta)  # 0.8 factor to avoid edges touching circle
                y = center_y + 0.8 * party_radius * 0.8 * np.sin(theta)
                vertex_positions[vertex_id] = (x, y)
    
    return vertex_positions

def visualize_initial_global_graph(workspace_dir: str, output_pdf: str = None):
    """
    Main visualization function
    Args:
        workspace_dir: Directory containing party files
        output_pdf: Path to save PDF (optional)
    """
    # Collect all party data
    parties = {}  # {party_id: {vertices, edges, data}}
    all_edges = []
    global vertex_data
    global vertex_to_party
    
    # Parse each party file
    for filename in os.listdir(workspace_dir):
        if not filename.startswith("party_") or not filename.endswith(".txt"):
            continue
            
        filepath = os.path.join(workspace_dir, filename)
        party_id = get_party_id(filename)
        if party_id == -1:
            continue
        
        try:
            vertices, data, edges = parse_party_file(filepath)
            parties[party_id] = {
                "vertices": vertices,
                "edges": edges,
                "data": data
            }
            all_edges.extend(edges)
            vertex_data.update(data)
            
            # Map each vertex to this party
            for vid in vertices:
                vertex_to_party[vid] = party_id
        except Exception as e:
            print(f"Error parsing {filename}: {str(e)}")
            continue

    # Create global graph
    G = nx.DiGraph()
    G.add_nodes_from(vertex_data.keys())
    G.add_edges_from(all_edges)

    # Calculate party circle centers
    party_centers = assign_party_positions(parties, center_radius=2)

    party_radius = 0.4  # Adjust based on number of parties
    
    # Calculate vertex positions within party circles
    vertex_positions = assign_vertices_in_party_circles(parties, party_centers, party_radius)
    
    # Create figure
    fig, ax = plt.subplots()
    fig.set_size_inches(15, 15)
    
    # Draw party circles
    for party_id, (center_x, center_y) in party_centers.items():
        circle = plt.Circle(
            (center_x, center_y), 
            party_radius, 
            fill=False,
            edgecolor=plt.cm.tab10(party_id % 10),
            linewidth=2,
            linestyle='-'
        )
        ax.add_patch(circle)
        
        # Add party label
        ax.text(
            center_x, 
            center_y - party_radius * 1.1,  # Position label below circle
            f'Party {party_id}', 
            ha='center', 
            va='center',
            bbox=dict(facecolor='white', alpha=0.8, boxstyle='round,pad=0.3')
        )
    
    # Draw nodes with correct party colors
    node_colors = ['red' if vertex_data[vid] == 1 else 'white' for vid in G.nodes()]
    # nx.draw_networkx_nodes(
    #     G,
    #     pos=vertex_positions,
    #     node_size=300,
    #     node_color=node_colors,
    #     ax=ax
    # )
    nx.draw_networkx_nodes(
        G,
        pos=vertex_positions,
        node_size=300,
        node_color=node_colors,          # White fill color
        edgecolors='black',          # Black outline
        linewidths=1.5,              # Thicker outline
        alpha=0.6,                   # Transparent fill
        ax=ax
    )
    
    # Draw edges
    nx.draw_networkx_edges(
        G,
        pos=vertex_positions,
        arrowstyle='->',
        arrowsize=12,
        edge_color='gray',
        alpha=0.7
    )
    
    # Draw labels
    nx.draw_networkx_labels(G, pos=vertex_positions, font_size=12)
    
    # Add title and formatting
    plt.title('Global Graph Visualization with Party Boundaries', fontsize=20)
    plt.axis('equal')
    plt.margins(0.1)  # Increase margins to accommodate labels
    plt.gca().set_aspect('equal', adjustable='box')
    
    # Save as PDF if requested
    if output_pdf:
        plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
        print(f"Visualization saved to {output_pdf}")
    
    plt.show()

def visualize_updated_global_graph(workspace_dir: str, output_pdf: str = None):
    """
    Main visualization function
    Args:
        workspace_dir: Directory containing party files
        output_pdf: Path to save PDF (optional)
    """
    # Collect all party data
    parties = {}  # {party_id: {vertices, edges, data}}
    all_edges = []
    global vertex_data
    global vertex_to_party
    
    # Parse each party file
    for filename in os.listdir(workspace_dir):
        if not filename.startswith("party_") or not filename.endswith(".txt"):
            continue
            
        filepath = os.path.join(workspace_dir, filename)
        party_id = get_party_id(filename)
        if party_id == -1:
            continue
        
        try:
            vertices, data, edges = parse_party_file(filepath)
            parties[party_id] = {
                "vertices": vertices,
                "edges": edges,
                "data": data
            }
            all_edges.extend(edges)
            vertex_data.update(data)
            
            # Map each vertex to this party
            for vid in vertices:
                vertex_to_party[vid] = party_id
        except Exception as e:
            print(f"Error parsing {filename}: {str(e)}")
            continue

    # Parse each party file
    for filename in os.listdir(workspace_dir):
        if not filename.startswith("party_") or not filename.endswith("_updated.txt"):
            continue
            
        filepath = os.path.join(workspace_dir, filename)
        party_id = get_party_id_updated(filename)
        if party_id == -1:
            continue
        
        try:
            data= parse_updated_file(filepath)
            vertex_data.update(data)
        
        except Exception as e:
            print(f"Error parsing {filename}: {str(e)}")
            continue

    # Create global graph
    G = nx.DiGraph()
    G.add_nodes_from(vertex_data.keys())
    G.add_edges_from(all_edges)

    # Calculate party circle centers
    party_centers = assign_party_positions(parties, center_radius=2)

    party_radius = 0.4  # Adjust based on number of parties
    
    # Calculate vertex positions within party circles
    vertex_positions = assign_vertices_in_party_circles(parties, party_centers, party_radius)
    
    # Create figure
    fig, ax = plt.subplots()
    fig.set_size_inches(15, 15)
    
    # Draw party circles
    for party_id, (center_x, center_y) in party_centers.items():
        circle = plt.Circle(
            (center_x, center_y), 
            party_radius, 
            fill=False,
            edgecolor=plt.cm.tab10(party_id % 10),
            linewidth=2,
            linestyle='-'
        )
        ax.add_patch(circle)
        
        # Add party label
        ax.text(
            center_x, 
            center_y - party_radius * 1.1,  # Position label below circle
            f'Party {party_id}', 
            ha='center', 
            va='center',
            bbox=dict(facecolor='white', alpha=0.8, boxstyle='round,pad=0.3')
        )
    
    # Draw nodes with correct party colors
    node_colors = ['red' if vertex_data[vid] == 1 else 'white' for vid in G.nodes()]
    # nx.draw_networkx_nodes(
    #     G,
    #     pos=vertex_positions,
    #     node_size=300,
    #     node_color=node_colors,
    #     ax=ax
    # )
    nx.draw_networkx_nodes(
        G,
        pos=vertex_positions,
        node_size=300,
        node_color=node_colors,          # White fill color
        edgecolors='black',          # Black outline
        linewidths=1.5,              # Thicker outline
        alpha=0.6,                   # Transparent fill
        ax=ax
    )
    
    # Draw edges
    nx.draw_networkx_edges(
        G,
        pos=vertex_positions,
        arrowstyle='->',
        arrowsize=12,
        edge_color='gray',
        alpha=0.7
    )
    
    # Draw labels
    nx.draw_networkx_labels(G, pos=vertex_positions, font_size=12)
    
    # Add title and formatting
    plt.title('Global Graph Visualization with Party Boundaries', fontsize=20)
    plt.axis('equal')
    plt.margins(0.1)  # Increase margins to accommodate labels
    plt.gca().set_aspect('equal', adjustable='box')
    
    # Save as PDF if requested
    if output_pdf:
        plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
        print(f"Visualization saved to {output_pdf}")
    
    plt.show()

def parse_scatter_file(filename: str) -> Tuple[int, List[int], List[Tuple[int, int, int]]]:
    """
    Parse scatter task file using party-vertex mapping for destination parties
    
    Args:
        filename: Path to scatter task file
        party_vertex_map: {party_id: [vertex_ids]} mapping
    
    Returns:
        (source_party, source_vertices, scatter_edges) with party validation
    """
    source_party = -1
    source_vertices = []
    scatter_edges = []
    global vertex_to_party
    all_parties = set(vertex_to_party.keys())
    
    with open(filename, 'r') as f:
        metPartyLine = False
        metSrcLine = False
        metDstLine = False

        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Extract source party
            if line.startswith(">>ScatterTask (Party") and not metPartyLine:
                match = re.search(r'Party (\d+)', line)
                if match:
                    metPartyLine = True
                    source_party = int(match.group(1))
                    if source_party not in all_parties:
                        raise ValueError(f"Invalid source party {source_party}")
            
            # Extract source vertices with party validation
            elif line.startswith(">>Src:") and not metSrcLine:
                vertices = re.findall(r'\((\d+)\)', line)
                source_vertices = [int(v) for v in vertices]
                metSrcLine = True
                # Validate source vertices belong to source party
                for vid in source_vertices:
                    if vertex_to_party.get(vid, -1) != source_party:
                        raise ValueError(f"Vertex {vid} not in Party {source_party}")
            
            # Extract destination edges with party validation
            elif line.startswith(">>Dst:") and not metDstLine:
                pairs = re.findall(r'\((\d+) (\d+)\)', line)
                pairs = [(int(x[0]), int(x[1])) for x in pairs]
                metDstLine = True
                for edge in pairs:
                    src_party = vertex_to_party.get(edge[0], -1)
                    if src_party != source_party:
                        raise ValueError(f"Source vertex of edge not in Party {source_party}")
                    dst_party = vertex_to_party.get(edge[1], -1)
                    scatter_edges.append((edge[0], dst_party, edge[1]))
    
    return source_party, source_vertices, scatter_edges

def parse_gather_file(filename: str) -> Tuple[int, List[Tuple[int, int, int]], List[int]]:
    """
    Parse gather task file using party-vertex mapping for destination vertices
    
    Args:
        filename: Path to gather task file
        vertex_to_party: {vertex_id: party_id} mapping
    
    Returns:
        (destination_party, gather_edges, destination_vertices) with party validation
    """
    destination_party = -1
    gather_edges = []
    destination_vertices = []
    global vertex_to_party
    all_parties = set(vertex_to_party.keys())
    
    with open(filename, 'r') as f:
        metPartyLine = False
        metDstLine = False
        metVertexLine = False

        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Extract destination party
            if line.startswith(">>GatherTask (Party") and not metPartyLine:
                match = re.search(r'Party (\d+)', line)
                if match:
                    metPartyLine = True
                    destination_party = int(match.group(1))
                    if destination_party not in all_parties:
                        raise ValueError(f"Invalid destination party {destination_party}")
            
            # Extract gather edges with party validation
            elif line.startswith(">>GatherDst:") and (not metDstLine):
                # print(line)
                pairs = re.findall(r'\((\d+) (\d+)\)', line)
                # print(pairs)
                pairs = [(int(x[0]), int(x[1])) for x in pairs]
                metDstLine = True
                for edge in pairs:
                    src_party = vertex_to_party.get(edge[0], -1)
                    dst_party = vertex_to_party.get(edge[1], -1)
                    if dst_party != destination_party:
                        raise ValueError(f"Destination vertex {edge[1]} not in Party {destination_party}")
                    gather_edges.append((edge[0], src_party, edge[1]))
            
            # Extract destination vertices with party validation
            elif line.startswith(">>Vertex:") and (not metVertexLine):
                vertices = re.findall(r'\((\d+)\)', line)
                destination_vertices = [int(v) for v in vertices]
                metVertexLine = True
                # Validate destination vertices belong to destination party
                for vid in destination_vertices:
                    if vertex_to_party.get(vid, -1) != destination_party:
                        raise ValueError(f"Vertex {vid} not in Party {destination_party}")
    
    return destination_party, gather_edges, destination_vertices

def visualize_scatter_process(scatter_file: str, output_pdf: str = None):
    """
    Visualize scatter process using party-vertex mapping for accurate party detection
    
    Args:
        scatter_file: Path to scatter task file
        output_pdf: Path to save PDF (optional)
    """
    # Create vertex-to-party mapping
    global vertex_to_party
    
    # Parse scatter data with validation
    try:
        source_party, src_vertices, edges = parse_scatter_file(scatter_file)
    except ValueError as e:
        print(f"Parsing error: {str(e)}")
        return

    # Prepare data for tables
    vertex_data = []  # (vertex_id, party_id, is_source)
    edge_data = []    # (src_id, dst_party, dst_id, edge_type)

    # Add source vertices
    for vid in src_vertices:
        vertex_data.append((vid, source_party, True))
    
    # Add destination vertices (deduped)
    dest_vertices = set()
    for _, dst_party, dst_id in edges:
        if (dst_id, dst_party) not in dest_vertices:
            vertex_data.append((dst_id, dst_party, False))
            dest_vertices.add((dst_id, dst_party))
    
    # Categorize edges
    for src_id, dst_party, dst_id in edges:
        edge_type = "intra" if dst_party == source_party else "inter"
        edge_data.append((src_id, dst_party, dst_id, edge_type))
    
    # Create figure with gridspec
    fig = plt.figure(figsize=(25, 15))
    gs = fig.add_gridspec(1, 2, width_ratios=[1, 1])
    
    # ==== Original Graph Visualization (Left) ====
    ax1 = fig.add_subplot(gs[0])
    ax1.set_title("Scatter Process Visualization", fontsize=18)
    
    # Create graph
    G = nx.DiGraph()
    
    # Add all vertices (including target party nodes)
    G.add_nodes_from(src_vertices)
    
    # Add edges and track target vertices
    target_vertices = {}  # Map: (party_id, vertex_id) -> position index

    for src_id, dst_party, dst_id in edges:
        if dst_party == source_party:
            # Intra-party edge (connect directly to destination vertex)
            G.add_edge(src_id, dst_id, type='intra')
        else:
            # Inter-party edge
            target_key = (dst_party, dst_id)
            if target_key not in target_vertices:
                # Assign a unique position index for each target vertex
                target_vertices[target_key] = len(target_vertices)
            G.add_edge(src_id, target_key, type='inter')

    # ==== Positioning ====
    pos = {}

    # Source vertices in central circle
    num_src = len(src_vertices)
    for i, vid in enumerate(src_vertices):
        theta = 2 * np.pi * (i / num_src)
        pos[vid] = (0.6 * np.cos(theta), 0.6 * np.sin(theta))

    # Target vertices in outer circles (grouped by party)
    target_parties = set(party for (party, _) in target_vertices.keys())
    for party_idx, party in enumerate(sorted(target_parties)):
        # Calculate base angle for this party's group
        base_theta = 2 * np.pi * (party_idx / len(target_parties))
        
        # Get all vertices for this party
        party_vertices = [v for (p, v) in target_vertices.keys() if p == party]
        num_vertices = len(party_vertices)
        
        for vertex_idx, vertex_id in enumerate(party_vertices):
            # Position each vertex around the party's circle
            offset_theta = 2 * np.pi * (vertex_idx / num_vertices) * (0.3 / len(target_parties))  # 0.3 scale for spacing
            radius = 1.2  # Base radius for target parties
            
            x = radius * np.cos(base_theta + offset_theta)
            y = radius * np.sin(base_theta + offset_theta)
            
            pos[(party, vertex_id)] = (x, y)

    # Draw source party border
    circle = plt.Circle((0, 0), 0.8, fill=False, edgecolor='blue', linewidth=2)
    ax1.add_patch(circle)
    ax1.text(0, -0.9, f'Party {source_party}', ha='center', va='center')

    # print(pos)

    # Draw target party groups
    for party in target_parties:
        # Calculate center for this party's group
        party_vertices = [v for (p, v) in target_vertices.keys() if p == party]
        group_center = np.mean([pos[(party, v)] for v in party_vertices], axis=0)
        
        # Draw party circle (larger to encompass all vertices)
        circle = plt.Circle(group_center, 0.2, fill=False, 
                        edgecolor=plt.cm.tab10(party % 10), linewidth=2)
        ax1.add_patch(circle)
        ax1.text(group_center[0], group_center[1] - 0.25, f'Party {party}', 
                ha='center', va='center')

    # Draw nodes
    for node in G.nodes():
        if isinstance(node, tuple):  # Target node (party, vertex)
            party, vertex = node
            nx.draw_networkx_nodes(
                G, pos=pos, nodelist=[node], label=[node[1]], node_size=800, 
                node_color='white', alpha=0.7,
                edgecolors='black', linewidths=1, ax=ax1
            )
            # # Label with vertex ID
            # ax1.text(pos[node][0], pos[node][1], str(vertex), 
            #         ha='center', va='center', fontsize=8, color='black')
        else:  # Source node
            nx.draw_networkx_nodes(
                G, pos=pos, nodelist=[node], node_size=600, 
                node_color='white', edgecolors='black', linewidths=1, ax=ax1
            )

    # Draw edges
    for u, v, d in G.edges(data=True):
        color = 'green' if d['type'] == 'intra' else 'red'
        nx.draw_networkx_edges(
            G, pos=pos, edgelist=[(u, v)], 
            edge_color=color, arrowstyle='->', arrowsize=20, ax=ax1
        )
    
    # Draw labels
    nx.draw_networkx_labels(G, pos=pos, font_size=8)
    ax1.axis('equal')
    ax1.axis('off')
    
    # ==== Combined Tables (Right) ====
    ax_tables = fig.add_subplot(gs[1])
    ax_tables.set_title("Vertex & Outgoing Edge Tables", fontsize=18)
    
    # Draw vertex table
    vertex_table = ax_tables.table(
        cellText=[[f"V{v}", f"P{p}", "Src"] for v, p, is_src in vertex_data if is_src],
        colLabels=["Vertex", "Party", "Role"],
        loc='center left',
        cellLoc='center',
        bbox=[0, 0.3, 0.45, 0.5]  # Left half of the table area
    )
    vertex_table.set_fontsize(12)
    vertex_table.scale(1, 1.5)

    edge_data = sorted(edge_data, key=lambda x: x[2])
    
    # Draw edge table
    edge_table = ax_tables.table(
        cellText=[[f"V{src}", f"P{dst_party}:V{dst}", et.upper()] for src, dst_party, dst, et in edge_data],
        colLabels=["Source", "Destination", "Type"],
        loc='center right',
        cellLoc='center',
        bbox=[0.55, 0.05, 0.45, 0.9]  # Right half of the table area
    )
    for i, (_, _, _, edge_type) in enumerate(edge_data):
        if edge_type == 'inter':
            for j in range(len(edge_table.get_celld()) // len(edge_data)):
                edge_table[(i+1, j)].set_facecolor("#858282")
        else:
            for j in range(len(edge_table.get_celld()) // len(edge_data)):
                edge_table[(i+1, j)].set_facecolor('#ffffff')

    edge_table.set_fontsize(12)
    edge_table.scale(1, 1.5)

    ax_tables.axis('off')
    
    # Add title and formatting
    plt.suptitle(f'ScatterTask from Party {source_party}', fontsize=20, y=0.95)
    
    # Save to PDF
    if output_pdf:
        plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
        print(f"Scatter visualization saved to {output_pdf}")

def visualize_gather_process(gather_file: str, output_pdf: str = None):
    """
    Visualize gather process using party-vertex mapping for accurate party detection
    
    Args:
        gather_file: Path to gather task file
        output_pdf: Path to save PDF (optional)
    """
    # Create vertex-to-party mapping
    global vertex_to_party
    
    # Parse gather data with validation
    try:
        dest_party, edges, dest_vertices = parse_gather_file(gather_file)
    except ValueError as e:
        print(f"Parsing error: {str(e)}")
        return

    # print(dest_party, edges, dest_vertices)

    # Prepare data for tables
    vertex_data = []  # (vertex_id, party_id, is_destination)
    edge_data = []    # (src_id, src_party, dst_id, edge_type)

    # Add destination vertices
    for vid in dest_vertices:
        vertex_data.append((vid, dest_party, True))
    
    # Add source vertices (deduped)
    src_vertices = set()
    for src_id, src_party, _ in edges:
        if (src_id, src_party) not in src_vertices:
            vertex_data.append((src_id, src_party, False))
            src_vertices.add((src_id, src_party))
    
    # Categorize edges
    for src_id, src_party, dst_id in edges:
        edge_type = "intra" if src_party == dest_party else "inter"
        edge_data.append((src_id, src_party, dst_id, edge_type))

    # print(edges)
    
    # Create figure with gridspec
    fig = plt.figure(figsize=(25, 15))
    gs = fig.add_gridspec(1, 2, width_ratios=[1, 1])
    
    # ==== Original Graph Visualization (Left) ====
    ax1 = fig.add_subplot(gs[0])
    ax1.set_title("Gather Process Visualization", fontsize=18)
    
    # Create graph
    G = nx.DiGraph()
    
    # Add all vertices (including source party nodes)
    G.add_nodes_from(dest_vertices)
    
    # Add edges and track source vertices
    source_groups = {}  # Map: (party_id, vertex_id) -> position index

    for src_id, src_party, dst_id in edges:
        if src_party == dest_party:
            # Intra-party edge (connect directly from source to destination)
            G.add_edge(src_id, dst_id, type='intra')
        else:
            # Inter-party edge
            source_key = (src_party, src_id)
            if source_key not in source_groups:
                source_groups[source_key] = len(source_groups)
            G.add_edge(source_key, dst_id, type='inter')

    # print(source_groups)

    # ==== Positioning ====
    pos = {}

    # Destination vertices in central circle
    num_dst = len(dest_vertices)
    for i, vid in enumerate(dest_vertices):
        theta = 2 * np.pi * (i / num_dst) if num_dst else 0
        pos[vid] = (0.6 * np.cos(theta), 0.6 * np.sin(theta))

    # Source vertices in outer circles (grouped by party)
    source_parties = set(party for (party, _) in source_groups.keys())
    for party_idx, party in enumerate(sorted(source_parties)):
        # Calculate base angle for this party's group
        base_theta = 2 * np.pi * (party_idx / len(source_parties)) if source_parties else 0
        
        # Get all vertices for this party
        party_vertices = [v for (p, v) in source_groups.keys() if p == party]
        num_vertices = len(party_vertices)
        
        for vertex_idx, vertex_id in enumerate(party_vertices):
            # Position each vertex around the party's circle
            offset_theta = 2 * np.pi * (vertex_idx / num_vertices) * (0.3 / len(source_parties))  # Spacing scale
            radius = 1.2  # Base radius for source parties
            
            x = radius * np.cos(base_theta + offset_theta)
            y = radius * np.sin(base_theta + offset_theta)
            
            pos[(party, vertex_id)] = (x, y)

    # Draw destination party border
    circle = plt.Circle((0, 0), 0.8, fill=False, edgecolor='red', linewidth=2)
    ax1.add_patch(circle)
    ax1.text(0, -0.9, f'Target Party {dest_party}', ha='center', va='center')

    # Draw source party groups
    for party in source_parties:
        # Calculate center for this party's group
        party_nodes = [(party, v) for v, _ in source_groups.items() if v[0] == party]
        group_center = np.mean([pos[node[1]] for node in party_nodes], axis=0) if party_nodes else (0,0)
        
        # Draw party circle
        circle = plt.Circle(group_center, 0.2, fill=False, 
                        edgecolor=plt.cm.tab10(party % 10), linewidth=2)
        ax1.add_patch(circle)
        ax1.text(group_center[0], group_center[1] + 0.25, f'Party {party}', 
                ha='center', va='center')

    # Draw nodes
    for node in G.nodes():
        if isinstance(node, tuple):  # Source node (party, vertex)
            party, vertex = node
            nx.draw_networkx_nodes(
                G, pos=pos, nodelist=[node], node_size=800, 
                node_color='white', alpha=0.7,
                edgecolors='black', linewidths=1, ax=ax1
            )
            ax1.text(pos[node][0], pos[node][1], str(vertex), 
                    ha='center', va='center', fontsize=8, color='white')
        else:  # Destination node
            nx.draw_networkx_nodes(
                G, pos=pos, nodelist=[node], node_size=600, 
                node_color='lightblue', edgecolors='black', linewidths=1, ax=ax1
            )
            ax1.text(pos[node][0], pos[node][1], str(node), 
                    ha='center', va='center', fontsize=8, color='black')

    # Draw edges
    for u, v, d in G.edges(data=True):
        color = 'green' if d['type'] == 'intra' else 'blue'
        nx.draw_networkx_edges(
            G, pos=pos, edgelist=[(u, v)], 
            edge_color=color, arrowstyle='->', arrowsize=20, ax=ax1
        )
    
    nx.draw_networkx_labels(G, pos=pos, font_size=8)
    ax1.axis('equal')
    ax1.axis('off')
    
    # ==== Combined Tables (Right) ====
    ax_tables = fig.add_subplot(gs[1])
    ax_tables.set_title("Incoming Edge & Vertex Tables", fontsize=18)

    edge_data = sorted(edge_data, key=lambda x: x[2])
    
    # Draw edge table
    edge_table = ax_tables.table(
        cellText=[[f"P{s_p}:V{s}", f"V{d}", et.upper()] for s, s_p, d, et in edge_data],
        colLabels=["Source", "Destination", "Type"],
        loc='center left',
        cellLoc='center',
        bbox=[0, 0.05, 0.45, 0.9]  # Right half
    )

    for i, (_, _, _, edge_type) in enumerate(edge_data):
        if edge_type == 'inter':
            for j in range(len(edge_table.get_celld()) // len(edge_data)):
                edge_table[(i+1, j)].set_facecolor("#858282")
        else:
            for j in range(len(edge_table.get_celld()) // len(edge_data)):
                edge_table[(i+1, j)].set_facecolor('#ffffff')

    edge_table.set_fontsize(12)
    edge_table.scale(1, 1.5)
    
    # Draw destination vertex table
    vertex_table = ax_tables.table(
        cellText=[[f"V{v}", f"P{p}", "Dest"] for v, p, is_dst in vertex_data if is_dst],
        colLabels=["Vertex", "Party", "Role"],
        loc='center right',
        cellLoc='center',
        bbox=[0.55, 0.3, 0.45, 0.5]  # Left half
    )
    vertex_table.set_fontsize(12)
    vertex_table.scale(1, 1.5)

    ax_tables.axis('off')
    
    # Add title and formatting
    plt.suptitle(f'GatherTask to Party {dest_party}', fontsize=20, y=0.95)
    
    # Save to PDF
    if output_pdf:
        plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
        print(f"Gather visualization saved to {output_pdf}")

def visualize_input(WORKSPACE_DIR, output_file=True):
    if output_file:
        OUTPUT_PDF = WORKSPACE_DIR + "/initial_graph_visualization.pdf"   
    else:
        OUTPUT_PDF = None 
    visualize_initial_global_graph(WORKSPACE_DIR, OUTPUT_PDF)    

def visualize_computing_process(WORKSPACE_DIR):
    match = re.search(r"num_parts_(\d+)", WORKSPACE_DIR)
    if match:
        num_parties = int(match.group(1))
        print(f"Number of parties: {num_parties}")
    else:
        print("Number of parties not found in the string.")

    for i in range(num_parties):
    # for i in range(1):
        SCATTER_FILE = WORKSPACE_DIR + f"/efficiency_{i}.log"
        OUTPUT_PDF = WORKSPACE_DIR + f"/scatter_visualization_{i}.pdf"
        
        visualize_scatter_process(SCATTER_FILE, OUTPUT_PDF)

        GATHER_FILE = WORKSPACE_DIR + f"/efficiency_{i}.log"
        OUTPUT_PDF = WORKSPACE_DIR + f"/gather_visualization_{i}.pdf"
    
        visualize_gather_process(GATHER_FILE, OUTPUT_PDF)

def visualize_output(WORKSPACE_DIR):
    OUTPUT_PDF = WORKSPACE_DIR + "/updated_graph_visualization.pdf"
    visualize_updated_global_graph(WORKSPACE_DIR, OUTPUT_PDF)   

def main():

    WORKSPACE_DIR = "log/demo/log/executable_0/net_cond_4000_1/num_parts_8/scale_3/avgDegree_3/interRatio_0.6/alg_0/iters_2"

    parser = argparse.ArgumentParser(description='Visualize input, computing process and output of RingSG')
    parser.add_argument('--input', action='store_true', help='Visualize Input')
    parser.add_argument('--compute', action='store_true', help='Visualize Computation')
    parser.add_argument('--output', action='store_true', help='Visualize Output')
    parser.add_argument('--all', action='store_true', help='Visualize ALL')
    args = parser.parse_args()

    if args.input:
        visualize_input(WORKSPACE_DIR)
    if args.compute:
        visualize_input(WORKSPACE_DIR, False)
        visualize_computing_process(WORKSPACE_DIR)
    if args.output:
        visualize_input(WORKSPACE_DIR, False)
        visualize_output(WORKSPACE_DIR)
    if args.all:
        visualize_input(WORKSPACE_DIR)
        visualize_computing_process(WORKSPACE_DIR)
        visualize_output(WORKSPACE_DIR)

# Example usage
if __name__ == "__main__":
    main()
