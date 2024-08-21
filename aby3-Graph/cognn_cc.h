#pragma once

#include "operators.h"
#include "OGA.h"
#include "OEP.h"

void cognn_scatter(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    bool isLocal,
    const std::vector<aby3::u64>& srcVertexTag, 
    const std::vector<aby3::u64>& edgeSrcTag, 
    const std::vector<aby3::u64>& edgeDstTag,
    const std::vector<aby3::u64>& dstVertexTag,
    const aby3::i64Matrix& inputShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

void cognn_gather(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::i64Matrix>& updateShares,
    const aby3::i64Matrix& vertexShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

