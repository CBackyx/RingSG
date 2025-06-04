#include "demo_func.h"

#include <cryptoTools/Common/CLP.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <stdexcept>

#include "aby3_tests/aby3_tests.h"
#include <tests_cryptoTools/UnitTests.h>
#include <aby3-ML/main-linear.h>
#include <aby3-ML/main-logistic.h>
#include "tests_cryptoTools/UnitTests.h"
#include "cryptoTools/Crypto/PRNG.h"

using namespace oc;
using namespace aby3;

void saveGraphToFiles(
    const std::vector<std::vector<u64>>& vertexIdLists,
    const std::vector<std::vector<u64>>& vertexDataLists,
    const std::vector<std::vector<std::vector<std::array<u64, 2>>>>& edgeLists,
    std::string workspace_dir
) {
    u64 numP = vertexIdLists.size();
    // Generate a separate file for each party
    for (u64 partyId = 0; partyId < numP; ++partyId) {
        std::string filename = workspace_dir + "/party_" + std::to_string(partyId) + ".txt";
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            continue;
        }

        // Section 1: Vertices (ID and Data)
        file << "# Vertices (Format: VertexID VertexData)" << std::endl;
        for (u64 i = 0; i < vertexIdLists[partyId].size(); ++i) {
            u64 vertexId = vertexIdLists[partyId][i];
            u64 vertexData = vertexDataLists[partyId][i];
            file << vertexId << " " << vertexData << std::endl;
        }
        file << std::endl;

        // Section 2: Incoming edges (grouped by source party)
        file << "# Incoming Edges (Format: SourceVertexID TargetVertexID)" << std::endl;
        for (u64 sourcePartyId = 0; sourcePartyId < numP; ++sourcePartyId) {
            file << "# From Party " << sourcePartyId << std::endl;
            for (const auto& edge : edgeLists[sourcePartyId][partyId]) {
                u64 src = edge[0];
                u64 dst = edge[1];
                file << src << " " << dst << std::endl;
            }
        }
        file << std::endl;

        // Section 3: Outgoing edges (grouped by target party)
        file << "# Outgoing Edges (Format: SourceVertexID TargetVertexID)" << std::endl;
        for (u64 targetPartyId = 0; targetPartyId < numP; ++targetPartyId) {
            file << "# To Party " << targetPartyId << std::endl;
            for (const auto& edge : edgeLists[partyId][targetPartyId]) {
                u64 src = edge[0];
                u64 dst = edge[1];
                file << src << " " << dst << std::endl;
            }
        }

        file.close();
        std::cout << "Graph data saved to file: " << filename << std::endl;
    }
}

void generateAndSaveGraph(
    u64 numP, 
    u64 pIndex, 
    u64 scale, 
    u64 avgDegree,
    double interRatio,
    int algId, 
    u64 numIters,
    std::string workspace_dir
) {
    // Calculate graph parameters
    u64 numVertexPerP = (1ULL << scale);
    u64 numEdgePerP = (1ULL << scale) * avgDegree;
    u64 numIntraEdgePerP = numEdgePerP * (1 - interRatio);
    u64 numInterEdgePerPair = (numEdgePerP * interRatio) / (numP - 1);

    // Initialize vertex lists
    std::vector<u64> numVertexList(numP, numVertexPerP);
    std::vector<std::vector<u64>> vertexIdLists(numP, std::vector<u64>(numVertexPerP, 0));
    std::vector<std::vector<u64>> vertexDataLists(numP, std::vector<u64>(numVertexPerP, 0));
    
    // Assign global vertex IDs
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numVertexPerP; ++j) {
            vertexIdLists[i][j] = i * numVertexPerP + j;
        }
    }
    
    // Set initial value for the first vertex in party 0
    vertexDataLists[0][0] = 1;

    // Initialize edge matrices and lists
    std::vector<std::vector<u64>> numEdgeMat(numP, std::vector<u64>(numP));
    std::vector<std::vector<std::vector<std::array<u64, 2>>>> edgeLists(
        numP, std::vector<std::vector<std::array<u64, 2>>>(numP));

    // Generate intra-party and inter-party edges
    for (u64 i = 0; i < numP; ++i) {
        for (u64 j = 0; j < numP; ++j) {
            if (i == j) {
                // Intra-party edges
                numEdgeMat[i][j] = numIntraEdgePerP;
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[i][k]});
                        if (++cnt >= numIntraEdgePerP) break;
                    }
                    if (cnt >= numIntraEdgePerP) break;
                }
            } else {
                // Inter-party edges (from party i to party j)
                numEdgeMat[i][j] = numInterEdgePerPair;
                u64 cnt = 0;
                for (u64 m = 0; m < numVertexPerP; ++m) {
                    for (u64 k = 0; k < numVertexPerP; ++k) {
                        edgeLists[i][j].push_back({vertexIdLists[i][m], vertexIdLists[j][k]});
                        if (++cnt >= numInterEdgePerPair) break;
                    }
                    if (cnt >= numInterEdgePerPair) break;
                }
            }
        }
    }

    // Save graph to files using the previously defined function
    saveGraphToFiles(vertexIdLists, vertexDataLists, edgeLists, workspace_dir);
}

