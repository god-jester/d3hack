#pragma once
#define snprintf nn::util::SNPrintf

namespace nn::util {
__attribute__ ((__format__(__printf__, 3, 4))) auto SNPrintf(char *output, size_t max_size, const char *format, ...) -> u32;
}