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
#ifdef REMOVE_OEP
    // Do nothing
#else
    OblvSwitchNet snet("test");
    PRNG prng(toBlock(444));
    if (role == 0) {
        snet.OEPClient(prevChl, nextChl, srcTag, dstTag, prng, inputShare, outputShare);
    } else if (role == 1) {
        snet.OEPServer(prevChl, nextChl, inputShare, outputShare);
    } else if (role == 2) {
        snet.OEPHelper(nextChl, prevChl, prng, dstTag.size(), srcTag.size(), inputShare.cols());
    }
#endif
}

void run_OP(
    Channel& prevChl,
    Channel& nextChl,
    int role,
    const std::vector<u64>& perm, 
    const Matrix<u8>& inputShare,
    Matrix<u8>& outputShare
) {
#ifdef REMOVE_OEP
    // Do nothing
#else
    OblvPermutation oblvPerm;
    // Reverse the perm first
    std::vector<u64> reversedPerm(perm.size(), 0);
    for (u64 i = 0; i < reversedPerm.size(); ++i) {
        reversedPerm[perm[i]] = i;
    }
    if (role == 0) {
        oblvPerm.OPClient(nextChl, prevChl, reversedPerm, inputShare, outputShare);
    } else if (role == 1) {
        oblvPerm.OPServer(prevChl, nextChl, inputShare, outputShare);
    } else if (role == 2) {
        oblvPerm.OPHelper(nextChl, prevChl, perm.size(), inputShare.cols());
    }
#endif
}