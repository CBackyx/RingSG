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
    const i64Matrix& inputShare,
    i64Matrix& outputShare,
    Alg alg
) {
    AggregationOp aop;
    if (alg == Alg::CC) {
        aop = AggregationOp::OR_AGG;
    } else if (alg == Alg::SP) {
        aop = AggregationOp::MIN_AGG;
    } else if (alg == Alg::PR) {
        aop = AggregationOp::ADD_AGG;
    } else {
        printf("Unexpected Scatter Op in CoGNN!\n");
        exit(-1);
    }
    u64 byteSize = inputShare.cols() * 8;
    i64Matrix srcVertexShare_int(edgeSrcTag.size(), inputShare.cols());

    BetaLibrary lib;
    auto multCir_64 = lib.uint_uint_mult(64, 64, 64);
    auto orCir_64 = lib.int_int_bitwiseOr(64, 64, 64);
    auto addCir_64 = lib.int_int_add(64, 64, 64);

    i64Matrix inputShare_preScatter(inputShare.rows(), inputShare.cols());
    i64Matrix inputScaler(inputShare.rows(), inputShare.cols());
    if (alg == Alg::CC) {
        inputShare_preScatter = inputShare;
    } else if (alg == Alg::SP) {
        inputShare_preScatter = inputShare;   
    } else if (alg == Alg::PR) {
        run_ConditionalMerge(
            prevChl,
            nextChl,
            role,
            inputShare,
            inputScaler,
            Matrix<u8>(),
            inputShare_preScatter,
            multCir_64,
            false,
            false        
        );     
    } else {
        printf("Unexpected Scatter Op in CoGNN!\n");
        exit(-1);
    }

    Matrix<u8> inputShare_byte(inputShare.rows(), byteSize);
    Matrix<u8> srcVertexShare_byte(edgeSrcTag.size(), byteSize);
    intMat2ByteMat(
        inputShare_preScatter,
        inputShare_byte,
        byteSize
    );    
    run_OEP(
        prevChl,
        nextChl,
        role,
        srcVertexTag, 
        edgeSrcTag, 
        inputShare_byte,
        srcVertexShare_byte        
    ); 

    byteMat2intMat(srcVertexShare_byte, srcVertexShare_int); 
    i64Matrix edgeShare(srcVertexShare_int.rows(), srcVertexShare_int.cols());
    if (alg == Alg::CC) {
        // Do nothing.
    } else if (alg == Alg::SP) {
        run_ConditionalMerge(
            prevChl,
            nextChl,
            role,
            srcVertexShare_int,
            edgeShare,
            Matrix<u8>(),
            srcVertexShare_int,
            addCir_64,
            false,
            false        
        );     
    } else if (alg == Alg::PR) {
        // Do nothing
    } else {
        printf("Unexpected Scatter Op in CoGNN!\n");
        exit(-1);
    }

    CommPkg comm = {prevChl, nextChl};
    Sh3Runtime rt(role, comm);
    Sh3Encryptor enc;
    enc.init(role, toBlock(role), toBlock((role + 1) % 3));
    Sh3BinaryEvaluator eval;    
    eval.mPrng.SetSeed(toBlock(role));
    Sh3ShareGen gen;
    gen.init(toBlock(role), toBlock((role + 1) % 3));

    // Merge   
#ifdef REMOVE_OGA    
    i64Matrix aggedSrcVertexShare_int = srcVertexShare_int;
#else
    i64Matrix aggedSrcVertexShare_int = prefix_network_aggregate(
        edgeDstTag,
        srcVertexShare_int,
        aop,
        eval,
        gen,
        rt,
        enc
    );
#endif

    Matrix<u8> aggedSrcVertexShare_byte(edgeDstTag.size(), byteSize);
    intMat2ByteMat(
        aggedSrcVertexShare_int,
        aggedSrcVertexShare_byte,
        byteSize
    );

    Matrix<u8> outputShare_byte(dstVertexTag.size(), byteSize);
    // Map
    if (!isLocal) {
        if (role == 0 || role == 1) {
            run_OEP(
                nextChl,
                prevChl,
                1 - role,
                edgeDstTag, 
                dstVertexTag, 
                aggedSrcVertexShare_byte,
                outputShare_byte        
            ); 
        } else {
            run_OEP(
                nextChl,
                prevChl,
                role,
                edgeDstTag, 
                dstVertexTag, 
                aggedSrcVertexShare_byte,
                outputShare_byte        
            ); 
        }
    } else {
        run_OEP(
            prevChl,
            nextChl,
            role,
            edgeDstTag, 
            dstVertexTag, 
            aggedSrcVertexShare_byte,
            outputShare_byte        
        );         
    }
    outputShare.resize(dstVertexTag.size(), inputShare.cols());
    byteMat2intMat(outputShare_byte, outputShare);
}

void cognn_gather(
    Channel& prevChl,
    Channel& nextChl,
    int pIdx,
    int role,
    const std::vector<i64Matrix>& updateShares,
    const i64Matrix& vertexShare,
    i64Matrix& outputShare,
    Alg alg
) {
    u64 numVertex = vertexShare.rows();
    u64 numP = updateShares.size();

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
        enc_updateShares[i].resize(numVertex, updateShares[i].cols() * 64);
        if (role == 0 || role == 1) {
            task = enc.localBinMatrix(task, updateShares[i], enc_updateShares[i]);
        } else {
            task = enc.remoteBinMatrix(task, enc_updateShares[i]);
        }     
    }
    sbMatrix enc_outputShare(numVertex, updateShares[0].cols() * 64);
    if (role == 0 || role == 1) {
        task = enc.localBinMatrix(task, vertexShare, enc_outputShare);
    } else {
        task = enc.remoteBinMatrix(task, enc_outputShare);
    }        
    task.get();

    BetaLibrary lib;
    BetaCircuit* mergeCir;
    BetaCircuit minCir;
    if (alg == Alg::CC) {
        mergeCir = lib.int_int_bitwiseOr(64, 64, 64);
    } else if (alg == Alg::SP) {
        get_min_Circ(minCir, 64);
        mergeCir = &minCir; 
    } else if (alg == Alg::PR) {
        mergeCir = lib.int_int_add(64, 64, 64);    
    } else {
        printf("Unexpected Scatter Op in Ours!\n");
        exit(-1);
    }   

    eval.setCir(mergeCir, numVertex, gen);
    for (u64 i = 0; i < numP; ++i) {
        eval.setInput(0, enc_outputShare);
        eval.setInput(1, enc_updateShares[i]);
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, enc_outputShare);
    }
    outputShare.resize(numVertex, vertexShare.cols());
    enc.revealToTwoParty(rt.noDependencies(), enc_outputShare, outputShare).get();
}