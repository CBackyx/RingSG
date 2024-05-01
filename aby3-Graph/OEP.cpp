#include "OEP.h"

using namespace oc;
using namespace aby3;

void run_OEP(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    const std::vector<u64>& srcTag, 
    const std::vector<u64>& dstTag, 
    const Matrix<u8>& inputShare,
    Matrix<u8>& outputShare
) {
    OblvSwitchNet snet("test");
    PRNG prng(toBlock(444));
    if (role == 0) {
        snet.OEPClient(prevChl, nextChl, srcTag, dstTag, prng, inputShare, outputShare);
    } else if (role == 1) {
        snet.OEPServer(prevChl, nextChl, inputShare, outputShare);
    } else if (role == 2) {
        snet.OEPHelper(nextChl, prevChl, prng, dstTag.size(), srcTag.size(), inputShare.cols());
    }
}