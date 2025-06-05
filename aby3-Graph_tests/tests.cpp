#include "tests.h"

#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include <random>
#include "cryptoTools/Crypto/PRNG.h"

#include <cryptoTools/Circuit/BetaLibrary.h>
#include <iomanip>
#include <atomic>
#include <string>
#include <thread>
#include <iostream>

#include "aby3-Graph/OGA.h"
#include "aby3-Graph/cc.h"
#include "aby3-Graph/ours.h"
#include "aby3-Graph/cognn_cc.h"
#include "aby3-Graph/shuffle.h"
#include "aby3-Graph/sort.h"
#include "aby3-Graph/graphsc.h"

using namespace oc;
using namespace aby3;

#define MAX_SEND_I64MAT_SIZE 10000 

std::mutex print_duration_mutex;

void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag) {
    print_duration_mutex.lock();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "::" << tag << " took "
              << ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()) / 1000
              << " seconds\n";
    print_duration_mutex.unlock();
}

void asyncSendI64Mat(const i64Matrix& mat, Channel& chl) {

    uint64_t step = MAX_SEND_I64MAT_SIZE;
    uint64_t matSize = mat.size();
    for (uint64_t i=0;i<matSize;i+=step) {
        uint64_t endIndex = i + step < matSize? i + step : matSize;
        i64Matrix buf(endIndex - i, 1);
        memcpy(buf.data(), mat.data() + i, (endIndex - i) * sizeof(u64));
        chl.asyncSendCopy(buf.data(), buf.size());
    }
}

void recvI64Mat(i64Matrix& mat, u64 matSize, Channel& chl) {

    uint64_t step = MAX_SEND_I64MAT_SIZE;
    for (uint64_t i=0;i<matSize;i+=step) {
        uint64_t endIndex = i + step < matSize? i + step : matSize;
        i64Matrix buf(endIndex - i, 1);
        chl.recv(buf.data(), buf.size());
        memcpy(mat.data() + i, buf.data(), (endIndex - i) * sizeof(u64));
    }
}

