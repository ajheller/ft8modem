/*
 *
 *
 *    cores.h
 *
 *    Read the number of CPU cores on Linux.
 *
 *    Copyright (C) 2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#ifndef __KK5JY_FT8_CORES_H
#define __KK5JY_FT8_CORES_H

#include <fstream>
#include <cctype>
#include "stype.h"


namespace KK5JY {
	namespace DSP {
		//
		//  cpu_cores() - return the number of physical CPU cores (not counting HT) on the system
		//
		inline size_t cpu_cores() {
			// first, try to find the "^cpu cores" string
			{
				const std::string pattern("cpu cores");
				std::ifstream cpuinfo("/proc/cpuinfo");
				while (cpuinfo) {
					std::string line;
					if ( ! std::getline(cpuinfo, line))
						break;
					if (line.substr(0, pattern.size()) == pattern) {
						std::string::size_type index = line.find(':');
						if (index != std::string::npos) {
							++index;
							std::string token = my::strip(line.substr(index));
							return stoi(token); // convert to integer
						}
					}
				}
			}

			// next, count the ^processor lines
			{
				const std::string pattern("processor");
				std::ifstream cpuinfo("/proc/cpuinfo");
				size_t count = 0;
				while (cpuinfo) {
					std::string line;
					if ( ! std::getline(cpuinfo, line))
						break;
					if (line.size() >= pattern.size() + 1) {
						bool match1 = line.substr(0, pattern.size()) == pattern;
						bool match2 = isspace(line[pattern.size()]);
						if (match1 && match2) {
							++count;
						}
					}
				}
				if (count)
					return count;
			}

			return 0; // fail
		}
	}
}

#endif // __KK5JY_FT8_CORES_H
