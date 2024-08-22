#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <iostream>

void eval_ours(int pIdx);
void eval_cognn(int pIdx);
void eval_graphsc(int pIdx);
void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag);
