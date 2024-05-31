#include "sort.h"

#include <vector>
#include <array>

using namespace oc;
using namespace aby3;
using namespace std;

u64 greatest_power_of_two_lessthan(u64 n) {
    u64 k = 1;

    while (k < n) {
        k = k << 1;
    }

    return (k >> 1);
}

void bitonic_merge(
    u64 lo,
    u64 n,
    bool dir,
    std::vector<std::pair<std::vector<u64>, std::vector<u64>>>& seq,
    u64& depth
) {
    if (n > 1) {
        u64 m = greatest_power_of_two_lessthan(n);

        if (seq.size() < (depth + 1)) {
            seq.resize(depth + 1, std::make_pair(std::vector<u64>(), std::vector<u64>()));
        }

        for (u64 i = lo; i < (lo + n - m); i++) {
            if (dir) {
                seq[depth].first.push_back(i);
                seq[depth].second.push_back(i + m);
            } else {
                seq[depth].first.push_back(i + m);
                seq[depth].second.push_back(i);
            }
        }

        depth = depth + 1;

        u64 lower_depth = depth;
        bitonic_merge(lo, m, dir, seq, lower_depth);

        u64 upper_depth = depth;
        bitonic_merge(lo + m, n - m, dir, seq, upper_depth);

        depth = std::max(lower_depth, upper_depth);
    }
}

void bitonic_sort(
    u64 lo,
    u64 n,
    bool dir,
    std::vector<std::pair<std::vector<u64>, std::vector<u64>>>& seq,
    u64& depth
) {
    if (n > 1) {
        u64 m = n / 2;

        u64 lower_depth = depth;
        bitonic_sort(lo, m, !dir, seq, lower_depth);

        u64 upper_depth = depth;
        bitonic_sort(lo + m, n - m, dir, seq, upper_depth);

        depth = std::max(lower_depth, upper_depth) + 1;

        bitonic_merge(lo, n, dir, seq, depth);
    }
}

void plaintext_cmp_swap(std::vector<i64>& inputs, const std::vector<u64>& lhs_indices, const std::vector<u64>& rhs_indices) {
    int length = lhs_indices.size();
    for (int i = 0; i < length; ++i) {
        if (inputs[rhs_indices[i]] < inputs[lhs_indices[i]]) {
            i64 tmp = inputs[lhs_indices[i]];
            inputs[lhs_indices[i]] = inputs[rhs_indices[i]];
            inputs[rhs_indices[i]] = tmp;
        }
    }
}

// A helper function to swap two elements
void swap(i64& a, i64& b) {
    i64 temp = a;
    a = b;
    b = temp;
}

void swap(u64& a, u64& b) {
    u64 temp = a;
    a = b;
    b = temp;
}

void swap(sbMatrix& a, sbMatrix& b, u64 a_index, u64 b_index) {
    u64 cols = a.i64Cols();
    sbMatrix tmp(1, cols * 64);
    for (u64 i = 0; i < cols; ++i) {
        tmp.mShares[0](0, i) = a.mShares[0](a_index, i);
        tmp.mShares[1](0, i) = a.mShares[1](a_index, i);
        a.mShares[0](a_index, i) = b.mShares[0](b_index, i);
        a.mShares[1](a_index, i) = b.mShares[1](b_index, i);
        b.mShares[0](b_index, i) = tmp.mShares[0](0, i);
        b.mShares[1](b_index, i) = tmp.mShares[1](0, i);
    }
}

void ss_cmp_swap(
    sbMatrix& inputs_share, 
    const std::vector<u64>& lhs_indices, 
    const std::vector<u64>& rhs_indices, 
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = lhs_indices.size();
    u64 bitSize = inputs_share.bitCount();
    sbMatrix lhs(length, bitSize);
    sbMatrix rhs(length, bitSize);
    for (int i = 0; i < length; ++i) {
        swap(lhs, inputs_share, i, lhs_indices[i]);
        swap(rhs, inputs_share, i, rhs_indices[i]);
    }

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(64, 64);
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);
    BetaCircuit *xorCir = lib.int_int_bitwiseXor(bitSize, bitSize, bitSize);

    sPackedBin C(length, 1);
    eval.setCir(ltCir, length, gen);
    eval.setInput(0, lhs);
    eval.setInput(1, rhs);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, C);

    sbMatrix Sum(length, bitSize);
    eval.setCir(xorCir, length, gen);
    eval.setInput(0, lhs);
    eval.setInput(1, rhs);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, Sum);

    // Multiplex
    eval.setCir(multiplexCir, length, gen);
    eval.setInput(0, lhs);
    eval.setInput(1, rhs);
    eval.setInput(2, C);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, lhs);

    eval.setCir(xorCir, length, gen);
    eval.setInput(0, Sum);
    eval.setInput(1, lhs);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, rhs);

    for (int i = 0; i < length; ++i) {
        swap(lhs, inputs_share, i, lhs_indices[i]);
        swap(rhs, inputs_share, i, rhs_indices[i]);
    }    
}

