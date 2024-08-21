#include "ours.h"
#include <algorithm>
#include <cryptoTools/Circuit/BetaLibrary.h>

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
    auto multCir_64 = lib.int_int_mult(64, 64, 64);
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
    run_OP(
        prevChl,
        nextChl,
        role,
        sortUpdateDst, 
        updateShare_byte,
        sortedUpdateShare_byte        
    );

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
    Matrix<u8> aggedUpdateShare(updateShare.rows(), byteSize);
    intMat2ByteMat(
        aggedUpdateShare_int,
        aggedUpdateShare,
        byteSize
    );

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

    // reconstruct_and_print_matrix(outputShare, pIdx, role, prevChl, nextChl);
}