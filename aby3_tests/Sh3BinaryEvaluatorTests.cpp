#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include <random>
#include "cryptoTools/Crypto/PRNG.h"

#include <cryptoTools/Circuit/BetaLibrary.h>
#include <iomanip>

using namespace oc;
using namespace aby3;

//
//void createBinarySharing(
//    PRNG& prng,
//    Eigen::Matrix<i64, Eigen::Dynamic, Eigen::Dynamic>& value,
//    Lynx::Engine::Matrix& s0,
//    Lynx::Engine::Matrix& s1,
//    Lynx::Engine::Matrix& s2)
//{
//    s0.mShares[0] = value;
//
//    s1.mShares[0].resizeLike(value);
//    s2.mShares[0].resizeLike(value);
//
//    //if (zeroshare) {
//    //	s1.mShares[0].setZero();
//    //	s2.mShares[0].setZero();
//    //}
//    //else 
//    {
//        prng.get(s1.mShares[0].data(), s1.mShares[0].size());
//        prng.get(s2.mShares[0].data(), s2.mShares[0].size());
//
//        for (u64 i = 0; i < s0.mShares[0].size(); ++i)
//            s0.mShares[0](i) ^= s1.mShares[0](i) ^ s2.mShares[0](i);
//    }
//
//    s0.mShares[1] = s2.mShares[0];
//    s1.mShares[1] = s0.mShares[0];
//    s2.mShares[1] = s1.mShares[0];
//}

//Lynx::Matrix toLynx(const sbMatrix& A)
//{
//    Lynx::Matrix r(A.rows(), A.i64Cols());
//    r.mShares[0] = A.mShares[0];
//    r.mShares[1] = A.mShares[1];
//    return r;
//}
//
//void compare(Sh3BinaryEvaluator& a, Lynx::BinaryEngine& b)
//{
//    if (a.mMem.bitCount() != b.mMem.bitCount())
//        throw std::runtime_error(LOCATION);
//
//    if (a.mMem.shareCount() != b.mMem.shareCount())
//        throw std::runtime_error(LOCATION);
//
//
//    if (neq(a.hashState(), b.hashState()))
//        ostreamLock(std::cout)
//            << "Ha " << a.hashState() << std::endl
//            << "Hb " << b.hashState() << std::endl;
//
//    for (u64 s = 0; s < 2; ++s)
//    {
//
//        for (u64 i = 0; i < a.mMem.bitCount(); ++i)
//        {
//            for (u64 j = 0; j < a.mMem.simdWidth(); ++j)
//            {
//                if (neq(a.mMem.mShares[s](i, j),b.mMem.mShares[s](i, j)))
//                {
//                    ostreamLock(std::cout) << " share " << s << " bit " << i << " word " << j << std::endl;
//                }
//            }
//        }
//    }
//
//    if (a.mPlainWires_DEBUG.size() != b.mPlainWires_DEBUG.size())
//        throw std::runtime_error(LOCATION);
//
//    for (u64 s = 0; s < 3; ++s)
//    {
//        for (u64 i = 0; i < a.mPlainWires_DEBUG.size(); ++i)
//        {
//
//            if (a.mPlainWires_DEBUG[i].size() != b.mPlainWires_DEBUG[i].size())
//                throw std::runtime_error(LOCATION);
//
//            for (u64 j = 0; j < a.mPlainWires_DEBUG[i].size(); ++j)
//            {
//                if (a.mPlainWires_DEBUG[i][j].mBits[s] !=
//                    b.mPlainWires_DEBUG[i][j].mBits[s])
//                {
//                    ostreamLock(std::cout) << "*share " << s << " bit " << i << " word " << j << std::endl;
//                }
//            }
//        }
//    }
//}

std::array<oc::Matrix<i64>, 3> getShares(sbMatrix& S, Sh3Runtime& rt, CommPkg& comm)
{
    std::array<oc::Matrix<i64>, 3> r;

    auto& r0 = r[rt.mPartyIdx];
    auto& r1 = r[(rt.mPartyIdx + 1) % 3];
    auto& r2 = r[(rt.mPartyIdx + 2) % 3];

    r0 = S.mShares[0];
    r1 = S.mShares[1];

    comm.mNext.asyncSend(r0.data(), r0.size());
    comm.mPrev.asyncSend(r1.data(), r1.size());

    r2.resize(S.rows(), S.i64Cols());

    comm.mNext.recv(r2.data(), r2.size());
    oc::Matrix<i64> check(r1.rows(), r1.cols());
    comm.mPrev.recv(check.data(), check.size());

    if (memcmp(check.data(), r1.data(), check.size() * sizeof(i64)) != 0)
        throw RTE_LOC;

    return r;

}

