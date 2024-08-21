#include "graphsc.h"
#include "OGA.h"

#include <cassert>
#include <set>
#include <chrono>
#include <fstream>

using namespace oc;
using namespace aby3;

// Function to load DataFrame from csv file
DataFrame load_dataframe_from_csv(std::string file_name, bool skip_first_row, bool skip_first_col) {
  // Create an empty DataFrame
  DataFrame df;
  // Open the csv file
  std::ifstream file(file_name);
  // Check if the file is opened successfully
  if (file.is_open()) {
    // Read each line of the file
    std::string line;
    if (skip_first_row) {
        std::getline(file, line); // The first line is not used.
    }
    while (std::getline(file, line)) {
      // Create an empty vector to store the values in the line
      std::vector<double> row;
      // Use a stringstream to parse the line
      std::stringstream ss(line);
      // Read each value separated by comma
      std::string value;
      if (skip_first_col) {
        std::getline(ss, value, ','); // The first element (ID) is not considered.
      }
      while (std::getline(ss, value, ',')) {
        // Convert the value to double and push it to the row vector
        row.push_back(std::stod(value));
      }
      // Push the row vector to the DataFrame
      df.push_back(row);
    }
    // Close the file
    file.close();
  }
  // Return the DataFrame
  return df;
}

std::vector<size_t> get_inverse_permutation(const std::vector<size_t>& permutation) {
    size_t perm_size = permutation.size();
    std::vector<size_t> inv_permutation(perm_size);
	for (int i=0; i < perm_size; ++i) {
		inv_permutation[permutation[i]] = i;
	}    
    return inv_permutation;
}

std::vector<std::vector<u64>> open_permute(std::vector<std::vector<u64>>& v_share, const std::vector<size_t>& perm) {
    assert(v_share.size() == perm.size());
    size_t perm_size = perm.size();
    std::vector<std::vector<u64>> result(perm_size);
    for (int i = 0; i < perm_size; ++i) {
        result[i] = v_share[perm[i]];
    }
    return result;
}

sbMatrix open_permute(sbMatrix& v_share, const std::vector<size_t>& perm) {
    assert(v_share.rows() == perm.size());
    size_t perm_size = perm.size();
    sbMatrix result(perm_size, v_share.bitCount());
    u64 cols = v_share.i64Cols();
    for (u64 i = 0; i < perm_size; ++i) {
        for (u64 j = 0; j < cols; ++j) {
            result.mShares[0](i, j) = v_share.mShares[0](perm[i], j);
            result.mShares[1](i, j) = v_share.mShares[1](perm[i], j);
        }
    }
    return result;
}

sbMatrix extract_col(sbMatrix& v_share, size_t col_index) {
    size_t rows = v_share.rows();
    sbMatrix result(rows, 64);
    for (u64 i = 0; i < rows; ++i) {
        result.mShares[0](i, 0) = v_share.mShares[0](i, col_index);
        result.mShares[1](i, 0) = v_share.mShares[1](i, col_index);
    }
    return result;
}

void fill_col(sbMatrix& v_share, const sbMatrix& col, size_t col_index) {
    size_t rows = v_share.rows();
    for (u64 i = 0; i < rows; ++i) {
        v_share.mShares[0](i, col_index) = col.mShares[0](i, 0);
        v_share.mShares[1](i, col_index) = col.mShares[1](i, 0);
    }
}

template <typename T>
Matrix<T> open_permute(Matrix<T>& v_share, const std::vector<size_t>& perm) {
    assert(v_share.rows() == perm.size());
    size_t perm_size = perm.size();
    Matrix<T> result(perm_size, v_share.cols());
    u64 cols = v_share.cols();
    for (u64 i = 0; i < perm_size; ++i) {
        for (u64 j = 0; j < cols; ++j)
            result(i, j) = v_share(perm[i], j);
    }
    return result;
}

GraphSC::GraphSC(const DataFrame& df, GraphParam _gp, int _party_id, int _role, Channel _prev, Channel _next) {
    assert(df.size() > 0);
    assert(df[0].size() > 0);
    raw_graph = df;
    param = _gp;
    party_id = _party_id;
    role = _role;
    prevChl = _prev;
    nextChl = _next;

    CommPkg comm = {prevChl, nextChl};
    rt.init(role, comm);
    enc.init(role, toBlock(role), toBlock((role + 1) % 3)); 
    eval.mPrng.SetSeed(toBlock(role));
    gen.init(toBlock(role), toBlock((role + 1) % 3));    
}

