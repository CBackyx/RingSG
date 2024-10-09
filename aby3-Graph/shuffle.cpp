#include "shuffle.h"
#include "utils.h"

#include <array>
#include <algorithm>

using namespace oc;
using namespace aby3;

Matrix<i64> xorMat(
    const Matrix<i64>& a,
    const Matrix<i64>& b
     
) {
    Matrix<i64> c(a.rows(), a.cols());
    for (i64 i = 0; i < c.size(); ++i) {
        c(i) = a(i) ^ b(i);
    }    
    return c;
}

Matrix<i64> permMat(
    const Matrix<i64>& a,
    const std::vector<u64>& perm
) {
    Matrix<i64> b(a.rows(), a.cols());
    for (i64 i = 0; i < b.rows(); ++i) {
        for (i64 j = 0; j < b.cols(); ++j) {
            b(i, j) = a(perm[i], j);
        }
    }    
    return b;
}

std::vector<u64> getRandomPerm(PRNG& prng, u64 length) {
    std::vector<u64> perm(length, 0);
    for (i64 i = 0; i < length; ++i) perm[i] = u64(i);
    for (i64 i = length - 1; i >= 0; --i) {
        //generate a random number [0, n-1]
        i64 j = prng.get<u64>() % (i+1);

        //swap the last element with element at random index
        u64 temp = perm[i];
        perm[i] = perm[j];
        perm[j] = temp;
    }    
    return perm;
}

void shuffle(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    block seed,
    const sbMatrix& input,
    sbMatrix& output,
    std::vector<aby3::u64>& prevPerm,
    std::vector<aby3::u64>& nextPerm,
    Sh3Encryptor& enc
) {
    nextChl.asyncSendCopy(seed);
    block prevSeed;
    prevChl.recv(prevSeed);
    PRNG prevPrng(prevSeed);
    PRNG curPrng(seed);

    // Gen permutations using seeds
    u64 length = input.rows();
    prevPerm = getRandomPerm(prevPrng, length);
    nextPerm = getRandomPerm(curPrng, length);

    // print_vector_lock(prevPerm);
    // print_vector_lock(nextPerm);

    shuffle(
        prevChl,
        nextChl,
        role,
        prevPerm,
        nextPerm,
        input,
        output,
        enc
    );
}

