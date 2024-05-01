#pragma once

#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <cryptoTools/Circuit/BetaLibrary.h>

#include "operators.h"

#include <iomanip>
#include <atomic>
#include <random>

void byteMat2intMat(
    const oc::Matrix<aby3::u8>& input,
    aby3::i64Matrix& output
);

void intMat2ByteMat(
    const aby3::i64Matrix& input,
    oc::Matrix<aby3::u8>& output,
    aby3::u64 byteSize
);

void evalConditionalMerge(
    const aby3::sbMatrix& A,
    const aby3::sbMatrix& B,
    const aby3::sPackedBin& C,
    aby3::sbMatrix& D,
    aby3::u64 width,
    aby3::u64 bitSize,
    oc::BetaCircuit* mergeCir,
    oc::BetaCircuit* multiplexCir,
    aby3::Sh3BinaryEvaluator& eval,
    aby3::Sh3ShareGen& gen,
    aby3::Sh3Runtime& rt
);

void evalConditionalMerge(
    const aby3::sPackedBin& A,
    const aby3::sPackedBin& B,
    const aby3::sPackedBin& C,
    aby3::sPackedBin& D,
    aby3::u64 width,
    aby3::u64 bitSize,
    oc::BetaCircuit* mergeCir,
    oc::BetaCircuit* multiplexCir,
    aby3::Sh3BinaryEvaluator& eval,
    aby3::Sh3ShareGen& gen,
    aby3::Sh3Runtime& rt
);

void run_OGA(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    std::vector<aby3::u64> groupId, 
    aby3::i64Matrix input,
    aby3::i64Matrix& output,
    oc::BetaCircuit* mergeCir,
    bool isRevealAll = true
);

void run_ConditionalMerge(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    const oc::Matrix<aby3::u8>& a,
    const oc::Matrix<aby3::u8>& b,
    const oc::Matrix<aby3::u8>& c,
    oc::Matrix<aby3::u8>& d,
    oc::BetaCircuit* mergeCir,
    bool isConditional,
    bool isRevealAll = true
);

