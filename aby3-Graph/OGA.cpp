#include "OGA.h"

#include <cassert>

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
    i64 size,
    std::vector<std::array<std::vector<u64>, 2>>& seqs
) {
    seqs.clear();
    i64 curSize = size;
    i64 step = 1;
    // printf("size %d\n", size);
    while (curSize > 1) {
        std::array<std::vector<u64>, 2> curSeq;
        for (i64 i = 0; i < size - step; i += 2 * step) {
            curSeq[0].push_back(i);
            curSeq[1].push_back(i + step);
            // printf("curseq %d %d\n", i, i + step);
        }
        // printf("curseq %d %d\n", i, i + step);
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
    eval.setCir(mergeCir, width, gen);
    eval.setInput(0, A);
    eval.setInput(1, B);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, D);
}

void evalMerge(
    const sbMatrix& A,
    const sbMatrix& B,
    sbMatrix& D,
    u64 width,
    u64 bitSize,
    BetaCircuit* mergeCir,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt
) {
    // Merge
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
        printf("Unequal sizes of srcIdx and dstIdx during sbMatrixExtractFill! %d %d %d %d\n", srcIdx.size(), dstIdx.size(), src.rows(), dst.rows());
        exit(-1);        
    }
    for (u64 i = 0; i < dstIdx.size(); ++i) {
        for (u64 j = 0; j < stride; ++j) {
            dst.mShares[0](dstIdx[i], j) = src.mShares[0](srcIdx[i], j);
            dst.mShares[1](dstIdx[i], j) = src.mShares[1](srcIdx[i], j);
        }
    }
}

void vecExtractFill(
    const std::vector<u64>& srcIdx,
    const std::vector<u64>& dstIdx,
    const std::vector<u64>& src,
    std::vector<u64>& dst
) {
    if (srcIdx.size() != dstIdx.size()) {
        printf("Unequal sizes of srcIdx and dstIdx during sbMatrixExtractFill!\n");
        exit(-1);        
    }
    for (u64 i = 0; i < dstIdx.size(); ++i) {
        dst[dstIdx[i]] = src[srcIdx[i]];
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
        // printf(">>%d %d %d %d\n", relaSeqs[r][0].size(), curRange.size(), sInput.rows(), curA.rows());
        // sbMatrixExtractFill(relaSeqs[r][0], curRange, sInput, curA);
        // sbMatrixExtractFill(relaSeqs[r][1], curRange, sInput, curB);
        sbMatrixExtractFill(seqs[r][0], curRange, sInput, curA);
        sbMatrixExtractFill(seqs[r][1], curRange, sInput, curB);
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
        // sInput = curD;
        // sbMatrixExtractFill(curRange, seqs[r][1], curB, sInput);
        sbMatrixExtractFill(curRange, seqs[r][0], curD, sInput);
    }
    // sOutput.mShares[0](0) = sInput.mShares[0](0);
    // sOutput.mShares[1](0) = sInput.mShares[1](0);
    sOutput = sInput;

    if (isRevealAll)
        task = enc.revealAll(task, sOutput, output);
    else
        task = enc.revealToTwoParty(task, sOutput, output);
    task.get();
}

// void run_ConditionalMerge(
//     Channel& prevChl,
//     Channel& nextChl,
//     int role,
//     const Matrix<u8>& a,
//     const Matrix<u8>& b,
//     const Matrix<u8>& c,
//     Matrix<u8>& d,
//     BetaCircuit* mergeCir,
//     bool isConditional,
//     bool isRevealAll
// ) {
//     u64 byteSize = a.cols();
//     u64 bitSize = byteSize * 8;
//     u64 width = a.rows();
//     if (width != b.rows()) {
//         printf("Unequal sizes of a and b during run_ConditionalMerge!\n");
//         exit(-1);
//     }

//     // Convert input to secret form
//     CommPkg comm = {prevChl, nextChl};
//     Sh3Runtime rt(role, comm);
//     Sh3Encryptor enc;
//     enc.init(role, toBlock(role), toBlock((role + 1) % 3));
//     Sh3BinaryEvaluator eval;    
//     eval.mPrng.SetSeed(toBlock(role));
//     Sh3ShareGen gen;
//     gen.init(toBlock(role), toBlock((role + 1) % 3));

//     sPackedBin A(width, bitSize);
//     sPackedBin B(width, bitSize);
//     sPackedBin D(width, bitSize);

