#include "aby3-DB/LowMC.h"
#include <iostream>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Common/TestCollection.h>
#include <cstdio>

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
#include <vector>
#include <string>


//////////////////
//     MAIN     //
//////////////////
using namespace oc;
using namespace aby3;

template<typename T>
struct vectorPrint
{
    const std::vector<T>& vec;

};
template<typename T>
std::ostream& operator<<(std::ostream& out, const vectorPrint<T>& v)
{
    out << "{";
    auto& vec = v.vec;

    if (vec.size())
    {
        out << vec[0];
    }

    for (u64 i = 1; i < vec.size(); ++i)
    {
        out << ", " << vec[i];
    }

    out << "}";
    return out;
}

void lowMC_Circuit_test() {
    // Example usage of the LowMC class
    // Instantiate a LowMC cipher instance called cipher using the key '1'.
    //LowMC cipher(1);
    LowMC2<> cipher2(false, 1);
    LowMC2<>::block m = 0xFFD5;

    //std::cout << "Plaintext:" << std::endl;
    //std::cout << m << std::endl;
    //auto c1 = cipher.encrypt(m);
    auto c2 = cipher2.encrypt(m);
    //std::cout << "Ciphertext:" << std::endl;
    //std::cout << c1 << std::endl;
    //std::cout << c2 << std::endl;
    //auto m1 = cipher.decrypt(c1);
    //auto m2 = cipher2.decrypt(c2);
    //std::cout << "Encryption followed by decryption of plaintext:" << std::endl;
    //std::cout << m1 << std::endl;
    //std::cout << m2 << std::endl;



    oc::BetaCircuit cir;
    cipher2.to_enc_circuit(cir);

    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    for (u64 r = 0; r < 13; ++r)
    {
        inputs[r + 1].resize(blocksize);
        for (u64 i = 0; i < blocksize; ++i)
        {
            inputs[r + 1][i] = cipher2.roundkeys[r][i];
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    //std::cout << outputs[0] << std::endl;


    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0][i] << std::endl;
            std::cout << "o " <<c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }

    //cipher2.print_matrices();

}

void lowMC_FileCircuit_test() {

#ifdef USE_JSON
    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0000101000111011000011010101010100001010100101001001100100110011110000001011110101100111000001011010110101110001100110000101110111100110100101110110101101100001011000101101001001101011011111010011001111010110011101010001000100001100101001100011111100011110", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + std::to_string(sizeof(LowMC2<>::block)) + "_k" + std::to_string(sizeof(LowMC2<>::keyblock)) +".json";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(false, 1);
        LowMC2<> cipher2(false, 1);


        for (u64 i = 0; i < cipher1.LinMatrices.size(); ++i)
        {
            if (cipher1.LinMatrices[i] != cipher2.LinMatrices[i])
            {
                std::cout << i << " \n" 
                    << vectorPrint<LowMC2<>::block>{ cipher1.LinMatrices[i] } 
                    << "\n != \n" 
                        << cipher2.LinMatrices[i][0] << std::endl;

                throw std::runtime_error(LOCATION);
            }
        }
        t.setTimePoint("lowMC done");

        cipher2.to_enc_circuit(cir);

        t.setTimePoint("to circuit done");
        std::ofstream out;
        out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);

        cir.writeJson(out);
        out.close();


        t.setTimePoint("write circuit");

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < 13; ++i)
        {
            for (u64 j = 0; j < cipher2.roundkeys[i].size(); ++j)
            {
                cipher2.roundkeys[i][j] = prng.get<bool>();
            }
        }

        auto c1 = cipher2.encrypt(m);
        if (c1 != c2)
        {
            std::cout << "verify exp " << c2 << std::endl;
            std::cout << "verify act " << c1 << std::endl;
            throw std::runtime_error(LOCATION);
        }

        t.setTimePoint("verify done");

    }
    else
    {
        cir.readJson(in);
        t.setTimePoint("read circuit done");

    }
    //if (cir != cir2)
    //    throw std::runtime_error(LOCATION);



    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    PRNG prng(ZeroBlock);
    for (u64 i = 0; i < 13; ++i)
    {
        inputs[i + 1].resize(blocksize);
        for (u64 j = 0; j < blocksize; ++j)
        {
            inputs[i + 1][j] = prng.get<bool>();
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    t.setTimePoint("circuit eval done");
    std::remove(filename.c_str());


    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0] << std::endl;
            std::cout << "o " << c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }
#else
	throw UnitTestSkipped("USE_JSON not defined");
#endif
}



