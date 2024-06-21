#include "OblvPermutation.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Matrix.h>
#include <unordered_set>
namespace osuCrypto
{
    u32 step = 1 << 14;

    void OblvPermutation::send(
        Channel& programChl,
        Channel& recvrChl,
        Matrix<u8> src,
        std::string tag)
    {
        u32 rows = (src.rows());
        //std::vector<u32> pi1(src.rows());
        //for (u32 i = 0; i < pi1.size(); ++i)
        //{
        //    pi1[i] = i;
        //}

        block seed = ZeroBlock;

        //ostreamLock(std::cout) << "send count " << (rows - 1 + step) / step << " (" << rows << ") " << tag << std::endl;


        //programChl.recv(seed);
        PRNG prng(seed, 256);
        PRNG prng2(seed ^ OneBlock, 256);
        for (u32 i = 0, j = rows; i < rows;)
        {
            auto min = std::min<u32>(rows - i, step);
            auto start = i;
            auto size = min * src.cols();;

            while (min--)
            {

                auto idx = prng.get<u32>() % j + i;
                //std::swap(pi1[i], pi1[idx]);

                //std::cout << pi1[i] << " -> " << i << " -> ___" << std::endl;

                auto srcBegin = &src(i, 0);
                auto srcEnd = srcBegin + src.cols();
                auto destBegin = &src(idx, 0);
                if(i != idx)
                    std::swap_ranges(srcBegin, srcEnd, destBegin);

                //std::cout << "data[" << i << "] = ";
                //for (u32 k = 0; k < src.cols(); ++k)
                //{
                //    std::swap(src(i, k), src(idx, k));

                //    //std::cout << " " << std::hex << int(src(i, k));
                //    //}
                //    ////std::cout << std::endl;
                //    //for (u32 k = 0; k < src.cols(); ++k)
                //    //{
                //    //auto c = prng.get<u8>();
                //    //src(i, k) ^= c;

                //    //if (k == 0)
                //    //{
                //    //    std::cout << "src[" << i << "] = " << int(c) << "\t" << int(src(i,k)) << std::endl;
                //    //}
                //    //    std::cout << " " << std::hex << int(src(i, k));
                //}
                //std::cout << std::endl << std::dec;

                ++i, --j;
            }

            auto data = src.data() + start * src.cols();

            for (u64 k = 0; k < size; )
            {
                //data[k] ^= prng2.get<u8>();
                // consume how many bytes are currently buffered in the PRNG.
                auto buffer = prng2.getBufferSpan(size - k);

                // XOR those over the data. 
                for (u64 m = 0; m < buffer.size(); ++m, ++k)
                {
                    data[k] ^= buffer[m];
                }
            }

            if (i == rows)
            {
                auto s = span<u8>(data, size);
                recvrChl.asyncSend(std::move(s), [a = std::move(src)](){});
            }
            else
                recvrChl.asyncSend(data, size);
        }

        //std::cout << "p0" << prng.get<int>() << std::endl;
    }

    void OblvPermutation::recv(
        Channel& programChl,
        Channel& sendrChl,
        MatrixView<u8> dest,
        u64 srcRows,
        std::string tag,
        OutputType type)
    {
        u32 recvCount = ((srcRows + step - 1) / step);

        std::vector<std::pair<std::future<void>, std::vector<u8>>> recvs1(recvCount);
        std::vector<std::pair<std::future<void>, std::vector<u32>>> recvs2(recvCount);
        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first = sendrChl.asyncRecv(recvs1[i].second);
            recvs2[i].first = programChl.asyncRecv(recvs2[i].second);

        }
        //ostreamLock(std::cout) << "recv count " << recvCount << " (" << srcRows <<") " << tag << std::endl;

        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first.get();
            recvs2[i].first.get();


            auto& perm = recvs2[i].second;
            auto& data = recvs1[i].second;
            if (perm.size() * dest.stride() != data.size())
                throw std::runtime_error(LOCATION);

            //ostreamLock(std::cout) << "recv size[" << i << "] -> " << data.size()<< " "<< tag << std::endl;

