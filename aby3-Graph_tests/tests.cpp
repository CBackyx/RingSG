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
    u64 numP = 5;
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
            scatter(
                computeComms[role].mPrev,
                computeComms[role].mNext,
                pIdx,
                role,
                srcTag, 
                dstTag, 
                vertexDataShare,
                updateShare
            );      

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

            gather(
                computeComms[role].mPrev,
                computeComms[role].mNext,
                pIdx,
                role,
                dstTag, 
                vertexTag,
                updateShare,
                vertexDataShare,
                updatedVertexDataShare
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
    u64 numP = 5;
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
    u64 numVertexPerP = (1 << 20);
    u64 numIntraEdgePerP = (1 << 20);
    u64 numInterEdgePerPair = (1 << 20);
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
                updateShare
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

            our_gather(
                computeComms[role].mPrev,
                computeComms[role].mNext,
                pIdx,
                role,
                dstTag, 
                vertexTag,
                updateShare,
                vertexDataShare,
                updatedVertexDataShare
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
        num_iters: 5
    };

    u64 numP = 5;
    u64 numVertexPerP = (1 << 10);
    u64 numIntraEdgePerP = (1 << 10);
    u64 numInterEdgePerPair = (1 << 10);
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

void Sh3_Graph_CoGNN_test()
{
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
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-01"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-01"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-02"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-02"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Server, std::string("comp") + std::to_string(i) + "-12"));
            computeSessions[curIndex].emplace_back(Session(ios, "127.0.0.1", SessionMode::Client, std::string("comp") + std::to_string(i) + "-12")); 
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
        Matrix<u8> clientVertexData(numVertexList[pIdx], 1);
        std::vector<Matrix<u8>> serverVertexDatas(numP);
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
        std::vector<Matrix<u8>> clientInterUpdateShare(numP);
        std::vector<Matrix<u8>> serverInterUpdateShare(numP);

        // Load the data to aby3 plaintext data structure
        // For different characteristics: client, server, helper (maybe in different threads)
        auto scatterThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, Matrix<u8>& updateShare1, int iter, Channel prevChl, Channel nextChl) {
            std::vector<u64> srcVertexTag;
            std::vector<u64> edgeSrcTag;
            std::vector<u64> edgeDstTag;
            std::vector<u64> dstVertexTag;
            Matrix<u8> updateShare;
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
                updateShare
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

        auto gatherThread = [&](int role, int clientPIdx, int serverPIdx, int dstPIdx, Matrix<u8>& vertexDataShare, const std::vector<Matrix<u8>>& updateShares, int iter, Channel prevChl, Channel nextChl) {
            u64 numVertex = numVertexList[dstPIdx];
            if (role == 2) {
                vertexDataShare.resize(numVertex, 1);
                vertexDataShare.setZero(); 
            }            
            
            Matrix<u8> updatedVertexDataShare = vertexDataShare;
            updatedVertexDataShare.setZero();
            std::vector<Matrix<u8>> orderedUpdateShares(numP);
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
                updatedVertexDataShare
            );        
            vertexDataShare = updatedVertexDataShare;

            if (role == 1 && iter != numIters - 1) {
                for (int i = 0; i < numP; ++i) {
                    if (i != ((clientPIdx + 1) % numP) && i != clientPIdx) delServerChls[i].asyncSend(vertexDataShare.data(), vertexDataShare.size());
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
            Matrix<u8> helperShare[2];
            scatterHelperThrds.emplace_back(scatterThread, 2, (pIdx - 2 + numP) % numP, (pIdx - 1 + numP) % numP, (pIdx - 2 + numP) % numP, std::ref(helperShare[0]), std::ref(helperShare[1]), iter, helperComms[(pIdx - 2 + numP) % numP].mPrev, helperComms[(pIdx - 2 + numP) % numP].mNext);
            scatterClientThrds[0].join();
            scatterServerThrds[0].join();
            scatterHelperThrds[0].join();
            std::vector<std::array<Matrix<u8>, 2>> helperShares(numP);
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
                    std::array<Matrix<u8>, 2>& helperShare = helperShares[i];
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

            std::vector<Matrix<u8>> clientUpdateShare = serverInterUpdateShare;
            clientUpdateShare[pIdx] = clientInterUpdateShare[pIdx];
            std::vector<Matrix<u8>> serverUpdateShare(2);
            serverUpdateShare[0] = clientInterUpdateShare[(pIdx - 1 + numP) % numP];
            serverUpdateShare[1] = serverInterUpdateShare[pIdx];

            std::vector<Matrix<u8>> helperShareVec;
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
        Matrix<u8> serverVertexData(numVertexList[pIdx], 1);
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