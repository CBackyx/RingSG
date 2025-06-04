#pragma once

#include <chrono>
#include <mutex>
#include <vector>
#include <array>

void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag);

void Sh3_Graph_multiplex_test();
void Sh3_Graph_compare_select_test();
void Sh3_Graph_OGA_test();
void Sh3_Graph_CC_test();
void Sh3_Graph_Ours_test();
void Sh3_Graph_eq_test();
void Sh3_Graph_shuffle_test();
void Sh3_Graph_sort_test();
void Sh3_Graph_PrefixAgg_test();
void Sh3_Graph_GraphSC_test();
void Sh3_Graph_CoGNN_test();
void Sh3_Graph_reverse_shuffle_test();

void Sh3_Graph_Ours_single_party(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
);
void Sh3_Graph_CoGNN_single_party(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
);
void Sh3_Graph_GraphSC_single_party(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
);
void Sh3_Graph_Ours_App_single_party(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters    
);

void Sh3_Graph_Ours_single_party_demo(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters,
    std::string workspace_dir,
    std::vector<unsigned long>& vertexIdList,
    std::vector<unsigned long>& vertexDataList,
    std::vector<std::vector<std::array<unsigned long, 2>>> incomingEdgeLists,
    std::vector<std::vector<std::array<unsigned long, 2>>> outgoingEdgeLists
);