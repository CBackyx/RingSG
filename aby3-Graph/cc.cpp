#include "cc.h"
#include <algorithm>
#include <cryptoTools/Circuit/BetaLibrary.h>

#include "utils.h"

using namespace oc;
using namespace aby3;

void scatter(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& srcTag, 
    const std::vector<u64>& dstTag, 
    const Matrix<u8>& inputShare,
    Matrix<u8>& outputShare
) {
    run_OEP(
        prevChl,
        nextChl,
        role,
        srcTag, 
        dstTag, 
        inputShare,
        outputShare        
    ); 
}

void gather(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<u64>& dstTag, 
    const std::vector<u64>& vertexTag,
    const Matrix<u8>& updateShare,
    const Matrix<u8>& vertexShare,
    Matrix<u8>& outputShare
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
    Matrix<u8> sortedUpdateShare(updateShare.rows(), updateShare.cols());
    // run_OEP(
    //     prevChl,
    //     nextChl,
    //     role,
    //     sortUpdateSrc, 
    //     sortUpdateDst, 
    //     updateShare,
    //     sortedUpdateShare        
    // );
    run_OP(
        prevChl,
        nextChl,
        role,
        sortUpdateDst, 
        updateShare,
        sortedUpdateShare        
    );

    // if (pIdx == 0 && role == 0) {
    //     print_vector_lock(sortedUpdateTag);
    //     // print_vector_lock(sortUpdateSrc);
    //     // print_vector_lock(sortUpdateDst);
    // }

    // reconstruct_and_print_matrix(sortedUpdateShare, pIdx, role, prevChl, nextChl); 

    i64Matrix unaggedUpdateShare_int(updateShare.rows(), (updateShare.cols() + 7) / 8);
    byteMat2intMat(sortedUpdateShare, unaggedUpdateShare_int);
    i64Matrix aggedUpdateShare_int(updateShare.rows(), (updateShare.cols() + 7) / 8);

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    // orCir->levelByAndDepth();    
    run_OGA(
        prevChl,
        nextChl,
        role,
        sortedUpdateTag, 
        unaggedUpdateShare_int,
        aggedUpdateShare_int,
        orCir_64,
        false
    );
    Matrix<u8> aggedUpdateShare(updateShare.rows(), updateShare.cols());
    intMat2ByteMat(
        aggedUpdateShare_int,
        aggedUpdateShare,
        1
    );

    Matrix<u8> aggedUpdateShareExtracted(vertexShare.rows(), vertexShare.cols());
    run_OEP(
        prevChl,
        nextChl,
        role,
        sortedUpdateTag, 
        vertexTag, 
        aggedUpdateShare,
        aggedUpdateShareExtracted        
    );

    auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    outputShare.resize(vertexShare.rows(), vertexShare.cols());
    // printf(">>> %lu %lu %lu\n", aggedUpdateShare.cols(), outputShare.cols(), vertexShare.cols());
    run_ConditionalMerge(
        prevChl,
        nextChl,
        role,
        aggedUpdateShareExtracted,
        vertexShare,
        Matrix<u8>(),
        outputShare,
        orCir_8,
        false,
        false        
    );

    // reconstruct_and_print_matrix(outputShare, pIdx, role, prevChl, nextChl);
}