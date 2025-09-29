/*
 *
 *
 *    pipes.h
 *
 *    Convenience wrapper for sending messages to standard output.
 *
 *    Copyright (C) 2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#ifndef __KK5JY_FT8_PIPES_H
#define __KK5JY_FT8_PIPES_H

#include <string>

namespace KK5JY {
	namespace FT8 {
		size_t send_message(const std::string &prefix, const std::string &s);
		size_t send_trace(const std::string &s);
		size_t send_debug(const std::string &s);
		size_t send_info(const std::string &s);
		size_t send_warning(const std::string &s);
		size_t send_error(const std::string &s);
		size_t send_ok(const std::string &s);

		std::string trim_zeroes(const std::string &s);
	}
}

#endif // __KK5JY_FT8_PIPES_H
