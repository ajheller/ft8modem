/*
 *
 *
 *    pipes.cc
 *
 *    Convenience wrapper for sending messages to standard output.
 *
 *    Copyright (C) 2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <string>
#include <iostream>

#include "stype.h"
#include "locker.h"
#include "pipes.h"

namespace KK5JY {
	namespace FT8 {
		// global mutex
		my::mutex io_lock;

		//
		//  common code - 'prefix' can be empty, but is still required
		//
		size_t send_message(const std::string &prefix, const std::string &raw_message) {
			// sanity check
			if ( ! raw_message.size())
				return 0;

			// build the total string to send
			std::string clean_message;
			clean_message.reserve(raw_message.size() + 16);
			if (prefix.size()) // prepend prefix if provided
				clean_message += prefix + ": ";
			clean_message += my::trim(raw_message);

			{
				// this block is a critical section
				my::locker lock(io_lock);

				// send the message and flush the stream
				std::cout << clean_message << std::endl;
				std::cout.flush();
			}

			// return the size of the message + the newline
			return clean_message.size() + 1;
		}


		//
		//  send_* methods-
		//
		//  Sends a single line message, with the global mutex locked,
		//  and terminated with a single newline.
		//

		// send a message with no prefix
		size_t send_raw(const std::string &s) {
			return send_message("", s);
		}

		// trace
		size_t send_trace(const std::string &s) {
			return send_message("TRACE", s);
		}

		// debug
		size_t send_debug(const std::string &s) {
			return send_message("DEBUG", s);
		}

		// info
		size_t send_info(const std::string &s) {
			return send_message("INFO", s);
		}

		// warning
		size_t send_warning(const std::string &s) {
			return send_message("WARN", s);
		}

		// error
		size_t send_error(const std::string &s) {
			return send_message("ERR", s);
		}

		// ok
		size_t send_ok(const std::string &s) {
			return send_message("OK", s);
		}


		//
		//  trim_zeroes(...) - clean up string from floating point conversion
		//
		std::string trim_zeroes(const std::string &s) {
			if (s.find('e') != std::string::npos || s.find('E') != std::string::npos)
				return s;

			// find the decimal point, if it exists
			std::string result(s);
			size_t point = result.find('.');
			if (point == std::string::npos)
				point = 0;
			else
				point = point + 1;

			// trim trailing '0's
			std::string tail(result.substr(point));
			while (tail.size() && (tail[tail.size() -1] == '0')) {
				result = result.substr(0, result.size() - 1);
				tail = result.substr(point);
			}

			// trim trailing '.'
			if (result.size() && (result[result.size() - 1] == '.')) {
				result = result.substr(0, result.size() - 1);
			}

			return result;
		}
	}
}

// EOF: pipes.cc