//     auto task = rt.noDependencies();
    
//     if (role == 0 || role == 1) {
//     // if (role == 1) {
//         task = enc.localPackedBinary(task, a, A, true);
//         task = enc.localPackedBinary(task, b, B, true);
//     } else {
//         task = enc.remotePackedBinary(task, A);
//         task = enc.remotePackedBinary(task, B);
//     } 

//     task.get();

//     if (isConditional) {
//         sPackedBin C(width, 1);
//         if (role == 0) {
//             task = enc.localPackedBinary(task, c, 1, C);  
//         } else {
//             task = enc.remotePackedBinary(task, C);  
//         }   
//         task.get();

//         BetaCircuit cd;
//         get_multiplex_Circ(cd, bitSize);
//         BetaCircuit *multiplexCir = &cd;

//         evalConditionalMerge(
//             A,
//             B,
//             C,
//             D,
//             width,
//             bitSize,
//             mergeCir,
//             multiplexCir,
//             eval,
//             gen,
//             rt
//         );
//     } else {
//         evalMerge(
//             A,
//             B,
//             D,
//             width,
//             bitSize,
//             mergeCir,
//             eval,
//             gen,
//             rt
//         );
//     }

//     if (isRevealAll)
//         task = enc.revealAll(task, D, d);
//     else
//         task = enc.revealToTwoParty(task, D, d);
//     task.get();
// }

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
    u64 wordSize = (bitSize + 63) / 64;
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

    i64Matrix aa;
    i64Matrix bb;
    i64Matrix dd;
    byteMat2intMat(a, aa);
    byteMat2intMat(b, bb);
    dd.resize(a.rows(), wordSize);

    sbMatrix A(width, bitSize);
    sbMatrix B(width, bitSize);
    sbMatrix D(width, bitSize);

    auto task = rt.noDependencies();
    
    if (role == 0 || role == 1) {
    // if (role == 1) {
        task = enc.localBinMatrix(task, aa, A);
        task = enc.localBinMatrix(task, bb, B);
    } else {
        task = enc.remoteBinMatrix(task, A);
        task = enc.remoteBinMatrix(task, B);
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
        task = enc.revealAll(task, D, dd);
    else
        task = enc.revealToTwoParty(task, D, dd);
    task.get();

    intMat2ByteMat(dd, d, 1);
}

