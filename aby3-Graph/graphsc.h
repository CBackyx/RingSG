#pragma once

// #include "graph_common.h"
#include "sort.h"
#include "operators.h"

struct GraphParam {
    int num_iters;
    Alg alg;
};
typedef struct GraphParam GraphParam;

typedef std::vector<std::vector<double>> DataFrame;

DataFrame load_dataframe_from_csv(std::string file_name, bool skip_first_row, bool skip_first_col);

class GraphSC {

public:
    DataFrame raw_graph; // Edge list
    aby3::i64Matrix plainTable;
    aby3::sbMatrix table; // Containing both vertices and edges
    int party_id;
    int role;
    oc::Channel prevChl;
    oc::Channel nextChl;
    aby3::Sh3Runtime rt;
    aby3::Sh3Encryptor enc;
    aby3::Sh3BinaryEvaluator eval;
    aby3::Sh3ShareGen gen;

    GraphParam param;

    GraphSC(const DataFrame& df, GraphParam, int _party_id, int _role, oc::Channel _prev, oc::Channel _next);
    void construct_table_from_raw_graph();
    void generate_shuffle_and_preprocess();
    void vectorized_scatter(aby3::sbMatrix& propagated_vertex_col, aby3::sbMatrix& data_col);
    void vectorized_gather(aby3::sbMatrix& dst_vertex_col, aby3::sbMatrix& data_col);
    void run();

    std::vector<size_t> common_src;
    std::vector<size_t> shuffle1_next;
    std::vector<size_t> shuffle1_prev;
    std::vector<size_t> inv_shuffle1_next;
    std::vector<size_t> inv_shuffle1_prev;

    std::vector<size_t> shuffle2_next;
    std::vector<size_t> shuffle2_prev;
    std::vector<size_t> inv_shuffle2_next;
    std::vector<size_t> inv_shuffle2_prev;

    std::vector<size_t> open_sort_by_src;
    std::vector<size_t> inv_open_sort_by_src;
    std::vector<size_t> open_sort_by_dst;
    std::vector<size_t> inv_open_sort_by_dst;

    aby3::sbMatrix src_before_Scatter;
    aby3::sPackedBin is_edge_before_Scatter;
    aby3::sbMatrix dst_before_Gather;
    aby3::sPackedBin is_vertex_before_Gather;

    size_t table_rows;
    size_t table_cols;

};