void shuffle(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    const std::vector<u64>& prevPerm,
    const std::vector<u64>& nextPerm,
    const sbMatrix& input,
    sbMatrix& output,
    Sh3Encryptor& enc
) {
    sbMatrix randMat(input.rows(), input.mBitCount);
    enc.rand(randMat);
    // print_matrix(randMat.mShares[0], role);
    // print_matrix(randMat.mShares[1], role);
    sbMatrix randResult(input.rows(), input.mBitCount);
    enc.rand(randResult);

    if (role == 0) {
        // Cal X1
        Matrix<i64> X1 = xorMat(input.mShares[1], input.mShares[0]);
        X1 = xorMat(X1, randMat.mShares[0]);
        X1 = permMat(X1, nextPerm);
        Matrix<i64> X2 = xorMat(X1, randMat.mShares[1]);
        X2 = permMat(X2, prevPerm);
        nextChl.asyncSendCopy(X2.data(), X2.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[1] = randResult.mShares[1];
        output.mShares[0] = randResult.mShares[0];
    } else if (role == 1) {
        // Cal Y1 and send
        Matrix<i64> Y1 = xorMat(input.mShares[0], randMat.mShares[1]);
        Y1 = permMat(Y1, prevPerm);
        nextChl.asyncSendCopy(Y1.data(), Y1.size());
        // Recv X2
        Matrix<i64> X2(Y1.rows(), Y1.cols());
        prevChl.recv(X2.data(), X2.size());
        // Cal X3
        Matrix<i64> X3 = xorMat(X2, randMat.mShares[0]);
        X3 = permMat(X3, nextPerm);
        // Cal C1
        Matrix<i64> C1 = xorMat(X3, randResult.mShares[1]);
        Matrix<i64> C2(C1.rows(), C1.cols());
        nextChl.asyncSendCopy(C1.data(), C1.size());
        nextChl.recv(C2.data(), C2.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[1] = randResult.mShares[1];
        output.mShares[0] = xorMat(C1, C2);     
    } else if (role == 2) {
        // Recv Y1
        Matrix<i64> Y1(input.mShares[0].rows(), input.mShares[0].cols());
        prevChl.recv(Y1.data(), Y1.size());
        Matrix<i64> Y2 = xorMat(Y1, randMat.mShares[0]);
        Y2 = permMat(Y2, nextPerm);
        Matrix<i64> Y3 = xorMat(Y2, randMat.mShares[1]);
        Y3 = permMat(Y3, prevPerm);
        Matrix<i64> C2 = xorMat(Y3, randResult.mShares[0]);
        Matrix<i64> C1(C2.rows(), C2.cols());
        prevChl.asyncSendCopy(C2.data(), C2.size());
        prevChl.recv(C1.data(), C1.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[1] = xorMat(C1, C2);
        output.mShares[0] = randResult.mShares[0];
    } else {
        printf("Illegal role during shuffle!\n");
        exit(-1);
    }
}

void reverse_shuffle(
    Channel& nextChl,
    Channel& prevChl,
    int role,
    const std::vector<u64>& nextPerm,
    const std::vector<u64>& prevPerm,
    const sbMatrix& input,
    sbMatrix& output,
    Sh3Encryptor& enc
) {
    sbMatrix randMat(input.rows(), input.mBitCount);
    enc.rand(randMat);
    // print_matrix(randMat.mShares[0], role);
    // print_matrix(randMat.mShares[1], role);
    sbMatrix randResult(input.rows(), input.mBitCount);
    enc.rand(randResult);

    // std::vector<u64> invPrevPerm = prevPerm;
    // std::vector<u64> invNextPerm = nextPerm;
    // for (u64 i = 0; i < prevPerm.size(); ++i) invPrevPerm[prevPerm[i]] = i;
    // for (u64 i = 0; i < nextPerm.size(); ++i) invNextPerm[nextPerm[i]] = i;

    if (role == 2) {
        // Cal X1
        Matrix<i64> X1 = xorMat(input.mShares[0], input.mShares[1]);
        X1 = xorMat(X1, randMat.mShares[1]);
        X1 = permMat(X1, nextPerm);
        Matrix<i64> X2 = xorMat(X1, randMat.mShares[0]);
        X2 = permMat(X2, prevPerm);
        nextChl.asyncSendCopy(X2.data(), X2.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[0] = randResult.mShares[0];
        output.mShares[1] = randResult.mShares[1];
    } else if (role == 1) {
        // Cal Y1 and send
        Matrix<i64> Y1 = xorMat(input.mShares[1], randMat.mShares[0]);
        Y1 = permMat(Y1, prevPerm);
        nextChl.asyncSendCopy(Y1.data(), Y1.size());
        // Recv X2
        Matrix<i64> X2(Y1.rows(), Y1.cols());
        prevChl.recv(X2.data(), X2.size());
        // Cal X3
        Matrix<i64> X3 = xorMat(X2, randMat.mShares[1]);
        X3 = permMat(X3, nextPerm);
        // Cal C1
        Matrix<i64> C1 = xorMat(X3, randResult.mShares[0]);
        Matrix<i64> C2(C1.rows(), C1.cols());
        nextChl.asyncSendCopy(C1.data(), C1.size());
        nextChl.recv(C2.data(), C2.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[0] = randResult.mShares[0];
        output.mShares[1] = xorMat(C1, C2);     
    } else if (role == 0) {
        // Recv Y1
        Matrix<i64> Y1(input.mShares[1].rows(), input.mShares[1].cols());
        prevChl.recv(Y1.data(), Y1.size());
        Matrix<i64> Y2 = xorMat(Y1, randMat.mShares[1]);
        Y2 = permMat(Y2, nextPerm);
        Matrix<i64> Y3 = xorMat(Y2, randMat.mShares[0]);
        Y3 = permMat(Y3, prevPerm);
        Matrix<i64> C2 = xorMat(Y3, randResult.mShares[1]);
        Matrix<i64> C1(C2.rows(), C2.cols());
        prevChl.asyncSendCopy(C2.data(), C2.size());
        prevChl.recv(C1.data(), C1.size());
        // Set result
        output.resize(input.rows(), input.mBitCount);
        output.mShares[0] = xorMat(C1, C2);
        output.mShares[1] = randResult.mShares[1];
    } else {
        printf("Illegal role during shuffle!\n");
        exit(-1);
    }
}