/*
 *
 *
 *    decimate.cc
 *
 *    Decimation utility; mostly for testing decimate.h.
 *
 *    Note that this utility does not provide a low-pass filter on
 *    the input stream.  See the notes in decimate.h.
 *
 *    Copyright (C) 2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include "decimate.h"
#include "sf.h"
#include "stype.h"

using namespace KK5JY::DSP;
using namespace std;

int main(int argc, char**argv) {
	if (argc != 4) {
		cerr << endl;
		cerr << "Usage: " << argv[0] << " <in> <out> <rate>" << endl;
		cerr << endl;
		cerr << "       Decimate <in> sampling rate, and write into <out>." << endl;
		cerr << endl;
		cerr << "       <in>   - the intput WAV file" << endl;
		cerr << "       <out>  - the output WAV file" << endl;
		cerr << "       <rate> - the output WAV sample rate (Hz)" << endl;
		cerr << endl;
		cerr << "       Note that sampling rates are arbitrary; the input rate does" << endl;
		cerr << "       not need to be divisible evenly by the output rate.  The" << endl;
		cerr << "       output rate *does* need to be less than the input rate." << endl;
		cerr << endl;
		cerr << "       Also, the program does not apply any filtering before" << endl;
		cerr << "       decimating.  See notes in decimate.h for details." << endl;
		cerr << endl;
		return 1;
	}

	// extract arguments
	std::string in = argv[1];
	std::string out = argv[2];
	int rate = atoi(argv[3]);

	// open WAV files
	SoundFile input(in);
	if (input.rate() <= rate) {
		cerr << "The output rate must be less than the input rate of " << input.rate() << " Hz." << endl;
		return 1;
	}
	SoundFile output(out, rate, input.channels(), SoundFile::major_formats::wav, SoundFile::minor_formats::s16);

	// make the decimator
	DecimatorLI<double, float> deci(input.rate(), output.rate());

	// write
	size_t count = 0;
	size_t samples = 0;
	const size_t win = 128;
	float *buffer = new float[win];
	do {
		count = input.read(buffer, win);
		float *ip = buffer;
		float *op = buffer;
		float sample;
		size_t sz_out = 0;
		for (size_t i = 0; i != count; ++i) {
			if (deci.run(*ip++, sample)) {
				*op++ = sample;
				++sz_out;
			}
		}
		samples += output.write(buffer, sz_out);
	} while (count != 0);

	// DEBUG:
	cerr << "Wrote " << samples << " samples (" << static_cast<double>(samples) / rate << " sec)." << endl;

	// done
	return 0;
}

// EOF