void plaintext_bitonic_sort(
    std::vector<i64>& inputs
) {
    std::vector<std::pair<std::vector<u64>, std::vector<u64>>> sequence;
    u64 depth = 0;
    bitonic_sort(
        0,
        inputs.size(),
        true,
        sequence,
        depth
    );

    depth = sequence.size();
    for (int i = 0; i < depth; ++i) {
        if (sequence[i].first.size() == 0) {
            continue;
        }

        plaintext_cmp_swap(inputs, sequence[i].first, sequence[i].second);
    }
}

void ss_bitonic_sort(
    sbMatrix& inputs_share,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>> sequence;
    size_t depth = 0;
    bitonic_sort(
        0,
        inputs_share.rows(),
        true,
        sequence,
        depth
    );

    depth = sequence.size();
    for (int i = 0; i < depth; ++i) {
        if (sequence[i].first.size() == 0) {
            continue;
        }

        ss_cmp_swap(inputs_share, sequence[i].first, sequence[i].second, eval, gen, rt, enc);
    }
}

// A function to partition the vector around a pivot element
u64 plaintext_partition(vector<i64>& v, u64 low, u64 high, vector<u64>& perm) {
    // Choose the last element as the pivot
    i64 pivot = v[high];
    // Initialize the index of smaller element
    u64 i = low - 1;
    // Loop through the elements from low to high - 1
    for (u64 j = low; j < high; j++) {
        // If the current element is smaller than or equal to the pivot
        if (v[j] <= pivot) {
            // Increment the index of smaller element
            i++;
            // Swap the current element with the element at i
            swap(v[i], v[j]);
            // Swap the corresponding elements in the permutation vector
            swap(perm[i], perm[j]);
        }
    }
    // Swap the pivot element with the element at i + 1
    swap(v[i + 1], v[high]);
    // Swap the corresponding elements in the permutation vector
    swap(perm[i + 1], perm[high]);
    // Return the index of the pivot element
    return i + 1;
}

// A function to perform quick sort on a vector
void plaintext_quick_sort(
    vector<i64>& v, 
    int low, 
    int high, 
    vector<u64>& perm
) {
    // Base case: if the vector has one or zero elements, it is already sorted
    if (low >= high) return;
    // Partition the vector around a pivot element and get its index
    u64 p = plaintext_partition(v, low, high, perm);
    // Recursively sort the left and right subvectors
    plaintext_quick_sort(v, low, p - 1, perm);
    plaintext_quick_sort(v, p + 1, high, perm);
}


u64 ss_partition(
    sbMatrix& v_share, 
    u64 low, 
    u64 high, 
    vector<u64>& perm, 
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    const u64 bitSize = 64;
    if (v_share.bitCount() != bitSize) {
        printf("Unexpected v_share.bitCount during ss_partition of ss_quick_sort!\n");
        exit(-1);
    }
    std::array<i64, 2> pivot = {v_share.mShares[0](high, 0), v_share.mShares[1](high, 0)};
    u64 i = low - 1;

    u64 length = high - low;
    sbMatrix lhs(length, bitSize);
    sbMatrix rhs(length, bitSize);
    for (int k = 0; k < length; ++k) {
        lhs.mShares[0](k, 0) = pivot[0];
        lhs.mShares[1](k, 0) = pivot[1];
        rhs.mShares[0](k, 0) = v_share.mShares[0](low + k, 0);
        rhs.mShares[1](k, 0) = v_share.mShares[1](low + k, 0);
    }

    PackedBin c(length, 1);
    sPackedBin C(length, 1);

    BetaLibrary lib;
    BetaCircuit *cir =  lib.int_int_lt(bitSize, bitSize);

    eval.setCir(cir, length, gen);
    eval.setInput(0, lhs);
    eval.setInput(1, rhs);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, C);    

    std::vector<bool> lt_result(length);
    enc.revealAll(rt.noDependencies(), C, c).get();
    BitIterator iter((u8*)c.mData.data(), 0);

    // printf("Here party %d : \n", partyId);
    // for (int i = 0; i < length; ++i) {
    //     printf("%d ", gt_result[i + low]? 1 : 0);
    // }
    // printf("\n");

    for (u64 j = low; j < high; j++) {
        lt_result[j - low] = (*iter == 1); 
        if (lt_result[j - low]) {
            i++;
            swap(v_share, v_share, i, j);
            swap(perm[i], perm[j]);
        }
        iter++;
    }

    swap(v_share, v_share, i + 1, high);
    swap(perm[i + 1], perm[high]);
    return i + 1;
}

void ss_quick_sort(
    sbMatrix& v, 
    int low, 
    int high, 
    vector<u64>& perm,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    if (low >= high) return;
    u64 p = ss_partition(v, low, high, perm, eval, gen, rt, enc);
    ss_quick_sort(v, low, p - 1, perm, eval, gen, rt, enc);
    ss_quick_sort(v, p + 1, high, perm, eval, gen, rt, enc);
}

void ss_open_sort(
    sbMatrix& v, 
    vector<u64>& perm,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc  
) {
    ss_quick_sort(v, 0, v.rows() - 1, perm, eval, gen, rt, enc);
}