void get_multiplex_Circ(
    BetaCircuit& cd,
    u64 elementSize
) {
    BetaLibrary lib;

    BetaBundle a(elementSize);
    BetaBundle b(elementSize);
    BetaBundle c(1);
    BetaBundle d(elementSize);
    BetaBundle temp(elementSize);

    cd.addInputBundle(a);
    cd.addInputBundle(b);
    cd.addInputBundle(c);
    cd.addOutputBundle(d);
    cd.addTempWireBundle(temp);

    lib.multiplex_build(
		cd,
		a,
		b,
		c,
		d,
		temp
    );
}

void getOGAMergeSequences(
    u64 size,
    std::vector<std::array<std::vector<u64>, 2>>& seqs
) {
    seqs.clear();
    size_t curSize = size;
    u64 step = 1;
    while (curSize > 1) {
        std::array<std::vector<u64>, 2> curSeq;
        for (u64 i = 0; i < size - step; i += 2 * step) {
            curSeq[0].push_back(i);
            curSeq[1].push_back(i + step);
        }
        seqs.push_back(curSeq);
        step *= 2;
        curSize = size / 2;
    }
}

void getOGAMergeRelativeSequences(
    u64 size,
    std::vector<std::array<std::vector<u64>, 2>>& seqs
) {
    seqs.clear();
    size_t curSize = size;
    u64 step = 1;
    while (curSize > 1) {
        std::array<std::vector<u64>, 2> curSeq;
        for (u64 i = 0; i < curSize - 1; i += 2) {
            curSeq[0].push_back(i);
            curSeq[1].push_back(i + 1);
        }
        seqs.push_back(curSeq);
        curSize /= 2;
    }
}

void getMergeIndicators(
    const std::vector<u64>& groupId,
    const std::vector<std::array<std::vector<u64>, 2>>& seqs,
    std::vector<std::vector<u8>>& indicators
) {
    u64 numSeq = seqs.size();
    indicators.clear();
    for (u64 i = 0; i < numSeq; ++i) {
        std::vector<u8> curInd(seqs[i][0].size(), 0);
        for (u64 j = 0; j < curInd.size(); ++j) {
            if (groupId[seqs[i][0][j]] == groupId[seqs[i][1][j]]) curInd[j] = 1;
        }
        indicators.push_back(curInd);
    }
}

void get_OGA_Circ(
    BetaCircuit& cd,
    u64 num,
    u64 size
) {
    BetaLibrary lib;
    // Get OGA sequence first
    std::vector<std::array<std::vector<u64>, 2>> seqs;
    std::vector<std::array<std::vector<u64>, 2>> relaSeqs;
    getOGAMergeSequences(num, seqs);
    getOGAMergeSequences(num, relaSeqs);
    u64 rounds = seqs.size();

    std::vector<BetaBundle> input(num);
    std::vector<BetaBundle> output(num);
    std::vector<std::vector<BetaBundle>> inds(rounds);
    std::vector<std::vector<BetaBundle>> mergeTemps(rounds);
    std::vector<std::vector<BetaBundle>> muxTemps(rounds);
    std::vector<std::vector<BetaBundle>> temps(rounds);
    for (u64 i = 0; i < num; ++i) {
        input[i].mWires.resize(size);
        // inds[i].mWires.resize(1);
        cd.addInputBundle(input[i]);
        cd.addOutputBundle(output[i]);
    }
    for (u64 i = 0; i < rounds; ++i) {
        inds[i].resize(seqs[i][0].size());
        mergeTemps[i].resize(seqs[i][0].size());
        muxTemps[i].resize(seqs[i][0].size());
        temps[i].resize(seqs[i][0].size());
        for (u64 j = 0; j < inds[i].size(); ++j) {
            inds[i][j].mWires.resize(1);
            mergeTemps[i][j].mWires.resize(size);
            muxTemps[i][j].mWires.resize(size);
            temps[i][j].mWires.resize(size);
            cd.addInputBundle(inds[i][j]);
            cd.addTempWireBundle(mergeTemps[i][j]);
            cd.addTempWireBundle(muxTemps[i][j]);
            cd.addTempWireBundle(temps[i][j]);
        }
    } 
    
    for (u64 i = 0; i < rounds; ++i) {
        for (int j = 0; j < relaSeqs[i][0].size(); ++j) {
            if (i == 0) {
                lib.bitwiseOr_build(cd, input[relaSeqs[i][0][j]], input[relaSeqs[i][1][j]], mergeTemps[i][j]);
                lib.multiplex_build(cd, mergeTemps[i][j], input[relaSeqs[i][0][j]], inds[i][j], muxTemps[i][j], temps[i][j]);
                cd.addCopy(input[relaSeqs[i][1][j]], output[seqs[i][1][j]]);
            } else {
                lib.bitwiseOr_build(cd, muxTemps[i-1][relaSeqs[i][0][j]], muxTemps[i-1][relaSeqs[i][1][j]], mergeTemps[i][j]);
                lib.multiplex_build(cd, mergeTemps[i][j], muxTemps[i-1][relaSeqs[i][0][j]], inds[i][j], muxTemps[i][j], temps[i][j]); 
                cd.addCopy(muxTemps[i-1][relaSeqs[i][1][j]], output[seqs[i][1][j]]);               
            }
        }
    }
}