void GraphSC::construct_table_from_raw_graph() {
    // print_vector_of_vector(raw_graph, 10);
    std::set<uint64_t> vertex_set;
    size_t num_edges = raw_graph.size();
    std::vector<std::vector<uint64_t>> _table;
    _table.resize(num_edges);
    if (raw_graph[0].size() == 2) { // Only vertex indices are provided in the raw graph
        for (int i = 0; i < num_edges; ++i) {
            _table[i] = {(uint64_t)raw_graph[i][0], (uint64_t)raw_graph[i][1], (uint64_t)1, (uint64_t)1};
            vertex_set.insert((uint64_t)raw_graph[i][0]);
            vertex_set.insert((uint64_t)raw_graph[i][1]);
        }
        for (auto it = vertex_set.begin(); it != vertex_set.end(); ++it) {
            _table.push_back({*it, *it, (uint64_t)0, (uint64_t)-1});
        }
    }
    // print_vector_of_vector(table, table.size());
    table_rows = _table.size();
    table_cols = _table[0].size();
    plainTable.resize(table_rows, table_cols);
    for (u64 i = 0; i < table_rows; ++i) {
        for (u64 j = 0; j < table_cols; ++j) {
            plainTable(i, j) = _table[i][j];
        }
    }
    printf("whole table size = %ld\n",table_rows);
    printf("number of edges = %ld\n", num_edges);
    printf("number of vertices = %ld\n", table_rows - num_edges);
    
    table.resize(table_rows, table_cols * 64);
    if (role == 0) enc.localBinMatrix(rt.noDependencies(), plainTable, table).get();
    else enc.remoteBinMatrix(rt.noDependencies(), table).get();
    
    // table = transpose(table);
}

void GraphSC::generate_shuffle_and_preprocess() {
    sbMatrix tmp_input(table_rows, 64);
    sbMatrix tmp_output(table_rows, 64);
    shuffle(
        prevChl,
        nextChl,
        role,
        toBlock(role),
        tmp_input,
        tmp_output,
        shuffle1_prev,
        shuffle1_next,
        enc
    );
    inv_shuffle1_prev = get_inverse_permutation(shuffle1_prev);
    inv_shuffle1_next = get_inverse_permutation(shuffle1_next);
    shuffle(
        prevChl,
        nextChl,
        role,
        toBlock(role + 3),
        tmp_input,
        tmp_output,
        shuffle2_prev,
        shuffle2_next,
        enc
    );
    inv_shuffle2_prev = get_inverse_permutation(shuffle2_prev);
    inv_shuffle2_next = get_inverse_permutation(shuffle2_next);

    common_src.resize(table_rows);
    for (u64 i = 0; i < table_rows; ++i) common_src[i] = i;

    // print_duration(t_shuffle_preprocess, "duration shuffle preprocess");
}

void concat_src_with_is_edge(const std::vector<std::vector<uint64_t>>& table, std::vector<uint64_t>& concat) {
    assert(table.size() >= 3);
    assert(table[0].size() > 0);
    size_t table_rows = table[0].size();
    concat.resize(table_rows);
    for (int i = 0; i < table_rows; ++i) {
        concat[i] = (table[0][i] << 1) + table[2][i];
    }
}

void concat_src_with_is_edge(const sbMatrix& table, sbMatrix& concat) {
    assert(table.mShares[0].cols() >= 3);
    assert(table.mShares[0].rows() > 0);
    size_t table_rows = table.mShares[0].rows();
    concat.resize(table_rows, 64);
    for (int i = 0; i < table_rows; ++i) {
        concat.mShares[0](i, 0) = (table.mShares[0](i, 0) << 1) + (table.mShares[0](i, 2) & 1UL);
        concat.mShares[1](i, 0) = (table.mShares[1](i, 0) << 1) + (table.mShares[1](i, 2) & 1UL);
    }
}

void concat_dst_with_is_edge(const std::vector<std::vector<uint64_t>>& table, std::vector<uint64_t>& concat) {
    assert(table.size() >= 3);
    assert(table[0].size() > 0);
    size_t table_rows = table[0].size();
    concat.resize(table_rows);
    for (int i = 0; i < table_rows; ++i) {
        concat[i] = (table[1][i] << 1) + table[2][i];
    }
}

void concat_dst_with_is_edge(const sbMatrix& table, sbMatrix& concat) {
    assert(table.mShares[0].cols() >= 3);
    assert(table.mShares[0].rows() > 0);
    size_t table_rows = table.mShares[0].rows();
    concat.resize(table_rows, 64);
    for (int i = 0; i < table_rows; ++i) {
        concat.mShares[0](i, 0) = (table.mShares[0](i, 1) << 1) + (table.mShares[0](i, 2) & 1UL);
        concat.mShares[1](i, 0) = (table.mShares[1](i, 1) << 1) + (table.mShares[1](i, 2) & 1UL);
    }
}

