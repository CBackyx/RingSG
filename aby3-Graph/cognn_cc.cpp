#include "cognn_cc.h"
#include <algorithm>
#include <cryptoTools/Circuit/BetaLibrary.h>

#include "utils.h"

using namespace oc;
using namespace aby3;

void cognn_scatter(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    bool isLocal,
    const std::vector<aby3::u64>& srcVertexTag, 
    const std::vector<aby3::u64>& edgeSrcTag, 
    const std::vector<aby3::u64>& edgeDstTag,
    const std::vector<aby3::u64>& dstVertexTag,
    const Matrix<u8>& inputShare,
    Matrix<u8>& outputShare
) {
    Matrix<u8> srcVertexShare(edgeSrcTag.size(), inputShare.cols());
    run_OEP(
        prevChl,
        nextChl,
        role,
        srcVertexTag, 
        edgeSrcTag, 
        inputShare,
        srcVertexShare        
    ); 

    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(role, comm);
    Sh3Encryptor enc;
    enc.init(role, toBlock(role), toBlock((role + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(role));
    Sh3ShareGen gen;
    gen.init(toBlock(role), toBlock((role + 1) % 3));

    // Merge
    i64Matrix srcVertexShare_int(srcVertexShare.rows(), (srcVertexShare.cols() + 7) / 8);
    byteMat2intMat(srcVertexShare, srcVertexShare_int);    
    i64Matrix aggedSrcVertexShare_int = prefix_network_aggregate(
        edgeDstTag,
        srcVertexShare_int,
        AggregationOp::OR_AGG,
        eval,
        gen,
        rt,
        enc
    );
    Matrix<u8> aggedSrcVertexShare(edgeDstTag.size(), inputShare.cols());
    intMat2ByteMat(
        aggedSrcVertexShare_int,
        aggedSrcVertexShare,
        1
    );

    // Map
    if (!isLocal) {
        if (role == 0 || role == 1) {
            run_OEP(
                nextChl,
                prevChl,
                1 - role,
                edgeDstTag, 
                dstVertexTag, 
                aggedSrcVertexShare,
                outputShare        
            ); 
        } else {
            run_OEP(
                nextChl,
                prevChl,
                role,
                edgeDstTag, 
                dstVertexTag, 
                aggedSrcVertexShare,
                outputShare        
            ); 
        }
    } else {
        run_OEP(
            prevChl,
            nextChl,
            role,
            edgeDstTag, 
            dstVertexTag, 
            aggedSrcVertexShare,
            outputShare        
        );         
    }
}

void cognn_gather(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<Matrix<u8>>& updateShares,
    const Matrix<u8>& vertexShare,
    Matrix<u8>& outputShare
) {
    u64 numVertex = vertexShare.rows();
    u64 numP = updateShares.size();
    std::vector<i64Matrix> updateShares_int(numP);
    i64Matrix vertexShare_int(numVertex, (vertexShare.cols() + 7) / 8);
    for (u64 i = 0; i < numP; ++i) {
        updateShares_int[i].resize(numVertex, (vertexShare.cols() + 7) / 8);
        byteMat2intMat(updateShares[i], updateShares_int[i]);
    }
    byteMat2intMat(vertexShare, vertexShare_int);

    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(role, comm);
    Sh3Encryptor enc;
    enc.init(role, toBlock(role), toBlock((role + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(role));
    Sh3ShareGen gen;
    gen.init(toBlock(role), toBlock((role + 1) % 3));

    auto task = rt.noDependencies();
    std::vector<sbMatrix> enc_updateShares(numP);
    for (u64 i = 0; i < numP; ++i) {
        enc_updateShares[i].resize(numVertex, updateShares_int[i].cols() * 64);
        if (role == 0 || role == 1) {
            task = enc.localBinMatrix(task, updateShares_int[i], enc_updateShares[i]);
        } else {
            task = enc.remoteBinMatrix(task, enc_updateShares[i]);
        }     
    }
    sbMatrix enc_outputShare(numVertex, updateShares_int[0].cols() * 64);
    if (role == 0 || role == 1) {
        task = enc.localBinMatrix(task, vertexShare_int, enc_outputShare);
    } else {
        task = enc.remoteBinMatrix(task, enc_outputShare);
    }        
    task.get();

    BetaLibrary lib;
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);

    eval.setCir(orCir_64, numVertex, gen);
    for (u64 i = 0; i < numP; ++i) {
        eval.setInput(0, enc_outputShare);
        eval.setInput(1, enc_updateShares[i]);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, enc_outputShare);
    }
    i64Matrix outputShare_int(numVertex, (vertexShare.cols() + 7) / 8);
    enc.revealToTwoParty(rt.noDependencies(), enc_outputShare, outputShare_int).get();    

    outputShare.resize(numVertex, vertexShare.cols());
    intMat2ByteMat(
        outputShare_int,
        outputShare,
        1
    );
}