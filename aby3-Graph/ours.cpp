#include "ours.h"
#include "shuffle.h"
#include <algorithm>
#include <cryptoTools/Circuit/BetaLibrary.h>
#include <cmath>

#include "utils.h"

using namespace oc;
using namespace aby3;

void our_scatter(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& srcTag, 
    const std::vector<u64>& dstTag, 
    const i64Matrix& inputShare,
    i64Matrix& outputShare,
    Alg alg
) {
    BetaLibrary lib;
    auto multCir_64 = lib.uint_uint_mult(64, 64, 64);
    auto addCir_64 = lib.int_int_add(64, 64, 64);

    u64 byteSize = inputShare.cols() * 8;
    i64Matrix inputShare_preScatter(inputShare.rows(), inputShare.cols());
    i64Matrix inputScaler(inputShare.rows(), inputShare.cols());
    if (alg == Alg::CC) {
        inputShare_preScatter = inputShare;
    } else if (alg == Alg::SP) {
        inputShare_preScatter = inputShare;   
    } else if (alg == Alg::PR) {
        run_ConditionalMerge(
            prevChl,
            nextChl,
            role,
            inputShare,
            inputScaler,
            Matrix<u8>(),
            inputShare_preScatter,
            multCir_64,
            false,
            false        
        );     
    } else {
        printf("Unexpected Scatter Op in Ours!\n");
        exit(-1);
    }
    Matrix<u8> inputShare_byte(inputShare.rows(), byteSize);
    Matrix<u8> outputShare_byte(dstTag.size(), byteSize);
    intMat2ByteMat(
        inputShare_preScatter,
        inputShare_byte,
        byteSize
    );
    run_OEP(
        prevChl,
        nextChl,
        role,
        srcTag, 
        dstTag, 
        inputShare_byte,
        outputShare_byte        
    ); 
    outputShare.resize(outputShare_byte.rows(), inputShare.cols());
    byteMat2intMat(outputShare_byte, outputShare);

    // printf("output_of_scatter: (leng = %lu)\n", dstTag.size());
    // reconstruct_and_print_matrix(outputShare_byte, pIdx, role, prevChl, nextChl); 

    i64Matrix edgeShare(outputShare.rows(), outputShare.cols());
    if (alg == Alg::CC) {
        // Do nothing.
    } else if (alg == Alg::SP) {
        run_ConditionalMerge(
            prevChl,
            nextChl,
            role,
            outputShare,
            edgeShare,
            Matrix<u8>(),
            outputShare,
            addCir_64,
            false,
            false        
        );     
    } else if (alg == Alg::PR) {
        // Do nothing
    } else {
        printf("Unexpected Scatter Op in Ours!\n");
        exit(-1);
    }
}