            auto iter = data.data();
            if (type == OutputType::Overwrite)
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    if (perm[j] != (u32)-1)
                    {

                        u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                        //std::cout << "____ -> " << j << " -> " << perm[j] << std::endl;

                        memcpy(destPtr, iter, dest.stride());
                    }
                    iter += dest.stride();
                }
            }
            else
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    if (perm[j] != (u32)-1)
                    {
                        u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                        for (u32 k = 0; k < dest.stride(); ++k)
                        {
                            destPtr[k] ^= *iter;
                            ++iter;
                        }
                    }
                    else
                    {
                        iter += dest.stride();
                    }
                }
            }
        }
    }



    void OblvPermutation::program(
        Channel& recvrChl,
        Channel& sendrChl,
        std::vector<u32> permutation,
        PRNG& p,
        MatrixView<u8> dest,
        std::string tag,
        OutputType type)
    {

        auto permPointer = std::make_shared<std::vector<u32>>(std::move(permutation));

        auto& perm = *permPointer;
        auto rows = (perm.size());

        //#ifndef NDEBUG
        //        if (true)
        //        {
        //            std::unordered_set<u32> set;
        //            set.reserve(dest.size());
        //            for (auto p : perm)
        //            {
        //                if (p != (u32)-1)
        //                {
        //                    auto r = set.emplace(p);
        //                    if (r.second == false)
        //                        throw std::runtime_error("");
        //                }
        //            }
        //
        //        }
        //#endif
        //
        //std::vector<u32> pi1(rows);
        //for (u32 i = 0; i <rows; ++i)
        //{
        //    pi1[i] = i;
        //}

        block seed = ZeroBlock;// p.get<block>();
                               //sendrChl.asyncSend(seed);
        PRNG prng(seed, 256);
        PRNG prng2(seed ^ OneBlock, 256);

        //ostreamLock(std::cout) << "prog count " << (rows - 1 + step) / step << " (" << rows << ") " << tag << std::endl;


        std::vector<u8> temp(dest.stride());
        for (u32 i = 0, j = rows; i < rows;)
        {
            auto min = std::min<u32>(rows - i, step);
            auto start = i;
            auto size = min;

            while (min)
            {
                auto buffer = prng.getBufferSpan(min * sizeof(u32));
                auto u32Buffer = span<u32>((u32*)buffer.data(), buffer.size() / sizeof(u32));

                min -= (u32)(u32Buffer.size());

                for (u64 k = 0; k < u32Buffer.size(); ++k)
                {
                    auto idx = u32Buffer[k] % j + i;
                    std::swap(perm[i], perm[idx]);

                    //if (perm[i] != (u32)-1)
                    //{
                    //    auto destPtr = &dest(perm[i], 0);

                    //    if (type == OutputType::Overwrite)
                    //    {
                    //        prng2.get(destPtr, dest.stride());
                    //        //std::cout << "dest[" << perm[i]<<", "<<i << "] = " << int(destPtr[0]) << std::endl;
                    //    }
                    //    else
                    //    {
                    //        for (u32 k = 0; k < dest.stride(); ++k)
                    //        {
                    //            destPtr[k] ^= prng2.get<u8>();
                    //        }
                    //    }
                    //}
                    //else
                    //{
                    //    prng2.get(temp.data(), temp.size());
                    //    //std::cout << "dest[" << perm[i] << ", " << i << "] = " << int(temp[0]) << std::endl;
                    //}

                    ++i, --j;
                }
            }

            u32* data = perm.data() + start;

            if (i == rows)
            {
                auto s = span<u32>(data, size);
                recvrChl.asyncSend(std::move(s), [permPointer]() {});
            }
            else
                recvrChl.asyncSend(data, size);




            //ostreamLock(std::cout) << "send size[" << i << "] -> " << size <<" " << tag << std::endl;
        }

        for (u32 i = 0; i < rows; ++i)
        {
            if (perm[i] != (u32)-1)
            {
                auto destPtr = &dest(perm[i], 0);

                if (type == OutputType::Overwrite)
                {
                    prng2.get(destPtr, dest.stride());
                    //std::cout << "dest[" << perm[i]<<", "<<i << "] = " << int(destPtr[0]) << std::endl;
                }
                else
                {
                    u32 k = 0;
                    do {
                        auto buff = prng2.getBufferSpan(dest.stride() - k);
                        for (auto j = 0; j < buff.size(); ++j, ++k)
                        {
                            destPtr[k] ^= buff[j];
                        }
                    } while (k < dest.stride());

                    //for (u32 k = 0; k < dest.stride(); ++k)
                    //{
                    //    destPtr[k] ^= prng2.get<u8>();
                    //}
                }
            }
            else
            {
                prng2.get(temp.data(), temp.size());
                //std::cout << "dest[" << perm[i] << ", " << i << "] = " << int(temp[0]) << std::endl;
            }
        }

        ///std::cout << "p0" << prng.get<int>() << std::endl;

    }

    void OblvPermutation::OPClient(Channel& serverChl, Channel& helperChl, std::vector<u64> perm, Matrix<u8> src, MatrixView<u8> dest, OutputType type) {
        // Randomize src with helper
        block seed = toBlock(1, -1);
        PRNG prng(seed, 256);
        Matrix<u8> tmp = src;
        tmp.setZero();
        prng.get(tmp.data(), tmp.size());
        for (u64 i = 0; i < tmp.size(); ++i) src(i) ^= tmp(i);

        dest.setZero();

        std::vector<u32> tmp_perm(perm.size(), 0);
        for (u64 i = 0; i < tmp_perm.size(); ++i) tmp_perm[i] = (u32)perm[i];
        program(serverChl, helperChl, tmp_perm, prng, dest, "tmp_tag", type);
        for (u64 i = 0; i < src.rows(); ++i) {
            for (u64 j = 0; j < src.cols(); ++j) {
                // printf("%d ", dest[i][j]);
                // printf("%d ", src[perm[i]][j]);
                dest[perm[i]][j] ^= src[i][j]; // Fixme: need confirm
                // printf("%d \n", dest[i][j]);
            }
        }


    }

    void OblvPermutation::OPServer(Channel& clientChl, Channel& helperChl, Matrix<u8> src, MatrixView<u8> dest, OutputType type) {
        helperChl.asyncSendCopy(src.data(), src.size());

        dest.setZero();

        recv(clientChl, helperChl, dest, src.rows(), "tmp_tag", type);
    }

    void OblvPermutation::OPHelper(Channel& clientChl, Channel& serverChl, u64 srcRows, u64 srcRowWidth) {
        Matrix<u8> src(srcRows, srcRowWidth);
        serverChl.recv(src.data(), src.size());
        // Randomize src with client
        Matrix<u8> tmp = src;
        tmp.setZero();
        block seed = toBlock(1, -1);
        PRNG prng(seed, 256);
        prng.get(tmp.data(), tmp.size());
        for (u64 i = 0; i < tmp.size(); ++i) src(i) ^= tmp(i);

        send(clientChl, serverChl, src, "tmp_tag");
    }
}