void run_ConditionalMerge(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    const i64Matrix& a,
    const i64Matrix& b,
    const Matrix<u8>& c,
    i64Matrix& d,
    BetaCircuit* mergeCir,
    bool isConditional,
    bool isRevealAll
) {
    u64 wordSize = a.cols();
    u64 byteSize = wordSize * 8;
    u64 bitSize = wordSize * 64;
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

    d.resize(a.rows(), wordSize);

    sbMatrix A(width, bitSize);
    sbMatrix B(width, bitSize);
    sbMatrix D(width, bitSize);

    auto task = rt.noDependencies();
    
    if (role == 0 || role == 1) {
    // if (role == 1) {
        task = enc.localBinMatrix(task, a, A);
        task = enc.localBinMatrix(task, b, B);
    } else {
        task = enc.remoteBinMatrix(task, A);
        task = enc.remoteBinMatrix(task, B);
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

void get_merge_sequence(
    const std::vector<u64>& index_vec, 
    std::vector<std::vector<u64>>& sequence
) {
    u64 length = index_vec.size();
    assert(length > 0);

    if (length == 1) {
        return;
    }

    if (length % 2 == 0) {
        sequence.push_back(index_vec);
    } else {
        sequence.push_back(std::vector<u64>(index_vec.begin(), index_vec.end() - 1));
    }

    u64 cur_length = (length + 1) / 2;
    std::vector<u64> cur_index_vec(cur_length);
    for (u64 i = 0; i < cur_length; i++) {
        cur_index_vec[i] = index_vec[i * 2];
    }

    get_merge_sequence(cur_index_vec, sequence);

    if (length <= 2) {
        return;
    }

    if (length % 2 == 0) {
        sequence.push_back(std::vector<u64>(index_vec.begin() + 1, index_vec.end() - 1));
    } else {
        sequence.push_back(std::vector<u64>(index_vec.begin() + 1, index_vec.end()));
    }
}

sPackedBin get_pair_indicator(
    const sbMatrix& group_id_lhs,
    const sbMatrix& group_id_rhs,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id_lhs.rows();
    assert(length > 0);
    assert(group_id_rhs.rows() == length);
    u64 bitSize = group_id_lhs.bitCount();
    if (bitSize != 64) {
        printf("Unexpected bitSize during get_pair_indicator!\n");
        exit(-1);
    }

    sPackedBin eq_result(length, 1);

    BetaLibrary lib;
    BetaCircuit *eqCir =  lib.int_eq(bitSize);

    eval.setCir(eqCir, length, gen);
    eval.setInput(0, group_id_lhs);
    eval.setInput(1, group_id_rhs);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, eq_result);

    return eq_result;
}

sPackedBin get_pair_indicator(
    const std::vector<u64>& group_id_lhs,
    const std::vector<u64>& group_id_rhs,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id_lhs.size();
    assert(length > 0);
    assert(group_id_rhs.size() == length);

    Matrix<u8> plainEq(length, 1);
    for (u64 i = 0; i < length; ++i) plainEq(i, 0) = group_id_lhs[i] == group_id_rhs[i]? 1 : 0;

    sPackedBin eq_result(length, 1);
    if (rt.mPartyIdx == 0) {
        enc.localPackedBinary(rt.noDependencies(), plainEq, 1, eq_result).get();
    } else {
        enc.remotePackedBinary(rt.noDependencies(), eq_result).get();
    }

    return eq_result;
}

sbMatrix conditional_merge(
    const sPackedBin& indicator,
    const sbMatrix& lhs,
    const sbMatrix& rhs,
    AggregationOp agg_op,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = indicator.shareCount();
    assert(length > 0);
    assert(length == lhs.rows() && length == rhs.rows());
    u64 bitSize = lhs.bitCount();

    BetaLibrary lib;
    BetaCircuit *ltCir =  lib.int_int_lt(64, 64);
    BetaCircuit *orCir =  lib.int_int_bitwiseOr(64, 64, 64);
    BetaCircuit *addCir =  lib.int_int_add(64, 64, 64);
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(bitSize);

    sbMatrix agg_result(length, bitSize);
    switch (agg_op) {
        case AggregationOp::NONE_AGG:
            throw std::runtime_error("Unsupported Aggregation Op");
        case AggregationOp::MIN_AGG: {
                sPackedBin C(length, 1);
                eval.setCir(ltCir, length, gen);
                eval.setInput(0, lhs);
                eval.setInput(1, rhs);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, C);

                // Multiplex
                eval.setCir(multiplexCir, length, gen);
                eval.setInput(0, lhs);
                eval.setInput(1, rhs);
                eval.setInput(2, C);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, agg_result);
                break;
            }
        case AggregationOp::OR_AGG: {
                eval.setCir(orCir, length, gen);
                eval.setInput(0, lhs);
                eval.setInput(1, rhs);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, agg_result);
                break;
            }
        case AggregationOp::ADD_AGG: {
                eval.setCir(addCir, length, gen);
                eval.setInput(0, lhs);
                eval.setInput(1, rhs);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, agg_result);
                break;
            }
        default:
            throw std::runtime_error("Unsupported Aggregation Op");
            break;
    }

    sbMatrix cond_result(length, bitSize);
    eval.setCir(multiplexCir, length, gen);
    eval.setInput(0, agg_result);
    eval.setInput(1, lhs);
    eval.setInput(2, indicator);
    eval.asyncEvaluate(rt.noDependencies()).get();
    eval.getOutput(0, cond_result); 

    return cond_result;
}

