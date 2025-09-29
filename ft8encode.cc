/*
 *
 *
 *    ft8encode.cc
 *
 *    WAV encoder for FT8.
 *
 *    Copyright (C) 2023-2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <time.h>

#include "encode.h"
#include "mfsk.h"
#include "sf.h"
#include "stype.h"
#include "presets.h"

using namespace KK5JY::DSP::MFSK;
using namespace KK5JY::FT8;
using namespace std;


//
//  get_noise(level)
//
static float get_noise(float level) {
	return level * ((random() / static_cast<float>(RAND_MAX)) - 0.5);
}


//
//  main(...)
//
int main(int argc, char**argv) {
	if (argc < 6) {
		cerr << endl;
		cerr << "Usage: " << argv[0] << " <mode> <fs> <f0> <wav> '<txt>' [<kwargs>]" << endl;
		cerr << endl;
		cerr << "       Generate an FT8 or FT4 message into a WAV file." << endl;
		cerr << endl;
		cerr << "       <mode> is one of { FT4, FT8, JT9, JT65, WSPR }" << endl;
		cerr << "       <fs> is the sampling frequency of the WAV file to create" << endl;
		cerr << "       <f0> is the lowest MFSK frequency to generate" << endl;
		cerr << "       <wav> is the name of the WAV file to generate" << endl;
		cerr << "       <txt> is the message text, and should be quoted" << endl;
		cerr << endl;
		cerr << "       Keyword args can include (mostly for testing):" << endl;
		cerr << "       'win' is the sample window size" << endl;
		cerr << "       'shape' selects the shaper in { es, cosine }" << endl;
		cerr << "       'bench' selects bench-testing mode" << endl;
		cerr << "       'k' adjusts the default MFSK filter preset" << endl;
		cerr << endl;
		return 1;
	}

	// extract arguments
	std::string mode = argv[1];
	std::string wav = argv[4];
	std::string txt = argv[5];
	std::string shaper;
	double rate = atof(argv[2]);
	double f0 = atof(argv[3]);
	double bps, shift, k_fc = 1.0;
	size_t win = 128;
	bool bench = false; // bench-testing flag

	// keyword arguments
	if (argc >= 7) {
		for (short i = 6; i != argc; ++i) {
			std::string arg(argv[i]);
			if (arg.size() > 4 && arg.substr(0, 4) == "win=") {
				win = stoi(arg.substr(4));
				if (win <= 0) {
					cerr << "Window must be > 0" << endl;
					return 1;
				}
				std::cerr << "Set win = " << win << std::endl;
			} else if (arg.size() > 2 && arg.substr(0, 2) == "k=") {
				k_fc = stod(arg.substr(2));
				if (k_fc <= 0) {
					cerr << "k must be > 0" << endl;
					return 1;
				}
				std::cerr << "Set k = " << k_fc << std::endl;
			} else if (arg.size() > 6 && arg.substr(0, 6) == "bench=") {
				bench = arg[6] != '0';
			} else if (arg.size() > 6 && arg.substr(0, 6) == "shape=") {
				shaper = arg.substr(6);
				std::cerr << "Set shaper to " << shaper << std::endl;
			}
		}
	}

	std::string rmode = my::toLower(mode);

	if (rmode == "ft8") {
		bps = bps_ft8;
		shift = bps;
	} else if (rmode == "ft4") {
		bps = bps_ft4;
		shift = bps;
	} else if (rmode == "jt65") {
		bps = bps_jt65;
		shift = bps;
	} else if (rmode == "jt9") {
		bps = bps_jt9;
		shift = bps;
	} else if (rmode == "wspr") {
		bps = bps_wspr;
		shift = bps;
	} else {
		cerr << "Invalid mode." << std::endl;
		return 1;
	}

	// open a new WAV file
	SoundFile output(wav, rate, 1, SoundFile::major_formats::wav, SoundFile::minor_formats::s16);

	// DEBUG:
	//cerr << "Using rate = " << rate << "; bps = " << bps << "; shift = " << shift << "; txt = " << txt << endl;

	// create a new modulator
	IFilter<float> *kf = 0;
	if (shaper == "cosine")
		kf = GetShaperCosine<float>(rmode, rate, k_fc);
	else if (shaper == "es")
		kf = GetShaperRC<float>(rmode, rate, k_fc);
	else if ( ! shaper.empty())
		throw new std::runtime_error("Invalid shaper provided");
	else
		kf = GetPreferredShaper<float>(rmode, rate, k_fc);
	KK5JY::DSP::MFSK::Modulator<float> mfsk(rate, f0, bps, shift, kf);
	mfsk.setVolume(0.5);

	// encode
	std::string message = KK5JY::FT8::encode(mode, txt);
	mfsk.transmit(message, f0);

	// write
	size_t count = 0;
	size_t samples = 0;
	float *buffer = new float[win];
	size_t target = 0;
	float noise = 0;

	// when bench-testing...
	if (bench) {
		// DEBUG:
		cout << "DEBUG: adding noise for bench-testing mode" << endl;

		// seed the PRNG with the clock
		srandom(time(0));

		// for JT65, add about -14dB of noise; for some reason the decoder needs it
		if (rmode == "jt65") {
			noise = 0.2;
		}

		// for WSPR, also add some noise
		else if (rmode == "wspr") {
			noise = 0.02;
		}

		// for others, add about -40dB of noise
		else {
			noise = 0.01;
		}
	}

	// pointers for walking the data
	float * const ep = buffer + win;
	float *       zp = buffer;

	// add more lead-in for certain modes
	if (rmode == "jt65" || rmode == "wspr" || rmode == "jt9") {
		target = 0.87 * rate;
		for (size_t i = 0; i != win; ++i) {
			buffer[i] = 0;
		}

		while (samples < target) {
			// noise it up if needed
			if (noise) {
				zp = buffer;
				while (zp != ep)
					*zp++ = get_noise(noise);
			}

			// write the buffer
			count = output.write(buffer, win);
			if (count > 0)
				samples += count;
		}
	}

	// write the tones
	do {
		if ((count = mfsk.read(buffer, win)) != 0) {
			if (noise) {
				zp = buffer;
				while (zp != ep) {
					*zp = ((1.0 - noise) * *zp) + get_noise(noise);
					++zp;
				}
			}
			size_t ct2 = output.write(buffer, count);
			if (count != ct2)
				throw runtime_error("Output mismatch");

			samples += count;
		}
	} while (count != 0);

	// zero out the unused part
	zp = buffer;
	while (zp != ep) {
		*zp++ = noise ? get_noise(noise) : 0;
	}

	// add half second of silence at the end
	target = samples + (rate / 2);

	// ...but when bench-testing...
	if (bench) {
		// ...use more for certain modes
		if (rmode == "jt65" || rmode == "jt9") {
			target = samples + (12 * rate);
		} else if (rmode == "wspr") {
			target = samples + (8.4 * rate);
		}
	}

	// write out the trailing silence
	do {
		if (noise) {
			zp = buffer;
			while (zp != ep) 
				*zp++ = get_noise(noise);
		}
		samples += output.write(buffer, win);
	} while (samples < target);

	// DEBUG:
	cerr << "Wrote " << samples << " samples (" << static_cast<double>(samples) / rate << " sec)." << endl;

	// done
	return 0;
}

// EOF
