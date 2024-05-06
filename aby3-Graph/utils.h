#pragma once

#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <cryptoTools/Circuit/BetaLibrary.h>

#include <iomanip>
#include <atomic>
#include <random>
#include <iostream>
#include <mutex>

extern std::mutex print_mtx;

template <typename T>
void print_matrix(oc::Matrix<T> mat, int pIdx) {
    print_mtx.lock();
    auto& oo = oc::lout;
    oo << "# " << pIdx << ": " << std::endl;
    for (aby3::u64 i = 0; i < mat.rows(); ++i) oo << aby3::u64(mat(i, 0)) << " ";
    oo << std::endl;
    print_mtx.unlock();
}

template <typename T>
void reconstruct_and_print_matrix(oc::Matrix<T> mat, int pIdx, int role, oc::Channel& prevChl, oc::Channel& nextChl) {
    if (role == 1) prevChl.asyncSendCopy(mat.data(), mat.size());
    if (role == 0) {
        oc::Matrix<T> toPrint(mat.rows(), mat.cols());
        toPrint.setZero();
        nextChl.recv(toPrint.data(), toPrint.size());
        for (aby3::u64 i = 0; i < toPrint.size(); ++i) toPrint(i) ^= mat(i); 
        print_matrix(toPrint, pIdx);
    }   
}

template <typename T>
void print_vector_lock(const std::vector<T>& v, aby3::u64 num = 0) {
    print_mtx.lock();
    aby3::u64 cnt = 0;
    for (auto elem : v) {
        if (num != 0 && cnt >= num) break;
        oc::lout << elem << " ";
        cnt++;
    }
    oc::lout << "\n";
    print_mtx.unlock();
}