// void evalConditionalMerge(
//     const sPackedBin& A,
//     const sPackedBin& B,
//     const sPackedBin& C,
//     sPackedBin& D
//     u64 width,
//     u64 elementSize,
//     BetaCircuit* mergeCir,
//     BetaCircuit* multiplexCir,
//     Sh3BinaryEvaluator& eval
//     Sh3ShareGen& gen
// ) {
//     // Merge
//     sPackedBin merged(width, bitSize);
//     eval.setCir(mergeCir, width, gen);
//     eval.setInput(0, A);
//     eval.setInput(1, B);
//     eval.asyncEvaluate(rt.noDependencies()).get();
//     eval.getOutput(0, merged);

//     // Multiplex
//     eval.setCir(multiplexCir, width, gen);
//     eval.setInput(0, merged);
//     eval.setInput(1, A);
//     eval.setInput(2, C);
//     eval.asyncEvaluate(rt.noDependencies()).get();
//     eval.getOutput(0, D);
// }

void run_OGA(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    std::vector<u64> groupId, 
    oc::Matrix<u8> input,
    oc::Matrix<u8>& output,
    BetaCircuit* mergeCir
) {
    u64 size = groupId.size();
    if (size != input.rows()) {
        printf("Unequal sizes of groupId and input!\n");
        exit(-1);
    }

    // Get OGA sequence first
    std::vector<std::array<std::vector<u64>, 2>> seqs;
    getOGAMergeSequences(size, seqs);
    std::vector<std::vector<u8>> inds;
    getMergeIndicators(groupId, seqs, inds);
    u64 rounds = inds.size();

    // Convert input to secret form
    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(pIdx, comm);
    Sh3Encryptor enc;
    enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(pIdx));
    Sh3ShareGen gen;
    gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

    u64 byteSize = input.cols();
    u64 bitSize = byteSize << 3;
    u64 width = size;
    std::vector<Matrix<u8>> plainInds(rounds);
    for (u64 i = 0; i < rounds; ++i) {
        plainInds[i].resize(inds[i].size(), 1);
        memcpy(plainInds[i].data(), inds[i].data(), inds[i].size());
        // for (u64 j = 0; j < inds[i].size(); ++j) {
        //     plainInds[i](j, 0) = inds[i][j];
        // }
    }
    sPackedBin sInput(width, bitSize);
    std::vector<sPackedBin> sInds(rounds);
    for (u64 i = 0; i < rounds; ++i) {
        sInds[i].reset(inds[i].size(), 1);
    }
    
    if (pIdx == 0 || pIdx == 1) {
        enc.localPackedBinary(rt.noDependencies(), input, sInput, true).get();
    } else {
        enc.remotePackedBinary(rt.noDependencies(), sInput).get();
    } 

    if (pIdx == 0) {
        for (u64 i = 0; i < rounds; ++i) {
            enc.localPackedBinary(rt.noDependencies(), plainInds[i], 1, sInds[i]).get();  
        } 
    } else {
        for (u64 i = 0; i < rounds; ++i) {
            enc.remotePackedBinary(rt.noDependencies(), sInds[i]).get();  
        } 
    }   

    // Write a conditional merge function
    BetaCircuit cd;
    get_multiplex_Circ(cd, bitSize);
    BetaCircuit *multiplexCir = &cd;
    mergeCir->levelByAndDepth();
    multiplexCir->levelByAndDepth();

    // Execute the conditional merge function for each party of the sequence

    //int_int_bitwiseAnd_build(*cd, a, b, c);    

}

