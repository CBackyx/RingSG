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

#include "aby3-Graph/OGA.h"

using namespace oc;
using namespace aby3;

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