sbMatrix prefix_network_aggregate(
    const sbMatrix& group_id,
    const sbMatrix& value,
    AggregationOp agg_op,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id.rows();
    assert(length == value.rows());
    assert(length > 0);
    u64 group_id_bitSize = 64;
    u64 value_bitSize = value.bitCount();

    sbMatrix ret_value = value;

    if (length == 1) {
        return ret_value;
    }

    std::vector<u64> index_vec(length);
    std::iota(index_vec.begin(), index_vec.end(), 0); // fill with 0, 1, ..., length - 1
    std::vector<std::vector<u64>> sequence;
    get_merge_sequence(index_vec, sequence);
    
    sPackedBin indicator(length, 1);

    u64 layer_num = sequence.size();

    for (u64 i = 0; i < layer_num; i++) {
        const std::vector<u64>& cur_sequence = sequence[i];
        u64 cur_length = cur_sequence.size();
        u64 pair_num = cur_length / 2;
        sbMatrix group_id_lhs(pair_num, group_id_bitSize);
        sbMatrix group_id_rhs(pair_num, group_id_bitSize);
        sbMatrix lhs(pair_num, value_bitSize);
        sbMatrix rhs(pair_num, value_bitSize);

        std::vector<u64> lhs_src;
        std::vector<u64> rhs_src;
        std::vector<u64> dst;
        for (u64 j = 0; j < pair_num; j++) {
            dst.push_back(j);
            lhs_src.push_back(cur_sequence[j * 2]);
            rhs_src.push_back(cur_sequence[j * 2 + 1]);           
        }
        sbMatrixExtractFill(lhs_src, dst, group_id, group_id_lhs);
        sbMatrixExtractFill(lhs_src, dst, ret_value, lhs);
        sbMatrixExtractFill(rhs_src, dst, group_id, group_id_rhs);
        sbMatrixExtractFill(rhs_src, dst, ret_value, rhs);

        sPackedBin cur_indicator = get_pair_indicator(group_id_lhs, group_id_rhs, eval, gen, rt, enc);

        // if (i == 0) {
        //     for (u64 j = 0; j < pair_num; j++) {
        //         indicator[j * 2] = cur_indicator[j];
        //     }
        // } else if (i == layer_num - 1) {
        //     for (u64 j = 0; j < pair_num; j++) {
        //         indicator[j * 2 + 1] = cur_indicator[j];
        //     }
        // }

        sbMatrix cur_merge_result = conditional_merge(cur_indicator, lhs, rhs, agg_op, eval, gen, rt, enc);

        sbMatrixExtractFill(dst, lhs_src, cur_merge_result, ret_value);
    }

    // // Set non-start-of-group to zero
    // std::vector<u8> filter_indicator = std::vector<u8>(indicator.begin(), indicator.end() - 1);
    // std::vector<u8> zero_filter_indicator = std::vector<u8>(filter_indicator.size(), 0);
    // std::vector<std::vector<u64>> ret_value_tail(ret_value.begin() + 1, ret_value.end());
    // std::vector<std::vector<u64>> zero_vec(length - 1, std::vector<u64>(ret_value[0].size(), 0));
    // std::vector<std::vector<u64>> filter_result;
    // if (party == sci::ALICE) sci::twoPartyMux2(ret_value_tail, zero_vec, filter_indicator, filter_result, coTid, party, is_group_id_one_side);
    // else sci::twoPartyMux2(ret_value_tail, zero_vec, zero_filter_indicator, filter_result, coTid, party, is_group_id_one_side);
    // for (u64 i = 0; i < length - 1; i++) {
    //     ret_value[i + 1] = filter_result[i];
    // }

    return ret_value;    
}


sbMatrix prefix_network_aggregate(
    const std::vector<u64>& group_id,
    const sbMatrix& value,
    AggregationOp agg_op,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id.size();
    assert(length == value.rows());
    assert(length > 0);
    u64 group_id_bitSize = 64;
    u64 value_bitSize = value.bitCount();

    sbMatrix ret_value = value;

    if (length == 1) {
        return ret_value;
    }

    std::vector<u64> index_vec(length);
    std::iota(index_vec.begin(), index_vec.end(), 0); // fill with 0, 1, ..., length - 1
    std::vector<std::vector<u64>> sequence;
    get_merge_sequence(index_vec, sequence);
    
    sPackedBin indicator(length, 1);

    u64 layer_num = sequence.size();

    for (u64 i = 0; i < layer_num; i++) {
        const std::vector<u64>& cur_sequence = sequence[i];
        u64 cur_length = cur_sequence.size();
        u64 pair_num = cur_length / 2;
        std::vector<u64> group_id_lhs(pair_num, 0);
        std::vector<u64> group_id_rhs(pair_num, 0);
        sbMatrix lhs(pair_num, value_bitSize);
        sbMatrix rhs(pair_num, value_bitSize);

        std::vector<u64> lhs_src;
        std::vector<u64> rhs_src;
        std::vector<u64> dst;
        for (u64 j = 0; j < pair_num; j++) {
            dst.push_back(j);
            lhs_src.push_back(cur_sequence[j * 2]);
            rhs_src.push_back(cur_sequence[j * 2 + 1]);           
        }
        vecExtractFill(lhs_src, dst, group_id, group_id_lhs);
        sbMatrixExtractFill(lhs_src, dst, ret_value, lhs);
        vecExtractFill(rhs_src, dst, group_id, group_id_rhs);
        sbMatrixExtractFill(rhs_src, dst, ret_value, rhs);

        sPackedBin cur_indicator = get_pair_indicator(group_id_lhs, group_id_rhs, eval, gen, rt, enc);

        sbMatrix cur_merge_result = conditional_merge(cur_indicator, lhs, rhs, agg_op, eval, gen, rt, enc);

        sbMatrixExtractFill(dst, lhs_src, cur_merge_result, ret_value);
    }

    return ret_value;    
}