void Sh3_Graph_eq_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 byteSize = 32;
    u64 bitSize = byteSize << 3;

    u64 width = 1 << 20;
    bool failed = false;
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    Matrix<u8> a(width, byteSize), b(width, byteSize);
    PRNG prng(ZeroBlock);
    prng.get(a.data(), a.size());
    prng.get(b.data(), b.size());

    // printf("H-1\n");

    BetaLibrary lib;
    BetaCircuit *cir =  lib.int_eq(bitSize);

    // printf("H-2\n");

    cir->levelByAndDepth();

    // printf("H-3\n");

    auto routine = [&](int pIdx) {

        PackedBin c(width, 1);

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sPackedBin A(width, bitSize), B(width, bitSize), C(width, 1);

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));

        // auto task = rt.noDependencies();

        if (pIdx == 0) {
            enc.localPackedBinary(rt.noDependencies(), a, A, true).get();
        } else {
            enc.remotePackedBinary(rt.noDependencies(), A).get();
        }

        if (pIdx == 1) {
            enc.localPackedBinary(rt.noDependencies(), b, B, true).get();
        } else {
            enc.remotePackedBinary(rt.noDependencies(), B).get();
        }

        // printf("H1\n");

        auto& eval = evals[pIdx];

        eval.mPrng.SetSeed(toBlock(pIdx));

        //eval.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));
        //if (pIdx == 0)
        //    oc::lout << "---------------------------------------" << std::endl;

        Sh3ShareGen gen;
        gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

        // case Manual:
        // task.get();
        eval.setCir(cir, width, gen);
        eval.setInput(0, A);
        eval.setInput(1, B);

        // printf("H2\n");
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, C);

        // printf("H3\n");
        
        // task = eval.asyncEvaluate(task, cir, gen, { &A, &B, &C }, { &D });

        // task.get();

        // printf("++ %lu %lu\n", d.rows(), d.cols());
        enc.revealAll(rt.noDependencies(), C, c).get();

        // printf("H4\n");

        // for (u64 i = 0; i < width; ++i)
        // {
        //     if (c(i, 0) == 1) {
        //         for (u64 j = 0; j < byteSize; ++j) {
        //             if (d(i, j) != a(i, j)) {
        //                 oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " << j << " " 
        //                     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
        //                 failed = true;
        //             } else {
        //                 // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
        //                 //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
        //             }
        //         }
        //     } else {
        //         for (u64 j = 0; j < byteSize; ++j) {
        //             if (d(i, j) != b(i, j)) {
        //                 oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " << j << " " 
        //                     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
        //                 failed = true;
        //             } else {
        //                 // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
        //                 //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
        //             }
        //         }
        //     }
            
        // }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;
    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_multiplex_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 byteSize = 8;
    u64 bitSize = byteSize << 3;

    u64 width = 1 << 20;
    bool failed = false;
    //bool manual = false;

    std::array < std::vector<oc::Matrix<i64>>, 3> CC;
    std::array < std::vector<oc::Matrix<i64>>, 3> CC2;
    Sh3BinaryEvaluator evals[3];

    Matrix<u8> a(width, byteSize), b(width, byteSize), c(width, 1);
    PRNG prng(ZeroBlock);
    prng.get(a.data(), a.size());
    prng.get(b.data(), b.size());
    for (u64 i = 0; i < (u64)c.rows(); ++i)
    {
        c(i, 0) = prng.get<u8>() & 1;
    }

    auto routine = [&](int pIdx) {

        BetaCircuit cd;
        get_multiplex_Circ(cd, bitSize);
        BetaCircuit *cir = &cd;

        cir->levelByAndDepth();

        //auto i = 0;
        Matrix<u8> d(width, byteSize);
        d.setZero();

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sPackedBin A(width, bitSize), B(width, bitSize), C(width, 1), D(width, bitSize);

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));

        // auto task = rt.noDependencies();

        if (pIdx == 0) {
            enc.localPackedBinary(rt.noDependencies(), a, A, true).get();
        } else {
            enc.remotePackedBinary(rt.noDependencies(), A).get();
        }

        if (pIdx == 1) {
            enc.localPackedBinary(rt.noDependencies(), b, B, true).get();
        } else {
            enc.remotePackedBinary(rt.noDependencies(), B).get();
        }

        if (pIdx == 2) {
            enc.localPackedBinary(rt.noDependencies(), c, 1, C).get();
        } else {
            enc.remotePackedBinary(rt.noDependencies(), C).get();
        }

        auto& eval = evals[pIdx];

        eval.mPrng.SetSeed(toBlock(pIdx));

        //eval.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));
        //if (pIdx == 0)
        //    oc::lout << "---------------------------------------" << std::endl;

        Sh3ShareGen gen;
        gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

        D.mShares[0](0) = 0;
        D.mShares[1](0) = 0;

        // case Manual:
        // task.get();
        eval.setCir(cir, width, gen);
        eval.setInput(0, A);
        eval.setInput(1, B);
        eval.setInput(2, C);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, D);
        
        // task = eval.asyncEvaluate(task, cir, gen, { &A, &B, &C }, { &D });

        // task.get();

        // printf("++ %lu %lu\n", d.rows(), d.cols());
        enc.revealAll(rt.noDependencies(), D, d).get();

        for (u64 i = 0; i < width; ++i)
        {
            if (c(i, 0) == 1) {
                for (u64 j = 0; j < byteSize; ++j) {
                    if (d(i, j) != a(i, j)) {
                        oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " << j << " " 
                            << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                        failed = true;
                    } else {
                        // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
                        //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                    }
                }
            } else {
                for (u64 j = 0; j < byteSize; ++j) {
                    if (d(i, j) != b(i, j)) {
                        oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " << j << " " 
                            << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                        failed = true;
                    } else {
                        // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
                        //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                    }
                }
            }
            
        }

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_compare_select_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 20;
    bool failed = false;
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    i64Matrix a_0(width, wordSize), a_1(width, wordSize), b_0(width, wordSize), b_1(width, wordSize);
    PRNG prng(ZeroBlock);
    prng.get(a_0.data(), a_0.size());
    prng.get(a_1.data(), a_1.size());
    prng.get(b_0.data(), b_0.size());
    prng.get(b_1.data(), b_1.size());

    auto routine = [&](int pIdx) {

        BetaCircuit cd;
        get_compare_select_Circ(cd, bitSize);
        BetaCircuit *cir = &cd;

        cir->levelByAndDepth();

        //auto i = 0;
        i64Matrix a_2(width, wordSize);
        i64Matrix b_2(width, wordSize);
        a_2.setZero();
        b_2.setZero();

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sbMatrix A_0(width, bitSize), A_1(width, bitSize), B_0(width, bitSize), B_1(width, bitSize), A_2(width, bitSize), B_2(width, bitSize);

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));

        // auto task = rt.noDependencies();

        if (pIdx == 0) {
            enc.localBinMatrix(rt.noDependencies(), a_0, A_0).get();
            enc.localBinMatrix(rt.noDependencies(), b_0, B_0).get();
        } else {
            enc.remoteBinMatrix(rt.noDependencies(), A_0).get();
            enc.remoteBinMatrix(rt.noDependencies(), B_0).get();
        }

        if (pIdx == 1) {
            enc.localBinMatrix(rt.noDependencies(), a_1, A_1).get();
            enc.localBinMatrix(rt.noDependencies(), b_1, B_1).get();
        } else {
            enc.remoteBinMatrix(rt.noDependencies(), A_1).get();
            enc.remoteBinMatrix(rt.noDependencies(), B_1).get();
        }

        auto& eval = evals[pIdx];

        eval.mPrng.SetSeed(toBlock(pIdx));

        //eval.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));
        //if (pIdx == 0)
        //    oc::lout << "---------------------------------------" << std::endl;

        Sh3ShareGen gen;
        gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

        // case Manual:
        // task.get();
        eval.setCir(cir, width, gen);
        eval.setInput(0, A_0);
        eval.setInput(1, A_1);
        eval.setInput(2, B_0);
        eval.setInput(3, B_1);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, A_2);
        eval.getOutput(1, B_2);
        
        // task = eval.asyncEvaluate(task, cir, gen, { &A, &B, &C }, { &D });

        // task.get();

        // printf("++ %lu %lu\n", d.rows(), d.cols());
        enc.revealAll(rt.noDependencies(), A_2, a_2).get();
        enc.revealAll(rt.noDependencies(), B_2, b_2).get();

        if (rt.mPartyIdx == 0) {
            for (u64 i = 0; i < width; ++i)
            {
                if ((u64)a_0(i, 0) < (u64)a_1(i,0)) {
                    if ((a_2(i, 0) != a_0(i, 0)) || (b_2(i, 0) != b_0(i, 0))) {
                        oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " 
                            << u64(a_0(i, 0)) << " " << u64(a_1(i, 0)) << " " << u64(a_2(i, 0)) << " " << u64(b_0(i, 0)) << " " << u64(b_1(i, 0)) << " " << u64(b_2(i, 0)) << std::endl << std::dec;
                        failed = true;
                    } else {
                        // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
                        //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                    }
                } else {
                    if ((a_2(i, 0) != a_1(i, 0)) || (b_2(i, 0) != b_1(i, 0))) {
                        oc::lout << Color::Red << "pidx: " << rt.mPartyIdx << " failed at " << i << " " 
                            << u64(a_0(i, 0)) << " " << u64(a_1(i, 0)) << " " << u64(a_2(i, 0)) << " " << u64(b_0(i, 0)) << " " << u64(b_1(i, 0)) << " " << u64(b_2(i, 0)) << std::endl << std::dec;
                        failed = true;
                    } else {
                        // oc::lout << Color::Green << "pidx: " << rt.mPartyIdx << " success at " << i << " " << j << " " 
                        //     << std::setw(2) << std::hex << int(c(i, 0)) << " " << int(a(i, j)) << " " << int(b(i, j)) << " " << int(d(i, j)) << std::endl << std::dec;
                    }
                }
                
            }
        }

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_OGA_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 20;
    std::atomic<bool> failed(false);
    //bool manual = false;

    std::array < std::vector<oc::Matrix<i64>>, 3> CC;
    std::array < std::vector<oc::Matrix<i64>>, 3> CC2;
    Sh3BinaryEvaluator evals[3];

    i64Matrix value(width, wordSize);

    std::vector<u64> group(width, 0);
    PRNG prng(ZeroBlock);
    prng.get(value.data(), value.size());
    // for (u64 i = 0; i < width; ++i) {
    //     for (u64 j = 0; j < wordSize; ++j) value(i, j) = i;
    // }
    u64 curGroupSize = (1 << 10);
    u64 curGroupId = 1;
    u64 groupMember = curGroupSize;
    for (u64 i = 0; i < width; ++i) {
        group[i] = curGroupId;
        groupMember -= 1;
        if (groupMember == 0) {
            // curGroupSize += 1;
            curGroupId += 1;
            curGroupSize >>= 1;
            if (curGroupSize == 0) curGroupSize = 1;
            groupMember = curGroupSize;
        }
    }
    // for (u64 i = 0; i < width; ++i) printf("%lu ", group[i]);
    // printf("\n");
    std::vector<u64> aggSlots;
    i64Matrix gtAgg(width, wordSize);
    u64 curGroup = group[width - 1];
    for (u64 j = 0; j < wordSize; ++j) gtAgg(width - 1, j) = value(width - 1, j);
    for (i64 i = width - 2; i >= 0; --i) {
        if (curGroup == group[i]) {
            for (u64 j = 0; j < wordSize; ++j) gtAgg(i, j) = value(i, j) | gtAgg(i + 1, j);
        } else {
            for (u64 j = 0; j < wordSize; ++j) gtAgg(i, j) = value(i, j);
            aggSlots.push_back(i + 1);
        }
        curGroup = group[i];
    }
    aggSlots.push_back(0);

    BetaLibrary lib;
    auto andCir = lib.int_int_bitwiseOr(bitSize, bitSize, bitSize);
    andCir->levelByAndDepth();

    auto routine = [&](int pIdx) {

        i64Matrix agged(width, wordSize);
        // oc::lout << "here " << pIdx << " H1" <<  std::endl;
        i64Matrix curValue(width, wordSize);
        if (pIdx == 0) curValue = value;
        else curValue.setZero();

        run_OGA(
            comms[pIdx].mPrev,
            comms[pIdx].mNext,
            pIdx,
            group,
            curValue,
            agged,
            andCir
        );
        // oc::lout << "here " << pIdx << " H2" <<  std::endl;
        
        for (auto slot : aggSlots)
        {
            for (u64 j = 0; j < wordSize; ++j) {
                if (gtAgg(slot, j) != agged(slot, j)) {
                    if (pIdx == 0) oc::lout << Color::Red << "pidx: " << pIdx << " failed at " << slot << " " << j << " "
                        << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;
                    failed = true;
                } else {
                    // if (pIdx == 0) oc::lout << Color::Green << "pidx: " << pIdx << " succeeded at " << slot << " " << j << " "
                    //     << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;                    
                }
            }
        }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;
    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}


void Sh3_Graph_CC_test()
{
    u64 numP = 7;
    // std::vector<u64> pIndices(5, 0);
    // for (u64 i = 0; i < numP; ++i) pIndices[i] = i;

    IOService ios;
    std::vector<std::vector<Session>> computeSessions(numP);
    std::vector<std::vector<Session>> delegateClientSessions(numP); 
    std::vector<std::vector<Session>> delegateServerSessions(numP); 
    std::vector<std::vector<Channel>> computeChls(numP);
    std::vector<std::vector<Channel>> delegateClientChls(numP); 
    std::vector<std::vector<Channel>> delegateServerChls(numP); 

    for (u64 i = 0; i < numP; ++i) {
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-01"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-01"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-02"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-02"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-12"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-12")); 
        for (u64 j = 0; j < 6; ++j)
            computeChls[i].emplace_back(computeSessions[i][j].addChannel("c"));     
    }

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateClientSessions[i].emplace_back(Session());
            else if (i < j)
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleClient") + std::to_string(i) + std::to_string(j)));
            else
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(i)));
        }

    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateClientChls[i].emplace_back(delegateClientSessions[i][j].addChannel("c"));
            else delegateClientChls[i].emplace_back(Channel());
        }    
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateServerSessions[i].emplace_back(Session());
            else if (i < j)
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleServer") + std::to_string(i) + std::to_string(j)));
            else
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(i)));
        }  
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateServerChls[i].emplace_back(delegateServerSessions[i][j].addChannel("c"));
            else delegateServerChls[i].emplace_back(Channel());
        }    
    }  

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << 20);
    u64 numIntraEdgePerP = (1 << 20);
    u64 numInterEdgePerPair = (1 << 20);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u8>> vertexDataLists(numP, std::vector<u8>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j)
            vertexIdLists[i][j] = i * numVertexPerP + j;
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
            }
        }
    }    

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    orCir_8->levelByAndDepth(); 

    auto routine = [&](int pIdx) {
        u64 serverDstIdx = (pIdx + numP - 1) % numP;
        u64 helperDstIdx = (pIdx + numP - 2) % numP;
        CommPkg computeComms[3];
        computeComms[0] = { computeChls[pIdx][2], computeChls[pIdx][0] }; // Client Comms
        computeComms[1] = { computeChls[serverDstIdx][1], computeChls[serverDstIdx][4] }; // Server Comms
        computeComms[2] = { computeChls[helperDstIdx][5], computeChls[helperDstIdx][3] }; // Helper Comms       
        std::vector<Channel>& delClientChls = delegateClientChls[pIdx];
        std::vector<Channel>& delServerChls = delegateServerChls[pIdx];
        std::vector<Matrix<u8>> vertexDatas(3);
        vertexDatas[0].resize(numVertexList[pIdx], 1);
        vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
        vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
        for (u64 i = 0; i < 3; ++i) vertexDatas[i].setZero();
        for (u64 i = 0; i < numVertexList[pIdx]; ++i) {
            vertexDatas[0](i, 0) = vertexDataLists[pIdx][i];
        }
        std::vector<Matrix<u8>> interUpdateShare1(3);
        std::vector<Matrix<u8>> interUpdateShare2(3);

        // Load the data to aby3 plaintext data structure
        // For different characteristics: client, server, helper (maybe in different threads)
        auto scatterThread = [&](int role, Matrix<u8>& vertexDataShare, Matrix<u8>& updateShare1, Matrix<u8>& updateShare2) {
            std::vector<u64> srcTag;
            std::vector<u64> dstTag;
            Matrix<u8> updateShare;
            u64 numEdge = 0;
            int clientPIdx = 0;
            if (role == 0) clientPIdx = pIdx;
            else if (role == 1) clientPIdx = serverDstIdx;
            else if (role == 2) clientPIdx = helperDstIdx;
            u64 numVertex = numVertexList[clientPIdx];
            for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[clientPIdx][i];
            if (role == 2) vertexDataShare.setZero();
            updateShare.resize(numEdge, 1);
            updateShare.setZero();
            srcTag.resize(numVertex);
            dstTag.resize(numEdge);
            if (role == 0) {
                for (u64 i = 0; i < numVertex; ++i) {
                    srcTag[i] = vertexIdLists[clientPIdx][i];
                }
                u64 cnt = 0;
                for (u64 i = 0; i < numP; ++i) {
                    for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                        dstTag[cnt++] = edgeLists[clientPIdx][i][j][0];
                    }
                }
            }
            // scatter(
            //     computeComms[role].mPrev,
            //     computeComms[role].mNext,
            //     pIdx,
            //     role,
            //     srcTag, 
            //     dstTag, 
            //     vertexDataShare,
            //     updateShare
            // );      

            // Decompose update Share and send    
            std::vector<Matrix<u8>> updateShares(numP);
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                updateShares[i].resize(numEdgeMat[clientPIdx][i], 1);
                for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                    updateShares[i](j, 0) = updateShare(cnt++, 0);
                }
            }
            if (role == 0) {
                updateShare1 = updateShares[clientPIdx];
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if ((i + 1) % numP != pIdx) {
                            if (!delClientChls[(i + 1) % numP].isConnected()) {
                                printf("Unexpected Unconnected Channel!\n");
                                exit(-1);
                            }
                            delClientChls[(i + 1) % numP].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        } else {
                            // Get the server share for P_{pIdx-1}
                            updateShare2 = updateShares[i];
                        }
                    }
                }
            } else if (role == 1) {
                updateShare1 = updateShares[clientPIdx];
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if (i != pIdx) {
                            if (!delServerChls[i].isConnected()) {
                                printf("Unexpected Unconnected Channel!\n");
                                exit(-1);
                            }
                            delServerChls[i].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        } else {
                            // Get the client share for P_{pIdx}
                            updateShare2 = updateShares[i];
                        }
                    }
                }                
            }
        };

        auto gatherThread = [&](int role, Matrix<u8>& vertexDataShare, const Matrix<u8>& updateShare1, const Matrix<u8>& updateShare2) {
            Matrix<u8> updatedVertexDataShare = vertexDataShare;
            updatedVertexDataShare.setZero();
            std::vector<u64> vertexTag;
            std::vector<u64> dstTag;
            Matrix<u8> updateShare;
            u64 numEdge = 0;
            int clientPIdx = 0;
            if (role == 0) clientPIdx = pIdx;
            else if (role == 1) clientPIdx = serverDstIdx;
            else if (role == 2) clientPIdx = helperDstIdx;
            u64 numVertex = numVertexList[clientPIdx];
            for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[i][clientPIdx];
            if (role == 2) vertexDataShare.setZero();
            updateShare.resize(numEdge, 1);
            updateShare.setZero();
            vertexTag.resize(numVertex);
            dstTag.resize(numEdge);
            // Decompose update Share and send    
            std::vector<Matrix<u8>> updateShares(numP);
            for (u64 i = 0; i < numP; ++i) {
                updateShares[i].resize(numEdgeMat[i][clientPIdx], 1);
            }
            if (role == 0) {
                updateShares[clientPIdx] = updateShare1;
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if ((i + 1) % numP != pIdx) {
                            delServerChls[(i + 1) % numP].recv(updateShares[i].data(), updateShares[i].size());
                        } else {
                            updateShares[i] = updateShare2;
                        }
                    }
                }
            } else if (role == 1) {
                updateShares[clientPIdx] = updateShare1;
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if (i != pIdx) {
                            delClientChls[i].recv(updateShares[i].data(), updateShares[i].size());
                        } else {
                            updateShares[i] = updateShare2;
                        }
                    }
                }                
            } 
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                    updateShare(cnt++, 0) = updateShares[i](j, 0);
                }
            }  
            if (role == 0) {
                for (u64 i = 0; i < numVertex; ++i) {
                    vertexTag[i] = vertexIdLists[clientPIdx][i];
                }
                u64 cnt = 0;
                for (u64 i = 0; i < numP; ++i) {
                    for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                        dstTag[cnt++] = edgeLists[i][clientPIdx][j][1];
                    }
                }
            } 

            // gather(
            //     computeComms[role].mPrev,
            //     computeComms[role].mNext,
            //     pIdx,
            //     role,
            //     dstTag, 
            //     vertexTag,
            //     updateShare,
            //     vertexDataShare,
            //     updatedVertexDataShare
            // );        
            vertexDataShare = updatedVertexDataShare;
        };

        u64 numIters = 5;
        for (u64 iter = 0; iter < numIters; ++iter) {
            std::vector<std::thread> scatterThrds; 
            for (u64 role = 0; role < 3; ++role) {
                scatterThrds.emplace_back(scatterThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
            }
            for (auto& thrd : scatterThrds)
                thrd.join();
            std::vector<std::thread> gatherThrds;
            for (u64 role = 0; role < 3; ++role) {
                if (role != 2) gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[1 - role]));
                else gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
            }
            for (auto& thrd : gatherThrds)
                thrd.join();            
        }
    
        computeComms[1].mPrev.asyncSendCopy(vertexDatas[1].data(), vertexDatas[1].size());
        Matrix<u8> serverVertexData(numVertexList[pIdx], 1);
        serverVertexData.setZero();
        computeComms[0].mNext.recv(serverVertexData.data(), serverVertexData.size());
        for (u64 i = 0; i < vertexDatas[0].size(); ++i) vertexDatas[0](i) ^= serverVertexData(i); 

        u64 sent = 0, recv = 0;
        for (u64 i = 0; i < 3; ++i) {
            sent += computeComms[i].mPrev.getTotalDataSent();
            recv += computeComms[i].mPrev.getTotalDataRecv();
            sent += computeComms[i].mNext.getTotalDataSent();
            recv += computeComms[i].mNext.getTotalDataRecv();
        }
        for (u64 i = 0; i < delClientChls.size(); ++i) {
            if (i != pIdx) {
                sent += delClientChls[i].getTotalDataSent();
                recv += delClientChls[i].getTotalDataRecv();
                sent += delServerChls[i].getTotalDataSent();
                recv += delServerChls[i].getTotalDataRecv();
            }
        }

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;
    };

    std::vector<std::thread> thrds;
    for (u64 i = 0; i < numP; ++i)
        thrds.emplace_back(std::thread(routine, i));

    for (u64 i = 0; i < numP; ++i)
        thrds[i].join();

    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_Ours_test()
{
    Alg alg = Alg::CC;
    u64 numP = 5;
    u64 scale = 10;
    std::vector<u64> pIndices(5, 0);
    for (u64 i = 0; i < numP; ++i) pIndices[i] = i;

    IOService ios;
    std::vector<std::vector<Session>> computeSessions(numP);
    std::vector<std::vector<Session>> delegateClientSessions(numP); 
    std::vector<std::vector<Session>> delegateServerSessions(numP); 
    std::vector<std::vector<Channel>> computeChls(numP);
    std::vector<std::vector<Channel>> delegateClientChls(numP); 
    std::vector<std::vector<Channel>> delegateServerChls(numP); 

    for (u64 i = 0; i < numP; ++i) {
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-01"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-01"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-02"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-02"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-12"));
        computeSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-12")); 
        for (u64 j = 0; j < 6; ++j)
            computeChls[i].emplace_back(computeSessions[i][j].addChannel("c"));     
    }

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateClientSessions[i].emplace_back(Session());
            else if (i < j)
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleClient") + std::to_string(i) + std::to_string(j)));
            else
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(i)));
        }

    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateClientChls[i].emplace_back(delegateClientSessions[i][j].addChannel("c"));
            else delegateClientChls[i].emplace_back(Channel());
        }    
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateServerSessions[i].emplace_back(Session());
            else if (i < j)
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleServer") + std::to_string(i) + std::to_string(j)));
            else
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(i)));
        }  
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateServerChls[i].emplace_back(delegateServerSessions[i][j].addChannel("c"));
            else delegateServerChls[i].emplace_back(Channel());
        }    
    }  

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    u64 numIntraEdgePerP = (1 << scale);
    u64 numInterEdgePerPair = (1 << scale);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j)
            vertexIdLists[i][j] = i * numVertexPerP + j;
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
            }
        }
    }    

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    // auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    // orCir_8->levelByAndDepth(); 

    auto routine = [&](int pIdx) {
        u64 serverDstIdx = (pIdx + numP - 1) % numP;
        u64 helperDstIdx = (pIdx + numP - 2) % numP;
        CommPkg computeComms[3];
        computeComms[0] = { computeChls[pIdx][2], computeChls[pIdx][0] }; // Client Comms
        computeComms[1] = { computeChls[serverDstIdx][1], computeChls[serverDstIdx][4] }; // Server Comms
        computeComms[2] = { computeChls[helperDstIdx][5], computeChls[helperDstIdx][3] }; // Helper Comms       
        std::vector<Channel>& delClientChls = delegateClientChls[pIdx];
        std::vector<Channel>& delServerChls = delegateServerChls[pIdx];
        std::vector<i64Matrix> vertexDatas(3);
        vertexDatas[0].resize(numVertexList[pIdx], 1);
        vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
        vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
        for (u64 i = 0; i < 3; ++i) vertexDatas[i].setZero();
        for (u64 i = 0; i < numVertexList[pIdx]; ++i) {
            vertexDatas[0](i, 0) = vertexDataLists[pIdx][i];
        }
        std::vector<i64Matrix> interUpdateShare1(3);
        std::vector<i64Matrix> interUpdateShare2(3);

        // Load the data to aby3 plaintext data structure
        // For different characteristics: client, server, helper (maybe in different threads)
        auto scatterThread = [&](int role, i64Matrix& vertexDataShare, i64Matrix& updateShare1, i64Matrix& updateShare2) {
            std::vector<u64> srcTag;
            std::vector<u64> dstTag;
            i64Matrix updateShare;
            u64 numEdge = 0;
            int clientPIdx = 0;
            if (role == 0) clientPIdx = pIdx;
            else if (role == 1) clientPIdx = serverDstIdx;
            else if (role == 2) clientPIdx = helperDstIdx;
            u64 numVertex = numVertexList[clientPIdx];
            for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[clientPIdx][i];
            if (role == 2) vertexDataShare.setZero();
            updateShare.resize(numEdge, 1);
            updateShare.setZero();
            srcTag.resize(numVertex);
            dstTag.resize(numEdge);
            if (role == 0) {
                for (u64 i = 0; i < numVertex; ++i) {
                    srcTag[i] = vertexIdLists[clientPIdx][i];
                }
                u64 cnt = 0;
                for (u64 i = 0; i < numP; ++i) {
                    for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                        dstTag[cnt++] = edgeLists[clientPIdx][i][j][0];
                    }
                }
            }
            our_scatter(
                computeComms[role].mPrev,
                computeComms[role].mNext,
                pIdx,
                role,
                srcTag, 
                dstTag, 
                vertexDataShare,
                updateShare,
                alg
            );      

            // Decompose update Share and send    
            std::vector<i64Matrix> updateShares(numP);
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                updateShares[i].resize(numEdgeMat[clientPIdx][i], 1);
                for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                    updateShares[i](j, 0) = updateShare(cnt++, 0);
                }
            }
            if (role == 0) {
                updateShare1 = updateShares[clientPIdx];
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if ((i + 1) % numP != pIdx) {
                            if (!delClientChls[(i + 1) % numP].isConnected()) {
                                printf("Unexpected Unconnected Channel!\n");
                                exit(-1);
                            }
                            delClientChls[(i + 1) % numP].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        } else {
                            // Get the server share for P_{pIdx-1}
                            updateShare2 = updateShares[i];
                        }
                    }
                }
            } else if (role == 1) {
                updateShare1 = updateShares[clientPIdx];
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if (i != pIdx) {
                            if (!delServerChls[i].isConnected()) {
                                printf("Unexpected Unconnected Channel!\n");
                                exit(-1);
                            }
                            delServerChls[i].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        } else {
                            // Get the client share for P_{pIdx}
                            updateShare2 = updateShares[i];
                        }
                    }
                }                
            }
        };

        auto gatherThread = [&](int role, i64Matrix& vertexDataShare, const i64Matrix& updateShare1, const i64Matrix& updateShare2) {
            i64Matrix updatedVertexDataShare = vertexDataShare;
            updatedVertexDataShare.setZero();
            std::vector<u64> vertexTag;
            std::vector<u64> dstTag;
            i64Matrix updateShare;
            u64 numEdge = 0;
            int clientPIdx = 0;
            if (role == 0) clientPIdx = pIdx;
            else if (role == 1) clientPIdx = serverDstIdx;
            else if (role == 2) clientPIdx = helperDstIdx;
            u64 numVertex = numVertexList[clientPIdx];
            for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[i][clientPIdx];
            if (role == 2) vertexDataShare.setZero();
            updateShare.resize(numEdge, 1);
            updateShare.setZero();
            vertexTag.resize(numVertex);
            dstTag.resize(numEdge);
            // Decompose update Share and send    
            std::vector<i64Matrix> updateShares(numP);
            for (u64 i = 0; i < numP; ++i) {
                updateShares[i].resize(numEdgeMat[i][clientPIdx], 1);
            }
            if (role == 0) {
                updateShares[clientPIdx] = updateShare1;
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if ((i + 1) % numP != pIdx) {
                            delServerChls[(i + 1) % numP].recv(updateShares[i].data(), updateShares[i].size());
                        } else {
                            updateShares[i] = updateShare2;
                        }
                    }
                }
            } else if (role == 1) {
                updateShares[clientPIdx] = updateShare1;
                for (int i = 0; i < numP; ++i) {
                    if (i != clientPIdx) {
                        if (i != pIdx) {
                            delClientChls[i].recv(updateShares[i].data(), updateShares[i].size());
                        } else {
                            updateShares[i] = updateShare2;
                        }
                    }
                }                
            } 
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                    updateShare(cnt++, 0) = updateShares[i](j, 0);
                }
            }  
            if (role == 0) {
                for (u64 i = 0; i < numVertex; ++i) {
                    vertexTag[i] = vertexIdLists[clientPIdx][i];
                }
                u64 cnt = 0;
                for (u64 i = 0; i < numP; ++i) {
                    for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                        dstTag[cnt++] = edgeLists[i][clientPIdx][j][1];
                    }
                }
            } 

            our_gather_dummied(
                computeComms[role].mPrev,
                computeComms[role].mNext,
                pIdx,
                role,
                dstTag, 
                vertexTag,
                updateShare,
                vertexDataShare,
                updatedVertexDataShare,
                alg
            );        
            vertexDataShare = updatedVertexDataShare;
        };

        u64 numIters = 5;
        for (u64 iter = 0; iter < numIters; ++iter) {
            std::vector<std::thread> scatterThrds; 
            for (u64 role = 0; role < 3; ++role) {
                scatterThrds.emplace_back(scatterThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
            }
            for (auto& thrd : scatterThrds)
                thrd.join();
            std::vector<std::thread> gatherThrds;
            for (u64 role = 0; role < 3; ++role) {
                if (role != 2) gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[1 - role]));
                else gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
            }
            for (auto& thrd : gatherThrds)
                thrd.join();            
        }
    
        computeComms[1].mPrev.asyncSendCopy(vertexDatas[1].data(), vertexDatas[1].size());
        i64Matrix serverVertexData(numVertexList[pIdx], 1);
        serverVertexData.setZero();
        computeComms[0].mNext.recv(serverVertexData.data(), serverVertexData.size());
        for (u64 i = 0; i < vertexDatas[0].size(); ++i) vertexDatas[0](i) ^= serverVertexData(i); 

        u64 sent = 0, recv = 0;
        for (u64 i = 0; i < 3; ++i) {
            sent += computeComms[i].mPrev.getTotalDataSent();
            recv += computeComms[i].mPrev.getTotalDataRecv();
            sent += computeComms[i].mNext.getTotalDataSent();
            recv += computeComms[i].mNext.getTotalDataRecv();
        }
        for (u64 i = 0; i < delClientChls.size(); ++i) {
            if (i != pIdx) {
                sent += delClientChls[i].getTotalDataSent();
                recv += delClientChls[i].getTotalDataRecv();
                sent += delServerChls[i].getTotalDataSent();
                recv += delServerChls[i].getTotalDataRecv();
            }
        }

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;
    };

    std::vector<std::thread> thrds;
    for (u64 i = 0; i < numP; ++i)
        thrds.emplace_back(std::thread(routine, i));

    for (u64 i = 0; i < numP; ++i)
        thrds[i].join();

    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_Ours_single_party(    
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
) {
	Alg alg;
	if (algId == 0) {
		alg = Alg::CC;
	} else if (algId == 1) {
		alg = Alg::SP;
	} else if (algId == 2) {
		alg = Alg::PR;
	} else {
		printf("Unexpected alg!\n");
		exit(-1);
	}

    IOService ios(0);
    std::vector<Session> computeSessions;
    std::vector<Session> delegateClientSessions; 
    std::vector<Session> delegateServerSessions; 
    std::vector<Channel> computeChls;
    std::vector<Channel> delegateClientChls; 
    std::vector<Channel> delegateServerChls; 

    u64 serverDstIdx = (pIndex + numP - 1) % numP;
    u64 helperDstIdx = (pIndex + numP - 2) % numP;

    uint32_t baseComPort = 1712;
    uint32_t baseDelPort = 1912;
    std::string baseIP = "10.0.0.";
    // std::string baseIP = "127.0.0.1";

    // computeChls[pIdx][2], computeChls[pIdx][0]
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 2, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP - 1) % numP) + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP - 1) % numP) + 2, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP + 2) % numP) + 0, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02")); 
    // ---->
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 2, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 2, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP + 2) % numP + 1), baseComPort + 3 * ((pIndex + numP + 2) % numP) + 0, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02")); 
    // <----
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * serverDstIdx + 0, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * helperDstIdx + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * serverDstIdx + 2, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * helperDstIdx + 2, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    // for (auto chl : channels)
    //     if (chl)
    //         chl->close();

    // for (auto& cs : computeSessions)
    //     cs.stop();
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-12"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-12")); 
    for (u64 j = 0; j < 6; ++j)
        computeChls.emplace_back(computeSessions[j].addChannel("c"));     

    // ---->
    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateClientSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    }


    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
        else delegateClientChls.emplace_back(Channel());
    }    

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateServerSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    }  

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
        else delegateServerChls.emplace_back(Channel());
    }  
    // <----  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateClientSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    // }


    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
    //     else delegateClientChls.emplace_back(Channel());
    // }    

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateServerSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    // }  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
    //     else delegateServerChls.emplace_back(Channel());
    // }    

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    int numEdgePerP = (1 << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j) {
            vertexIdLists[i][j] = i * numVertexPerP + j;
        }
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[i][k]});
                        if (++cnt > numIntraEdgePerP) break;
                    }
                    if (cnt > numIntraEdgePerP) break;
                }
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[j][k]});
                        if (++cnt > numInterEdgePerPair) break;
                    }
                    if (cnt > numInterEdgePerPair) break;
                }
            }
        }
    }   

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    // auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    // orCir_8->levelByAndDepth(); 

    CommPkg computeComms[3];
    computeComms[0] = { computeChls[5], computeChls[2] }; // Client Comms
    computeComms[1] = { computeChls[4], computeChls[1] }; // Server Comms
    computeComms[2] = { computeChls[3], computeChls[0] }; // Helper Comms       
    std::vector<Channel>& delClientChls = delegateClientChls;
    std::vector<Channel>& delServerChls = delegateServerChls;
    std::vector<i64Matrix> vertexDatas(3);
    vertexDatas[0].resize(numVertexList[pIndex], 1);
    vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
    vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
    for (u64 i = 0; i < 3; ++i) vertexDatas[i].setZero();
    for (u64 i = 0; i < numVertexList[pIndex]; ++i) {
        vertexDatas[0](i, 0) = vertexDataLists[pIndex][i];
    }
    std::vector<i64Matrix> interUpdateShare1(3);
    std::vector<i64Matrix> interUpdateShare2(3);

    // Load the data to aby3 plaintext data structure
    // For different characteristics: client, server, helper (maybe in different threads)
    auto scatterThread = [&](int role, i64Matrix& vertexDataShare, i64Matrix& updateShare1, i64Matrix& updateShare2) {
        std::vector<u64> srcTag;
        std::vector<u64> dstTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[clientPIdx][i];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        srcTag.resize(numVertex);
        dstTag.resize(numEdge);
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                srcTag[i] = vertexIdLists[clientPIdx][i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                    dstTag[cnt++] = edgeLists[clientPIdx][i][j][0];
                }
            }
        }
        our_scatter(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            srcTag, 
            dstTag, 
            vertexDataShare,
            updateShare,
            alg
        );      

        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[clientPIdx][i], 1);
            for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                updateShares[i](j, 0) = updateShare(cnt++, 0);
            }
        }
        if (role == 0) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("send delClient %d -> %d\n", pIndex, (i + 1) % numP);
                        // if (!delClientChls[(i + 1) % numP].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delClientChls %d %d %d\n", (i + 1) % numP, pIndex, role);
                        //     exit(-1);
                        // }
                        // delClientChls[(i + 1) % numP].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delClientChls[(i + 1) % numP]);
                    } else {
                        // Get the server share for P_{pIdx-1}
                        updateShare2 = updateShares[i];
                    }
                }
            }
        } else if (role == 1) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // printf("send delServer %d -> %d\n", pIndex, i);
                        // if (!delServerChls[i].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delServerChls %d %d %d\n", i, pIndex, role);
                        //     exit(-1);
                        // }
                        // delServerChls[i].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delServerChls[i]);
                    } else {
                        // Get the client share for P_{pIdx}
                        updateShare2 = updateShares[i];
                    }
                }
            }                
        }
    };

    auto gatherThread = [&](int role, i64Matrix& vertexDataShare, const i64Matrix& updateShare1, const i64Matrix& updateShare2) {
        i64Matrix updatedVertexDataShare = vertexDataShare;
        updatedVertexDataShare.setZero();
        std::vector<u64> vertexTag;
        std::vector<u64> dstTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[i][clientPIdx];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        vertexTag.resize(numVertex);
        dstTag.resize(numEdge);
        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[i][clientPIdx], 1);
        }
        if (role == 0) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("recv delServer %d -> %d\n", (i + 1) % numP, pIndex);
                        // delServerChls[(i + 1) % numP].recv(updateShares[i].data(), updateShares[i].size());
                        recvI64Mat(updateShares[i], updateShares[i].size(), delServerChls[(i + 1) % numP]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }
        } else if (role == 1) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // delClientChls[i].recv(updateShares[i].data(), updateShares[i].size());
                        // printf("recv delClient %d -> %d\n", i, pIndex);
                        recvI64Mat(updateShares[i], updateShares[i].size(), delClientChls[i]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }                
        } 
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                updateShare(cnt++, 0) = updateShares[i](j, 0);
            }
        }  
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                vertexTag[i] = vertexIdLists[clientPIdx][i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                    dstTag[cnt++] = edgeLists[i][clientPIdx][j][1];
                }
            }
        } 

        our_gather_dummied(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            dstTag, 
            vertexTag,
            updateShare,
            vertexDataShare,
            updatedVertexDataShare,
            alg
        );        
        vertexDataShare = updatedVertexDataShare;
    };

    for (u64 iter = 0; iter < numIters; ++iter) {
        auto t_tmp = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> scatterThrds; 
        for (u64 role = 0; role < 3; ++role) {
            scatterThrds.emplace_back(scatterThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : scatterThrds)
            thrd.join();

        print_duration(t_tmp, "Scatter");
        t_tmp = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> gatherThrds;
        for (u64 role = 0; role < 3; ++role) {
            if (role != 2) gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[1 - role]));
            else gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : gatherThrds)
            thrd.join();

        print_duration(t_tmp, "Gather");            
    }

    // computeComms[1].mPrev.asyncSendCopy(vertexDatas[1].data(), vertexDatas[1].size());
    asyncSendI64Mat(vertexDatas[1], computeComms[1].mPrev);
    i64Matrix serverVertexData(numVertexList[pIndex], 1);
    serverVertexData.setZero();
    // computeComms[0].mNext.recv(serverVertexData.data(), serverVertexData.size());
    recvI64Mat(serverVertexData, serverVertexData.size(), computeComms[0].mNext);
    for (u64 i = 0; i < vertexDatas[0].size(); ++i) vertexDatas[0](i) ^= serverVertexData(i); 

    u64 sent = 0, recv = 0;
    for (u64 i = 0; i < 3; ++i) {
        sent += computeComms[i].mPrev.getTotalDataSent();
        recv += computeComms[i].mPrev.getTotalDataRecv();
        sent += computeComms[i].mNext.getTotalDataSent();
        recv += computeComms[i].mNext.getTotalDataRecv();
    }
    for (u64 i = 0; i < delClientChls.size(); ++i) {
        if (i != pIndex) {
            sent += delClientChls[i].getTotalDataSent();
            recv += delClientChls[i].getTotalDataRecv();
            sent += delServerChls[i].getTotalDataSent();
            recv += delServerChls[i].getTotalDataRecv();
        }
    }

    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
        << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

    // for (u64 j = 0; j < 6; ++j) {
    //     if (j != pIndex) {
    //         computeChls[j].close();
    //         computeSessions[j].stop();
    //     }
    // }
    // for (u64 j = 0; j < numP; ++j) {
    //     if (j != pIndex) {
    //         delegateClientChls[j].close();
    //         delegateClientSessions[j].stop();
    //     }
    //     if (j != pIndex) {
    //         delegateServerChls[j].close();
    //         delegateServerSessions[j].stop();
    //     }
    // }


    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_Ours_single_party_demo(    
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters,
    std::string workspace_dir,
    std::vector<u64>& vertexIdList,
    std::vector<u64>& vertexDataList,
    std::vector<std::vector<std::array<u64, 2>>> incomingEdgeLists,
    std::vector<std::vector<std::array<u64, 2>>> outgoingEdgeLists
) {
	Alg alg;
	if (algId == 0) {
		alg = Alg::CC;
	} else if (algId == 1) {
		alg = Alg::SP;
	} else if (algId == 2) {
		alg = Alg::PR;
	} else {
		printf("Unexpected alg!\n");
		exit(-1);
	}

    IOService ios(0);
    std::vector<Session> computeSessions;
    std::vector<Session> delegateClientSessions; 
    std::vector<Session> delegateServerSessions; 
    std::vector<Channel> computeChls;
    std::vector<Channel> delegateClientChls; 
    std::vector<Channel> delegateServerChls; 

    u64 serverDstIdx = (pIndex + numP - 1) % numP;
    u64 helperDstIdx = (pIndex + numP - 2) % numP;

    uint32_t baseComPort = 1712;
    uint32_t baseDelPort = 1912;
    std::string baseIP = "10.0.0.";

    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 2, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 2, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP + 2) % numP + 1), baseComPort + 3 * ((pIndex + numP + 2) % numP) + 0, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02")); 

    for (u64 j = 0; j < 6; ++j)
        computeChls.emplace_back(computeSessions[j].addChannel("c"));     

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateClientSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    }

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
        else delegateClientChls.emplace_back(Channel());
    }    

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateServerSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    }  

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
        else delegateServerChls.emplace_back(Channel());
    }  

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    int numEdgePerP = (1 << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);
    std::vector<u64> numVertexList(numP, numVertexPerP);
  
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
            }
        }
    }  

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    // auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    // orCir_8->levelByAndDepth(); 

    CommPkg computeComms[3];
    computeComms[0] = { computeChls[5], computeChls[2] }; // Client Comms
    computeComms[1] = { computeChls[4], computeChls[1] }; // Server Comms
    computeComms[2] = { computeChls[3], computeChls[0] }; // Helper Comms       
    std::vector<Channel>& delClientChls = delegateClientChls;
    std::vector<Channel>& delServerChls = delegateServerChls;
    std::vector<i64Matrix> vertexDatas(3);
    vertexDatas[0].resize(numVertexList[pIndex], 1);
    vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
    vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
    for (u64 i = 0; i < 3; ++i) vertexDatas[i].setZero();
    for (u64 i = 0; i < numVertexList[pIndex]; ++i) {
        vertexDatas[0](i, 0) = vertexDataList[i];
    }
    std::vector<i64Matrix> interUpdateShare1(3);
    std::vector<i64Matrix> interUpdateShare2(3);

    // Load the data to aby3 plaintext data structure
    // For different characteristics: client, server, helper (maybe in different threads)
    auto scatterThread = [&](int role, i64Matrix& vertexDataShare, i64Matrix& updateShare1, i64Matrix& updateShare2) {
        std::vector<u64> srcTag;
        std::vector<u64> dstTag;
        std::vector<u64> dstEndTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[clientPIdx][i];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        srcTag.resize(numVertex);
        dstTag.resize(numEdge);
        dstEndTag.resize(numEdge);
        if (role == 0) {
            printf("numEdge: %lu\n", numEdge);
            for (u64 i = 0; i < numVertex; ++i) {
                srcTag[i] = vertexIdList[i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                    dstEndTag[cnt] = outgoingEdgeLists[i][j][1];
                    dstTag[cnt++] = outgoingEdgeLists[i][j][0];
                }
            }
        }

        if (role == 0) {
            print_duration_mutex.lock();
            printf(">>ScatterTask (Party %lu):\n", clientPIdx);
            printf(">>Src: ");
            for (u64 tt = 0; tt < srcTag.size(); ++tt) printf("(%lu) ", srcTag[tt]);
            printf("\n");
            printf(">>Dst: ");
            for (u64 tt = 0; tt < dstTag.size(); ++tt) printf("(%lu %lu) ", dstTag[tt], dstEndTag[tt]);
            printf("\n");
            print_duration_mutex.unlock();
        }

        our_scatter(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            srcTag, 
            dstTag, 
            vertexDataShare,
            updateShare,
            alg
        );      

        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[clientPIdx][i], 1);
            for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                updateShares[i](j, 0) = updateShare(cnt++, 0);
            }
        }
        if (role == 0) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("send delClient %d -> %d\n", pIndex, (i + 1) % numP);
                        // if (!delClientChls[(i + 1) % numP].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delClientChls %d %d %d\n", (i + 1) % numP, pIndex, role);
                        //     exit(-1);
                        // }
                        // delClientChls[(i + 1) % numP].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delClientChls[(i + 1) % numP]);
                    } else {
                        // Get the server share for P_{pIdx-1}
                        updateShare2 = updateShares[i];
                    }
                }
            }
        } else if (role == 1) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // printf("send delServer %d -> %d\n", pIndex, i);
                        // if (!delServerChls[i].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delServerChls %d %d %d\n", i, pIndex, role);
                        //     exit(-1);
                        // }
                        // delServerChls[i].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delServerChls[i]);
                    } else {
                        // Get the client share for P_{pIdx}
                        updateShare2 = updateShares[i];
                    }
                }
            }                
        }
    };

    auto gatherThread = [&](int role, i64Matrix& vertexDataShare, const i64Matrix& updateShare1, const i64Matrix& updateShare2) {
        i64Matrix updatedVertexDataShare = vertexDataShare;
        updatedVertexDataShare.setZero();
        std::vector<u64> vertexTag;
        std::vector<u64> dstTag;
        std::vector<u64> dstBeginTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[i][clientPIdx];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        vertexTag.resize(numVertex);
        dstTag.resize(numEdge);
        dstBeginTag.resize(numEdge);
        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[i][clientPIdx], 1);
        }
        if (role == 0) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("recv delServer %d -> %d\n", (i + 1) % numP, pIndex);
                        // delServerChls[(i + 1) % numP].recv(updateShares[i].data(), updateShares[i].size());
                        recvI64Mat(updateShares[i], updateShares[i].size(), delServerChls[(i + 1) % numP]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }
        } else if (role == 1) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // delClientChls[i].recv(updateShares[i].data(), updateShares[i].size());
                        // printf("recv delClient %d -> %d\n", i, pIndex);
                        recvI64Mat(updateShares[i], updateShares[i].size(), delClientChls[i]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }                
        } 
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                updateShare(cnt++, 0) = updateShares[i](j, 0);
            }
        }  
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                vertexTag[i] = vertexIdList[i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                    dstBeginTag[cnt] = incomingEdgeLists[i][j][0];
                    dstTag[cnt++] = incomingEdgeLists[i][j][1];
                }
            }
        } 

        if (role == 0) {
            print_duration_mutex.lock();
            printf(">>GatherTask (Party %lu):\n", clientPIdx);
            printf(">>GatherDst:");
            for (u64 tt = 0; tt < dstTag.size(); ++tt) printf("(%lu %lu) ", dstBeginTag[tt], dstTag[tt]);
            printf("\n");
            printf(">>Vertex:");
            for (auto tt : vertexTag) printf("(%lu) ", tt);
            printf("\n");
            print_duration_mutex.unlock();
        }

        our_gather_dummied(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            dstTag, 
            vertexTag,
            updateShare,
            vertexDataShare,
            updatedVertexDataShare,
            alg
        );        
        vertexDataShare = updatedVertexDataShare;
    };

    for (u64 iter = 0; iter < numIters; ++iter) {
        auto t_tmp = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> scatterThrds; 
        for (u64 role = 0; role < 3; ++role) {
            scatterThrds.emplace_back(scatterThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : scatterThrds)
            thrd.join();

        print_duration(t_tmp, "Scatter");
        t_tmp = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> gatherThrds;
        for (u64 role = 0; role < 3; ++role) {
            if (role != 2) gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[1 - role]));
            else gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : gatherThrds)
            thrd.join();

        print_duration(t_tmp, "Gather");            
    }

    // computeComms[1].mPrev.asyncSendCopy(vertexDatas[1].data(), vertexDatas[1].size());
    asyncSendI64Mat(vertexDatas[1], computeComms[1].mPrev);
    i64Matrix serverVertexData(numVertexList[pIndex], 1);
    serverVertexData.setZero();
    // computeComms[0].mNext.recv(serverVertexData.data(), serverVertexData.size());
    recvI64Mat(serverVertexData, serverVertexData.size(), computeComms[0].mNext);
    for (u64 i = 0; i < vertexDatas[0].size(); ++i) vertexDatas[0](i) ^= serverVertexData(i); 
    for (u64 i = 0; i <vertexDatas[0].size(); ++i) {
        vertexDataList[i] = vertexDatas[0](i, 0);
    }

    u64 sent = 0, recv = 0;
    for (u64 i = 0; i < 3; ++i) {
        sent += computeComms[i].mPrev.getTotalDataSent();
        recv += computeComms[i].mPrev.getTotalDataRecv();
        sent += computeComms[i].mNext.getTotalDataSent();
        recv += computeComms[i].mNext.getTotalDataRecv();
    }
    for (u64 i = 0; i < delClientChls.size(); ++i) {
        if (i != pIndex) {
            sent += delClientChls[i].getTotalDataSent();
            recv += delClientChls[i].getTotalDataRecv();
            sent += delServerChls[i].getTotalDataSent();
            recv += delServerChls[i].getTotalDataRecv();
        }
    }

    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
        << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

    // for (u64 j = 0; j < 6; ++j) {
    //     if (j != pIndex) {
    //         computeChls[j].close();
    //         computeSessions[j].stop();
    //     }
    // }
    // for (u64 j = 0; j < numP; ++j) {
    //     if (j != pIndex) {
    //         delegateClientChls[j].close();
    //         delegateClientSessions[j].stop();
    //     }
    //     if (j != pIndex) {
    //         delegateServerChls[j].close();
    //         delegateServerSessions[j].stop();
    //     }
    // }


    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_shuffle_test() {

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 20;
    std::atomic<bool> failed(false);
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    i64Matrix value(width, wordSize);

    PRNG prng(ZeroBlock);
    prng.get(value.data(), value.size());
    for (u64 i = 0; i < width; ++i) {
        for (u64 j = 0; j < wordSize; ++j) value(i, j) = i;
    }

    auto routine = [&](int pIdx) {
        CommPkg& comm = comms[pIdx];
        int role = pIdx;
        Sh3Runtime rt(role, comm);
        Sh3Encryptor enc;
        enc.init(role, toBlock(role), toBlock((role + 1) % 3));
        Sh3BinaryEvaluator eval;    
        eval.mPrng.SetSeed(toBlock(role));
        Sh3ShareGen gen;
        gen.init(toBlock(role), toBlock((role + 1) % 3));
        
        sbMatrix input(width, bitSize), output(width, bitSize);
        i64Matrix plainOutput(width, wordSize);

        if (pIdx == 0) enc.localBinMatrix(rt.noDependencies(), value, input).get();
        else enc.remoteBinMatrix(rt.noDependencies(), input).get();

        std::vector<u64> prevPerm, nextPerm;
        shuffle(
            comm.mPrev,
            comm.mNext,
            role,
            toBlock(pIdx),
            input,
            output,
            prevPerm,
            nextPerm,
            enc
        );

        // output = input;

        enc.revealAll(rt.noDependencies(), output, plainOutput).get();
        
        // if (pIdx == 0) {
        //     for (u64 i = 0; i < width; ++i) {
        //         for (u64 j = 0; j < wordSize; ++j) {
        //             oc::lout << u64(plainOutput(i, j)) << " ";
        //             // if (gtAgg(slot, j) != agged(slot, j)) {
        //             //     if (pIdx == 0) oc::lout << Color::Red << "pidx: " << pIdx << " failed at " << slot << " " << j << " "
        //             //         << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;
        //             //     failed = true;
        //             // } else {
        //             //     // if (pIdx == 0) oc::lout << Color::Green << "pidx: " << pIdx << " succeeded at " << slot << " " << j << " "
        //             //     //     << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;                    
        //             // }
        //         }
        //         oc::lout << std::endl;
        //     }
        // }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_sort_test() {

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 20;
    std::atomic<bool> failed(false);
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    i64Matrix value(width, wordSize);

    PRNG prng(ZeroBlock);
    prng.get(value.data(), value.size());
    for (u64 i = 0; i < width; ++i) {
        for (u64 j = 0; j < wordSize; ++j) value(i, j) = prng.get<u64>() % (width * 2);
    }

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(64, 64);
    ltCir->levelByAndDepth();
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);
    multiplexCir->levelByAndDepth();
    BetaCircuit *xorCir = lib.int_int_bitwiseXor(bitSize, bitSize, bitSize);
    xorCir->levelByAndDepth();

    auto routine = [&](int pIdx) {
        CommPkg& comm = comms[pIdx];
        int role = pIdx;
        Sh3Runtime rt(role, comm);
        Sh3Encryptor enc;
        enc.init(role, toBlock(role), toBlock((role + 1) % 3));
        Sh3BinaryEvaluator eval;    
        eval.mPrng.SetSeed(toBlock(role));
        Sh3ShareGen gen;
        gen.init(toBlock(role), toBlock((role + 1) % 3));
        
        sbMatrix input(width, bitSize), output(width, bitSize);
        i64Matrix plainOutput(width, wordSize);

        if (pIdx == 0) enc.localBinMatrix(rt.noDependencies(), value, input).get();
        else enc.remoteBinMatrix(rt.noDependencies(), input).get();

        std::vector<u64> perm(width);
        for (u64 i = 0; i < width; ++i) perm[i] = i;
        
        output = input;
        // ss_open_sort(
        //     output, 
        //     perm,
        //     eval,
        //     gen,
        //     rt,
        //     enc  
        // );
        ss_bitonic_sort(
            output, 
            eval,
            gen,
            rt,
            enc  
        );        

        enc.revealAll(rt.noDependencies(), output, plainOutput).get();
        
        // if (pIdx == 0) {
        //     for (u64 i = 0; i < width; ++i) {
        //         for (u64 j = 0; j < wordSize; ++j) {
        //             oc::lout << u64(value(i, j)) << " ";
        //             oc::lout << u64(plainOutput(i, j)) << " ";
        //             // if (gtAgg(slot, j) != agged(slot, j)) {
        //             //     if (pIdx == 0) oc::lout << Color::Red << "pidx: " << pIdx << " failed at " << slot << " " << j << " "
        //             //         << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;
        //             //     failed = true;
        //             // } else {
        //             //     // if (pIdx == 0) oc::lout << Color::Green << "pidx: " << pIdx << " succeeded at " << slot << " " << j << " "
        //             //     //     << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;                    
        //             // }
        //         }
        //         oc::lout << std::endl;
        //     }

        //     for (u64 i = 0; i < width; ++i) {
        //         printf("%lu ", perm[i]);
        //     }
        //     printf("\n");
        // }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_PrefixAgg_test() {

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 20;
    std::atomic<bool> failed(false);
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    i64Matrix value(width, wordSize);
    i64Matrix group_id(width, 1);
    std::vector<u64> group_id_vec(width, 0);

    PRNG prng(ZeroBlock);
    prng.get(value.data(), value.size());
    for (u64 i = 0; i < width; ++i) {
        for (u64 j = 0; j < wordSize; ++j) value(i, j) = prng.get<u64>() % (width * 2);
        group_id(i, 0) = i / 4;
        group_id_vec[i] = i / 4;
    }

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(64, 64);
    ltCir->levelByAndDepth();
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);
    multiplexCir->levelByAndDepth();
    BetaCircuit *xorCir = lib.int_int_bitwiseXor(bitSize, bitSize, bitSize);
    xorCir->levelByAndDepth();

    auto routine = [&](int pIdx) {
        CommPkg& comm = comms[pIdx];
        int role = pIdx;
        Sh3Runtime rt(role, comm);
        Sh3Encryptor enc;
        enc.init(role, toBlock(role), toBlock((role + 1) % 3));
        Sh3BinaryEvaluator eval;    
        eval.mPrng.SetSeed(toBlock(role));
        Sh3ShareGen gen;
        gen.init(toBlock(role), toBlock((role + 1) % 3));
        
        sbMatrix input(width, bitSize), output(width, bitSize);
        sbMatrix G(width, 64);
        i64Matrix plainOutput(width, wordSize);

        if (pIdx == 0) enc.localBinMatrix(rt.noDependencies(), value, input).get();
        else enc.remoteBinMatrix(rt.noDependencies(), input).get();

        if (pIdx == 0) enc.localBinMatrix(rt.noDependencies(), group_id, G).get();
        else enc.remoteBinMatrix(rt.noDependencies(), G).get();
        
        // output = prefix_network_aggregate(
        //     G,
        //     input,
        //     AggregationOp::MIN_AGG,
        //     eval,
        //     gen,
        //     rt,
        //     enc
        // );      

        output = prefix_network_aggregate(
            group_id_vec,
            input,
            AggregationOp::MIN_AGG,
            eval,
            gen,
            rt,
            enc
        );    

        // output = prefix_network_propagate(
        //     G,
        //     input,
        //     eval,
        //     gen,
        //     rt,
        //     enc
        // );     

        enc.revealAll(rt.noDependencies(), output, plainOutput).get();
        
        // if (pIdx == 0) {
        //     for (u64 i = 0; i < width; ++i) {
        //         oc::lout << u64(group_id(i, 0)) << " ";
        //         for (u64 j = 0; j < wordSize; ++j) {
        //             oc::lout << u64(value(i, j)) << " ";
        //             oc::lout << u64(plainOutput(i, j)) << " ";
        //             // if (gtAgg(slot, j) != agged(slot, j)) {
        //             //     if (pIdx == 0) oc::lout << Color::Red << "pidx: " << pIdx << " failed at " << slot << " " << j << " "
        //             //         << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;
        //             //     failed = true;
        //             // } else {
        //             //     // if (pIdx == 0) oc::lout << Color::Green << "pidx: " << pIdx << " succeeded at " << slot << " " << j << " "
        //             //     //     << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;                    
        //             // }
        //         }
        //         oc::lout << std::endl;
        //     }

        //     printf("\n");
        // }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_GraphSC_test() {

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    std::atomic<bool> failed(false);
    //bool manual = false;

    GraphParam param = GraphParam {
        num_iters: 5,
        alg: Alg::SP
    };
    u64 scale = 10;

    u64 numP = 5;
    u64 numVertexPerP = (1 << scale);
    u64 numIntraEdgePerP = (1 << scale);
    u64 numInterEdgePerPair = (1 << scale);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j)
            vertexIdLists[i][j] = i * numVertexPerP + j;
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
            }
        }
    }  

    // std::string file_path = "./../test-data/small_graph.csv";
    DataFrame df; // = load_dataframe_from_csv(file_path, false, false);
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            for (u64 k = 0; k < edgeLists[i][j].size(); ++k)
                df.push_back({(double)edgeLists[i][j][k][0], (double)edgeLists[i][j][k][1]});
        }
    }

    u64 bitSize = 64;

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(bitSize, bitSize);
    ltCir->levelByAndDepth();
    BetaCircuit *orCir =  lib.int_int_bitwiseOr(bitSize, bitSize, bitSize);
    orCir->levelByAndDepth();
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);
    multiplexCir->levelByAndDepth();
    BetaCircuit *xorCir = lib.int_int_bitwiseXor(bitSize, bitSize, bitSize);
    xorCir->levelByAndDepth();

    auto routine = [&](int pIdx) {
        GraphSC ana(
            df,
            param,
            pIdx,
            pIdx,
            comms[pIdx].mPrev,
            comms[pIdx].mNext
        );

        ana.run();
        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_Graph_GraphSC_single_party(    
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
) {
	Alg alg;
	if (algId == 0) {
		alg = Alg::CC;
	} else if (algId == 1) {
		alg = Alg::SP;
	} else if (algId == 2) {
		alg = Alg::PR;
	} else {
		printf("Unexpected alg!\n");
		exit(-1);
	}

    IOService ios;
    // std::string baseIP = "127.0.0.1";
    std::string baseIP = "10.0.0.";
    uint32_t basePort = 1712;
    std::vector<Session> sessions;
    if (pIndex == 0) {
        sessions.emplace_back(Session(ios, baseIP + std::to_string(2 + 1), basePort + 2, SessionMode::Client, "02")); // 02
        sessions.emplace_back(Session(ios, baseIP + std::to_string(0 + 1), basePort + 0, SessionMode::Server, "01")); // 01
    } else if (pIndex == 1) {
        sessions.emplace_back(Session(ios, baseIP + std::to_string(0 + 1), basePort + 0, SessionMode::Client, "01")); // 10
        sessions.emplace_back(Session(ios, baseIP + std::to_string(1 + 1), basePort + 1, SessionMode::Server, "12")); // 12
    } else if (pIndex == 2) {
        sessions.emplace_back(Session(ios, baseIP + std::to_string(1 + 1), basePort + 1, SessionMode::Client, "12")); // 21
        sessions.emplace_back(Session(ios, baseIP + std::to_string(2 + 1), basePort + 2, SessionMode::Server, "02")); // 20
    } else {
        printf("Illegal pIndex for GraphSC!\n");
        exit(-1);
    }
    // if (pIndex == 0) {
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 2, SessionMode::Client, "02")); // 02
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 0, SessionMode::Server, "01")); // 01
    // } else if (pIndex == 1) {
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 0, SessionMode::Client, "01")); // 10
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 1, SessionMode::Server, "12")); // 12
    // } else if (pIndex == 2) {
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 1, SessionMode::Client, "12")); // 21
    //     sessions.emplace_back(Session(ios, baseIP, basePort + 2, SessionMode::Server, "02")); // 20
    // } else {
    //     printf("Illegal pIndex for GraphSC!\n");
    //     exit(-1);
    // }

    Channel chl_front = sessions[0].addChannel("c");
    Channel chl_latter = sessions[1].addChannel("c");


    CommPkg comm;
    comm = { chl_front, chl_latter };

    std::atomic<bool> failed(false);
    //bool manual = false;

    GraphParam param = GraphParam {
        num_iters: numIters,
        alg: alg
    };

    u64 numVertexPerP = (1 << scale);
    int numEdgePerP = (1 << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j) {
            vertexIdLists[i][j] = i * numVertexPerP + j;
        }
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[i][k]});
                        if (++cnt > numIntraEdgePerP) break;
                    }
                    if (cnt > numIntraEdgePerP) break;
                }
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[j][k]});
                        if (++cnt > numInterEdgePerPair) break;
                    }
                    if (cnt > numInterEdgePerPair) break;
                }
            }
        }
    } 

    // std::string file_path = "./../test-data/small_graph.csv";
    DataFrame df; // = load_dataframe_from_csv(file_path, false, false);
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            for (u64 k = 0; k < edgeLists[i][j].size(); ++k)
                df.push_back({(double)edgeLists[i][j][k][0], (double)edgeLists[i][j][k][1]});
        }
    }

    u64 bitSize = 64;

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(bitSize, bitSize);
    ltCir->levelByAndDepth();
    BetaCircuit *orCir =  lib.int_int_bitwiseOr(bitSize, bitSize, bitSize);
    orCir->levelByAndDepth();
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);
    multiplexCir->levelByAndDepth();
    BetaCircuit *xorCir = lib.int_int_bitwiseXor(bitSize, bitSize, bitSize);
    xorCir->levelByAndDepth();

    GraphSC ana(
        df,
        param,
        pIndex,
        pIndex,
        comm.mPrev,
        comm.mNext
    );

    ana.run();
    u64 sent = 0, recv = 0;
    sent += comm.mPrev.getTotalDataSent();
    sent += comm.mNext.getTotalDataSent();
    recv += comm.mPrev.getTotalDataRecv();
    recv += comm.mNext.getTotalDataRecv();

    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
        << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

}

