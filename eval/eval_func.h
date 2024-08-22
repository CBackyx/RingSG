#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <iostream>

void eval_ours(int numParts, int pIdx, int scale, int alg, int iterations);
void eval_cognn(int numParts, int pIdx, int scale, int alg, int iterations);
void eval_graphsc(int numParts, int pIdx, int scale, int alg, int iterations);
void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag);
