#include "OGA.h"

using namespace oc;
using namespace aby3;

template <typename T>
void print_vector(const std::vector<T>& v, u64 num = 0) {
  u64 cnt = 0;
  for (auto elem : v) {
    if (num != 0 && cnt >= num) break;
    oc::lout << elem << " ";
    cnt++;
  }
  oc::lout << "\n";
}

void getOGAMergeSequences(
    u64 size,
    std::vector<std::array<std::vector<u64>, 2>>& seqs
) {
    seqs.clear();
    u64 curSize = size;
    u64 step = 1;
    while (curSize > 1) {
        std::array<std::vector<u64>, 2> curSeq;
        for (u64 i = 0; i < size - step; i += 2 * step) {
            curSeq[0].push_back(i);
            curSeq[1].push_back(i + step);
        }
        seqs.push_back(curSeq);
        step *= 2;
        curSize /= 2;
    }
}

void getOGAMergeRelativeSequences(
    u64 size,
    std::vector<std::array<std::vector<u64>, 2>>& seqs
) {
    seqs.clear();
    u64 curSize = size;
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
    u64 size,
    int role
) {
    BetaLibrary lib;
    // Get OGA sequence first
    std::vector<std::array<std::vector<u64>, 2>> seqs;
    std::vector<std::array<std::vector<u64>, 2>> relaSeqs;
    getOGAMergeSequences(num, seqs);
    getOGAMergeRelativeSequences(num, relaSeqs);
    u64 rounds = seqs.size();

    std::vector<BetaBundle> inputs(num);
    std::vector<BetaBundle> outputs(num);
    std::vector<std::vector<BetaBundle>> inds(rounds);
    std::vector<std::vector<BetaBundle>> mergeTemps(rounds);
    std::vector<std::vector<BetaBundle>> muxTemps(rounds);
    std::vector<std::vector<BetaBundle>> temps(rounds);

    // BetaBundle t1(size), t2(size), t3(size), t4(size), t5(size);
    // cd.addInputBundle(t1);
    // cd.addInputBundle(t2);
    // cd.addInputBundle(t3);
    // cd.addInputBundle(t4);
    // cd.addInputBundle(t5);
    for (u64 i = 0; i < num; ++i) {
        inputs[i].mWires.resize(size);
        cd.addInputBundle(inputs[i]);
    }

    for (u64 i = 0; i < rounds; ++i) {
        // if (role == 0) {
        //     print_vector(seqs[i][0]);
        //     print_vector(seqs[i][1]);
        //     print_vector(relaSeqs[i][0]);
        //     print_vector(relaSeqs[i][1]);
        // }

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
    
    for (u64 i = 0; i < num; ++i) {
        outputs[i].mWires.resize(size);
        cd.addOutputBundle(outputs[i]);
    }    

    for (u64 i = 0; i < num; ++i) {
        cd.addCopy(inputs[i], outputs[i]);
    }    

    // for (u64 i = 0; i < rounds; ++i) {
    //     for (int j = 0; j < relaSeqs[i][0].size(); ++j) {
    //         if (i == 0) {
    //             lib.bitwiseOr_build(cd, inputs[relaSeqs[i][0][j]], inputs[relaSeqs[i][1][j]], mergeTemps[i][j]);
    //             lib.multiplex_build(cd, mergeTemps[i][j], inputs[relaSeqs[i][0][j]], inds[i][j], muxTemps[i][j], temps[i][j]);
    //             cd.addCopy(inputs[relaSeqs[i][1][j]], outputs[seqs[i][1][j]]);
    //         } else {
    //             lib.bitwiseOr_build(cd, muxTemps[i-1][relaSeqs[i][0][j]], muxTemps[i-1][relaSeqs[i][1][j]], mergeTemps[i][j]);
    //             lib.multiplex_build(cd, mergeTemps[i][j], muxTemps[i-1][relaSeqs[i][0][j]], inds[i][j], muxTemps[i][j], temps[i][j]); 
    //             cd.addCopy(muxTemps[i-1][relaSeqs[i][1][j]], outputs[seqs[i][1][j]]);               
    //         }
    //     }
    // }
}

void evalConditionalMerge(
    const sbMatrix& A,
    const sbMatrix& B,
    const sPackedBin& C,
    sbMatrix& D,
    u64 width,
    u64 bitSize,
    BetaCircuit* mergeCir,
    BetaCircuit* multiplexCir,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt
) {
    // Merge
    sbMatrix merged(width, bitSize);
    eval.setCir(mergeCir, width, gen);
    eval.setInput(0, A);
    eval.setInput(1, B);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, merged);

    // Multiplex
    eval.setCir(multiplexCir, width, gen);
    eval.setInput(0, merged);
    eval.setInput(1, A);
    eval.setInput(2, C);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, D);
}

