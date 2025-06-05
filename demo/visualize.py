# import os
# import re
# import matplotlib.pyplot as plt
# import networkx as nx
# import numpy as np
# from typing import List, Dict, Tuple, Set

# # Set up plotting parameters
# plt.rcParams.update({
#     "figure.figsize": (12, 12),
#     "font.family": "sans-serif",
#     "font.size": 10
# })

# def parse_party_file(filename: str) -> Tuple[List[int], Dict[int, int], List[Tuple[int, int]]]:
#     """
#     Parse a party's graph file and extract vertices, data, and edges
    
#     Args:
#         filename: Path to the party's graph file
    
#     Returns:
#         (vertex_ids, vertex_data, all_edges)
#     """
#     vertex_ids = []
#     vertex_data = {}
#     all_edges = []
#     current_section = None

#     with open(filename, 'r') as f:
#         for line in f:
#             line = line.strip()
#             if not line:
#                 continue
            
#             # Detect section headers
#             if line.startswith("# Vertices"):
#                 current_section = "vertices"
#             elif line.startswith("# Incoming Edges") or line.startswith("# Outgoing Edges"):
#                 current_section = "edges"
#             elif line.startswith("# From Party") or line.startswith("# To Party"):
#                 continue  # Skip party headers, we'll handle edges globally
            
#             # Skip comments
#             if line.startswith("#"):
#                 continue
            
#             # Parse data based on section
#             try:
#                 if current_section == "vertices":
#                     vid, data = map(int, line.split())
#                     vertex_ids.append(vid)
#                     vertex_data[vid] = data
#                 elif current_section == "edges":
#                     src, dst = map(int, line.split())
#                     all_edges.append((src, dst))
#             except ValueError:
#                 continue

#     return vertex_ids, vertex_data, all_edges

# def get_party_id(filename: str) -> int:
#     """Extract party ID from filename (e.g., party_3.txt -> 3)"""
#     match = re.search(r'party_(\d+)\.txt', filename)
#     return int(match.group(1)) if match else -1

# def assign_circular_positions(parties: Dict[int, List[int]], radius: float = 0.8) -> Dict[int, Tuple[float, float]]:
#     """
#     Assign circular positions to vertices while grouping by party
#     Args:
#         parties: {party_id: [vertex_ids]}
#         radius: Radius for node placement (0 to 1)
#     Returns:
#         {vertex_id: (x, y)}
#     """
#     num_parties = len(parties)
#     positions = {}
    
#     for party_id, vertices in parties.items():
#         # Calculate angular range for the party
#         start_angle = 2 * np.pi * (party_id / num_parties)
#         end_angle = 2 * np.pi * ((party_id + 1) / num_parties)
#         angle_range = end_angle - start_angle
        
#         # Distribute vertices evenly within the angular range
#         num_vertices = len(vertices)
#         for i, vid in enumerate(vertices):
#             theta = start_angle + angle_range * (i + 0.5) / num_vertices
#             x = radius * np.cos(theta)
#             y = radius * np.sin(theta)
#             positions[vid] = (x, y)
    
#     return positions

# def visualize_global_graph(workspace_dir: str, output_pdf = None):
#     """
#     Main visualization function
#     Args:
#         workspace_dir: Directory containing party files
#     """
#     # Collect all party data
#     parties = {}  # {party_id: {vertices, edges, data}}
#     all_edges = []
#     vertex_data = {}
#     vertex_to_party = {}  # Map each vertex to its party
    
#     # Parse each party file
#     for filename in os.listdir(workspace_dir):
#         if not filename.startswith("party_") or not filename.endswith(".txt"):
#             continue
            
#         filepath = os.path.join(workspace_dir, filename)
#         party_id = get_party_id(filename)
        
#         try:
#             vertices, data, edges = parse_party_file(filepath)
#             parties[party_id] = {
#                 "vertices": vertices,
#                 "edges": edges,
#                 "data": data
#             }
#             all_edges.extend(edges)
#             vertex_data.update(data)

#             # Map each vertex to this party
#             for vid in vertices:
#                 vertex_to_party[vid] = party_id
#         except Exception as e:
#             print(f"Error parsing {filename}: {str(e)}")
#             continue

#     # Create global graph
#     G = nx.DiGraph()
#     G.add_nodes_from(vertex_data.keys())
#     G.add_edges_from(all_edges)

#     # Prepare party vertex groups
#     party_vertices = {pid: d["vertices"] for pid, d in parties.items()}
    
#     # Calculate positions
#     positions = assign_circular_positions(party_vertices)
    
#     # Create figure
#     fig, ax = plt.subplots()
#     # Draw nodes with party colors
#     node_colors = [plt.cm.tab10(vertex_to_party[vid] % 10) for vid in G.nodes()]
#     nx.draw_networkx_nodes(
#         G,
#         pos=positions,
#         node_size=500,
#         node_color=node_colors,
#         ax=ax
#     )
#     nx.draw_networkx_edges(
#         G,
#         pos=positions,
#         arrowstyle='->',
#         arrowsize=12,
#         edge_color='gray',
#         alpha=0.7
#     )
#     nx.draw_networkx_labels(G, pos=positions, font_size=8)
    
