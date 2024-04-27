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
    int pIdx,
    std::vector<aby3::u64> groupId, 
    aby3::i64Matrix input,
    aby3::i64Matrix& output,
    oc::BetaCircuit* mergeCir
);