void Sh3_Graph_CoGNN_test()
{
    Alg alg = Alg::PR;
    u64 scale = 10;
    u64 numP = 5;
    std::vector<u64> pIndices(5, 0);
    for (u64 i = 0; i < numP; ++i) pIndices[i] = i;

    IOService ios;
    std::vector<std::vector<Session>> computeSessions(numP * numP);
    std::vector<std::vector<Session>> delegateClientSessions(numP); 
    std::vector<std::vector<Session>> delegateServerSessions(numP); 
    std::vector<std::vector<Channel>> computeChls(numP * numP);
    std::vector<std::vector<Channel>> delegateClientChls(numP); 
    std::vector<std::vector<Channel>> delegateServerChls(numP); 

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) continue;
            u64 curIndex = i * numP + j;
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(curIndex) + "-01"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(curIndex) + "-01"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(curIndex) + "-02"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(curIndex) + "-02"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(curIndex) + "-12"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(curIndex) + "-12")); 
            for (u64 k = 0; k < 6; ++k)
                computeChls[curIndex].emplace_back(computeSessions[curIndex][k].addChannel("c"));   
        }  
    }

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateClientSessions[i].emplace_back(Session());
            else if (i < j)
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleClient") + std::to_string(i) + std::to_string(j)));
            else
                delegateClientSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(i)));
        }

    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateClientChls[i].emplace_back(delegateClientSessions[i][j].addChannel("c"));
            else delegateClientChls[i].emplace_back(Channel());
        }    
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) 
                delegateServerSessions[i].emplace_back(Session());
            else if (i < j)
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("deleServer") + std::to_string(i) + std::to_string(j)));
            else
                delegateServerSessions[i].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(i)));
        }  
    }    

    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i != j) delegateServerChls[i].emplace_back(delegateServerSessions[i][j].addChannel("c"));
            else delegateServerChls[i].emplace_back(Channel());
        }    
    }  

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    u64 numIntraEdgePerP = (1 << scale);
    u64 numInterEdgePerPair = (1 << scale);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j)
            vertexIdLists[i][j] = i * numVertexPerP + j;
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
            }
        }
    }    

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    orCir_8->levelByAndDepth(); 

    u64 numIters = 5;

    auto routine = [&](int pIdx) {
        // u64 serverDstIdx = (pIdx + numP - 1) % numP;
        // u64 helperDstIdx = (pIdx + numP - 2) % numP;
        std::vector<CommPkg> clientComms(numP);
        std::vector<CommPkg> serverComms(numP);
        std::vector<CommPkg> helperComms(numP);
        for (u64 i = 0; i < numP; ++i) {
            if (i != pIdx) {
                clientComms[i] = { computeChls[pIdx * numP + i][2], computeChls[pIdx * numP + i][0] };
                serverComms[i] = { computeChls[i * numP + pIdx][1], computeChls[i * numP + pIdx][4] };
                if (i != (pIdx - 1 + numP) % numP) helperComms[i] = { computeChls[i * numP + ((pIdx - 1 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 1 + numP) % numP)][3] };
                else helperComms[i] = { computeChls[i * numP + ((pIdx - 2 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 2 + numP) % numP)][3] };
            }
        }
        // computeComms[0] = { computeChls[pIdx][2], computeChls[pIdx][0] }; // Client Comms
        // computeComms[1] = { computeChls[serverDstIdx][1], computeChls[serverDstIdx][4] }; // Server Comms
        // computeComms[2] = { computeChls[helperDstIdx][5], computeChls[helperDstIdx][3] }; // Helper Comms       
        std::vector<Channel>& delClientChls = delegateClientChls[pIdx];
        std::vector<Channel>& delServerChls = delegateServerChls[pIdx];
        i64Matrix clientVertexData(numVertexList[pIdx], 1);
        std::vector<i64Matrix> serverVertexDatas(numP);
        // vertexDatas[0].resize(numVertexList[pIdx], 1);
        // vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
        // vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
        for (u64 i = 0; i < numP; ++i) {
            serverVertexDatas[i].resize(numVertexList[i], 1);
            serverVertexDatas[i].setZero();
        }
        for (u64 i = 0; i < numVertexList[pIdx]; ++i) {
            clientVertexData(i, 0) = vertexDataLists[pIdx][i];
        }
        std::vector<i64Matrix> clientInterUpdateShare(numP);
        std::vector<i64Matrix> serverInterUpdateShare(numP);

        // Load the data to aby3 plaintext data structure
        // For different characteristics: client, server, helper (maybe in different threads)
        auto scatterThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, i64Matrix& vertexDataShare, i64Matrix& updateShare1, int iter, Channel prevChl, Channel nextChl) {
            std::vector<u64> srcVertexTag;
            std::vector<u64> edgeSrcTag;
            std::vector<u64> edgeDstTag;
            std::vector<u64> dstVertexTag;
            i64Matrix updateShare;
            int helperPIdx = (serverPIdx + 1) % numP;;
            if (helperPIdx == clientPIdx) helperPIdx = (serverPIdx + 2) % numP;
 
            u64 numVertex = numVertexList[clientPIdx];
            u64 numDstVertex = numVertexList[dstPIdx];
            u64 numEdge = numEdgeMat[clientPIdx][dstPIdx];
            if (role == 2) {
                vertexDataShare.resize(numVertex, 1);
                vertexDataShare.setZero();
            }
            if (role == 1 && iter != 0 && serverPIdx != (clientPIdx + 1) % numP) {
                delServerChls[(clientPIdx + 1) % numP].recv(vertexDataShare.data(), vertexDataShare.size());                
            }
            
            updateShare.resize(numDstVertex, 1);
            updateShare.setZero();
            srcVertexTag.resize(numVertex);
            edgeSrcTag.resize(numEdge);
            edgeDstTag.resize(numEdge);
            dstVertexTag.resize(numDstVertex);
            if (role == 0) {
                for (u64 i = 0; i < numVertex; ++i) {
                    srcVertexTag[i] = vertexIdLists[clientPIdx][i];
                }
                for (u64 i = 0; i < numEdge; ++i) {
                    edgeSrcTag[i] = edgeLists[clientPIdx][dstPIdx][i][0];
                }
                for (u64 i = 0; i < numEdge; ++i) {
                    edgeDstTag[i] = edgeLists[clientPIdx][dstPIdx][i][1];
                }
                if (clientPIdx == dstPIdx) {
                    for (u64 i = 0; i < numDstVertex; ++i) {
                        dstVertexTag[i] = vertexIdLists[dstPIdx][i];
                    }                    
                }
            } else if (role == 1 && serverPIdx == dstPIdx) {
                for (u64 i = 0; i < numEdge; ++i) {
                    edgeDstTag[i] = edgeLists[clientPIdx][dstPIdx][i][1];
                }
                for (u64 i = 0; i < numDstVertex; ++i) {
                    dstVertexTag[i] = vertexIdLists[dstPIdx][i];
                }
            }
            bool isLocal = (dstPIdx == clientPIdx);
            cognn_scatter(
                prevChl,
                nextChl,
                pIdx,
                role,
                isLocal,
                srcVertexTag, 
                edgeSrcTag, 
                edgeDstTag,
                dstVertexTag,
                vertexDataShare,
                updateShare,
                alg
            );      

            if (!isLocal && role == 0 && (pIdx != (dstPIdx + 1) % numP)) {
                delClientChls[(dstPIdx + 1) % numP].asyncSendCopy(updateShare.data(), updateShare.size());
            } 

            if (role == 0 && (isLocal || pIdx == (dstPIdx + 1) % numP)) {
                updateShare1 = updateShare;
            } else if (role == 1) {
                updateShare1 = updateShare;
            }
        };

        auto gatherThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, i64Matrix& vertexDataShare, const std::vector<i64Matrix>& updateShares, int iter, Channel prevChl, Channel nextChl) {
            u64 numVertex = numVertexList[dstPIdx];
            if (role == 2) {
                vertexDataShare.resize(numVertex, 1);
                vertexDataShare.setZero(); 
            }            
            
            i64Matrix updatedVertexDataShare = vertexDataShare;
            updatedVertexDataShare.setZero();
            std::vector<i64Matrix> orderedUpdateShares(numP);
            int helperPIdx = (serverPIdx + 1) % numP;;
            if (helperPIdx == clientPIdx) helperPIdx = (serverPIdx + 2) % numP;

            if (role == 0) {
                orderedUpdateShares = updateShares;
            } else if (role == 1 || role == 2) {       
                for (u64 i = 0; i < numP; ++i) {
                    orderedUpdateShares[i].resize(numVertex, 1);
                    orderedUpdateShares[i].setZero();
                } 
            }

            if (role == 1) {
                orderedUpdateShares[serverPIdx] = updateShares[0];
                orderedUpdateShares[dstPIdx] = updateShares[1];
                for (int i = 0; i < numP; ++i) {
                    if (i != serverPIdx && i != clientPIdx) {
                        delClientChls[i].recv(orderedUpdateShares[i].data(), orderedUpdateShares[i].size());
                    }
                }
            } 

            cognn_gather(
                prevChl,
                nextChl,
                pIdx,
                role,
                orderedUpdateShares,
                vertexDataShare,
                updatedVertexDataShare,
                alg
            );        
            vertexDataShare = updatedVertexDataShare;

            if (role == 1 && iter != numIters - 1) {
                for (int i = 0; i < numP; ++i) {
                    if (i != ((clientPIdx + 1) % numP) && i != clientPIdx) delServerChls[i].asyncSendCopy(vertexDataShare.data(), vertexDataShare.size());
                }
            }
        };

        for (u64 iter = 0; iter < numIters; ++iter) {
            std::vector<std::thread> scatterClientThrds; 
            std::vector<std::thread> scatterServerThrds;
            std::vector<std::thread> scatterHelperThrds;
            // std::thread localScatter(0, pIdx, (pIdx + 1) % numP, pIdx, std::ref(vertexDatas[role][pIdx]), std::ref(interUpdateShare1[role][pIdx]), iter, Channel& prevChl, Channel& nextChl);
            scatterClientThrds.emplace_back(scatterThread, 0, pIdx, (pIdx + 1) % numP, pIdx, std::ref(clientVertexData), std::ref(clientInterUpdateShare[pIdx]), iter, clientComms[(pIdx + 1) % numP].mPrev, clientComms[(pIdx + 1) % numP].mNext);
            scatterServerThrds.emplace_back(scatterThread, 1, (pIdx - 1 + numP) % numP, pIdx, (pIdx - 1 + numP) % numP, std::ref(serverVertexDatas[pIdx]), std::ref(serverInterUpdateShare[pIdx]), iter, serverComms[(pIdx - 1 + numP) % numP].mPrev, serverComms[(pIdx - 1 + numP) % numP].mNext);
            i64Matrix helperShare[2];
            scatterHelperThrds.emplace_back(scatterThread, 2, (pIdx - 2 + numP) % numP, (pIdx - 1 + numP) % numP, (pIdx - 2 + numP) % numP, std::ref(helperShare[0]), std::ref(helperShare[1]), iter, helperComms[(pIdx - 2 + numP) % numP].mPrev, helperComms[(pIdx - 2 + numP) % numP].mNext);
            scatterClientThrds[0].join();
            scatterServerThrds[0].join();
            scatterHelperThrds[0].join();
            std::vector<std::array<i64Matrix, 2>> helperShares(numP);
            for (u64 i = 0; i < numP; ++i) {
                if (i != pIdx) {
                    // int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, Matrix<u8>& updateShare1, int iter, Channel& prevChl, Channel& nextChl
                    scatterClientThrds.emplace_back(scatterThread, 0, pIdx, i, i, std::ref(clientVertexData), std::ref(clientInterUpdateShare[i]), iter, clientComms[i].mPrev, clientComms[i].mNext);
                    scatterServerThrds.emplace_back(scatterThread, 1, i, pIdx, pIdx, std::ref(serverVertexDatas[i]), std::ref(serverInterUpdateShare[i]), iter, serverComms[i].mPrev, serverComms[i].mNext);
                    int clientPIdx, serverPIdx;
                    if (i != (pIdx - 1 + numP) % numP) {
                        clientPIdx = i;
                        serverPIdx = (pIdx - 1 + numP) % numP;
                    } else {
                        clientPIdx = i;
                        serverPIdx = (pIdx - 2 + numP) % numP;                       
                    }
                    std::array<i64Matrix, 2>& helperShare = helperShares[i];
                    scatterHelperThrds.emplace_back(scatterThread, 2, clientPIdx, serverPIdx, serverPIdx, std::ref(helperShare[0]), std::ref(helperShare[1]), iter, helperComms[i].mPrev, helperComms[i].mNext);
                    // if (i != (pIdx - 1 + numP) % numP) helperComms[i] = { computeChls[i * numP + ((pIdx - 1 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 1 + numP) % numP)][3] };
                    // else helperComms[i] = { computeChls[i * numP + ((pIdx - 2 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 2 + numP) % numP)][3] };
                }
            }
            for (int i = 1; i < numP; ++i) {
                scatterClientThrds[i].join();
                scatterServerThrds[i].join();
                scatterHelperThrds[i].join();
            }

            std::vector<i64Matrix> clientUpdateShare = serverInterUpdateShare;
            clientUpdateShare[pIdx] = clientInterUpdateShare[pIdx];
            std::vector<i64Matrix> serverUpdateShare(2);
            serverUpdateShare[0] = clientInterUpdateShare[(pIdx - 1 + numP) % numP];
            serverUpdateShare[1] = serverInterUpdateShare[pIdx];

            std::vector<i64Matrix> helperShareVec;
            // int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, const std::vector<Matrix<u8>>& updateShares, int iter, Channel& prevChl, Channel& nextChl
            std::thread gatherClientThrd(gatherThread, 0, pIdx, (pIdx + 1) % numP, pIdx, std::ref(clientVertexData), std::ref(clientUpdateShare), iter, clientComms[(pIdx + 1) % numP].mPrev, clientComms[(pIdx + 1) % numP].mNext); 
            std::thread gatherServerThrd(gatherThread, 1, (pIdx - 1 + numP) % numP, pIdx, (pIdx - 1 + numP) % numP, std::ref(serverVertexDatas[(pIdx - 1 + numP) % numP]), std::ref(serverUpdateShare), iter, serverComms[(pIdx - 1 + numP) % numP].mPrev, serverComms[(pIdx - 1 + numP) % numP].mNext);  
            std::thread gatherHelperThrd(gatherThread, 2, (pIdx - 2 + numP) % numP, (pIdx - 1 + numP) % numP, (pIdx - 2 + numP) % numP, std::ref(helperShare[0]), std::ref(helperShareVec), iter, helperComms[(pIdx - 2 + numP) % numP].mPrev, helperComms[(pIdx - 2 + numP) % numP].mNext);     
            gatherClientThrd.join(); 
            gatherServerThrd.join();
            gatherHelperThrd.join();
            serverVertexDatas[pIdx] = serverVertexDatas[(pIdx - 1 + numP) % numP];

            // std::cout << IoStream::lock;
            // std::cout << "pIdx::" << pIdx << " iter = " << iter << std::endl;
            // std::cout << IoStream::unlock;
        }
    
        serverComms[(pIdx - 1 + numP) % numP].mPrev.asyncSendCopy(serverVertexDatas[(pIdx - 1 + numP) % numP].data(), serverVertexDatas[(pIdx - 1 + numP) % numP].size());
        i64Matrix serverVertexData(numVertexList[pIdx], 1);
        serverVertexData.setZero();
        clientComms[(pIdx + 1) % numP].mNext.recv(serverVertexData.data(), serverVertexData.size());
        for (u64 i = 0; i < clientVertexData.size(); ++i) clientVertexData(i) ^= serverVertexData(i); 

        u64 sent = 0, recv = 0;
        for (u64 i = 0; i < numP; ++i) {
            if (i != pIdx) {
                sent += clientComms[i].mPrev.getTotalDataSent();
                sent += clientComms[i].mNext.getTotalDataSent();
                recv += clientComms[i].mPrev.getTotalDataRecv();
                recv += clientComms[i].mNext.getTotalDataRecv();
                sent += serverComms[i].mNext.getTotalDataSent();
                sent += serverComms[i].mPrev.getTotalDataSent();
                recv += serverComms[i].mNext.getTotalDataRecv();
                recv += serverComms[i].mPrev.getTotalDataRecv();
                sent += helperComms[i].mNext.getTotalDataSent();
                sent += helperComms[i].mPrev.getTotalDataSent();
                recv += helperComms[i].mNext.getTotalDataRecv();
                recv += helperComms[i].mPrev.getTotalDataRecv();
            }
        }
        for (u64 i = 0; i < delClientChls.size(); ++i) {
            if (i != pIdx) {
                sent += delClientChls[i].getTotalDataSent();
                recv += delClientChls[i].getTotalDataRecv();
                sent += delServerChls[i].getTotalDataSent();
                recv += delServerChls[i].getTotalDataRecv();
            }
        }

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;
    };

    std::vector<std::thread> thrds;
    for (u64 i = 0; i < numP; ++i)
        thrds.emplace_back(std::thread(routine, i));

    for (u64 i = 0; i < numP; ++i)
        thrds[i].join();

    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_CoGNN_single_party(
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
) {
	Alg alg;
	if (algId == 0) {
		alg = Alg::CC;
	} else if (algId == 1) {
		alg = Alg::SP;
	} else if (algId == 2) {
		alg = Alg::PR;
	} else {
		printf("Unexpected alg!\n");
		exit(-1);
	}

    IOService ios;
    std::vector<std::vector<Session>> computeSessions(numP);
    std::vector<Session> delegateClientSessions; 
    std::vector<Session> delegateServerSessions; 
    std::vector<std::vector<Channel>> computeChls(numP);
    std::vector<Channel> delegateClientChls; 
    std::vector<Channel> delegateServerChls; 
    // std::string baseIP = "127.0.0.1";
    std::string baseIP = "10.0.0.";

    uint32_t baseComPort = 1712;
    uint32_t baseDelPort = 1912;

    // for (u64 i = 0; i < numP; ++i) {
    //     if (i != pIdx) {
    //         clientComms[i] = { computeChls[pIdx * numP + i][2], computeChls[pIdx * numP + i][0] };
    //         serverComms[i] = { computeChls[i * numP + pIdx][1], computeChls[i * numP + pIdx][4] };
    //         if (i != (pIdx - 1 + numP) % numP) helperComms[i] = { computeChls[i * numP + ((pIdx - 1 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 1 + numP) % numP)][3] };
    //         else helperComms[i] = { computeChls[i * numP + ((pIdx - 2 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 2 + numP) % numP)][3] };
    //     }
    // }

    // ---->
    for (u64 i = 0; i < numP; ++i) {
        if (i != pIndex) {
            u64 tmpH = 0; 
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + numP * 3 * pIndex + i * 3 + 0, SessionMode::Server, std::string("comp") + std::to_string(pIndex * numP + i) + "-01"));
            if (((i + 1) % numP) != pIndex) {
                tmpH = ((i + 1) % numP);
            } else {
                tmpH = ((i + 2) % numP);
            }
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(tmpH + 1), baseComPort + numP * 3 * tmpH + pIndex * 3 + 2, SessionMode::Client, std::string("comp") + std::to_string(pIndex * numP + i) + "-02"));
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + numP * 3 * pIndex + i * 3 + 1, SessionMode::Server, std::string("comp") + std::to_string(i * numP + pIndex) + "-12"));
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(i + 1), baseComPort + numP * 3 * i + pIndex * 3 + 0, SessionMode::Client, std::string("comp") + std::to_string(i * numP + pIndex) + "-01"));
            u64 tmp = 0; 
            if (i != (pIndex + numP - 1) % numP) {
                tmp = i * numP + ((pIndex - 1 + numP) % numP);
            } else {
                tmp = i * numP + ((pIndex - 2 + numP) % numP);
            }
            u64 tmpS = 0; 
            if (i != ((pIndex + numP - 1) % numP)) {
                tmpS = ((pIndex + numP - 1) % numP);
            } else {
                tmpS = ((pIndex + numP - 2) % numP);
            }
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + numP * 3 * pIndex + i * 3 + 2, SessionMode::Server, std::string("comp") + std::to_string(tmp) + "-02"));
            computeSessions[i].emplace_back(Session(ios, baseIP + std::to_string(tmpS + 1), baseComPort + numP * 3 * tmpS + i * 3 + 1, SessionMode::Client, std::string("comp") + std::to_string(tmp) + "-12"));
            for (u64 k = 0; k < 6; ++k)
                computeChls[i].emplace_back(computeSessions[i][k].addChannel("c"));   
        }
    }
    // <----

    // for (u64 i = 0; i < numP; ++i) {
    //     if (i != pIndex) {
    //         u64 tmpH = 0; 
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * pIndex + i * 3 + 0, SessionMode::Server, std::string("comp") + std::to_string(pIndex * numP + i) + "-01"));
    //         if (((i + 1) % numP) != pIndex) {
    //             tmpH = ((i + 1) % numP);
    //         } else {
    //             tmpH = ((i + 2) % numP);
    //         }
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * tmpH + pIndex * 3 + 2, SessionMode::Client, std::string("comp") + std::to_string(pIndex * numP + i) + "-02"));
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * pIndex + i * 3 + 1, SessionMode::Server, std::string("comp") + std::to_string(i * numP + pIndex) + "-12"));
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * i + pIndex * 3 + 0, SessionMode::Client, std::string("comp") + std::to_string(i * numP + pIndex) + "-01"));
    //         u64 tmp = 0; 
    //         if (i != (pIndex + numP - 1) % numP) {
    //             tmp = i * numP + ((pIndex - 1 + numP) % numP);
    //         } else {
    //             tmp = i * numP + ((pIndex - 2 + numP) % numP);
    //         }
    //         u64 tmpS = 0; 
    //         if (i != ((pIndex + numP - 1) % numP)) {
    //             tmpS = ((pIndex + numP - 1) % numP);
    //         } else {
    //             tmpS = ((pIndex + numP - 2) % numP);
    //         }
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * pIndex + i * 3 + 2, SessionMode::Server, std::string("comp") + std::to_string(tmp) + "-02"));
    //         computeSessions[i].emplace_back(Session(ios, baseIP, baseComPort + numP * 3 * tmpS + i * 3 + 1, SessionMode::Client, std::string("comp") + std::to_string(tmp) + "-12"));
    //         for (u64 k = 0; k < 6; ++k)
    //             computeChls[i].emplace_back(computeSessions[i][k].addChannel("c"));   
    //     }
    // }

    // ---->
    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateClientSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    }


    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
        else delegateClientChls.emplace_back(Channel());
    }    

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateServerSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    }  

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
        else delegateServerChls.emplace_back(Channel());
    }  
    // <----  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateClientSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    // }


    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
    //     else delegateClientChls.emplace_back(Channel());
    // }    

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateServerSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    // }  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
    //     else delegateServerChls.emplace_back(Channel());
    // }  

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    int numEdgePerP = (1 << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j) {
            vertexIdLists[i][j] = i * numVertexPerP + j;
        }
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[i][k]});
                        if (++cnt > numIntraEdgePerP) break;
                    }
                    if (cnt > numIntraEdgePerP) break;
                }
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[j][k]});
                        if (++cnt > numInterEdgePerPair) break;
                    }
                    if (cnt > numInterEdgePerPair) break;
                }
            }
        }
    }   

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    orCir_8->levelByAndDepth(); 

    std::vector<CommPkg> clientComms(numP);
    std::vector<CommPkg> serverComms(numP);
    std::vector<CommPkg> helperComms(numP);
    for (u64 i = 0; i < numP; ++i) {
        if (i != pIndex) {
            clientComms[i] = { computeChls[i][1], computeChls[i][0] };
            serverComms[i] = { computeChls[i][3], computeChls[i][2] };
            helperComms[i] = { computeChls[i][5], computeChls[i][4] };
        }
    }
    // computeComms[0] = { computeChls[pIdx][2], computeChls[pIdx][0] }; // Client Comms
    // computeComms[1] = { computeChls[serverDstIdx][1], computeChls[serverDstIdx][4] }; // Server Comms
    // computeComms[2] = { computeChls[helperDstIdx][5], computeChls[helperDstIdx][3] }; // Helper Comms       
    std::vector<Channel>& delClientChls = delegateClientChls;
    std::vector<Channel>& delServerChls = delegateServerChls;
    i64Matrix clientVertexData(numVertexList[pIndex], 1);
    std::vector<i64Matrix> serverVertexDatas(numP);
    // vertexDatas[0].resize(numVertexList[pIdx], 1);
    // vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
    // vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
    for (u64 i = 0; i < numP; ++i) {
        serverVertexDatas[i].resize(numVertexList[i], 1);
        serverVertexDatas[i].setZero();
    }
    for (u64 i = 0; i < numVertexList[pIndex]; ++i) {
        clientVertexData(i, 0) = vertexDataLists[pIndex][i];
    }
    std::vector<i64Matrix> clientInterUpdateShare(numP);
    std::vector<i64Matrix> serverInterUpdateShare(numP);

    // Load the data to aby3 plaintext data structure
    // For different characteristics: client, server, helper (maybe in different threads)
    auto scatterThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, i64Matrix& vertexDataShare, i64Matrix& updateShare1, int iter, Channel prevChl, Channel nextChl) {
        std::vector<u64> srcVertexTag;
        std::vector<u64> edgeSrcTag;
        std::vector<u64> edgeDstTag;
        std::vector<u64> dstVertexTag;
        i64Matrix updateShare;
        int helperPIdx = (serverPIdx + 1) % numP;;
        if (helperPIdx == clientPIdx) helperPIdx = (serverPIdx + 2) % numP;

        u64 numVertex = numVertexList[clientPIdx];
        u64 numDstVertex = numVertexList[dstPIdx];
        u64 numEdge = numEdgeMat[clientPIdx][dstPIdx];
        if (role == 2) {
            vertexDataShare.resize(numVertex, 1);
            vertexDataShare.setZero();
        }
        if (role == 1 && iter != 0 && serverPIdx != (clientPIdx + 1) % numP) {
            // delServerChls[(clientPIdx + 1) % numP].recv(vertexDataShare.data(), vertexDataShare.size());
            recvI64Mat(vertexDataShare, vertexDataShare.size(), delServerChls[(clientPIdx + 1) % numP]);               
        }
        
        updateShare.resize(numDstVertex, 1);
        updateShare.setZero();
        srcVertexTag.resize(numVertex);
        edgeSrcTag.resize(numEdge);
        edgeDstTag.resize(numEdge);
        dstVertexTag.resize(numDstVertex);
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                srcVertexTag[i] = vertexIdLists[clientPIdx][i];
            }
            for (u64 i = 0; i < numEdge; ++i) {
                edgeSrcTag[i] = edgeLists[clientPIdx][dstPIdx][i][0];
            }
            for (u64 i = 0; i < numEdge; ++i) {
                edgeDstTag[i] = edgeLists[clientPIdx][dstPIdx][i][1];
            }
            if (clientPIdx == dstPIdx) {
                for (u64 i = 0; i < numDstVertex; ++i) {
                    dstVertexTag[i] = vertexIdLists[dstPIdx][i];
                }                    
            }
        } else if (role == 1 && serverPIdx == dstPIdx) {
            for (u64 i = 0; i < numEdge; ++i) {
                edgeDstTag[i] = edgeLists[clientPIdx][dstPIdx][i][1];
            }
            for (u64 i = 0; i < numDstVertex; ++i) {
                dstVertexTag[i] = vertexIdLists[dstPIdx][i];
            }
        }
        bool isLocal = (dstPIdx == clientPIdx);
        cognn_scatter(
            prevChl,
            nextChl,
            pIndex,
            role,
            isLocal,
            srcVertexTag, 
            edgeSrcTag, 
            edgeDstTag,
            dstVertexTag,
            vertexDataShare,
            updateShare,
            alg
        );      

        if (!isLocal && role == 0 && (pIndex != (dstPIdx + 1) % numP)) {
            // delClientChls[(dstPIdx + 1) % numP].asyncSendCopy(updateShare.data(), updateShare.size());
            asyncSendI64Mat(updateShare, delClientChls[(dstPIdx + 1) % numP]);
        } 

        if (role == 0 && (isLocal || pIndex == (dstPIdx + 1) % numP)) {
            updateShare1 = updateShare;
        } else if (role == 1) {
            updateShare1 = updateShare;
        }
    };

    auto gatherThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, i64Matrix& vertexDataShare, const std::vector<i64Matrix>& updateShares, int iter, Channel prevChl, Channel nextChl) {
        u64 numVertex = numVertexList[dstPIdx];
        if (role == 2) {
            vertexDataShare.resize(numVertex, 1);
            vertexDataShare.setZero(); 
        }            
        
        i64Matrix updatedVertexDataShare = vertexDataShare;
        updatedVertexDataShare.setZero();
        std::vector<i64Matrix> orderedUpdateShares(numP);
        int helperPIdx = (serverPIdx + 1) % numP;;
        if (helperPIdx == clientPIdx) helperPIdx = (serverPIdx + 2) % numP;

        if (role == 0) {
            orderedUpdateShares = updateShares;
        } else if (role == 1 || role == 2) {       
            for (u64 i = 0; i < numP; ++i) {
                orderedUpdateShares[i].resize(numVertex, 1);
                orderedUpdateShares[i].setZero();
            } 
        }

        if (role == 1) {
            orderedUpdateShares[serverPIdx] = updateShares[0];
            orderedUpdateShares[dstPIdx] = updateShares[1];
            for (int i = 0; i < numP; ++i) {
                if (i != serverPIdx && i != clientPIdx) {
                    // delClientChls[i].recv(orderedUpdateShares[i].data(), orderedUpdateShares[i].size());
                    recvI64Mat(orderedUpdateShares[i], orderedUpdateShares[i].size(), delClientChls[i]);
                }
            }
        } 

        cognn_gather(
            prevChl,
            nextChl,
            pIndex,
            role,
            orderedUpdateShares,
            vertexDataShare,
            updatedVertexDataShare,
            alg
        );        
        vertexDataShare = updatedVertexDataShare;

        if (role == 1 && iter != numIters - 1) {
            for (int i = 0; i < numP; ++i) {
                if (i != ((clientPIdx + 1) % numP) && i != clientPIdx) 
                    asyncSendI64Mat(vertexDataShare, delServerChls[i]);
                    // delServerChls[i].asyncSendCopy(vertexDataShare.data(), vertexDataShare.size());
            }
        }
    };

    for (u64 iter = 0; iter < numIters; ++iter) {
        auto t_tmp = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> scatterClientThrds; 
        std::vector<std::thread> scatterServerThrds;
        std::vector<std::thread> scatterHelperThrds;
        // std::thread localScatter(0, pIdx, (pIdx + 1) % numP, pIdx, std::ref(vertexDatas[role][pIdx]), std::ref(interUpdateShare1[role][pIdx]), iter, Channel& prevChl, Channel& nextChl);
        scatterClientThrds.emplace_back(scatterThread, 0, pIndex, (pIndex + 1) % numP, pIndex, std::ref(clientVertexData), std::ref(clientInterUpdateShare[pIndex]), iter, clientComms[(pIndex + 1) % numP].mPrev, clientComms[(pIndex + 1) % numP].mNext);
        scatterServerThrds.emplace_back(scatterThread, 1, (pIndex - 1 + numP) % numP, pIndex, (pIndex - 1 + numP) % numP, std::ref(serverVertexDatas[pIndex]), std::ref(serverInterUpdateShare[pIndex]), iter, serverComms[(pIndex - 1 + numP) % numP].mPrev, serverComms[(pIndex - 1 + numP) % numP].mNext);
        i64Matrix helperShare[2];
        scatterHelperThrds.emplace_back(scatterThread, 2, (pIndex - 2 + numP) % numP, (pIndex - 1 + numP) % numP, (pIndex - 2 + numP) % numP, std::ref(helperShare[0]), std::ref(helperShare[1]), iter, helperComms[(pIndex - 2 + numP) % numP].mPrev, helperComms[(pIndex - 2 + numP) % numP].mNext);
        scatterClientThrds[0].join();
        scatterServerThrds[0].join();
        scatterHelperThrds[0].join();
        std::vector<std::array<i64Matrix, 2>> helperShares(numP);
        for (u64 i = 0; i < numP; ++i) {
            if (i != pIndex) {
                // int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, Matrix<u8>& updateShare1, int iter, Channel& prevChl, Channel& nextChl
                scatterClientThrds.emplace_back(scatterThread, 0, pIndex, i, i, std::ref(clientVertexData), std::ref(clientInterUpdateShare[i]), iter, clientComms[i].mPrev, clientComms[i].mNext);
                scatterServerThrds.emplace_back(scatterThread, 1, i, pIndex, pIndex, std::ref(serverVertexDatas[i]), std::ref(serverInterUpdateShare[i]), iter, serverComms[i].mPrev, serverComms[i].mNext);
                int clientPIdx, serverPIdx;
                if (i != (pIndex - 1 + numP) % numP) {
                    clientPIdx = i;
                    serverPIdx = (pIndex - 1 + numP) % numP;
                } else {
                    clientPIdx = i;
                    serverPIdx = (pIndex - 2 + numP) % numP;                       
                }
                std::array<i64Matrix, 2>& helperShare = helperShares[i];
                scatterHelperThrds.emplace_back(scatterThread, 2, clientPIdx, serverPIdx, serverPIdx, std::ref(helperShare[0]), std::ref(helperShare[1]), iter, helperComms[i].mPrev, helperComms[i].mNext);
                // if (i != (pIdx - 1 + numP) % numP) helperComms[i] = { computeChls[i * numP + ((pIdx - 1 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 1 + numP) % numP)][3] };
                // else helperComms[i] = { computeChls[i * numP + ((pIdx - 2 + numP) % numP)][5], computeChls[i * numP + ((pIdx - 2 + numP) % numP)][3] };
            }
        }
        for (int i = 1; i < numP; ++i) {
            scatterClientThrds[i].join();
            scatterServerThrds[i].join();
            scatterHelperThrds[i].join();
        }

        print_duration(t_tmp, "Scatter");
        t_tmp = std::chrono::high_resolution_clock::now();

        std::vector<i64Matrix> clientUpdateShare = serverInterUpdateShare;
        clientUpdateShare[pIndex] = clientInterUpdateShare[pIndex];
        std::vector<i64Matrix> serverUpdateShare(2);
        serverUpdateShare[0] = clientInterUpdateShare[(pIndex - 1 + numP) % numP];
        serverUpdateShare[1] = serverInterUpdateShare[pIndex];

        std::vector<i64Matrix> helperShareVec;
        // int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, const std::vector<Matrix<u8>>& updateShares, int iter, Channel& prevChl, Channel& nextChl
        std::thread gatherClientThrd(gatherThread, 0, pIndex, (pIndex + 1) % numP, pIndex, std::ref(clientVertexData), std::ref(clientUpdateShare), iter, clientComms[(pIndex + 1) % numP].mPrev, clientComms[(pIndex + 1) % numP].mNext); 
        std::thread gatherServerThrd(gatherThread, 1, (pIndex - 1 + numP) % numP, pIndex, (pIndex - 1 + numP) % numP, std::ref(serverVertexDatas[(pIndex - 1 + numP) % numP]), std::ref(serverUpdateShare), iter, serverComms[(pIndex - 1 + numP) % numP].mPrev, serverComms[(pIndex - 1 + numP) % numP].mNext);  
        std::thread gatherHelperThrd(gatherThread, 2, (pIndex - 2 + numP) % numP, (pIndex - 1 + numP) % numP, (pIndex - 2 + numP) % numP, std::ref(helperShare[0]), std::ref(helperShareVec), iter, helperComms[(pIndex - 2 + numP) % numP].mPrev, helperComms[(pIndex - 2 + numP) % numP].mNext);     
        gatherClientThrd.join(); 
        gatherServerThrd.join();
        gatherHelperThrd.join();
        serverVertexDatas[pIndex] = serverVertexDatas[(pIndex - 1 + numP) % numP];

        print_duration(t_tmp, "Gather");

        // std::cout << IoStream::lock;
        // std::cout << "pIdx::" << pIdx << " iter = " << iter << std::endl;
        // std::cout << IoStream::unlock;
    }

    // serverComms[(pIndex - 1 + numP) % numP].mPrev.asyncSendCopy(serverVertexDatas[(pIndex - 1 + numP) % numP].data(), serverVertexDatas[(pIndex - 1 + numP) % numP].size());
    asyncSendI64Mat(serverVertexDatas[(pIndex - 1 + numP) % numP], serverComms[(pIndex - 1 + numP) % numP].mPrev);
    i64Matrix serverVertexData(numVertexList[pIndex], 1);
    serverVertexData.setZero();
    // clientComms[(pIndex + 1) % numP].mNext.recv(serverVertexData.data(), serverVertexData.size());
    recvI64Mat(serverVertexData, serverVertexData.size(), clientComms[(pIndex + 1) % numP].mNext);
    for (u64 i = 0; i < clientVertexData.size(); ++i) clientVertexData(i) ^= serverVertexData(i); 

    u64 sent = 0, recv = 0;
    for (u64 i = 0; i < numP; ++i) {
        if (i != pIndex) {
            sent += clientComms[i].mPrev.getTotalDataSent();
            sent += clientComms[i].mNext.getTotalDataSent();
            recv += clientComms[i].mPrev.getTotalDataRecv();
            recv += clientComms[i].mNext.getTotalDataRecv();
            sent += serverComms[i].mNext.getTotalDataSent();
            sent += serverComms[i].mPrev.getTotalDataSent();
            recv += serverComms[i].mNext.getTotalDataRecv();
            recv += serverComms[i].mPrev.getTotalDataRecv();
            sent += helperComms[i].mNext.getTotalDataSent();
            sent += helperComms[i].mPrev.getTotalDataSent();
            recv += helperComms[i].mNext.getTotalDataRecv();
            recv += helperComms[i].mPrev.getTotalDataRecv();
        }
    }
    for (u64 i = 0; i < delClientChls.size(); ++i) {
        if (i != pIndex) {
            sent += delClientChls[i].getTotalDataSent();
            recv += delClientChls[i].getTotalDataRecv();
            sent += delServerChls[i].getTotalDataSent();
            recv += delServerChls[i].getTotalDataRecv();
        }
    }

    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
        << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