#     # Draw party borders
#     num_parties = len(parties)
#     for party_id in parties.keys():
#         # Calculate border points
#         start_angle = 2 * np.pi * (party_id / num_parties)
#         end_angle = 2 * np.pi * ((party_id + 1) / num_parties)
#         angles = np.linspace(start_angle, end_angle, 100)
#         xs = np.cos(angles) * 1.0  # Slightly larger radius for borders
#         ys = np.sin(angles) * 1.0
        
#         # Create polygon for border
#         border = plt.Polygon(
#             np.column_stack((xs, ys)),
#             fill=False,
#             edgecolor=plt.cm.tab10(party_id % 10),
#             linewidth=2,
#             linestyle='--'
#         )
#         ax.add_patch(border)
        
#         # Add party label
#         mid_angle = start_angle + (end_angle - start_angle) / 2
#         label_x = np.cos(mid_angle) * 1.1
#         label_y = np.sin(mid_angle) * 1.1
#         ax.text(label_x, label_y, f'Party {party_id}', ha='center', va='center')

#     # Add title and formatting
#     plt.title('Global Graph Visualization with Party Boundaries', fontsize=14)
#     plt.axis('equal')
#     plt.margins(0.2)
#     plt.gca().set_aspect('equal', adjustable='box')

#     # Save as PDF if requested
#     if output_pdf:
#         plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
#         print(f"Visualization saved to {output_pdf}")

#     plt.show()

# # Example usage
# if __name__ == "__main__":
#     WORKSPACE_DIR = "log/demo/log/executable_0/net_cond_4000_1/num_parts_8/scale_3/avgDegree_3/interRatio_0.4/alg_0/iters_2"  # Update this path
#     OUTPUT_PDF = "graph_visualization.pdf"

#     visualize_global_graph(WORKSPACE_DIR, OUTPUT_PDF)

import os
import re
import matplotlib.pyplot as plt
import networkx as nx
import numpy as np
from typing import List, Dict, Tuple, Set

vertex_to_party = {}  # Map each vertex to its party

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

def get_party_id(filename: str) -> int:
    """Extract party ID from filename (e.g., party_3.txt -> 3)"""
    match = re.search(r'party_(\d+)\.txt', filename)
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

