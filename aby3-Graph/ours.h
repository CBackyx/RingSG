#pragma once

#include "operators.h"
#include "OGA.h"
#include "OEP.h"

void our_scatter(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& srcTag, 
    const std::vector<aby3::u64>& dstTag, 
    const aby3::i64Matrix& inputShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

void our_gather(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& dstTag, 
    const std::vector<aby3::u64>& vertexTag,
    const aby3::i64Matrix& updateShare,
    const aby3::i64Matrix& vertexShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

void our_gather_dummied(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& dstTag, 
    const std::vector<aby3::u64>& vertexTag,
    const aby3::i64Matrix& updateShare,
    const aby3::i64Matrix& vertexShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

void our_extract(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<aby3::u64>& vertexTag, 
    const aby3::i64Matrix& inputShare,
    aby3::i64Matrix& outputShare,
    Alg alg
);