struct CommunicationStats {
    u64 sent;
    u64 recv;
};

CommunicationStats calculateCommunicationStats(
    const CommPkg (&computeComms)[3],  // Reference to array of 3 CommPkg objects
    const std::vector<Channel>& delClientChls,
    const std::vector<Channel>& delServerChls,
    u64 pIndex
) {
    CommunicationStats stats = {0, 0};
    
    // Process computeComms (size is known to be 3)
    for (u64 i = 0; i < 3; ++i) {
        stats.sent += computeComms[i].mPrev.getTotalDataSent();
        stats.recv += computeComms[i].mPrev.getTotalDataRecv();
        stats.sent += computeComms[i].mNext.getTotalDataSent();
        stats.recv += computeComms[i].mNext.getTotalDataRecv();
    }
    
    // Process delClientChls and delServerChls
    for (u64 i = 0; i < delClientChls.size(); ++i) {
        if (i != pIndex) {
            stats.sent += delClientChls[i].getTotalDataSent();
            stats.recv += delClientChls[i].getTotalDataRecv();
            stats.sent += delServerChls[i].getTotalDataSent();
            stats.recv += delServerChls[i].getTotalDataRecv();
        }
    }
    
    return stats;
}

void Sh3_Graph_Ours_App_single_party(    
    unsigned long long numP, 
    unsigned long long pIndex, 
    unsigned long long scale, 
    unsigned long long avgDegree,
    double interRatio,
    int algId, 
    unsigned long long numIters
) {
	Alg alg;
	if (algId == 0) {
		alg = Alg::CC;
	} else if (algId == 1) {
		alg = Alg::SP;
	} else if (algId == 2) {
		alg = Alg::PR;
	} else {
		printf("Unexpected alg!\n");
		exit(-1);
	}

    IOService ios(0);
    std::vector<Session> computeSessions;
    std::vector<Session> delegateClientSessions; 
    std::vector<Session> delegateServerSessions; 
    std::vector<Channel> computeChls;
    std::vector<Channel> delegateClientChls; 
    std::vector<Channel> delegateServerChls; 

    u64 serverDstIdx = (pIndex + numP - 1) % numP;
    u64 helperDstIdx = (pIndex + numP - 2) % numP;

    uint32_t baseComPort = 1712;
    uint32_t baseDelPort = 1912;
    std::string baseIP = "10.0.0.";
    // std::string baseIP = "127.0.0.1";

    // computeChls[pIdx][2], computeChls[pIdx][0]
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 2, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP - 1) % numP) + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP - 1) % numP) + 2, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * ((pIndex + numP + 2) % numP) + 0, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02")); 
    // ---->
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseComPort + 3 * pIndex + 2, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP - 1) % numP + 1), baseComPort + 3 * ((pIndex + numP - 1) % numP) + 2, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    computeSessions.emplace_back(Session(ios, baseIP + std::to_string((pIndex + numP + 2) % numP + 1), baseComPort + 3 * ((pIndex + numP + 2) % numP) + 0, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02")); 
    // <----
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 0, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * serverDstIdx + 0, SessionMode::Client, std::string("comp") + std::to_string(serverDstIdx) + "-01"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * pIndex + 1, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * helperDstIdx + 1, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-02"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * serverDstIdx + 2, SessionMode::Server, std::string("comp") + std::to_string(serverDstIdx) + "-12"));
    // computeSessions.emplace_back(Session(ios, baseIP, baseComPort + 3 * helperDstIdx + 2, SessionMode::Client, std::string("comp") + std::to_string(helperDstIdx) + "-12"));
    // for (auto chl : channels)
    //     if (chl)
    //         chl->close();

    // for (auto& cs : computeSessions)
    //     cs.stop();
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-01"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-02"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Server, std::string("comp") + std::to_string(pIndex) + "-12"));
    // computeSessions.emplace_back(Session(ios, "127.0.0.1", basePort + pIndex, SessionMode::Client, std::string("comp") + std::to_string(pIndex) + "-12")); 
    for (u64 j = 0; j < 6; ++j)
        computeChls.emplace_back(computeSessions[j].addChannel("c"));     

    // ---->
    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateClientSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateClientSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    }


    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
        else delegateClientChls.emplace_back(Channel());
    }    

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex == j) 
            delegateServerSessions.emplace_back(Session());
        else if (pIndex < j)
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(j + 1), baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
        else
            delegateServerSessions.emplace_back(Session(ios, baseIP + std::to_string(pIndex + 1), baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    }  

    for (u64 j = 0; j < numP; ++j) {
        if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
        else delegateServerChls.emplace_back(Channel());
    }  
    // <----  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateClientSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + pIndex, SessionMode::Client, std::string("deleClient") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateClientSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + j, SessionMode::Server, std::string("deleClient") + std::to_string(j) + std::to_string(pIndex)));
    // }


    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateClientChls.emplace_back(delegateClientSessions[j].addChannel("c"));
    //     else delegateClientChls.emplace_back(Channel());
    // }    

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex == j) 
    //         delegateServerSessions.emplace_back(Session());
    //     else if (pIndex < j)
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * j + numP + pIndex, SessionMode::Client, std::string("deleServer") + std::to_string(pIndex) + std::to_string(j)));
    //     else
    //         delegateServerSessions.emplace_back(Session(ios, baseIP, baseDelPort + 2 * numP * pIndex + numP + j, SessionMode::Server, std::string("deleServer") + std::to_string(j) + std::to_string(pIndex)));
    // }  

    // for (u64 j = 0; j < numP; ++j) {
    //     if (pIndex != j) delegateServerChls.emplace_back(delegateServerSessions[j].addChannel("c"));
    //     else delegateServerChls.emplace_back(Channel());
    // }    

    // Initialize graph data
    // Vertex tag \in {0, 1}
    // Edges (src, dst)
    u64 numVertexPerP = (1 << scale);
    int numEdgePerP = (1 << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j) {
            vertexIdLists[i][j] = i * numVertexPerP + j;
        }
    }
    vertexDataLists[0][0] = 1;
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(numP, std::vector<std::vector<std::array<u64, 2>>>(numP));
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                numEdgeMat[i][j] = numIntraEdgePerP;
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[i][0]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[i][k]});
                        if (++cnt > numIntraEdgePerP) break;
                    }
                    if (cnt > numIntraEdgePerP) break;
                }
                // for (u64 k = 0; k < numIntraEdgePerP; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[i][k]});
            } else {
                numEdgeMat[i][j] = numInterEdgePerPair;
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][k], vertexIdLists[j][0]});
                // for (u64 k = 0; k < numInterEdgePerPair; ++k) edgeLists[i][j].push_back({vertexIdLists[i][0], vertexIdLists[j][k]});
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[j][k]});
                        if (++cnt > numInterEdgePerPair) break;
                    }
                    if (cnt > numInterEdgePerPair) break;
                }
            }
        }
    }   

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    orCir_64->levelByAndDepth();   
    // auto orCir_8 = lib.int_int_bitwiseOr(8, 8, 8);
    // orCir_8->levelByAndDepth(); 

    CommPkg computeComms[3];
    computeComms[0] = { computeChls[5], computeChls[2] }; // Client Comms
    computeComms[1] = { computeChls[4], computeChls[1] }; // Server Comms
    computeComms[2] = { computeChls[3], computeChls[0] }; // Helper Comms       
    std::vector<Channel>& delClientChls = delegateClientChls;
    std::vector<Channel>& delServerChls = delegateServerChls;
    std::vector<i64Matrix> vertexDatas(3);
    vertexDatas[0].resize(numVertexList[pIndex], 1);
    vertexDatas[1].resize(numVertexList[serverDstIdx], 1);
    vertexDatas[2].resize(numVertexList[helperDstIdx], 1);
    for (u64 i = 0; i < 3; ++i) vertexDatas[i].setZero();
    for (u64 i = 0; i < numVertexList[pIndex]; ++i) {
        vertexDatas[0](i, 0) = vertexDataLists[pIndex][i];
    }
    std::vector<i64Matrix> interUpdateShare1(3);
    std::vector<i64Matrix> interUpdateShare2(3);

    // Load the data to aby3 plaintext data structure
    // For different characteristics: client, server, helper (maybe in different threads)
    auto scatterThread = [&](int role, i64Matrix& vertexDataShare, i64Matrix& updateShare1, i64Matrix& updateShare2) {
        std::vector<u64> srcTag;
        std::vector<u64> dstTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[clientPIdx][i];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        srcTag.resize(numVertex);
        dstTag.resize(numEdge);
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                srcTag[i] = vertexIdLists[clientPIdx][i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                    dstTag[cnt++] = edgeLists[clientPIdx][i][j][0];
                }
            }
        }
        our_scatter(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            srcTag, 
            dstTag, 
            vertexDataShare,
            updateShare,
            alg
        );      

        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[clientPIdx][i], 1);
            for (u64 j = 0; j < numEdgeMat[clientPIdx][i]; ++j) {
                updateShares[i](j, 0) = updateShare(cnt++, 0);
            }
        }
        if (role == 0) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("send delClient %d -> %d\n", pIndex, (i + 1) % numP);
                        // if (!delClientChls[(i + 1) % numP].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delClientChls %d %d %d\n", (i + 1) % numP, pIndex, role);
                        //     exit(-1);
                        // }
                        // delClientChls[(i + 1) % numP].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delClientChls[(i + 1) % numP]);
                    } else {
                        // Get the server share for P_{pIdx-1}
                        updateShare2 = updateShares[i];
                    }
                }
            }
        } else if (role == 1) {
            updateShare1 = updateShares[clientPIdx];
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // printf("send delServer %d -> %d\n", pIndex, i);
                        // if (!delServerChls[i].isConnected()) {
                        //     printf("Unexpected Unconnected Channel! delServerChls %d %d %d\n", i, pIndex, role);
                        //     exit(-1);
                        // }
                        // delServerChls[i].asyncSendCopy(updateShares[i].data(), updateShares[i].size());
                        asyncSendI64Mat(updateShares[i], delServerChls[i]);
                    } else {
                        // Get the client share for P_{pIdx}
                        updateShare2 = updateShares[i];
                    }
                }
            }                
        }
    };

    auto gatherThread = [&](int role, i64Matrix& vertexDataShare, const i64Matrix& updateShare1, const i64Matrix& updateShare2) {
        i64Matrix updatedVertexDataShare = vertexDataShare;
        updatedVertexDataShare.setZero();
        std::vector<u64> vertexTag;
        std::vector<u64> dstTag;
        i64Matrix updateShare;
        u64 numEdge = 0;
        int clientPIdx = 0;
        if (role == 0) clientPIdx = pIndex;
        else if (role == 1) clientPIdx = serverDstIdx;
        else if (role == 2) clientPIdx = helperDstIdx;
        u64 numVertex = numVertexList[clientPIdx];
        for (u64 i = 0; i < numP; ++i) numEdge += numEdgeMat[i][clientPIdx];
        if (role == 2) vertexDataShare.setZero();
        updateShare.resize(numEdge, 1);
        updateShare.setZero();
        vertexTag.resize(numVertex);
        dstTag.resize(numEdge);
        // Decompose update Share and send    
        std::vector<i64Matrix> updateShares(numP);
        for (u64 i = 0; i < numP; ++i) {
            updateShares[i].resize(numEdgeMat[i][clientPIdx], 1);
        }
        if (role == 0) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if ((i + 1) % numP != pIndex) {
                        // printf("recv delServer %d -> %d\n", (i + 1) % numP, pIndex);
                        // delServerChls[(i + 1) % numP].recv(updateShares[i].data(), updateShares[i].size());
                        recvI64Mat(updateShares[i], updateShares[i].size(), delServerChls[(i + 1) % numP]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }
        } else if (role == 1) {
            updateShares[clientPIdx] = updateShare1;
            for (int i = 0; i < numP; ++i) {
                if (i != clientPIdx) {
                    if (i != pIndex) {
                        // delClientChls[i].recv(updateShares[i].data(), updateShares[i].size());
                        // printf("recv delClient %d -> %d\n", i, pIndex);
                        recvI64Mat(updateShares[i], updateShares[i].size(), delClientChls[i]);
                    } else {
                        updateShares[i] = updateShare2;
                    }
                }
            }                
        } 
        u64 cnt = 0;
        for (u64 i = 0; i < numP; ++i) {
            for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                updateShare(cnt++, 0) = updateShares[i](j, 0);
            }
        }  
        if (role == 0) {
            for (u64 i = 0; i < numVertex; ++i) {
                vertexTag[i] = vertexIdLists[clientPIdx][i];
            }
            u64 cnt = 0;
            for (u64 i = 0; i < numP; ++i) {
                for (u64 j = 0; j < numEdgeMat[i][clientPIdx]; ++j) {
                    dstTag[cnt++] = edgeLists[i][clientPIdx][j][1];
                }
            }
        } 

        our_gather_dummied(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            dstTag, 
            vertexTag,
            updateShare,
            vertexDataShare,
            updatedVertexDataShare,
            alg
        );        
        vertexDataShare = updatedVertexDataShare;
    };

    auto t_invo = std::chrono::high_resolution_clock::now();

    for (u64 iter = 0; iter < numIters; ++iter) {
        auto t_tmp = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> scatterThrds; 
        for (u64 role = 0; role < 3; ++role) {
            scatterThrds.emplace_back(scatterThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : scatterThrds)
            thrd.join();

        print_duration(t_tmp, "Scatter");
        t_tmp = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> gatherThrds;
        for (u64 role = 0; role < 3; ++role) {
            if (role != 2) gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[1 - role]));
            else gatherThrds.emplace_back(gatherThread, role, std::ref(vertexDatas[role]), std::ref(interUpdateShare1[role]), std::ref(interUpdateShare2[role]));
        }
        for (auto& thrd : gatherThrds)
            thrd.join();

        print_duration(t_tmp, "Gather");            
    }

    print_duration(t_invo, "ProtocolInvocation");   

    CommunicationStats stats = calculateCommunicationStats(
        computeComms, delClientChls, delServerChls, pIndex
    );    
    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << stats.recv / 1024.0 / 1024.0 << "MB sent:" << stats.sent / 1024.0 / 1024.0 << "MB "
        << "prot-invoke: " << (stats.recv + stats.sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

    // Application-specific Result Extraction
    if (pIndex == 0 || pIndex == 1 || pIndex == 2) {
        auto t_ext = std::chrono::high_resolution_clock::now();
        u64 role = pIndex;
        u64 clientPIdx = 0;
        u64 numVertex = numVertexList[clientPIdx];
        std::vector<u64> srcTag(numVertex, 0);
        our_extract(
            computeComms[role].mPrev,
            computeComms[role].mNext,
            pIndex,
            role,
            srcTag, 
            vertexDatas[role],
            vertexDatas[role],
            alg            
        );
        print_duration(t_ext, "ResultExtract");
    }

    // computeComms[1].mPrev.asyncSendCopy(vertexDatas[1].data(), vertexDatas[1].size());
    asyncSendI64Mat(vertexDatas[1], computeComms[1].mPrev);
    i64Matrix serverVertexData(numVertexList[pIndex], 1);
    serverVertexData.setZero();
    // computeComms[0].mNext.recv(serverVertexData.data(), serverVertexData.size());
    recvI64Mat(serverVertexData, serverVertexData.size(), computeComms[0].mNext);
    for (u64 i = 0; i < vertexDatas[0].size(); ++i) vertexDatas[0](i) ^= serverVertexData(i); 

    stats = calculateCommunicationStats(
        computeComms, delClientChls, delServerChls, pIndex
    );

    std::cout << IoStream::lock;
    std::cout << "pIdx::" << pIndex << " " << std::endl;
    std::cout << "recv: " << stats.recv / 1024.0 / 1024.0 << "MB sent:" << stats.sent / 1024.0 / 1024.0 << "MB "
        << "total: " << (stats.recv + stats.sent) / 1024.0 / 1024.0 << "MB" << std::endl;
    std::cout << IoStream::unlock;

    // for (u64 j = 0; j < 6; ++j) {
    //     if (j != pIndex) {
    //         computeChls[j].close();
    //         computeSessions[j].stop();
    //     }
    // }
    // for (u64 j = 0; j < numP; ++j) {
    //     if (j != pIndex) {
    //         delegateClientChls[j].close();
    //         delegateClientSessions[j].stop();
    //     }
    //     if (j != pIndex) {
    //         delegateServerChls[j].close();
    //         delegateServerSessions[j].stop();
    //     }
    // }


    // if (failed)
    //     throw std::runtime_error(LOCATION);
}