void our_gather(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& dstTag, 
    const std::vector<u64>& vertexTag,
    const i64Matrix& updateShare,
    const i64Matrix& vertexShare,
    i64Matrix& outputShare,
    Alg alg
) {
    u64 numUpdates = dstTag.size();
    std::vector<std::array<u64, 2>> dstTagIdx(numUpdates);
    for (u64 i = 0; i < numUpdates; ++i) dstTagIdx[i] = {dstTag[i], i};
    std::sort(
        dstTagIdx.begin(), 
        dstTagIdx.end(), 
        [](const std::array<u64, 2>& a, const std::array<u64, 2>& b){return a[0] < b[0];}
    );
    std::vector<u64> sortUpdateSrc(numUpdates);   
    std::vector<u64> sortUpdateDst(numUpdates); 
    std::vector<u64> sortedUpdateTag(numUpdates);
    for (u64 i = 0; i < numUpdates; ++i) {
        sortUpdateSrc[i] = i;
        sortUpdateDst[i] = dstTagIdx[i][1];
        sortedUpdateTag[i] = dstTagIdx[i][0];
    }
    u64 byteSize = updateShare.cols() * 8;
    Matrix<u8> sortedUpdateShare_byte(updateShare.rows(), byteSize);
    // run_OEP(
    //     prevChl,
    //     nextChl,
    //     role,
    //     sortUpdateSrc, 
    //     sortUpdateDst, 
    //     updateShare,
    //     sortedUpdateShare        
    // );
    Matrix<u8> updateShare_byte(updateShare.rows(), byteSize);
    intMat2ByteMat(
        updateShare,
        updateShare_byte,
        byteSize
    );

    // if (role == 0) printf("input_of_gather: (leng = %lu)\n", updateShare.rows());
    // reconstruct_and_print_matrix(updateShare_byte, pIdx, role, prevChl, nextChl);     

    run_OP(
        prevChl,
        nextChl,
        role,
        sortUpdateDst, 
        updateShare_byte,
        sortedUpdateShare_byte        
    );

    // if (role == 0) print_vector_lock(sortUpdateDst);
    // if (role == 0) print_vector_lock(sortedUpdateTag);
    // if (role == 0) printf("sortedUpdateShare_byte: (leng = %lu)\n", sortedUpdateShare_byte.rows());
    // reconstruct_and_print_matrix(sortedUpdateShare_byte, pIdx, role, prevChl, nextChl);  

    // if (pIdx == 0 && role == 0) {
    //     print_vector_lock(sortedUpdateTag);
    //     // print_vector_lock(sortUpdateSrc);
    //     // print_vector_lock(sortUpdateDst);
    // }

    // reconstruct_and_print_matrix(sortedUpdateShare, pIdx, role, prevChl, nextChl); 

    i64Matrix unaggedUpdateShare_int(updateShare.rows(), updateShare.cols());
    byteMat2intMat(sortedUpdateShare_byte, unaggedUpdateShare_int);
    i64Matrix aggedUpdateShare_int(updateShare.rows(), updateShare.cols());

    BetaLibrary lib;
    BetaCircuit* mergeCir;
    BetaCircuit minCir;
    if (alg == Alg::CC) {
        mergeCir = lib.int_int_bitwiseOr(64, 64, 64);
    } else if (alg == Alg::SP) {
        get_min_Circ(minCir, 64);
        mergeCir = &minCir; 
    } else if (alg == Alg::PR) {
        mergeCir = lib.int_int_add(64, 64, 64);    
    } else {
        printf("Unexpected Scatter Op in Ours!\n");
        exit(-1);
    }    
    // orCir->levelByAndDepth();    

#ifdef REMOVE_OGA
    aggedUpdateShare_int = unaggedUpdateShare_int;
#else
    if (role == 0) printf("OGA\n");
    run_OGA(
        prevChl,
        nextChl,
        role,
        sortedUpdateTag, 
        unaggedUpdateShare_int,
        aggedUpdateShare_int,
        mergeCir,
        false
    );
#endif

    Matrix<u8> aggedUpdateShare(updateShare.rows(), byteSize);
    intMat2ByteMat(
        aggedUpdateShare_int,
        aggedUpdateShare,
        byteSize
    );

    // if (role == 0) printf("aggedUpdateShare: (leng = %lu)\n", aggedUpdateShare.rows());
    // reconstruct_and_print_matrix(aggedUpdateShare, pIdx, role, prevChl, nextChl);  

    Matrix<u8> aggedUpdateShareExtracted(vertexShare.rows(), byteSize);
    run_OEP(
        prevChl,
        nextChl,
        role,
        sortedUpdateTag, 
        vertexTag, 
        aggedUpdateShare,
        aggedUpdateShareExtracted        
    );
    i64Matrix aggedUpdateShareExtracted_int(vertexShare.rows(), vertexShare.cols());
    byteMat2intMat(aggedUpdateShareExtracted, aggedUpdateShareExtracted_int);

    outputShare.resize(vertexShare.rows(), byteSize);
    // printf(">>> %lu %lu %lu\n", aggedUpdateShare.cols(), outputShare.cols(), vertexShare.cols());
    run_ConditionalMerge(
        prevChl,
        nextChl,
        role,
        aggedUpdateShareExtracted_int,
        vertexShare,
        Matrix<u8>(),
        outputShare,
        mergeCir,
        false,
        false        
    );

    Matrix<u8> outputShare_byte(outputShare.rows(), byteSize);
    intMat2ByteMat(
        outputShare,
        outputShare_byte,
        byteSize
    );

    // if (role == 0) printf("outputShare: (leng = %lu)\n", outputShare_byte.rows());
    // reconstruct_and_print_matrix(outputShare_byte, pIdx, role, prevChl, nextChl);  

    // reconstruct_and_print_matrix(outputShare, pIdx, role, prevChl, nextChl);
}

u64 nextPowerOfTwo(u64 n) {
    if (n == 0) return 1;
    if ((n & (n - 1)) == 0) return n;
    return 1ULL << (64 - __builtin_clzll(n - 1));
}

void appendToMakePowerOfTwoGroups(std::vector<u64>& dstTag) {
    if (dstTag.empty()) return;
    std::vector<u64> dstTagCopy = dstTag;

    // Sort to group identical elements
    std::sort(dstTagCopy.begin(), dstTagCopy.end());

    u64 prevTag = dstTagCopy[0];
    u64 count = 1;
    
    for (auto it = dstTagCopy.begin() + 1; it != dstTagCopy.end();) {
        if (*it == prevTag) {
            count++;
            it++;
        } else {
            // Calculate required elements for current group
            const u64 nextPower = nextPowerOfTwo(count); // C++20, use alternative for older compilers
            const u64 need = nextPower - count;
            
            // Append 'need' copies of prevTag
            dstTag.insert(dstTag.end(), need, prevTag);
            
            // Reset for next group
            prevTag = *it;
            count = 1;
            it++;
        }
    }

    // Process the last group
    const u64 nextPower = nextPowerOfTwo(count);
    const u64 need = nextPower - count;
    dstTag.insert(dstTag.end(), need, prevTag);
}