void lowMC_BinFileCircuit_test() {

    throw oc::UnitTestSkipped("known issues");

    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0000101000111011000011010101010100001010100101001001100100110011110000001011110101100111000001011010110101110001100110000101110111100110100101110110101101100001011000101101001001101011011111010011001111010110011101010001000100001100101001100011111100011110", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + std::to_string(sizeof(LowMC2<>::block)) + "_k" + std::to_string(sizeof(LowMC2<>::keyblock)) + ".bin";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(false, 1);
        LowMC2<> cipher2(false, 1);


        for (u64 i = 0; i < cipher1.LinMatrices.size(); ++i)
        {
            if (cipher1.LinMatrices[i] != cipher2.LinMatrices[i])
                throw std::runtime_error(LOCATION);
        }
        t.setTimePoint("lowMC done");

        cipher2.to_enc_circuit(cir);

        t.setTimePoint("to circuit done");
        std::ofstream out;
        out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);

        cir.writeBin(out);
        out.close();


        t.setTimePoint("write circuit");

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < 13; ++i)
        {
            for (u64 j = 0; j < cipher2.roundkeys[i].size(); ++j)
            {
                cipher2.roundkeys[i][j] = prng.get<bool>();
            }
        }

        auto c1 = cipher2.encrypt(m);
        if (c1 != c2)
        {
            std::cout << "verify exp " << c2 << std::endl;
            std::cout << "verify act " << c1 << std::endl;
            throw std::runtime_error(LOCATION);
        }

        t.setTimePoint("verify done");

    }
    else
    {
        cir.readBin(in);
        t.setTimePoint("read circuit done");

    }
    //if (cir != cir2)
    //    throw std::runtime_error(LOCATION);



    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    PRNG prng(ZeroBlock);
    for (u64 i = 0; i < 13; ++i)
    {
        inputs[i + 1].resize(blocksize);
        for (u64 j = 0; j < blocksize; ++j)
        {
            inputs[i + 1][j] = prng.get<bool>();
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    t.setTimePoint("circuit eval done");


    std::remove(filename.c_str());
    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {

            //std::cout << "failed " << i << std::endl;

            //std::cout << "m " << m << std::endl;
            //std::cout << "M " << inputs[0] << std::endl;
            //std::cout << "o " << c2 << std::endl;
            //std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestSkipped("Known issue, need to investigate. " LOCATION);
        }
    }
}

std::vector<u64> serializeStringToU64(const std::string& str) {
    std::vector<u64> vec;
    u64 buffer = 0;
    int bufferIndex = 0;

    for (char c : str) {
        buffer |= static_cast<u64>(c) << (8 * bufferIndex++);
        if (bufferIndex == 8) {
            vec.push_back(buffer);
            buffer = 0;
            bufferIndex = 0;
        }
    }

    // Handle any remaining characters in the buffer
    if (bufferIndex > 0) {
        vec.push_back(buffer);
    }

    return vec;
}

std::string deserializeU64ToString(const std::vector<u64>& vec, int n) {
    std::string result;
    result.reserve(n); // Reserve space for n characters

    for (u64 num : vec) {
        for (int i = 0; i < 8 && n > 0; ++i, --n) {
            char c = static_cast<char>((num >> (8 * i)) & 0xFF);
            result.push_back(c);
        }
    }

    return result;
}

void lowMC_CircuitEval_test() {
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

    BetaCircuit lowMCCir;

    std::string id = "kk";
    std::string value = "hello-world";
    std::string filename = "./lowMCCircuit_ " + id + ".bin";

    // std::ifstream in;
    // in.open(filename, std::ios::in | std::ios::binary);
    LowMC2<> cipher1(false, 1);
    cipher1.to_enc_circuit(lowMCCir);

    // if (in.is_open() == false) {
    //     LowMC2<> cipher1(false, 1);
    //     cipher1.to_enc_circuit(lowMCCir);

    //     std::ofstream out;
    //     out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);
    //     lowMCCir.levelByAndDepth();

    //     lowMCCir.writeBin(out);
    //     out.close();
    // } else {
    //     lowMCCir.readBin(in);
    // }
    u64 rounds = lowMCCir.mInputs.size() - 1;
    u64 blockSize = 256;
    u64 wordSize = blockSize / 64;

    std::vector<u64> serialized = serializeStringToU64(id + "::" + value);
    u64 width = serialized.size() / wordSize;
    if (serialized.size() % wordSize != 0) width += 1;

    auto routine = [&](int pIdx) {
        i64Matrix kv(width, wordSize);
        i64Matrix enckv(width, wordSize);
        PRNG prng(toBlock(0, pIdx));
        std::vector<i64Matrix> keys(rounds);
        std::vector<i64Matrix> mergedKeys(rounds);
        for (u64 i = 0; i < rounds; ++i) {
            keys[i].resize(1, wordSize);
            mergedKeys[i].resize(1, wordSize);
            prng.get(keys[i].data(), keys[i].size());
        }
        for (u64 i = 0; i < serialized.size(); ++i) {
            kv(i / wordSize, i % wordSize) = serialized[i];
        }

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sbMatrix KV(width, blockSize);
        sbMatrix encKV(width, blockSize);
        std::vector<sbMatrix> Keys(rounds);
        for (u64 i = 0; i < rounds; ++i) {
            Keys[i].resize(1, blockSize);
        }

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));
        Sh3BinaryEvaluator eval;
        eval.mPrng.SetSeed(toBlock(pIdx));
        Sh3ShareGen gen;
        gen.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));

        auto task = rt.noDependencies();

        if (pIdx == 0) {
            task = enc.localBinMatrix(task, kv, KV);
        } else {
            task = enc.remoteBinMatrix(task, KV);
        }
        for (u64 i = 0; i < rounds; ++i) {
            task = enc.localBinMatrix(task, keys[i], Keys[i]);
        }
        task.get();


        eval.setCir(&lowMCCir, width, gen);
        eval.setInput(0, KV);
        for (u64 i = 0; i < rounds; ++i) {
            eval.setReplicatedInput(i + 1, Keys[i]);
        }
        eval.asyncEvaluate(rt.noDependencies()).get();
        eval.getOutput(0, encKV);

        task = enc.revealAll(rt.noDependencies(), encKV, enckv);
        for (u64 i = 0; i < rounds; ++i) {
            task = enc.revealAll(task, Keys[i], mergedKeys[i]);
        }
        task.get();

        u64 digest = 0;
        for (u64 i = 0; i < width; ++i) {
            for (u64 j = 0; j < wordSize; ++j) digest ^= enckv(i, j);
        }
    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();
}