void Sh3_Graph_reverse_shuffle_test() {

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    u64 wordSize = 1;
    u64 bitSize = wordSize << 6;

    u64 width = 1 << 3;
    std::atomic<bool> failed(false);
    //bool manual = false;

    Sh3BinaryEvaluator evals[3];

    i64Matrix value(width, wordSize);

    PRNG prng(ZeroBlock);
    for (u64 i = 0; i < width; ++i) {
        for (u64 j = 0; j < wordSize; ++j) value(i, j) = i;
    }

    std::vector<std::vector<u64>> perms(3);
    std::vector<std::vector<u64>> inv_perms(3);
    perms[0] = {1, 0, 2, 3, 4, 5, 6, 7};
    perms[1] = {0, 2, 1, 3, 4, 5, 6, 7};
    perms[2] = {0, 1, 3, 2, 4, 5, 6, 7};
    inv_perms[0] = perms[0];
    for (u64 i = 0; i < perms[0].size(); ++i) inv_perms[0][perms[0][i]] = i;
    inv_perms[1] = perms[1];
    for (u64 i = 0; i < perms[1].size(); ++i) inv_perms[1][perms[1][i]] = i;
    inv_perms[2] = perms[2];
    for (u64 i = 0; i < perms[2].size(); ++i) inv_perms[2][perms[2][i]] = i;

    auto routine = [&](int pIdx) {
        CommPkg& comm = comms[pIdx];
        // CommPkg invComm;
        // invComm.mNext = comm.mPrev;
        // invComm.mPrev = comm.mNext;
        int role = pIdx;
        Sh3Runtime rt(role, comm);
        Sh3Encryptor enc;
        enc.init(role, toBlock(role), toBlock((role + 1) % 3));
        
        sbMatrix input(width, bitSize), output(width, bitSize);
        i64Matrix plainOutput(width, wordSize);

        if (pIdx == 0) enc.localBinMatrix(rt.noDependencies(), value, input).get();
        else enc.remoteBinMatrix(rt.noDependencies(), input).get();

        shuffle(
            comm.mPrev,
            comm.mNext,
            role,
            perms[role],
            perms[(role + 1) % 3],
            input,
            output,
            enc 
        );

        input = output;

        reverse_shuffle(
            comm.mPrev,
            comm.mNext,
            role,
            inv_perms[role],
            inv_perms[(role + 1) % 3],
            input,
            output,
            enc
        );

        enc.revealAll(rt.noDependencies(), output, plainOutput).get();
        
        if (pIdx == 0) {
            for (u64 i = 0; i < width; ++i) {
                for (u64 j = 0; j < wordSize; ++j) {
                    oc::lout << u64(plainOutput(i, j)) << " ";
                    // if (gtAgg(slot, j) != agged(slot, j)) {
                    //     if (pIdx == 0) oc::lout << Color::Red << "pidx: " << pIdx << " failed at " << slot << " " << j << " "
                    //         << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;
                    //     failed = true;
                    // } else {
                    //     // if (pIdx == 0) oc::lout << Color::Green << "pidx: " << pIdx << " succeeded at " << slot << " " << j << " "
                    //     //     << std::setw(2) << i64(gtAgg(slot, j)) << " " << i64(agged(slot, j)) << std::endl << std::dec;                    
                    // }
                }
                oc::lout << std::endl;
            }
        }

        u64 sent = 0, recv = 0;
        sent += comms[pIdx].mPrev.getTotalDataSent();
        sent += comms[pIdx].mNext.getTotalDataSent();
        recv += comms[pIdx].mPrev.getTotalDataRecv();
        recv += comms[pIdx].mNext.getTotalDataRecv();

        std::cout << IoStream::lock;
        std::cout << "pIdx::" << pIdx << " " << std::endl;
        std::cout << "recv: " << recv / 1024.0 / 1024.0 << "MB sent:" << sent / 1024.0 / 1024.0 << "MB "
            << "total: " << (recv + sent) / 1024.0 / 1024.0 << "MB" << std::endl;
        std::cout << IoStream::unlock;

    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}