void our_gather_dummied(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& dstTag, 
    const std::vector<u64>& vertexTag,
    const i64Matrix& updateShare,
    const i64Matrix& vertexShare,
    i64Matrix& outputShare,
    Alg alg
) {
    // Caveat: This function is only for cost evaluation
    u64 numUpdates = dstTag.size();
    u64 dummiedNum = 2 * numUpdates - 1;
    std::vector<u64> dummiedDstTag = dstTag;
    // for (u64 i = numUpdates; i < dummiedNum; ++i) {
    //     dummiedDstTag[i] = (u64)-1;
    // }
    appendToMakePowerOfTwoGroups(dummiedDstTag);
    dummiedDstTag.insert(dummiedDstTag.end(), dummiedNum - dummiedDstTag.size(), -1);

    i64Matrix dummiedUpdateShare(dummiedNum, updateShare.cols());
    dummiedUpdateShare.setZero();
    memcpy(dummiedUpdateShare.data(), updateShare.data(), updateShare.size() * sizeof(i64));

    our_gather(
        prevChl, 
        nextChl,
        pIdx,
        role,
        dummiedDstTag,
        vertexTag,
        dummiedUpdateShare,
        vertexShare,
        outputShare,
        alg
    );
}

void our_extract(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& vertexTag, 
    const i64Matrix& inputShare,
    i64Matrix& outputShare,
    Alg alg
) {
    BetaLibrary lib;
    auto multCir_64 = lib.uint_uint_mult(64, 64, 64);
    auto addCir_64 = lib.int_int_add(64, 64, 64);

    u64 byteSize = inputShare.cols() * 8;
    u64 bitSize = inputShare.cols() * 64;
    u64 width = inputShare.rows();
    // i64Matrix inputShare_preScatter(inputShare.rows(), inputShare.cols());
    // i64Matrix inputScaler(inputShare.rows(), inputShare.cols());
    outputShare.resize(width, inputShare.cols());
    
    if (alg == Alg::CC) {
        // Reorder the vertex list to selected out the group we care;
        std::vector<u64> orderedTag = vertexTag; // This is mocked
        Matrix<u8> inputShare_byte(width, byteSize);
        Matrix<u8> orderedShare_byte(width, byteSize);
        intMat2ByteMat(
            inputShare,
            inputShare_byte,
            byteSize
        );
        run_OEP(
            prevChl,
            nextChl,
            role,
            vertexTag, 
            orderedTag, 
            inputShare_byte,
            orderedShare_byte        
        ); 
        
        // Aggregate the Connected label;
        i64Matrix orderedShare_int(width, inputShare.cols());
        byteMat2intMat(orderedShare_byte, orderedShare_int);
        auto mergeCir = lib.int_int_bitwiseOr(64, 64, 64);
        std::vector<u64> aggTag = vertexTag; // This is mocked

#ifdef REMOVE_OGA
        outputShare = orderedShare_int;
#else
        run_OGA(
            prevChl,
            nextChl,
            role,
            aggTag, // This is mocked
            orderedShare_int,
            outputShare,
            mergeCir,
            false
        );
#endif

    } else if (alg == Alg::SP) {
        // Merge the Connected labels  
        i64Matrix labelReversed(inputShare.rows(), inputShare.cols());
        i64Matrix merged(inputShare.rows(), inputShare.cols());
        run_ConditionalMerge(
            prevChl,
            nextChl,
            role,
            inputShare,
            labelReversed,
            Matrix<u8>(),
            merged,
            multCir_64,
            false,
            false        
        );             
        // Shuffle
        // Convert input to secret form
        CommPkg comm = {prevChl, nextChl};
        Sh3Runtime rt(role, comm);
        Sh3Encryptor enc;
        enc.init(role, toBlock(role), toBlock((role + 1) % 3));
        Sh3BinaryEvaluator eval;    
        eval.mPrng.SetSeed(toBlock(role));
        Sh3ShareGen gen;
        gen.init(toBlock(role), toBlock((role + 1) % 3));

        sbMatrix Merged(width, bitSize);
        sbMatrix Shuffled(width, bitSize);

        auto task = rt.noDependencies();
        
        if (role == 0 || role == 1) {
        // if (role == 1) {
            task = enc.localBinMatrix(task, merged, Merged);
        } else {
            task = enc.remoteBinMatrix(task, Merged);
        } 
        std::vector<size_t> shuffle_next(width);
        std::vector<size_t> shuffle_prev(width);
        for (u64 i = 0; i < width; ++i) {
            shuffle_next[i] = i;
            shuffle_prev[i] = i;
        }
        shuffle(
            prevChl,
            nextChl,
            role,
            shuffle_prev,
            shuffle_next,
            Merged,
            Shuffled,
            enc
        );
        // Selectively Open
        i64Matrix opened(width, inputShare.cols());
        task = enc.revealAll(task, Shuffled, opened);
        task.get();
    } else {
        printf("Unexpected Alg in Ours Extract!\n");
        exit(-1);
    }
}
