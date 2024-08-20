#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <iostream>

void eval_ours();
void print_duration(std::chrono::_V2::system_clock::time_point t1, std::string tag);
