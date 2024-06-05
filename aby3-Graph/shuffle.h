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
#include <vector>

void shuffle(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    oc::block seed,
    const aby3::sbMatrix& input,
    aby3::sbMatrix& output,
    std::vector<aby3::u64>& prevPerm,
    std::vector<aby3::u64>& nextPerm,
    aby3::Sh3Encryptor& enc
);

// For reverse shuffle or negotiated shuffle
void shuffle(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    const std::vector<aby3::u64>& prevPerm,
    const std::vector<aby3::u64>& nextPerm,
    const aby3::sbMatrix& input,
    aby3::sbMatrix& output,
    aby3::Sh3Encryptor& enc
);

void reverse_shuffle(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    const std::vector<aby3::u64>& prevPerm,
    const std::vector<aby3::u64>& nextPerm,
    const aby3::sbMatrix& input,
    aby3::sbMatrix& output,
    aby3::Sh3Encryptor& enc    
);