void Sh3_BinaryEngine_test(
    BetaCircuit* cir,
    std::function<i64(i64, i64)> binOp,
    bool debug,
    std::string opName,
    u64 valMask = ~0ull)
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

    debugComm[0] = { comms[0].mPrev.getSession().addChannel(), comms[0].mNext.getSession().addChannel() };
    debugComm[1] = { comms[1].mPrev.getSession().addChannel(), comms[1].mNext.getSession().addChannel() };
    debugComm[2] = { comms[2].mPrev.getSession().addChannel(), comms[2].mNext.getSession().addChannel() };

    cir->levelByAndDepth();
    u64 width = 1 << 24;
    bool failed = false;
    //bool manual = false;

    enum Mode { Manual, Auto, Replicated };

    std::array < std::vector<oc::Matrix<i64>>, 3> CC;
    std::array < std::vector<oc::Matrix<i64>>, 3> CC2;
    std::array<std::atomic<int>, 3> ac;
    Sh3BinaryEvaluator evals[3];

    //block tag = oc::sysRandomSeed();

    ac[0] = 0;
    ac[1] = 0;
    ac[2] = 0;
    auto aSize = cir->mInputs[0].size();
    auto bSize = cir->mInputs[1].size();
    auto cSize = cir->mOutputs[0].size();

    auto routine = [&](int pIdx) {
        //auto i = 0;

        i64Matrix a(width, 1), b(width, 1), c(width, 1), ar(1, 1);
        i64Matrix aa(width, 1), bb(width, 1);

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < (u64)a.size(); ++i)
        {
            a(i) = prng.get<i64>() & valMask;
            b(i) = prng.get<i64>() & valMask;
        }
        ar(0) = prng.get<i64 >() & valMask;

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sbMatrix A(width, aSize), B(width, bSize), C(width, cSize);
        sbMatrix Ar(1, aSize);

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));

        auto task = rt.noDependencies();

        enc.localBinMatrix(rt.noDependencies(), a, A).get();
        enc.localBinMatrix(rt.noDependencies(), ar, Ar).get();
        enc.localBinMatrix(rt.noDependencies(), b, B).get();

        auto& eval = evals[pIdx];

        eval.mPrng.SetSeed(toBlock(pIdx));

#ifdef BINARY_ENGINE_DEBUG
        if (debug)
            eval.enableDebug(pIdx, 0, debugComm[pIdx].mPrev, debugComm[pIdx].mNext);
#endif

        // for (auto mode : { Manual, Auto, Replicated })
        for (auto mode : { Manual })
        {
            //eval.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));
            //if (pIdx == 0)
            //    oc::lout << "---------------------------------------" << std::endl;

            Sh3ShareGen gen;
            gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

            C.mShares[0](0) = 0;
            C.mShares[1](0) = 0;
            switch (mode)
            {
            case Manual:
                task.get();
                eval.setCir(cir, width, gen);
                eval.setInput(0, A);
                eval.setInput(1, B);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, C);
                break;
            case Auto:
                task = eval.asyncEvaluate(task, cir, gen, { &A, &B }, { &C });
                break;
            case Replicated:
                for (u64 i = 0; i < width; ++i)
                    a(i) = ar(0);

                task.get();
                eval.setCir(cir, width, gen);
                eval.setReplicatedInput(0, Ar);
                eval.setInput(1, B);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, C);
                break;

            }
            task.get();

            auto ccc0 = C.mShares[0](0);
            CC[pIdx].push_back(C.mShares[0]);
            CC2[pIdx].push_back(C.mShares[1]);
            ++ac[pIdx];
            auto ccc1 = C.mShares[0](0);
            auto ccc2 = CC[pIdx].back()(0);

            if (pIdx)
            {

                enc.reveal(task, 0, C).get();

                //auto CC = getShares(C, rt, debugComm[pIdx]);

            }
            else
            {
                enc.reveal(task, C, c).get();

                auto ci = CC[pIdx].size() - 1;;


                for (u64 j = 0; j < width; ++j)
                {
                    if (c(j) != binOp(a(j), b(j)))
                    {
                        oc::lout << "pidx: " << rt.mPartyIdx << " mode: " << (int)mode << " debug:" << int(debug) << " failed at " << j << " " 
                            << std::hex << c(j) << " != " << std::hex << binOp(a(j), b(j))
                            << " = " << opName << "(" << std::hex << a(j) << ", " << std::hex << b(j) << ")" << std::endl << std::dec;
                        failed = true;
                    }
                }

                if (a(0) == 0x66 && failed)
                {
                    while (ac[1] < ac[0]);
                    while (ac[2] < ac[0]);
                    printf("Here1\n");
                    auto& oo = oc::lout;
                    oo << "pidx: " << rt.mPartyIdx << " check\n";
                    oo << "      a " << A.mShares[0](0) << " " << A.mShares[1](0) << std::endl;
                    oo << "      b " << B.mShares[0](0) << " " << B.mShares[1](0) << std::endl;
                    oo << "      c " << C.mShares[0](0) << " " << C.mShares[1](0) << std::endl;
                    oo << "      C " << CC[0][ci](0) << " " << CC[1][ci](0) << " " << CC[2][ci](0) << std::endl;
                    oo << "      D " << CC2[0][ci](0) << " " << CC2[1][ci](0) << " " << CC2[2][ci](0) << std::endl;
                    oo << "        " << ccc0 << " " << ccc1 << " " << ccc2 << std::endl;
                    oo << "   recv " << eval.mRecvFutr.size() << std::endl;

                    //oo << "   " << eval.mLog.str() << std::endl;

                    //oo << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
                    //oo << evals[1].mLog.str() << std::endl;
                    //oo << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
                    //oo << evals[2].mLog.str() << std::endl;
                }
            }

            //oc::lout << oc::Color::Green << eval.mLog.str() << oc::Color::Default << std::endl;
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

