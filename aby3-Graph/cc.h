#pragma once

#include "operators.h"
#include "OGA.h"
#include "OEP.h"

void scatter(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& srcTag, 
    const std::vector<aby3::u64>& dstTag, 
    const oc::Matrix<aby3::u8>& inputShare,
    oc::Matrix<aby3::u8>& outputShare
);

void gather(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& dstTag, 
    const std::vector<aby3::u64>& vertexTag,
    const oc::Matrix<aby3::u8>& updateShare,
    const oc::Matrix<aby3::u8>& vertexShare,
    oc::Matrix<aby3::u8>& outputShare
);

