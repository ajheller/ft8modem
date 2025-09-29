/*
 *
 *
 *    encode.h
 *
 *    Conversion from text to keying symbols.
 *
 *    Copyright (C) 2023-2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <string>

namespace KK5JY {
	namespace FT8 {
		//
		//  encode(mode, text) - return the keying symbols for a message
		//
		//  The 'mode' is one of { "ft8", "ft4", "jt9", "jt65", "wspr" }
		//  The 'text' is a valid message for the mode; this function doesn't
		//  validate the text; that should be done prior to calling.
		//
		//  The result is a single string containing keying symbols, directly
		//  usable by MFSK::Modulator<T>::transmit(...).  See mfsk.h.
		//
		std::string encode(const std::string &mode, const std::string &txt);
	}
}

// EOF