i64Matrix prefix_network_aggregate(
    const std::vector<u64>& group_id,
    const i64Matrix& value,
    AggregationOp agg_op,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id.size();
    assert(length == value.rows());
    assert(length > 0);
    u64 group_id_bitSize = 64;
    u64 value_bitSize = value.cols() * 64;

    sbMatrix ret_value(length, value_bitSize);

    auto task = rt.noDependencies();    
    int role = rt.mPartyIdx;
    if (role == 0 || role == 1) {
        task = enc.localBinMatrix(task, value, ret_value);
    } else {
        task = enc.remoteBinMatrix(task, ret_value);
    }     
    task.get();

    if (length == 1) {
        return value;
    }

    std::vector<u64> index_vec(length);
    std::iota(index_vec.begin(), index_vec.end(), 0); // fill with 0, 1, ..., length - 1
    std::vector<std::vector<u64>> sequence;
    get_merge_sequence(index_vec, sequence);
    
    sPackedBin indicator(length, 1);

    u64 layer_num = sequence.size();

    for (u64 i = 0; i < layer_num; i++) {
        const std::vector<u64>& cur_sequence = sequence[i];
        u64 cur_length = cur_sequence.size();
        u64 pair_num = cur_length / 2;
        std::vector<u64> group_id_lhs(pair_num, 0);
        std::vector<u64> group_id_rhs(pair_num, 0);
        sbMatrix lhs(pair_num, value_bitSize);
        sbMatrix rhs(pair_num, value_bitSize);

        std::vector<u64> lhs_src;
        std::vector<u64> rhs_src;
        std::vector<u64> dst;
        for (u64 j = 0; j < pair_num; j++) {
            dst.push_back(j);
            lhs_src.push_back(cur_sequence[j * 2]);
            rhs_src.push_back(cur_sequence[j * 2 + 1]);           
        }
        vecExtractFill(lhs_src, dst, group_id, group_id_lhs);
        sbMatrixExtractFill(lhs_src, dst, ret_value, lhs);
        vecExtractFill(rhs_src, dst, group_id, group_id_rhs);
        sbMatrixExtractFill(rhs_src, dst, ret_value, rhs);

        sPackedBin cur_indicator = get_pair_indicator(group_id_lhs, group_id_rhs, eval, gen, rt, enc);

        sbMatrix cur_merge_result = conditional_merge(cur_indicator, lhs, rhs, agg_op, eval, gen, rt, enc);

        sbMatrixExtractFill(dst, lhs_src, cur_merge_result, ret_value);
    }

    i64Matrix ret_plain_value(value.rows(), value.cols());
    enc.revealToTwoParty(rt.noDependencies(), ret_value, ret_plain_value).get();

    return ret_plain_value;    
}