def visualize_global_graph(workspace_dir: str, output_pdf: str = None):
    """
    Main visualization function
    Args:
        workspace_dir: Directory containing party files
        output_pdf: Path to save PDF (optional)
    """
    # Collect all party data
    parties = {}  # {party_id: {vertices, edges, data}}
    all_edges = []
    vertex_data = {}
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
    party_centers = assign_party_positions(parties)

    party_radius = 0.3  # Adjust based on number of parties
    
    # Calculate vertex positions within party circles
    vertex_positions = assign_vertices_in_party_circles(parties, party_centers, party_radius)
    
    # Create figure
    fig, ax = plt.subplots()
    
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
    node_colors = [plt.cm.tab10(vertex_to_party[vid] % 10) for vid in G.nodes()]
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
        node_color='white',          # White fill color
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
    nx.draw_networkx_labels(G, pos=vertex_positions, font_size=8)
    
    # Add title and formatting
    plt.title('Global Graph Visualization with Party Circles', fontsize=14)
    plt.axis('equal')
    plt.margins(0.3)  # Increase margins to accommodate labels
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
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            # Extract source party
            if line.startswith(">>ScatterTask (Party"):
                match = re.search(r'Party (\d+)', line)
                if match:
                    source_party = int(match.group(1))
                    if source_party not in all_parties:
                        raise ValueError(f"Invalid source party {source_party}")
            
            # Extract source vertices with party validation
            elif line.startswith(">>Src:"):
                vertices = re.findall(r'\((\d+)\)', line)
                source_vertices = [int(v) for v in vertices]
                # Validate source vertices belong to source party
                for vid in source_vertices:
                    if vertex_to_party.get(vid, -1) != source_party:
                        raise ValueError(f"Vertex {vid} not in Party {source_party}")
            
            # Extract destination edges with party validation
            elif line.startswith(">>Dst:"):
                pairs = re.findall(r'\((\d+) (\d+)\)', line)
                pairs = [(int(x[0]), int(x[1])) for x in pairs]
                for edge in pairs:
                    src_party = vertex_to_party.get(edge[0], -1)
                    if src_party != source_party:
                        raise ValueError(f"Source vertex of edge not in Party {source_party}")
                    dst_party = vertex_to_party.get(edge[1], -1)
                    scatter_edges.append((edge[0], dst_party, edge[1]))
    
    return source_party, source_vertices, scatter_edges

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
    
    # Categorize edges
    intra_edges = []   # (src, dst_vertex) within the same party
    inter_edges = []   # (src, dest_party, dst_vertex) to other parties
    
    for src, dest_party, dest_vertex in edges:
        if dest_party == source_party:
            intra_edges.append((src, dest_vertex))
        else:
            inter_edges.append((src, dest_party, dest_vertex))
    
    # Create graph
    G = nx.DiGraph()
    G.add_nodes_from(src_vertices)
    G.add_nodes_from([(p, 'target') for p in set(dest_party for _, dest_party, _ in inter_edges)])  # Target party nodes
    
    # Add intra-edges
    for src, dst in intra_edges:
        G.add_edge(src, dst, type='intra')
    
    # Add inter-edges
    for src, dest_party, dst in inter_edges:
        G.add_edge(src, (dest_party, 'target'), type='inter', dest_vertex=dst)
    
    # Positioning
    pos = {}
    num_src = len(src_vertices)
    
    # Source party layout (central circle)
    for i, vid in enumerate(src_vertices):
        theta = 2 * np.pi * (i / num_src)
        pos[vid] = (0.8 * np.cos(theta), 0.8 * np.sin(theta))
    
    # Target parties layout (outer circle)
    target_parties = list({p for _, p, _ in inter_edges})
    for i, party in enumerate(target_parties):
        theta = 2 * np.pi * (i / len(target_parties)) if len(target_parties) > 0 else 0
        pos[(party, 'target')] = (1.2 * np.cos(theta), 1.2 * np.sin(theta))
    
    # Create figure
    fig, ax = plt.subplots()
    
    # Draw source party border
    circle = plt.Circle((0, 0), 0.8, fill=False, edgecolor='blue', linewidth=2)
    ax.add_patch(circle)
    ax.text(0, -0.9, f'Party {source_party}', ha='center', va='center', bbox=dict(facecolor='white', alpha=0.8))
    
    # Draw target party borders
    for party in target_parties:
        x, y = pos[(party, 'target')]
        circle = plt.Circle((x, y), 0.1, fill=False, edgecolor=plt.cm.tab10(party % 10), linewidth=2)
        ax.add_patch(circle)
        ax.text(x, y - 0.15, f'Party {party}', ha='center', va='center', bbox=dict(facecolor='white', alpha=0.8))
    
    # Draw nodes
    nx.draw_networkx_nodes(
        G,
        pos=pos,
        node_size=500,
        node_color='white',
        edgecolors='black',
        linewidths=1,
        ax=ax
    )
    
    # Draw edges with labels
    for u, v, d in G.edges(data=True):
        if d['type'] == 'intra':
            # Intra-party edge (same party)
            nx.draw_networkx_edges(
                G, pos=pos, edgelist=[(u, v)],
                edge_color='green', arrowstyle='->', arrowsize=10, ax=ax
            )
        else:
            # Inter-party edge (different party)
            nx.draw_networkx_edges(
                G, pos=pos, edgelist=[(u, v)],
                edge_color='red', arrowstyle='->', arrowsize=10, ax=ax
            )
            # Add destination vertex label near target party
            dest_vertex = d.get('dest_vertex', '')
            if dest_vertex:
                x, y = pos[v]
                ax.text(x, y + 0.1, f'V{dest_vertex}', ha='center', va='bottom', fontsize=8)
    
    # Add source vertex labels
    nx.draw_networkx_labels(G, pos=pos, font_size=8)
    
    # Legend
    ax.plot([], [], color='green', label='Intra-party Edge', linestyle='-', marker='>')
    ax.plot([], [], color='red', label='Inter-party Edge', linestyle='-', marker='>')
    plt.legend()
    
    # Formatting
    plt.title(f'Scatter Process from Party {source_party}', fontsize=14)
    plt.axis('equal')
    plt.margins(0.3)
    plt.gca().set_aspect('equal', adjustable='box')
    
    # Save to PDF
    if output_pdf:
        plt.savefig(output_pdf, format='pdf', bbox_inches='tight')
        print(f"Scatter visualization saved to {output_pdf}")
    
    plt.show()

# Example usage
if __name__ == "__main__":
    WORKSPACE_DIR = "log/demo/log/executable_0/net_cond_4000_1/num_parts_8/scale_3/avgDegree_3/interRatio_0.4/alg_0/iters_2"  # Update this path
    OUTPUT_PDF = "graph_visualization.pdf"
    
    visualize_global_graph(WORKSPACE_DIR, OUTPUT_PDF)

    SCATTER_FILE = "log/demo/log/executable_0/net_cond_4000_1/num_parts_8/scale_3/avgDegree_3/interRatio_0.4/alg_0/iters_2/efficiency_1.log"
    OUTPUT_PDF = "scatter_visualization.pdf"
    
    visualize_scatter_process(SCATTER_FILE, OUTPUT_PDF)