void GraphSC::vectorized_scatter(sbMatrix& propagated_vertex_col, sbMatrix& data_col) {
    sbMatrix scattered(data_col.rows(), data_col.bitCount());

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    auto ltCir_64 =  lib.int_int_lt(64, 64);
    auto addCir_64 =  lib.int_int_add(64, 64, 64);
    auto multCir_64 = lib.int_int_mult(64, 64, 64);
    auto multiplexCir =  lib.int_int_multiplex(64);
    u64 width = data_col.rows();

    if (this->param.alg == Alg::SP) {
        eval.setCir(addCir_64, width, gen);
        eval.setInput(0, data_col);
        eval.setInput(1, propagated_vertex_col);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, scattered);
    } else if (this->param.alg == Alg::CC) {
        scattered = propagated_vertex_col;
    } else if (this->param.alg == Alg::PR) {
        eval.setCir(multCir_64, width, gen);
        eval.setInput(0, data_col);
        eval.setInput(1, propagated_vertex_col);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, scattered);
    } else {
        printf("Unexpected Scatter Op in GraphSC!\n");
        exit(-1);        
    }
    // sci::twoPartyAdd(data_col, propagated_vertex_col, scattered, party_id, party, SCALER_BITS);
    // sci::twoPartySelectedAssign(data_col, scattered, is_edge_before_Scatter, party_id, party, SCALER_BITS);
    eval.setCir(multiplexCir, width, gen);
    eval.setInput(0, scattered);
    eval.setInput(1, data_col);
    eval.setInput(2, is_edge_before_Scatter);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, data_col);
}

void GraphSC::vectorized_gather(sbMatrix& dst_vertex_col, sbMatrix& data_col) {
    BetaLibrary lib;
    auto multiplexCir =  lib.int_int_multiplex(64);
    u64 width = data_col.rows();

    AggregationOp aop;
    if (this->param.alg == Alg::CC) {
        aop = AggregationOp::OR_AGG;
    } else if (this->param.alg == Alg::SP) {
        aop = AggregationOp::MIN_AGG;
    } else if (this->param.alg == Alg::PR) {
        aop = AggregationOp::ADD_AGG;
    } else {
        printf("Unexpected Scatter Op in CoGNN!\n");
        exit(-1);
    }

    sbMatrix agg_result = prefix_network_aggregate(
        dst_vertex_col, 
        data_col, 
        aop,
        eval,
        gen,
        rt,
        enc
    );
    // sci::twoPartySelectedAssign(data_col, agg_result, is_vertex_before_Gather, party_id, party, SCALER_BITS);
    eval.setCir(multiplexCir, width, gen);
    eval.setInput(0, agg_result);
    eval.setInput(1, data_col);
    eval.setInput(2, is_vertex_before_Gather);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, data_col);
}

