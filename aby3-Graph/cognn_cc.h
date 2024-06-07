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
    const oc::Matrix<aby3::u8>& inputShare,
    oc::Matrix<aby3::u8>& outputShare
);

void cognn_gather(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<oc::Matrix<aby3::u8>>& updateShares,
    const oc::Matrix<aby3::u8>& vertexShare,
    oc::Matrix<aby3::u8>& outputShare
);

