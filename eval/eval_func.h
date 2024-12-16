#pragma once

#include "aby3-Graph_tests/tests.h"

#include <string>
#include <iostream>

void eval_ours(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations);
void eval_cognn(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations);
void eval_graphsc(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations);
void eval_ours_app(int numParts, int pIdx, int scale, int avgDegree, double interRatio, int alg, int iterations);
