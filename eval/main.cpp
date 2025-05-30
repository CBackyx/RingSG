
#include "eval_func.h"

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
		eval_ours(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations);
	} else if (scheme == 1) {
		eval_cognn(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations);
	} else if (scheme == 2) {
		eval_graphsc(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations);
	} else if (scheme == 3) {
		eval_ours_app(num_parts, party_id, scale, avgDegree, interRatio, alg, iterations);
	} else {
		printf("Unexpected scheme!\n");
		exit(-1);
	}
	return 0;
}
