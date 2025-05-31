# RingSG

This repository contains prototype implementation of the protocols proposed in *RingSG: Optimal Secure Vertex-Centric Computation for Collaborative Graph Processing (Accepted by ACM CCS 2025)*, with primary focuses on reproducing the paper's experimental results and fostering future research.

The codebase is largely built upon [aby3](https://github.com/ladnir/aby3), a widely adopted scheme for efficient privacy-preserving computation. Our code adheres to the original organization/style of the [aby3](https://github.com/ladnir/aby3) library, and use modular protocol realizations with unit tests to help future utilization.

Here is the table of contents of this document:


> Caveat!!! Similar to the original [aby3](https://github.com/ladnir/aby3) library, this codebase should NOT be considered fully secure. It has not had a security review and there are still several security related issues that have not been fully implemented. Only use this codebase as a proof-of-concept or to benchmark the perfromance. Future work is required for this implementation to be considered secure.

## 0 Necessary Backgrounds

### 0.1 Collaborative Graph Processing

*Collaborative graph processing* refers to the jointly analysis of the private graph data held by multiple graph owners, *without revealing each owner's raw graph data* to any other graph owners. The local graphs of different graph owners are interleaved by some *inter-edges* and finally consititute a *global graph*.

- For example, in financial scenarios, each graph owner can be a bank, then each local graph is the transfer graph inside a bank, and inter-edges correspond to inter-bank transfers. These local transfer graphs are concatenated into a global transfer graph by inter-bank transfers.

The goal of Collaborative Graph Processing is to have the parties (graph owners) jointly run *a graph algorithm* on the global graph, thus obtaining data insights that are unavailable from a siloed graph held by a single graph owner.

- The graph algorithm can be as traditional as Connected Component Labeling, Shortest Path and PageRank, or be more Advanced like Graph Neural Network training/inference.
- A straightforward example is in Anti-money laundering (AML), where we MUST aggregates graph data from multiple financial institutions to detect malicious cross-border fund flows, which ecomes infeasible when relying solely on isolated local graph data maintained by individual banks.

A primary requirement of collaborative graph processing is to protect the raw graph data privacy of each graph owner. We want the graph owners to jointly obtain a graph algorithm output without viewing/direct sharing others' raw graph data, which raises substantial privacy concerns and may violate regulatory requirements.

### 0.2 RingSG and Our Core Contributions

RingSG is a new system proposed for collaborative graph processing. It is built upon [the vertex-centric abstraction](https://dl.acm.org/doi/10.1145/1807167.1807184) (abstracted as iterations of Scatter/Gather operation centering vertices) for generally supporting various graph algorithms (like Connected Component, Shortest Path and PageRank), and cryptographic techniques (secret share and general-purpose secure multi-party computation) to enforce end-to-end provable security/privacy (under the semi-honest threat model). Distinguished from prior efforts, RingSG is featured by:
- **Ring-ScatterGather**: A novel computation paradigm that securely decomposes secure vertex-centric computation into parrallel tasks, where each task handles a *subgraph* of the *global graph (consisting of all graph owners' graphs)* and is assigned to a (sub)group of parties among all the graph owners for execution. Ring-ScatterGather eliminates expensive cryptographic operations used in prior works (e.g., oblivious sort used in GraphSC, IEEE S&P'15 and Graphiti, ACM CCS'24), and simultaneously ensures that the MPC tasks are mutually exclusive (which means that the graph data handled by different tasks is non-overlapping, different from the overlapped tasks in CoGNN, ACM CCS'24). This finally leads to the first touch of the *optimal computation/communication complexity for secure vertex-centric computation*.
    - In particular, for each iteration of secure vertex-centric computation, the overall computation/communication overhead of all parties in RingSG is $O(|V|+|E|)$, where $|V|$ and $|E|$ represent the numbers of vertices and edges in the global graph, respectively. This is \emph{optimal} because it is linear to $(|V|+|E|)$ and independent of the number of parties $N$, making it more efficient than both state-of-the-art outsourced computation schemes (GraphSC and Graphiti, $O((|V|+|E|)\log(|V| + |E|))$) and CoGNN ($O(N|V|+|E|)$). See [Section 4 Ring-ScatterGather Paradigm].
- **Concrete Efficiency via Multiple Protocol-Level Optimizations**: Within the Ring-ScatterGather paradigm, RingSG introduces two key concrete-efficiency optimizations:
    - On-demand Incorporation of 3PC via Share Conversion: While the Ring-ScatterGather paradigm requires 2-out-of-2 secret share for workload decomposition and task distribution, we propose dynamic share-conversion mechanisms to allow incorporation of 3PC based on 2-out-of-3 secret share for efficient task execution. See [Section 5.1 On-demand Incorporation of 3PC].
    - Oblivious Group Aggregation (OGA) with halved rounds: We pinpoint the most cost-heavy operation in RingSG (i.e., OGA, which is used for securely aggregating edge-generated updates targeting the same vertices) and design a novel protocol that halves the communication rounds of the state-of-the-art protocol without introducing extra operational overheads. This leads to significant decrease in the system running time. See [Section 5.2 OGA with halved rounds].
- **End-to-End System Instantiation**: We present the end-to-end instantiation of RingSG in two *real-world anti-money laundering applications*, named Detect Group Connection and Trace Transfer Chain. The design of RingSG has enabled efficient and privacy-preserving extraction of application-specific results from the protocol outputs, which has never been achieved or discussed in prior state-of-the-arts. See [Section 6 End-to-end System Instantiation].

## 0.3 The Evaluations Performed in the Paper

Our experiments center around evaluating the efficiency advantages of RingSG compared to prior state-of-the-arts. Our baselines include:
- GraphSC, which refer to a series of works based on outsourced computation. We utilize its state-of-the-art design proposed in Graphiti, ACM CCS 2024, and reimplemented it via aby3 for fair comparison.
- CoGNN, ACM CCS 2024, which is more similar to RingSG due the its collaborative computation nature (instead of outsourced computation). We reimplemented CoGNN by replacing its expensive 2PC with aby3-based 3PC, through our dynamic share conversion mechanisms. This baseline should fairly demonstrate the paradigm-specific advantage of RingSG compared to CoGNN, by neutralizing the efficiency difference caused by different MPC backends.

The experiments include:
- Figure 8: Running time and Per-party communication for different global graph sizes.
- Figure 9: Running time and Per-party communication for different numbers of parties.
- Figure 10: Running time and Per-party communication for different numbers of parties.
- Table 2: Per-iteration duration and per-party communication of various schemes. (This experiment contains a comparison with the original 2PC-based CoGNN implementation, which is not included in this codebase.)
- Table 3 and Table 4: Per-iteration duration breakdowns of RingSG and CoGNN to demonstrate the efficiency of our OGA protocol.
- Table 5: Running time and communication of the two end-to-end instantiations of RingSG, with a comparison to the non-end-to-end counterparts of prior state-of-the-arts.

## 1 Introduction

The organization of this codebase and basic information on each folder/file are as below:

```bash
└── 📁aby3
    └── 📁aby3
    └── 📁aby3_tests
    └── 📁aby3-DB
        └── CMakeLists.txt
        └── OblvSwitchNet.cpp # The Oblivious Extended Permuation (OEP) Implementation
        └── OblvSwitchNet.h
        └── OblvPermutation.cpp # The Oblivious Permuation (OP) Implementation
        └── OblvPermutation.h
    └── 📁aby3-DB_tests
        └── CMakeLists.txt
        └── PermutaitonTests.cpp  # Unit tests for OEP and OP
        └── PermutaitonTests.h
    └── 📁aby3-Graph
        └── CMakeLists.txt
        └── cognn_cc.cpp # 3PC-based CoGNN Implementation
        └── cognn_cc.h 
        └── graphsc.cpp # 3PC-based GraphSC (Graphiti) Implementation
        └── graphsc.h
        └── OEP.cpp # OEP Wrapper
        └── OEP.h
        └── OGA.cpp # OGA Implementation
        └── OGA.h
        └── operators.cpp # Some basic circuits
        └── operators.h
        └── ours.cpp # RingSG Implementation
        └── ours.h
        └── shuffle.cpp # 3PC-based secret-shared shuffle (for GraphSC)
        └── shuffle.h
        └── sort.cpp # 3PC-based secure sort (for GraphSC)
        └── sort.h
        └── utils.cpp
        └── utils.h
    └── 📁aby3-Graph_tests # Unit tests for graph-related protocols
        └── CMakeLists.txt
        └── graph_tests.cpp
        └── graph_tests.h
        └── tests.cpp
        └── tests.h
    └── 📁eval # Folder of evaluation scripts
        └── 📁scripts # Scripts for setting network namespaces (for simulating the running environment of each party)
        └── 📁log # Logs of each experiment
        └── 📁plot
            └── 📁fig
            └── plot_3pc_cmp.py # Plot Table 2
            └── plot_ablation.py # Plot Table 3 & 4
            └── plot_e2e.py # Plot Table 5
            └── plot_num_parties.py # Plot Figure 9
            └── plot_scales.py # Plot Figure 8
            └── plot_vertex_degrees.py # Plot Figure 10
        └── CMakeLists.txt
        └── eval_func.cpp
        └── eval_func.h
        └── main.cpp # Entrance of evaluation executable
        └── tmp_run_cluster.py # Script for running all (or each part of) evaluations
    └── 📁frontend
    └── 📁thirdparty # Third-party dependencies
    └── .gitignore
    └── build.py # The building script
    └── CMakeLists.txt
    └── LICENSE
    └── README.md
```





# ABY 3 and Applications
 
## Introduction
 
This library provides the semi-honest implementation of [ABY 3](https://eprint.iacr.org/2018/403.pdf) and [Fast Database Joins for Secret Shared Data](https://eprint.iacr.org/2019/518.pdf).

The repo includes the following application:
 * Linear Regression (training/inference)
 * Logistic Regression (training/inference)
 * Database Inner, Left and Full Joins
 * Database Union
 * Set Cardinality
 * Threat Log Comparison ([see Section 5](https://eprint.iacr.org/2019/518.pdf))
 * ERIC Application ([see Section 5](https://eprint.iacr.org/2019/518.pdf))

A tutorial can be found [here](https://github.com/ladnir/aby3/blob/master/frontend/aby3Tutorial.cpp). It includes a description of how to use the API and a discussion at the end on how the framework is implemented.

## Warning 

This codebase should **NOT** be considered fully secure. It has not had a security review and there are still several security related issues that have not been fully implemented. Only use this codebase as a proof-of-concept or to benchmark the perfromance. Future work is required for this implementation to be considered secure. 

Moreover, some features have not been fully developed and contains bugs. For example, the task scheduler sometime fails. This is a known issue.

## Build
 
The library is *cross platform* and has been tested on Windows and Linux. The dependencies are:

 * [libOTe](https://github.com/osu-crypto/libOTe)
 * [Boost](http://www.boost.org/) (networking)
 * [function2](https://github.com/Naios/function2)
 * [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page)

 
In short, this will build the project

```
git clone https://github.com/ladnir/aby3.git
cd aby3/
python3 build.py --setup
python3 build.py 
```

To see all the command line options, execute the program 
 
`out/build/linux/frontend`

or

`out/build/x64-Release/frontend/frontend`

The library can be linked by linking the binraries in `lib/` and `thirdparty/win` or `thirdparty/unix` depending on the platform.

## Help
 
Contact Peter Rindal peterrindal@gmail.com for any assistance on building  or running the library.

## Citing

 Spread the word!

```
@misc{aby3,
    author = {Peter Rindal},
    title = {{The ABY3 Framework for Machine Learning and Database Operations.}},
    howpublished = {\url{https://github.com/ladnir/aby3}},
}
```