/**
 * Parse a party's graph file and populate vertex and edge lists.
 * 
 * @param filename Path to the party's graph file
 * @param vertexIdList Output vector for vertex IDs
 * @param vertexDataList Output vector for vertex data
 * @param incomingEdgeLists Output matrix for incoming edges (indexed by source party)
 * @param outGoingEdgeLists Output matrix for outgoing edges (indexed by target party)
 * @param numParties Total number of parties in the system
 * @return true if parsing successful, false otherwise
 */
bool parsePartyFile(const std::string& filename,
                   std::vector<u64>& vertexIdList,
                   std::vector<u64>& vertexDataList,
                   std::vector<std::vector<std::array<u64, 2>>>& incomingEdgeLists,
                   std::vector<std::vector<std::array<u64, 2>>>& outGoingEdgeLists,
                   u64 numParties) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // Initialize edge lists with empty vectors for each party
    incomingEdgeLists.resize(numParties);
    outGoingEdgeLists.resize(numParties);

    enum class Section { NONE, VERTICES, INCOMING, OUTGOING };
    Section currentSection = Section::NONE;
    u64 currentParty = 0;

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) continue;

        // Check for section headers
        if (line.find("# Vertices") == 0) {
            currentSection = Section::VERTICES;
            continue;
        } else if (line.find("# Incoming Edges") == 0) {
            currentSection = Section::INCOMING;
            continue;
        } else if (line.find("# Outgoing Edges") == 0) {
            currentSection = Section::OUTGOING;
            continue;
        } 
        // Check for party subheaders
        else if (line.find("# From Party ") == 0) {
            if (currentSection != Section::INCOMING) continue;
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                currentParty = std::stoull(line.substr(pos));
                if (currentParty >= numParties) {
                    std::cerr << "Invalid party ID in file: " << currentParty << std::endl;
                    return false;
                }
            }
            continue;
        } else if (line.find("# To Party ") == 0) {
            if (currentSection != Section::OUTGOING) continue;
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                currentParty = std::stoull(line.substr(pos));
                if (currentParty >= numParties) {
                    std::cerr << "Invalid party ID in file: " << currentParty << std::endl;
                    return false;
                }
            }
            continue;
        }

        // Skip comment lines
        if (line[0] == '#') continue;

        // Parse data lines based on current section
        std::istringstream iss(line);
        u64 v1, v2;

        switch (currentSection) {
            case Section::VERTICES:
                if (iss >> v1 >> v2) {
                    vertexIdList.push_back(v1);
                    vertexDataList.push_back(v2);
                } else {
                    std::cerr << "Error parsing vertex line: " << line << std::endl;
                    return false;
                }
                break;

            case Section::INCOMING:
                if (iss >> v1 >> v2) {
                    incomingEdgeLists[currentParty].push_back({v1, v2});
                } else {
                    std::cerr << "Error parsing incoming edge line: " << line << std::endl;
                    return false;
                }
                break;

            case Section::OUTGOING:
                if (iss >> v1 >> v2) {
                    outGoingEdgeLists[currentParty].push_back({v1, v2});
                } else {
                    std::cerr << "Error parsing outgoing edge line: " << line << std::endl;
                    return false;
                }
                break;

            default:
                // Ignore lines before any section header
                break;
        }
    }

    file.close();
    return true;
}

