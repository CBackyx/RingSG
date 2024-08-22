
#include "eval_func.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	int scheme = std::atoi(argv[1]);
	int num_parts = std::atoi(argv[2]);
	int party_id = std::atoi(argv[3]);
	int scale = std::atoi(argv[4]);
	int alg = std::atoi(argv[5]);
	int iterations = std::atoi(argv[6]);

	if (scheme == 0) {
		eval_ours(num_parts, party_id, scale, alg, iterations);
	} else if (scheme == 1) {
		eval_cognn(num_parts, party_id, scale, alg, iterations);
	} else if (scheme == 2) {
		eval_graphsc(num_parts, party_id, scale, alg, iterations);
	} else {
		printf("Unexpected scheme!\n");
		exit(-1);
	}
	return 0;
}
