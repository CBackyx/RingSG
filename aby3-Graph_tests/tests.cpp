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
#include "aby3-Graph/shuffle.h"

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

    u64 width = 1 << 3;
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