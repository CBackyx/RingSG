#pragma once
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Network/Channel.h>
namespace osuCrypto
{
    class PRNG;
    enum class OutputType
    {
        Overwrite,
        Additive
    };

    class OblvPermutation
    {
    public:



        void send(Channel& programChl, Channel& revcrChl, Matrix<u8> src, std::string tag);
        void recv(Channel& programChl, Channel& sendrChl, MatrixView<u8> dest, u64 srcRows, std::string tag, OutputType type = OutputType::Overwrite);
        void program(Channel& revcrChl, Channel& sendrChl, std::vector<u32> perm, PRNG& prng, MatrixView<u8> dest, std::string tag, OutputType type = OutputType::Overwrite);

        // Note that this is a reversed perm, meaning that dest[perm[i]] = src[i].
        void OPClient(Channel& serverChl, Channel& helperChl, std::vector<u64> perm, Matrix<u8> src, MatrixView<u8> dest, OutputType type = OutputType::Overwrite);
        void OPServer(Channel& clientChl, Channel& helperChl, Matrix<u8> src, MatrixView<u8> dest, OutputType type = OutputType::Overwrite);
        void OPHelper(Channel& clientChl, Channel& serverChl, u64 srcRows, u64 srcRowWidth);
    };

}