sbMatrix prefix_network_propagate(
    const sbMatrix& group_id,
    const sbMatrix& value,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id.rows();
    assert(length == value.rows());
    assert(length > 0);
    u64 group_id_bitSize = 64;
    u64 value_bitSize = value.bitCount();

    sbMatrix ret_value = value;

    if (length == 1) {
        return ret_value;
    }

    // print_decoded_vector(group_id, 10);
    // print_decoded_vector(value, 10);

    std::vector<u64> index_vec(length);
    std::iota(index_vec.begin(), index_vec.end(), 0); // fill with 0, 1, ..., length - 1
    // print_vector(index_vec, 10);
    std::vector<std::vector<u64>> sequence;
    get_merge_sequence(index_vec, sequence);

    u64 layer_num = sequence.size();

    BetaLibrary lib;
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(value_bitSize);

    for (int i = layer_num - 1; i >= 0; i--) {
        const std::vector<u64>& cur_sequence = sequence[i];
        // printf("%ld\n", i);
        // print_vector(cur_sequence, cur_sequence.size());
        u64 cur_length = cur_sequence.size();
        u64 pair_num = cur_length / 2;
        sbMatrix group_id_lhs(pair_num, group_id_bitSize);
        sbMatrix group_id_rhs(pair_num, group_id_bitSize);
        sbMatrix lhs(pair_num, value_bitSize);
        sbMatrix rhs(pair_num, value_bitSize);

        std::vector<u64> lhs_src;
        std::vector<u64> rhs_src;
        std::vector<u64> dst;
        for (u64 j = 0; j < pair_num; j++) {
            dst.push_back(j);
            lhs_src.push_back(cur_sequence[j * 2]);
            rhs_src.push_back(cur_sequence[j * 2 + 1]);           
        }
        sbMatrixExtractFill(lhs_src, dst, group_id, group_id_lhs);
        sbMatrixExtractFill(lhs_src, dst, ret_value, lhs);
        sbMatrixExtractFill(rhs_src, dst, group_id, group_id_rhs);
        sbMatrixExtractFill(rhs_src, dst, ret_value, rhs);

        sPackedBin cur_indicator = get_pair_indicator(group_id_lhs, group_id_rhs, eval, gen, rt, enc);

        eval.setCir(multiplexCir, pair_num, gen);
        eval.setInput(0, lhs);
        eval.setInput(1, rhs);
        eval.setInput(2, cur_indicator);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, rhs);

        sbMatrixExtractFill(dst, rhs_src, rhs, ret_value);
    }

    // print_decoded_vector(ret_value, 10);

    return ret_value;
}

sbMatrix prefix_network_propagate(
    const std::vector<u64>& group_id,
    const sbMatrix& value,
    Sh3BinaryEvaluator& eval,
    Sh3ShareGen& gen,
    Sh3Runtime& rt,
    Sh3Encryptor& enc
) {
    u64 length = group_id.size();
    assert(length == value.rows());
    assert(length > 0);
    u64 group_id_bitSize = 64;
    u64 value_bitSize = value.bitCount();

    sbMatrix ret_value = value;

    if (length == 1) {
        return ret_value;
    }

    // print_decoded_vector(group_id, 10);
    // print_decoded_vector(value, 10);

    std::vector<u64> index_vec(length);
    std::iota(index_vec.begin(), index_vec.end(), 0); // fill with 0, 1, ..., length - 1
    // print_vector(index_vec, 10);
    std::vector<std::vector<u64>> sequence;
    get_merge_sequence(index_vec, sequence);

    u64 layer_num = sequence.size();

    BetaLibrary lib;
    BetaCircuit *multiplexCir =  lib.int_int_multiplex(value_bitSize);

    for (int i = layer_num - 1; i >= 0; i--) {
        const std::vector<u64>& cur_sequence = sequence[i];
        // printf("%ld\n", i);
        // print_vector(cur_sequence, cur_sequence.size());
        u64 cur_length = cur_sequence.size();
        u64 pair_num = cur_length / 2;
        std::vector<u64> group_id_lhs(pair_num, 0);
        std::vector<u64> group_id_rhs(pair_num, 0);
        sbMatrix lhs(pair_num, value_bitSize);
        sbMatrix rhs(pair_num, value_bitSize);

        std::vector<u64> lhs_src;
        std::vector<u64> rhs_src;
        std::vector<u64> dst;
        for (u64 j = 0; j < pair_num; j++) {
            dst.push_back(j);
            lhs_src.push_back(cur_sequence[j * 2]);
            rhs_src.push_back(cur_sequence[j * 2 + 1]);           
        }
        vecExtractFill(lhs_src, dst, group_id, group_id_lhs);
        sbMatrixExtractFill(lhs_src, dst, ret_value, lhs);
        vecExtractFill(rhs_src, dst, group_id, group_id_rhs);
        sbMatrixExtractFill(rhs_src, dst, ret_value, rhs);

        sPackedBin cur_indicator = get_pair_indicator(group_id_lhs, group_id_rhs, eval, gen, rt, enc);

        eval.setCir(multiplexCir, pair_num, gen);
        eval.setInput(0, lhs);
        eval.setInput(1, rhs);
        eval.setInput(2, cur_indicator);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, rhs);

        sbMatrixExtractFill(dst, rhs_src, rhs, ret_value);
    }

    // print_decoded_vector(ret_value, 10);

    return ret_value;
}

