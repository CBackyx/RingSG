#include "eval_func.h"

#include <cryptoTools/Common/CLP.h>
#include <thread>

#include "aby3_tests/aby3_tests.h"
#include <tests_cryptoTools/UnitTests.h>
#include <aby3-ML/main-linear.h>
#include <aby3-ML/main-logistic.h>
#include "tests_cryptoTools/UnitTests.h"
#include "cryptoTools/Crypto/PRNG.h"

using namespace oc;
using namespace aby3;


void eval_ours(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations)
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	// Sh3_Graph_CC_test();

    // std::vector<std::thread> thrds;
    // u64 numP = 5;
    // for (u64 i = 0; i < numP; ++i)
    //     thrds.emplace_back(std::thread(Sh3_Graph_Ours_single_party, i));

    // for (u64 i = 0; i < numP; ++i)
    //     thrds[i].join();

    Sh3_Graph_Ours_single_party(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations);

    print_duration(t_tmp, "ours");
}

void eval_cognn(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations)
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	// Sh3_Graph_CC_test();

    // std::vector<std::thread> thrds;
    // u64 numP = 5;
    // for (u64 i = 0; i < numP; ++i)
    //     thrds.emplace_back(std::thread(Sh3_Graph_CoGNN_single_party, i));

    // for (u64 i = 0; i < numP; ++i)
    //     thrds[i].join();

    Sh3_Graph_CoGNN_single_party(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations);

    print_duration(t_tmp, "CoGNN");
}

void eval_graphsc(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations)
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	// Sh3_Graph_CC_test();

    // std::vector<std::thread> thrds;
    // u64 numP = 3;
    // for (u64 i = 0; i < numP; ++i)
    //     thrds.emplace_back(std::thread(Sh3_Graph_GraphSC_single_party, i));

    // for (u64 i = 0; i < numP; ++i)
    //     thrds[i].join();

    Sh3_Graph_GraphSC_single_party(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations);

    print_duration(t_tmp, "GraphSC");
}

void eval_ours_app(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations)
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	// Sh3_Graph_CC_test();

    // std::vector<std::thread> thrds;
    // u64 numP = 5;
    // for (u64 i = 0; i < numP; ++i)
    //     thrds.emplace_back(std::thread(Sh3_Graph_Ours_single_party, i));

    // for (u64 i = 0; i < numP; ++i)
    //     thrds[i].join();

    Sh3_Graph_Ours_App_single_party(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations);

    print_duration(t_tmp, "oursApp");
}