void evalConditionalMerge(
    const sPackedBin& A,
    const sPackedBin& B,
    const sPackedBin& C,
    sPackedBin& D,
    u64 width,
    u64 bitSize,
    BetaCircuit* mergeCir,
    BetaCircuit* multiplexCir,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt
) {
    // Merge
    sPackedBin merged(width, bitSize);
    eval.setCir(mergeCir, width, gen);
    eval.setInput(0, A);
    eval.setInput(1, B);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, merged);

    // Multiplex
    eval.setCir(multiplexCir, width, gen);
    eval.setInput(0, merged);
    eval.setInput(1, A);
    eval.setInput(2, C);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, D);
}

void evalMerge(
    const sPackedBin& A,
    const sPackedBin& B,
    sPackedBin& D,
    u64 width,
    u64 bitSize,
    BetaCircuit* mergeCir,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt
) {
    // Merge
    sPackedBin merged(width, bitSize);
    eval.setCir(mergeCir, width, gen);
    eval.setInput(0, A);
    eval.setInput(1, B);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, D);
}

// We need a function to convert Matrix<u8> to i64Matrix (and vice versa) here.

void byteMat2intMat(
    const Matrix<u8>& input,
    i64Matrix& output
) {
    u64 rows = input.rows();
    u64 byteSize = input.cols();
    u64 wordSize = (byteSize + 7) / 8;
    u64 bitSize = byteSize << 3;
    
    output.resize(rows, wordSize);
    oc::MatrixView<u8> out((u8*)output.data(), rows, wordSize * 8);
    for (u64 i = 0; i < rows; ++i) {
        for (u64 j = 0; j < byteSize; ++j) out(i, j) = input(i, j);
        for (u64 j = byteSize; j < wordSize * 8; ++j) out(i, j) = 0;
    }
}

void intMat2ByteMat(
    const i64Matrix& input,
    Matrix<u8>& output,
    u64 byteSize
) {
    u64 wordSize = input.cols();
    if (wordSize != (byteSize + 7) / 8) {
        printf("Unexpected input byteSize during intMat2ByteMat conversion!\n");
        exit(-1);
    }
    u64 rows = input.rows();
    output.resize(rows, byteSize);
    oc::MatrixView<u8> in((u8*)input.data(), rows, wordSize * 8);
    for (u64 i = 0; i < rows; ++i) {
        for (u64 j = 0; j < byteSize; ++j) output(i, j) = in(i, j);
    }    
}

void sbMatrixExtractFill(
    const std::vector<u64>& srcIdx,
    const std::vector<u64>& dstIdx,
    const sbMatrix& src,
    sbMatrix& dst
) {
    u64 stride = src.mShares[0].cols();
    if (stride != dst.mShares[0].cols()) {
        printf("Unequal src stride and dst stride during sbMatrixExtractFill!\n");
        exit(-1);
    }
    if (srcIdx.size() != dstIdx.size()) {
        printf("Unequal sizes of srcIdx and dstIdx during sbMatrixExtractFill!\n");
        exit(-1);        
    }
    for (u64 i = 0; i < dstIdx.size(); ++i) {
        for (u64 j = 0; j < stride; ++j) {
            dst.mShares[0](dstIdx[i], j) = src.mShares[0](srcIdx[i], j);
            dst.mShares[1](dstIdx[i], j) = src.mShares[1](srcIdx[i], j);
        }
    }
}