void GraphSC::run() {
    construct_table_from_raw_graph();
    generate_shuffle_and_preprocess();

    // Shuffle 1
    shuffle(
        prevChl,
        nextChl,
        role,
        shuffle1_prev,
        shuffle1_next,
        table,
        table,
        enc
    );

    // auto t_open_sort = std::chrono::high_resolution_clock::now();

    // Get two open sorts
    sbMatrix concat_src;
    concat_src_with_is_edge(table, concat_src);
    open_sort_by_src = common_src;
    std::cout << IoStream::lock;
    std::cout << "pIdx::" << role << " begin open sort by src." << std::endl;
    std::cout << IoStream::unlock;    
    // ss_open_sort(
    //     concat_src,
    //     open_sort_by_src,
    //     eval,
    //     gen,
    //     rt,
    //     enc
    // );
    inv_open_sort_by_src = get_inverse_permutation(open_sort_by_src);
    std::cout << IoStream::lock;
    std::cout << "pIdx::" << role << " finish open sort by src." << std::endl;
    std::cout << IoStream::unlock;    

    sbMatrix concat_dst;
    concat_dst_with_is_edge(table, concat_dst);
    // Shuffle 2
    shuffle(
        prevChl,
        nextChl,
        role,
        shuffle2_prev,
        shuffle2_next,
        concat_dst,
        concat_dst,
        enc
    );
    open_sort_by_dst = common_src;
    std::cout << IoStream::lock;
    std::cout << "pIdx::" << role << " begin open sort by dst." << std::endl;
    std::cout << IoStream::unlock;    
    // ss_open_sort(
    //     concat_dst,
    //     open_sort_by_dst,
    //     eval,
    //     gen,
    //     rt,
    //     enc
    // );
    inv_open_sort_by_dst = get_inverse_permutation(open_sort_by_dst);
    std::cout << IoStream::lock;
    std::cout << "pIdx::" << role << " finish open sort by dst." << std::endl;
    std::cout << IoStream::unlock;    

    // print_duration(t_open_sort, "t_open_sort");

    // Record src before Scatter & dst before Gather
    src_before_Scatter = extract_col(table, 0);
    src_before_Scatter = open_permute(src_before_Scatter, open_sort_by_src);
    dst_before_Gather = extract_col(table, 1);
    // Shuffle 2
    shuffle(
        prevChl,
        nextChl,
        role,
        shuffle2_prev,
        shuffle2_next,
        dst_before_Gather,
        dst_before_Gather,
        enc
    );
    dst_before_Gather = open_permute(dst_before_Gather, open_sort_by_dst);

    // Record is_edge flag before Scatter
    Matrix<u8> tmp_flag(table_rows, 1);
    if (role == 0) {
        for (u64 i = 0; i < table_rows; ++i) {
            tmp_flag(i, 0) = (u8) (table.mShares[0](i, 2) ^ table.mShares[1](i, 2));
        }
    } else if (role == 1) {
        for (u64 i = 0; i < table_rows; ++i) {
            tmp_flag(i, 0) = (u8) (table.mShares[0](i, 2));
        }        
    }
    tmp_flag = open_permute(tmp_flag, open_sort_by_src);
    is_edge_before_Scatter.reset(tmp_flag.rows(), 1);
    if (role == 0 || role == 1)
        enc.localPackedBinary(rt.noDependencies(), tmp_flag, 1, is_edge_before_Scatter).get();
    else
        enc.remotePackedBinary(rt.noDependencies(), is_edge_before_Scatter).get();

    // Record is_edge flag before Gather
    sbMatrix tmp_flag_i64 = extract_col(table, 2);
    // Shuffle 2
    shuffle(
        prevChl,
        nextChl,
        role,
        shuffle2_prev,
        shuffle2_next,
        tmp_flag_i64,
        tmp_flag_i64,
        enc
    );
    if (role == 0) {
        for (u64 i = 0; i < table_rows; ++i) {
            tmp_flag(i, 0) = (u8) (tmp_flag_i64.mShares[0](i, 0) ^ tmp_flag_i64.mShares[1](i, 0) ^ 1);
        }
    } else if (role == 1) {
        for (u64 i = 0; i < table_rows; ++i) {
            tmp_flag(i, 0) = (u8) (tmp_flag_i64.mShares[0](i, 0));
        }        
    }
    tmp_flag = open_permute(tmp_flag, open_sort_by_dst);
    is_vertex_before_Gather.reset(tmp_flag.rows(), 1);
    if (role == 0 || role == 1)
        enc.localPackedBinary(rt.noDependencies(), tmp_flag, 1, is_vertex_before_Gather).get();
    else
        enc.remotePackedBinary(rt.noDependencies(), is_vertex_before_Gather).get();

    // GAS iterations
    sbMatrix data_col = extract_col(table, 3);
    for (int i = 0; i < param.num_iters; ++i) {
        auto t_iter = std::chrono::high_resolution_clock::now();

        // Scatter
        data_col = open_permute(data_col, open_sort_by_src);
        sbMatrix propagated_vertex_data = prefix_network_propagate(
            src_before_Scatter,
            data_col,
            eval,
            gen,
            rt,
            enc
        );
        vectorized_scatter(propagated_vertex_data, data_col);

        // Gather
        data_col = open_permute(data_col, inv_open_sort_by_src);
        // Shuffle 2
        shuffle(
            prevChl,
            nextChl,
            role,
            shuffle2_prev,
            shuffle2_next,
            data_col,
            data_col,
            enc
        );
        data_col = open_permute(data_col, open_sort_by_dst);
        vectorized_gather(dst_before_Gather, data_col);
        data_col = open_permute(data_col, inv_open_sort_by_dst);
        // Inv Shuffle 2
        reverse_shuffle(
            prevChl,
            nextChl,
            role,
            inv_shuffle2_prev,
            inv_shuffle2_next,
            data_col,
            data_col,
            enc
        );

        // print_duration(t_iter, "duration per GAS iter");
        std::cout << IoStream::lock;
        std::cout << "pIdx::" << role << " iter = " << i << std::endl;
        std::cout << IoStream::unlock;
    }
    // table[3] = data_col;
    fill_col(table, data_col, 3);

    // Inv Shuffle 1
    reverse_shuffle(
        prevChl,
        nextChl,
        role,
        inv_shuffle1_prev,
        inv_shuffle1_next,
        table,
        table,
        enc
    );

}