bool saveUpdatedVertexData(const std::string& filename,
                          const std::vector<u64>& vertexIdList,
                          const std::vector<u64>& vertexDataList) {
    if (vertexIdList.size() != vertexDataList.size()) {
        std::cerr << "Error: Vertex ID list and data list sizes mismatch!" << std::endl;
        return false;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // Write header
    file << "# Updated Vertex Data (Format: VertexID NewValue)" << std::endl;
    
    // Write each vertex and its updated data
    for (size_t i = 0; i < vertexIdList.size(); ++i) {
        file << vertexIdList[i] << " " << vertexDataList[i] << std::endl;
    }

    file.close();
    std::cout << "Updated vertex data saved to: " << filename << std::endl;
    return true;
}

void demo_ours(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations, std::string workspace_dir, std::string command)
{
    auto t_tmp = std::chrono::high_resolution_clock::now();
	// Sh3_Graph_CC_test();

    // std::vector<std::thread> thrds;
    // u64 numP = 5;
    // for (u64 i = 0; i < numP; ++i)
    //     thrds.emplace_back(std::thread(Sh3_Graph_Ours_single_party, i));

    // for (u64 i = 0; i < numP; ++i)
    //     thrds[i].join();
    std::vector<u64> vertexIdList;
    std::vector<u64> vertexDataList;
    std::vector<std::vector<std::array<u64, 2>>> incomingEdgeLists;
    std::vector<std::vector<std::array<u64, 2>>> outGoingEdgeLists;

    if (command == "GenTestGraph") {
        if (pIdx == 0) {
            generateAndSaveGraph(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations, workspace_dir);
        }
    } else {
        parsePartyFile(workspace_dir + "/party_" + std::to_string(pIdx) + ".txt", vertexIdList, vertexDataList, incomingEdgeLists, outGoingEdgeLists, numParts);

        // Debug prints for vertex data
        std::cout << "\n[DEBUG] Parsed " << vertexIdList.size() << " vertices:" << std::endl;
        for (size_t i = 0; i < std::min<size_t>(5, vertexIdList.size()); i++) {
            std::cout << "  Vertex ID: " << vertexIdList[i] 
                      << ", Data: " << vertexDataList[i] << std::endl;
        }
        if (vertexIdList.size() > 5) {
            std::cout << "  ... and " << (vertexIdList.size() - 5) << " more vertices" << std::endl;
        }
        
        // Debug prints for incoming edges
        size_t totalIncoming = 0;
        for (u64 srcParty = 0; srcParty < numParts; srcParty++) {
            totalIncoming += incomingEdgeLists[srcParty].size();
        }
        std::cout << "[DEBUG] Parsed " << totalIncoming << " incoming edges from " 
                  << numParts << " source parties:" << std::endl;
        for (u64 srcParty = 0; srcParty < std::min<u64>(10, numParts); srcParty++) {
            std::cout << "  From Party " << srcParty << ": " 
                      << incomingEdgeLists[srcParty].size() << " edges" << std::endl;
            for (size_t i = 0; i < std::min<size_t>(10, incomingEdgeLists[srcParty].size()); i++) {
                auto& edge = incomingEdgeLists[srcParty][i];
                std::cout << "    " << edge[0] << " -> " << edge[1] << std::endl;
            }
            if (incomingEdgeLists[srcParty].size() > 10) {
                std::cout << "    ..." << std::endl;
            }
        }
        
        // Debug prints for outgoing edges
        size_t totalOutgoing = 0;
        for (u64 tgtParty = 0; tgtParty < numParts; tgtParty++) {
            totalOutgoing += outGoingEdgeLists[tgtParty].size();
        }
        std::cout << "[DEBUG] Parsed " << totalOutgoing << " outgoing edges to " 
                  << numParts << " target parties:" << std::endl;
        for (u64 tgtParty = 0; tgtParty < std::min<u64>(10, numParts); tgtParty++) {
            std::cout << "  To Party " << tgtParty << ": " 
                      << outGoingEdgeLists[tgtParty].size() << " edges" << std::endl;
            for (size_t i = 0; i < std::min<size_t>(10, outGoingEdgeLists[tgtParty].size()); i++) {
                auto& edge = outGoingEdgeLists[tgtParty][i];
                std::cout << "    " << edge[0] << " -> " << edge[1] << std::endl;
            }
            if (outGoingEdgeLists[tgtParty].size() > 10) {
                std::cout << "    ..." << std::endl;
            }
        }

        Sh3_Graph_Ours_single_party_demo(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations, workspace_dir, vertexIdList, vertexDataList, incomingEdgeLists, outGoingEdgeLists);
        saveUpdatedVertexData(workspace_dir + "/party_" + std::to_string(pIdx) + "_updated.txt", vertexIdList, vertexDataList);
    }

    print_duration(t_tmp, "ours");
}

// void demo_ours_app(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations)
// {
//     auto t_tmp = std::chrono::high_resolution_clock::now();
// 	// Sh3_Graph_CC_test();

//     // std::vector<std::thread> thrds;
//     // u64 numP = 5;
//     // for (u64 i = 0; i < numP; ++i)
//     //     thrds.emplace_back(std::thread(Sh3_Graph_Ours_single_party, i));

//     // for (u64 i = 0; i < numP; ++i)
//     //     thrds[i].join();

//     Sh3_Graph_Ours_App_single_party(numParts, pIdx, scale, avgDegree, interRatio, alg, iterations);

//     print_duration(t_tmp, "oursApp");
// }