void Sh3_BinaryEngine_and_test()
{
    printf("Here\n");

    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    // and
    {
        auto cir = lib.int_int_bitwiseAnd(size, size, size);
        cir->levelByAndDepth();

        // Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; }, true, "AND", mask);
        Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; }, false, "AND", mask);
    }



    // // na_and
    // {
    //     BetaCircuit cd;

    //     BetaBundle a(size);
    //     BetaBundle b(size);
    //     BetaBundle c(size);

    //     cd.addInputBundle(a);
    //     cd.addInputBundle(b);
    //     cd.addOutputBundle(c);

    //     //int_int_bitwiseAnd_build(*cd, a, b, c);
    //     for (u64 j = 0; j < c.mWires.size(); ++j)
    //     {
    //         cd.addGate(
    //             a.mWires[j],
    //             b.mWires[j],
    //             GateType::na_And,
    //             c.mWires[j]);
    //     }


    //     Sh3_BinaryEngine_test(&cd, [](i64 a, i64 b) {
    //         return ~a & b;
    //         }, false, "na_AND", mask);

    // }


    // // copy
    // {
    //     BetaCircuit cir;

    //     BetaBundle a(size);
    //     BetaBundle b(size);
    //     BetaBundle c(size);

    //     cir.addInputBundle(a);
    //     cir.addInputBundle(b);
    //     cir.addOutputBundle(c);
    //     cir.addCopy(a, c);

    //     Sh3_BinaryEngine_test(&cir, [](i64 a, i64 b) {return a; }, true, "copy", mask);
    //     Sh3_BinaryEngine_test(&cir, [](i64 a, i64 b) {return a; }, false, "copy", mask);
    // }


}


void Sh3_BinaryEngine_multiplex_test()
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

        auto task = rt.noDependencies();

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
        task.get();
        eval.setCir(cir, width, gen);
        eval.setInput(0, A);
        eval.setInput(1, B);
        eval.setInput(2, C);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, D);
        
        // task = eval.asyncEvaluate(task, cir, gen, { &A, &B, &C }, { &D });

        task.get();

        // printf("++ %lu %lu\n", d.rows(), d.cols());
        enc.revealAll(task, D, d).get();

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


void Sh3_BinaryEngine_add_test()
{

    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    auto cir = lib.int_int_add(size, size, size, BetaLibrary::Optimized::Depth);

    Sh3_BinaryEngine_test(cir, [mask](i64 a, i64 b) {return (a + b) & mask; }, true, "Plus", mask);
    Sh3_BinaryEngine_test(cir, [mask](i64 a, i64 b) {return (a + b) & mask; }, false, "Plus", mask);

}


void Sh3_BinaryEngine_add_msb_test()
{
    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    auto cir = lib.int_int_add_msb(size);

    Sh3_BinaryEngine_test(cir, [size](i64 a, i64 b) {return ((a + b) >> (size - 1)) & 1; }, true, "msb", mask);
    Sh3_BinaryEngine_test(cir, [size](i64 a, i64 b) {return ((a + b) >> (size - 1)) & 1; }, false, "msb", mask);
}