void run_OGA(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    std::vector<u64> groupId, 
    i64Matrix input,
    i64Matrix& output,
    BetaCircuit* mergeCir,
    bool isRevealAll
) {
    u64 wordSize = input.cols();
    u64 bitSize = wordSize * 64;
    u64 width = groupId.size();
    if (width != input.rows()) {
        printf("Unequal sizes of groupId and input!\n");
        exit(-1);
    }

    // Get OGA sequence first
    std::vector<std::array<std::vector<u64>, 2>> seqs;
    getOGAMergeSequences(width, seqs);
    std::vector<std::array<std::vector<u64>, 2>> relaSeqs;
    getOGAMergeRelativeSequences(width, relaSeqs);
    std::vector<std::vector<u8>> inds;
    getMergeIndicators(groupId, seqs, inds);
    u64 rounds = inds.size();

    // Convert input to secret form
    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(role, comm);
    Sh3Encryptor enc;
    enc.init(role, toBlock(role), toBlock((role + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(role));
    Sh3ShareGen gen;
    gen.init(toBlock(role), toBlock((role + 1) % 3));

    sbMatrix sInput(width, bitSize);
    sbMatrix sOutput(width, bitSize);
    std::vector<sPackedBin> sInds(rounds);
    std::vector<Matrix<u8>> plainInds(rounds); 
    for (u64 i = 0; i < rounds; ++i) {
        sInds[i].reset(inds[i].size(), 1);
        plainInds[i].resize(inds[i].size(), 1);
        for (u64 j = 0; j < inds[i].size(); ++j) {
            plainInds[i](j, 0) = inds[i][j];
        }
    }

    auto task = rt.noDependencies();
    
    // oc::lout << "here " << role << " H1.-1" <<  std::endl;
    
    if (role == 0 || role == 1) {
    // if (role == 1) {
        task = enc.localBinMatrix(task, input, sInput);
    } else {
        task = enc.remoteBinMatrix(task, sInput);
    } 

    if (role == 0) {
        for (u64 i = 0; i < rounds; ++i) {
            task = enc.localPackedBinary(task, plainInds[i], 1, sInds[i]);  
        } 
    } else {
        for (u64 i = 0; i < rounds; ++i) {
            task = enc.remotePackedBinary(task, sInds[i]);  
        } 
    }   
    task.get();

    // oc::lout << "here " << role << " H1.1" <<  std::endl;
    BetaCircuit cd;
    get_multiplex_Circ(cd, bitSize);
    BetaCircuit *multiplexCir = &cd;
    // mergeCir->levelByAndDepth();
    multiplexCir->levelByAndDepth();

    for (u64 r = 0; r < rounds; ++r) {
        u64 curWidth = inds[r].size();
        std::vector<u64> curRange(curWidth, 0);
        for (u64 i = 0; i < curWidth; ++i) curRange[i] = i;
        sbMatrix curA(curWidth, bitSize);
        sbMatrix curB(curWidth, bitSize);
        sbMatrix curD(curWidth, bitSize);
        sbMatrixExtractFill(relaSeqs[r][0], curRange, sInput, curA);
        sbMatrixExtractFill(relaSeqs[r][1], curRange, sInput, curB);
        evalConditionalMerge(
            curA,
            curB,
            sInds[r],
            curD,
            curWidth,
            bitSize,
            mergeCir,
            multiplexCir,
            eval,
            gen,
            rt
        );
        sInput = curD;
        sbMatrixExtractFill(curRange, seqs[r][1], curB, sOutput);
    }
    sOutput.mShares[0](0) = sInput.mShares[0](0);
    sOutput.mShares[1](0) = sInput.mShares[1](0);

    if (isRevealAll)
        task = enc.revealAll(task, sOutput, output);
    else
        task = enc.revealToTwoParty(task, sOutput, output);
    task.get();
}

void run_ConditionalMerge(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    const Matrix<u8>& a,
    const Matrix<u8>& b,
    const Matrix<u8>& c,
    Matrix<u8>& d,
    BetaCircuit* mergeCir,
    bool isConditional,
    bool isRevealAll
) {
    u64 byteSize = a.cols();
    u64 bitSize = byteSize * 8;
    u64 width = a.rows();
    if (width != b.rows()) {
        printf("Unequal sizes of a and b during run_ConditionalMerge!\n");
        exit(-1);
    }

    // Convert input to secret form
    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(role, comm);
    Sh3Encryptor enc;
    enc.init(role, toBlock(role), toBlock((role + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(role));
    Sh3ShareGen gen;
    gen.init(toBlock(role), toBlock((role + 1) % 3));

    sPackedBin A(width, bitSize);
    sPackedBin B(width, bitSize);
    sPackedBin D(width, bitSize);

    auto task = rt.noDependencies();
    
    if (role == 0 || role == 1) {
    // if (role == 1) {
        task = enc.localPackedBinary(task, a, A, true);
        task = enc.localPackedBinary(task, b, B, true);
    } else {
        task = enc.remotePackedBinary(task, A);
        task = enc.remotePackedBinary(task, B);
    } 

    task.get();

    if (isConditional) {
        sPackedBin C(width, 1);
        if (role == 0) {
            task = enc.localPackedBinary(task, c, 1, C);  
        } else {
            task = enc.remotePackedBinary(task, C);  
        }   
        task.get();

        BetaCircuit cd;
        get_multiplex_Circ(cd, bitSize);
        BetaCircuit *multiplexCir = &cd;

        evalConditionalMerge(
            A,
            B,
            C,
            D,
            width,
            bitSize,
            mergeCir,
            multiplexCir,
            eval,
            gen,
            rt
        );
    } else {
        evalMerge(
            A,
            B,
            D,
            width,
            bitSize,
            mergeCir,
            eval,
            gen,
            rt
        );
    }

    if (isRevealAll)
        task = enc.revealAll(task, D, d);
    else
        task = enc.revealToTwoParty(task, D, d);
    task.get();
}

