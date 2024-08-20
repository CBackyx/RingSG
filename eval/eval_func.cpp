#include "eval_func.h"

#include <cryptoTools/Common/CLP.h>

#include "aby3_tests/aby3_tests.h"
#include <tests_cryptoTools/UnitTests.h>
#include <aby3-ML/main-linear.h>
#include <aby3-ML/main-logistic.h>
#include "tests_cryptoTools/UnitTests.h"
#include "cryptoTools/Crypto/PRNG.h"

#include "aby3-Graph_tests/tests.h"

std::mutex print_duration_mutex;

void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag) {
    print_duration_mutex.lock();
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "::" << tag << " took "
              << ((double)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count()) / 1000
              << " seconds\n";
    print_duration_mutex.unlock();
}

using namespace oc;
using namespace aby3;


void eval_ours()
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	Sh3_Graph_CC_test();
    print_duration(t_tmp, "ours");
}
