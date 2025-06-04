
#include "demo_func.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	int scheme = std::atoi(argv[1]);
	int num_parts = std::atoi(argv[2]);
	int party_id = std::atoi(argv[3]);
	int scale = std::atoi(argv[4]);
	int avgDegree = std::atoi(argv[5]);
	double interRatio = std::atof(argv[6]);
	int alg = std::atoi(argv[7]);
	int iterations = std::atoi(argv[8]);
	std::string workspace_dir = argv[9];
	std::string command = argv[10];

    #ifdef REMOVE_OGA
        printf("OGA DISABLED\n");
    #else
        printf("OGA ENABLED\n");
    #endif	

    #ifdef REMOVE_OEP
        printf("OEP DISABLED\n");
    #else
        printf("OEP ENABLED\n");
    #endif	

	if (scheme == 0) {
		demo_ours(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations, workspace_dir, command);
	} else if (scheme == 3) {
		// demo_ours_app(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations);
	} else {
		printf("Unexpected scheme!\n");
		exit(-1);
	}
	return 0;
}
