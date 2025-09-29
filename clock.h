/*
 *
 *
 *    clock.h
 *
 *    Wall clock helper methods.
 *
 *    Copyright (C) 2023-2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#ifndef __KK5JY_FT8_CLOCK_H
#define __KK5JY_FT8_CLOCK_H

#include <string>
#include <sys/time.h>
#include <math.h>

namespace KK5JY {
	namespace FT8 {
		//
		//  abstime() - return absolute clock in seconds
		//
		inline double abstime() {
			// read the clock
			struct timeval tv;
			struct timezone tz;
			size_t tries = 0;
			const size_t limit = 20;
			while (tries++ < limit && ::gettimeofday(&tv, &tz) != 0)
				::usleep(200); // usec
			if (tries >= limit)
				return 0.0;

			double result = static_cast<double>(tv.tv_sec);
			result += static_cast<double>(tv.tv_usec) / 1000000.0;
			return result;
		}


		//
		//  timestring() - return current clock time as a string (Z)
		//
		inline std::string timestring(time_t t = 0) {
			short msec = -1;
			if (t == 0) {
				// read the clock
				struct timeval tv;
				struct timezone tz;
				::gettimeofday(&tv, &tz);
				t = tv.tv_sec;
				msec = tv.tv_usec / 1000;
			}

			tm now;
			gmtime_r(&t, &now);
			char buffer[16];
			if (msec >= 0)
				snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", now.tm_hour, now.tm_min, now.tm_sec, msec);
			else
				snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
			return std::string(buffer);
		}


		//
		//  class FrameClock
		//
		class FrameClock {
			private:
				// a known 'zero' seconds on the clock
				time_t m_epoch;
				
			public:
				// ctor
				FrameClock();

				// return the number of seconds in the current minute
				//    with microsecond precision
				double seconds(double mod, double fudge = 0.0) const volatile;

				// return the correction
				int offset() const { return m_epoch; }
		};


		//
		//  FrameClock() ctor
		//
		inline FrameClock::FrameClock() {
			// read the clock
			struct timeval tv;
			struct timezone tz;
			::gettimeofday(&tv, &tz);

			// now convert that into HMS
			time_t time = tv.tv_sec;
			tm *hms = gmtime(&time);
			
			// figure out where 'zero' is relative to 'tv.tv_sec'
			tv.tv_sec -= hms->tm_sec;
			if (hms->tm_min % 2)
				tv.tv_sec -= 60;
			m_epoch = tv.tv_sec;
		}


		//
		//  FrameClock::seconds()
		//
		inline double FrameClock::seconds(double mod, double fudge) const volatile {
			// read the clock
			double now_sec = abstime() + fudge;
			double result = fmod(now_sec - m_epoch, mod);
			return result;
		}
	}
}

#endif // __KK5JY_FT8_CLOCK_H
