#pragma once

#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>
#include <aby3-DB/OblvPermutation.h>
#include <aby3-DB/OblvSwitchNet.h>
#include <cryptoTools/Crypto/PRNG.h>

#include "operators.h"

#include <iomanip>
#include <atomic>
#include <random>

void run_OEP(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    const std::vector<aby3::u64>& srcTag, 
    const std::vector<aby3::u64>& dstTag, 
    const oc::Matrix<aby3::u8>& inputShare,
    oc::Matrix<aby3::u8>& outputShare
);

void run_OP(
    oc::Channel& prevChl,
    oc::Channel& nextChl,
    int role,
    const std::vector<aby3::u64>& perm, 
    const oc::Matrix<aby3::u8>& inputShare,
    oc::Matrix<aby3::u8>& outputShare
);

