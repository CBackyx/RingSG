#pragma once

#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Crypto/PRNG.h"

#include <cryptoTools/Circuit/BetaLibrary.h>

enum Alg {
    CC,
    SP,
    PR
};

void get_multiplex_Circ(
    oc::BetaCircuit& cd,
    aby3::u64 elementSize
);

void get_compare_select_Circ(
    oc::BetaCircuit& cd,
    aby3::u64 elementSize
);

void get_min_Circ(
    oc::BetaCircuit& cd,
    aby3::u64 elementSize
);