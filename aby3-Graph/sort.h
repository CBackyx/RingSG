#pragma once

#include "shuffle.h"

void ss_bitonic_sort(
    aby3::sbMatrix& inputs_share,
    aby3::Sh3BinaryEvaluator& eval,
    aby3::Sh3ShareGen& gen,
    aby3::Sh3Runtime& rt,
    aby3::Sh3Encryptor& enc
);

void ss_open_sort(
    aby3::sbMatrix& v, 
    std::vector<aby3::u64>& perm,
    aby3::Sh3BinaryEvaluator& eval,
    aby3::Sh3ShareGen& gen,
    aby3::Sh3Runtime& rt,
    aby3::Sh3Encryptor& enc  
);