
#include "eval_func.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
	// eval_ours(std::atoi(argv[1]));
	eval_cognn(std::atoi(argv[1]));
	return 0;
}
