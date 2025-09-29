/*
 *
 *
 *    ft8modem.cc
 *
 *    Software modem for FT8, etc.
 *
 *    Copyright (C) 2023-2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <strings.h>
#include <pwd.h>

// for socket/UDP support
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "snddev.h"
#include "level.h"
#include "pipes.h"
#include "version.h"

using namespace std;
using namespace KK5JY::FT8;
using namespace KK5JY::DSP;
using namespace KK5JY::DSP::MFSK;

// size of I/O buffers to the sound device
const unsigned WindowSize = 256;

// lowest audio frequency that the user can request for transmitting
const unsigned af_low = 1;

// highest audio frequency that the user can request for transmitting
const unsigned af_high = 3100;

// when using UDP, this is the maximum time to wait for new audio data (usec)
//   if this time is exceeded, small bits of silence are generated to fill
//   in the gap, and keep the sound handler timers running
const suseconds_t max_idle = 200000; // 200 msec

// this is the select timeout delay (usec)
const suseconds_t select_timeout = 50000; // 50 msec


//
//  usage()
//
static void usage(const string &s) {
	cout << endl;
	cout << "Usage: " << s << " [options] <mode> [<device>]" << endl;
	cout << endl;
	cout << "    Starts a software modem for FT8, FT4, and similar HF digital modes." << endl;
	cout << endl;
	cout << "       <mode> is one of { FT4, FT8, JT9, JT65, WSPR }" << endl;
	cout << "       <device> is the sound card ID, and may be one of these devices:" << endl;
	cout << endl;
	// scan the devices for promising candidates
	{
		RtAudio adc;
		typedef std::vector<unsigned int> idlist_t;
		idlist_t ids = adc.getDeviceIds();
		for (idlist_t::const_iterator i = ids.begin(); i != ids.end(); ++i) {
			RtAudio::DeviceInfo info = adc.getDeviceInfo(*i);
			size_t best_rate = 0;

			// skip devices with no inputs
			if (info.inputChannels == 0)
				continue;

			for (size_t r = 0; r != info.sampleRates.size(); ++r) {
				size_t rate = info.sampleRates[r];
				if (rate > best_rate && rate <= 48000)
					best_rate = rate;
			}

			if (best_rate >= 12000) {
				std::cout
					<< "          + ID = " << *i
					<< ": \"" << info.name << "\""
					<< ", best rate = " << best_rate << "\n";
			}
		}
	}
	cout << endl;
	cout << "    If your sound device is not shown above, it is likely because it" << endl;
	cout << "    reports no inputs to the operating system, or it is already in" << endl;
	cout << "    use by another program." << endl;
	cout << endl;
	cout << "    Device ID can also be a string.  This string is compared against" << endl;
	cout << "    each device name, and if the string is found within a device name," << endl;
	cout << "    that device is used." << endl;
	cout << endl;
	cout << "    Device ID can also have the form 'udp:port', where 'port' is a UDP" << endl;
	cout << "    port on the local machine.  In this case, the modem does not open" << endl;
	cout << "    a sound device, but rather reads raw sound data from a UDP socket." << endl;
	cout << "    This is only for use with the -u option of ft8sdr, for SDR receivers," << endl;
	cout << "    as the protocol is specific to the ft8modem." << endl;
	cout << endl;
	cout << "    The device ID will be persisted to .ft8modemrc, and that value" << endl;
	cout << "    will be used next time the modem is run, if not provided.  As" << endl;
	cout << "    such, it is optional, but must be provided at least once.  The" << endl;
	cout << "    command line value, if provided, takes precedence." << endl;
	cout << endl;
	cout << "    The 'options' can include the following; each option will have a" << endl;
	cout << "    reasonable default value if omitted:" << endl;
	cout << "         -i <msec>     set transmit lead-in time, in milliseconds" << endl;
	cout << "         -e <name>     set temp-file instance folder name" << endl;
	cout << "         -t <path>     set full path to the temp directory" << endl;
	cout << "         -T <threads>  set maximum number of decoder threads" << endl;
	cout << "         -j <decoder>  set path to 'jt9' program to use" << endl;
	cout << "         -r <rate>     force sample rate of the sound device to <rate> Hz" << endl;
	cout << "         -f <msec>     apply a frame clock offset; mostly for SDRs" << endl;
	cout << "         -w <msec>     adjust the trailing window timing; mostly for SDRs" << endl;
	cout << "         -y            disable the decimation filter for rate > 12kHz" << endl;
	cout << endl;
	cout << "    When using the -j option, the program can be anything, but it must" << endl;
	cout << "    accept the same arguments as jt9(1), even if it ignores them." << endl;
	cout << endl;
	cout << "    When using the -r option, the rate must be at least 12000 Hz, since" << endl;
	cout << "    jt9(1) requires exactly 12000 Hz.  The -r option is required when" << endl;
	cout << "    using a UDP audio source." << endl;
	cout << endl;
	cout << "    The -e and -t options cannot be used together." << endl;
	cout << endl;
	cout << "    These options are mostly for testing (use with caution):" << endl;
	cout << "         -l  also run decoder on transmitted audio" << endl;
	cout << "         -k  don't delete WAV files after decoding" << endl;
	cout << "         -m  specify the modulator's data filter" << endl;
	cout << endl;
	cout.flush();
}


//
//  get_rc_path()
//
static string get_rc_path() {
	passwd *pw = getpwuid(getuid());
	if ( ! pw) {
		return string();
	}
	string path = pw->pw_dir;
	path += "/.ft8modemrc";
	return path;
}


//
//  clean_db(f) - format dB figure to one decimal place
//
std::string clean_db(float db) {
	stringstream clean;
	clean << setprecision(1) << fixed << db;
	return clean.str();
}


//
//  isdigits(s) - returns true iff the entire string is numeric
//
static bool isdigits(const string &s) {
	bool result = false;
	for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
		result = true;
		if ( ! isdigit(*i))
			return false;
	}
	return result;
}


//
//  findDeviceByName(s);
//
static int findDeviceByName(const string &name) {
	const string key(my::toUpper(name));
	RtAudio adc; //(RtAudio::LINUX_ALSA);

	// Scan through devices for various capabilities
	typedef std::vector<unsigned int> idlist_t;
	idlist_t ids = adc.getDeviceIds();
	for (idlist_t::const_iterator i = ids.begin(); i != ids.end(); ++i) {
		RtAudio::DeviceInfo info = adc.getDeviceInfo(*i);
		string haystack = my::toUpper(info.name);
		if (haystack.find(key) != string::npos)
			return (*i);
	}
	return -1;
}


//
//  parse the frequency/slot token
//
static TimeSlots getFrequencyAndSlot(const std::string &token, size_t &out_freq) {
	if (token.empty())
		throw runtime_error("Invalid frequency/slot provided.");

	// try to read the time slot
	TimeSlots eo = NextSlot;
	const char eoc = ::toupper(token[token.size() - 1]);
	const bool hasSlot = ! ::isdigit(eoc);
	const std::string::size_type eof = hasSlot ? token.size() - 1 : token.size();
	const std::string freq = token.substr(0, eof);

	// validate the frequency first
	if ( ! my::isDigit(freq)) {
		throw runtime_error("Invalid frequency provided: " + freq);
	}

	// parse the slot if it is given
	if (hasSlot) {
		switch(eoc) {
			case 'E':
				eo = EvenSlot;
				break;
			case 'O':
				eo = OddSlot;
				break;
			default:
				throw runtime_error("Invalid slot specified; must be 'E' or 'O'.");
		}
	}

	// read the frequency, and set the output parameter
	try {
		out_freq = stoi(freq);
	} catch (...) {
		throw runtime_error("Invalid frequency provided: " + freq);
	}

	// return the slot
	return eo;
}


//
//  main()
//
int main(int argc, char**argv) {
	// read the '.ft8modemrc' file
	float level = 0;
	int depth = 2;
	string devname;

	// keep maps with timing adjustment values; the mode isn't parsed
	//   until later, so store them all until we know which to use
	typedef std::map<std::string, int> timings_t;
	timings_t leads;  // lead-in values
	timings_t trails; // window adjustments

	// read the '.ft8modemrc' file
	{
		ifstream rc(get_rc_path());

		// read the whole file into 'buffer'
		string buffer;
		{
			char iobuf[128];
			while (rc) {
				streamsize ct = rc.readsome(iobuf, sizeof(iobuf) - 1);
				if (ct == 0)
					break;
				if (ct > 0) {
					iobuf[ct] = 0;
					buffer += iobuf;
				}
			}
		}

		// now process line by line
		while (buffer.size()) {
			// process out the next line and adjust the buffer
			string line;
			{
				size_t idx = buffer.find('\n');
				if (idx == string::npos) {
					line = buffer;
					buffer.clear();
				} else {
					line = buffer.substr(0, idx);
					buffer = buffer.substr(idx + 1);
				}
			}

			// process the line that was extracted
			if (line.size()) {
				// skip empty lines and comments
				line = my::trim(line);
				if (line.size() == 0 || line[0] == '#')
					continue;

				// get the key-value pair
				size_t idx = line.find(' ');
				if (idx == string::npos)
					continue; // give up on this line
				string key = my::toUpper(my::trim(line.substr(0, idx)));
				string value = my::toUpper(my::trim(line.substr(idx + 1)));

				// read in the setting
				if (key == "LEVEL") {
					float newval = stof(value);
					if (newval <= 0 && newval >= -80)
						level = newval;
				} else if (key == "DEPTH") {
					int newval = stoi(value);
					if (newval > 0)
						depth = newval;
				} else if ((key.size() > 7) && (key.substr(0, 7) == "LEADIN.")) {
					int newval = stoi(value);
					if (newval >= 0)
						leads[my::toUpper(key.substr(7))] = newval;
				} else if ((key.size() > 6) && (key.substr(0, 6) == "TRAIL.")) {
					int newval = stoi(value);
					if (newval >= 0)
						trails[my::toUpper(key.substr(6))] = newval;
				} else if (key == "DEVICE") {
					devname = value;
				}
			}
		}
	}

	// read command-line options
	typedef std::vector<std::string> optlist_t;
	optlist_t nonopts;
	std::string tempPath;
	std::string sthreads;
	std::string jt9;
	std::string modfilter;
	std::string ext;
	unsigned int rate = 0;
	int lead = -1;  // transmit lead-in time
	int trail = -1; // receive window trail extension
	int fudge = 0;  // frame clock offset
	bool keep = false; // flag to keep WAV after each decode
	bool loop = false; // flag to decode transmitted audio, too
	bool no_filter = false; // flag to disable decimation filter
	{
		int opt;
		while ((opt = getopt(argc, argv, "e:i:j:t:T:f:hklm:r:v::yw:")) != -1) {
			switch (opt) {
				case 'e':
					ext = optarg;
					break;
				case 'i':
					lead = atoi(optarg);
					break;
				case 'f':
					fudge = atoi(optarg);
					break;
				case 'l':
					loop = true;
					break;
				case 'j':
					jt9 = optarg;
					break;
				case 'k':
					keep = true;
					break;
				case 'r':
					rate = atoi(optarg);
					break;
				case 't':
					tempPath = optarg;
					break;
				case 'm':
					modfilter = my::toLower(optarg);
					break;
				case 'T':
					sthreads = optarg;
					break;
				case 'v':
					cout << "ft8modem version " << GetModemVersion() << endl;
					return 0;
				case 'y':
					no_filter = true;
					break;
				case 'w':
					trail = atoi(optarg);
					break;
				case 'h':
				default:
					usage(argv[0]);
					return 1;
			}
		}

		// collect non-option arguments
		while (optind < argc) {
			nonopts.push_back(argv[optind++]);
		}

		// make sure non-option list is reasonable size
		if (nonopts.empty() || nonopts.size() > 2) {
			usage(argv[0]);
			return 1;
		}
	}

	// sanity checks
	if (ext.size() && tempPath.size()) {
		cerr << "The -e and -t options cannot be used together." << endl;
		return 1;
	}

	// read the MODE
	string mode = my::toUpper(nonopts[0]);

	// apply lead-in if user didn't supply one
	if ((lead < 0) && (leads.find(mode) != leads.end())) {
		lead = leads[mode];
	}

	// apply trail if user didn't supply one
	if ((trail < 0) && (trails.find(mode) != trails.end())) {
		trail = trails[mode];
	}

	// read the DEVICE NAME
	if (nonopts.size() == 2)
		devname = nonopts[1];
	
	// use numeric if that's what was given
	int devid = -1;
	int port = 0;
	if (isdigits(devname)) {
		devid = stoi(devname);
	} else if (devname.size() > 4 && my::toLower(devname.substr(0, 4)) == "udp:") {
		port = stoi(devname.substr(4));
		if (port <= 0 || port > 65535) {
			send_error("Port must be between 1 and 65535");
			return 1;
		}
	} else {
		devid = findDeviceByName(devname);
	}
	
	// make sure there is a valid ID
	if (devid < 0 && port <= 0) {
		usage(argv[0]);
		return 1;
	}

	// validate mode (must be FT8, FT4, JT65, or JT9)
	if (mode != "FT8" && mode != "FT4" && mode != "JT65" && mode != "JT9" && mode != "WSPR") {
		send_error("Invalid mode requested: " + mode);
		return 1;
	}

	// pick the shaper
	ShaperTypes shaper = ShaperTypes::DefaultShaper;
	if (modfilter == "es") {
		send_debug("Modulator bit-shaper type is ExponentialSmoother");
		shaper = ShaperTypes::ExponentialSmoother;
	} else if (modfilter == "cosine") {
		send_debug("Modulator bit-shaper type is RaisedCosine");
		shaper = ShaperTypes::RaisedCosine;
	} else if ( ! modfilter.empty()) {
		send_error("Invalid shaper type '" + modfilter + "' - valid types in { es, cosine }");
		return 1;
	}

	// find the best sampling rate
	if (rate == 0) {
		if (devid > 0) {
			const unsigned int hi_rate = 48000; // 48kHz is enough, and it cleanly decimates to 12kHz
			RtAudio audio;
			RtAudio::DeviceInfo info = audio.getDeviceInfo(devid);
			for (std::vector<unsigned int>::const_iterator i = info.sampleRates.begin(); i != info.sampleRates.end(); ++i) {
				if (*i > rate && *i <= hi_rate) {
					rate = *i;
				}
			}

			// if no rate found...
			if (rate == 0) {
				send_error("Could not find a usable sample rate from " + std::to_string(info.sampleRates.size()) + " candidates.");
				return 1;
			}

			// talk about what was decided
			send_debug("Sound device is '" + info.name + "'" + " - ID = " + std::to_string(devid));
		} else { // UDP mode
			if (port > 0) {
				send_error("UDP audio source requires option '-r' to specify sample rate");
				return 1;
			}
		}
	}
	send_debug("Sound sampling rate is " + std::to_string(rate) + " Hz.");
	cout.flush();

	// cap level to 100%
	if (level > 0)
		level = 0;
 
	// UDP setup
	int sock = 0;
	if (devid < 0 && port > 0) {
		// use zero ID to mean UDP port
		devid = 0;

		// build a UDP socket
		sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock <= 0) {
			send_error("Could not allocate UDP audio socket");
			return 1;
		}

		// bind to loopback
		struct sockaddr_in bindaddr;
		::memset(&bindaddr, 0, sizeof(bindaddr)); // shouldn't be needed for C++, but...
		bindaddr.sin_family = AF_INET;
		bindaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		bindaddr.sin_port = htons(port);
		int ok = ::bind(sock, reinterpret_cast<const struct sockaddr*>(&bindaddr), sizeof(bindaddr));
		if (ok < 0) {
			send_error("Could not bind UDP port " + to_string(port));
			return 1;
		}

		// if no extension given, use the UDP port
		if (ext.empty()) {
			ext = "udp-" + std::to_string(port);
		}
	}

	// if no extension given, use the device ID
	if (ext.empty()) {
		ext = "audio-" + std::to_string(devid);
	}

	// initialize sound card object, even if used only with UDP
	ModemSoundDevice audio(mode, devid, rate, WindowSize, ext);
	audio.setVolume(linear(level));
	audio.setDepth(depth);
	audio.setShaper(shaper);
	audio.setMonitor(loop);
	if (no_filter)
		audio.setFilter(0); // disables the decimation filter
	if ( ! tempPath.empty())
		audio.setTemp(tempPath);
	if ( ! sthreads.empty()) {
		int threads = stoi(sthreads);
		if (threads > 0)
			audio.setThreads(threads);
	}
	if ( ! jt9.empty())
		audio.setDecoder(jt9);
	if (keep)
		audio.setKeep(true);
	if (lead >= 0) // the user lead-in is in msec, so convert to samples
		audio.setLead((rate * lead) / 1000);
	if (trail > 0)
		audio.setTrail((double)(trail) / 1000);
	if (fudge != 0)
		audio.setFudge((double)(fudge) / 1000);

	// start the sound card recording
	if ( ! audio.start()) {
		send_error("Could not start recording on audio device.");
		return 1;
	}

	// DEBUG:
	send_debug("Output level is " + clean_db(level) + " dB");
	send_debug("Decoding depth is " + std::to_string(depth));
	send_debug("Lead-in is " + to_string(audio.getLead()) + " samples (" +
		to_string(static_cast<int>((1000 * audio.getLead())) / rate) + " msec)");
	if (trail > 0)
		send_debug("Trailing window adjustment is " + std::to_string(trail) + " msec");
	if (fudge != 0)
		send_debug("Frame clock offset is " + std::to_string(fudge) + " msec");

	// report the mode to the caller
	send_message("MODE", mode);

	// allocate socket address for client
	struct sockaddr_in client;
	::memset(&client, 0, sizeof(client)); // shouldn't be needed for C++, but...
	int lastport = 0;

	// read transmit messages
	string msg = "";
	fd_set rs;
	timeval tv;
	char iobuffer[16];     // standard input ingress buffer
	bool active = false;   // reports when the sound source goes active
	uint8_t sockbuf[1024]; // the socket ingress buffer
	size_t quiet = 0;      // watches for breaks in the audio data
	int32_t nextseq = -1;  // tracks the sequence number
	size_t lastwin = 0;
	while (true) {
		// watch for stdin
		FD_ZERO(&rs);
		FD_SET(0, &rs); // stdin
		if (sock > 0)
			FD_SET(sock, &rs); // socket (if enabled)
		tv.tv_sec = 0;
		tv.tv_usec = select_timeout;
		int ct = ::select(sock + 1, &rs, 0, 0, &tv);

		// DEBUG: notify when sound card goes active the first time
		if ( ! active) {
			active = audio.isActive();
			if (active)
				send_debug("Sound callback is active.");
		}

		// process decoder messages and sound card mode changes
		audio.run();

		// handle select(...) errors
		if (ct < 0) {
			send_warning("select(...) returned -1, errno = " + to_string(errno));
			continue;
		}

		// UDP: if socket data, handle it
		if (sock > 0) {
			// data waiting?
			if (FD_ISSET(sock, &rs)) {
				// set up the incoming address structure
				sockaddr * addrptr = reinterpret_cast<sockaddr*>(&client);
				socklen_t addrlen = sizeof(client);

				// do the receive call
				ct = ::recvfrom(sock, sockbuf, sizeof(sockbuf), 0, addrptr, &addrlen);

				// check the port number, to see if it has changed
				if (lastport && client.sin_port != lastport) {
					send_warning("Sender port number changed; audio may be corrupt or missing");
				}

				// save the most recent port number
				lastport = client.sin_port;

				// check the length
				if ((ct - 2) % 4) {
					send_warning("Data payload was " + to_string(ct - 2) + "bytes, but should be divisible by four (4)");
				}

				// extract the sequence field
				uint8_t *sp = sockbuf + (ct - 2);
				int32_t seq = *sp++;
				seq |= (*sp << 8);

				// check the sequence number
				if ((nextseq > 0) && (nextseq != seq)) {
					send_warning(
						"Data sequence was " + to_string(seq) +
						", but expected " + to_string(nextseq) +
						"; audio may be corrupt or missing");
				}

				// compute the next expected sequence number
				nextseq = (seq + 1) & 0xFFFF; // 16-bit unsigned value

				// feed the 'audio' object
				audio.udp_audio(sockbuf, ct - 2);
				lastwin = ct - 2;

				// reset the quiet detection
				quiet = 0;
			} else {
				// if the UDP sender quits, keep the sound handler alive by sending
				//    some silence for it to process; this makes sure timers, etc.,
				//    keep running even if the audio sender goes down...
				suseconds_t elapsed = select_timeout - tv.tv_usec;
				quiet += elapsed;
				if (quiet >= max_idle) {
					// reset the quiet detection
					quiet = 0;

					// send last UDP size, if available, or ~5ms @48k otherwise
					size_t quiet_time = lastwin ? lastwin : 256;

					// generate silence into the socket buffer and send
					bzero(sockbuf, sizeof(sockbuf));
					audio.udp_audio(sockbuf, quiet_time);
				}
			}
		}

		// if no stdin data, skip the rest
		if ( ! FD_ISSET(0, &rs)) {
			continue;
		}

		// read from stdin
		ct = ::read(0, iobuffer, sizeof(iobuffer));

		// if EOF, close the program
		if (ct <= 0)
			goto do_exit;

		// process data from stdin
		for (int i = 0; i != ct; ++i) {
			char ch = iobuffer[i];

			// drop non-digits
			if (isalnum(ch) || ch == ' ' || ch == '.' || ch == '-' || ch == '+') {
				msg += ch;
			}

			// terminate line
			if (ch == '\n') {
				// uppercase everything
				msg = my::strip(my::toUpper(msg));

				if (msg == "STOP") {
					if (audio.cancelTransmit())
						send_warning("Cancel transmit");
					msg.clear(); // this only clears the local command buffer
					continue;
				} else if (msg == "LEVEL") {
					send_message("LEVEL", clean_db(decibels(audio.getVolume())));
					msg.clear();
					continue;
				} else if (msg == "DEPTH") {
					send_message("DEPTH", std::to_string(audio.getDepth()));
					msg.clear();
					continue;
				} else if (msg == "LEADIN") {
					send_message("LEADIN", std::to_string((1000 * audio.getLead()) / rate));
					msg.clear();
					continue;
				} else if (msg == "PURGE") {
					audio.purge(); // tell the audio device to discard next round of decodes
					msg.clear();
					continue;
				} else if (msg == "Q" || msg == "QUIT") {
					goto do_exit;
				}

				size_t idx = msg.find(' ');
				if (idx == string::npos) {
					if (my::isDigit(msg)) {
						send_error("No message provided.");
					} else {
						send_error("Invalid command: " + msg);
					}
					msg.clear();
					continue;
				}

				// pick off the frequency (or command)
				string freq = my::toUpper(msg.substr(0, idx));

				// and the message (or argument)
				msg = my::toUpper(my::strip(msg.substr(idx + 1)));

				// handle commands
				if (freq == "LEVEL") {
					float level = 0.0;
					bool adjust = false;

					// if the new value prefixed by 'A', this is an ajustment
					if (::toupper(msg[0]) == 'A') {
						msg = msg.substr(1);
						adjust = true;
					}

					// read the float part of the value
					try {
						level = stof(msg);
					} catch (...) {
						send_error("Invalid level given: " + msg);
						continue;
					}

					// if this is an adjustment, add to the current level
					if (adjust) {
						level = level + decibels(audio.getVolume());
					}

					// use nearest tenth of a dB
					level = ::nearbyintf(10.0 * level) / 10.0;

					// if the new level is sane, set it
					if (level <= 0 && level >= -80) {
						audio.setVolume(linear(level));
						send_ok("Level now " + clean_db(level) + " dB");
					} else {
						send_error("Invalid level provided; must be between -80 and 0.");
					}
					msg.clear();
					continue;
				} else if (freq == "DEPTH") {
					int depth = stoi(msg);
					if (depth >= 1 && depth <= 3) {
						audio.setDepth(depth);
						send_ok("Depth now " + std::to_string(depth));
					} else {
						send_error("Invalid depth provided; must be 1 to 3.");
					}
					msg.clear();
					continue;
				} else if (freq == "LEADIN") {
					lead = stoi(msg);
					if (lead >= 0) {
						int samples = (lead * rate) / 1000;
						audio.setLead(samples);
						send_ok("Lead-in now " + std::to_string(lead));
					} else {
						send_error("Invalid lead-in provided; must be >= 0.");
					}
					msg.clear();
					continue;
				} else if ( ! ::isdigit(freq[0])) {
					send_error("Unknown command: " + freq);
					msg.clear();
					continue;
				}

				// handle even/odd
				size_t f = 0;
				TimeSlots eo;
				try {
					eo = getFrequencyAndSlot(freq, f);
				} catch (const std::exception &e) {
					send_error(e.what());
					msg = "";
					continue;
				}

				// if it is in range, transmit
				if (f >= af_low && f <= af_high) {
					send_ok("Send @ " + std::to_string(static_cast<int>(f)) + "Hz: '" + msg + "'");
					audio.transmit(msg, static_cast<float>(f), eo);
					msg = "";
				} else {
					// if not, complain
					send_error("Frequency out of range: " + to_string(f));
				}

				// clear the message buffer
				msg.clear();
			}
		}
	}

do_exit: // clean up, save config, and exit

	// stop the sound card
	audio.stop();

	// save the RC file
	{
		ofstream rc(get_rc_path());
		rc << "DEVICE " << devname << endl;
		rc << "LEVEL "  << clean_db(decibels(audio.getVolume())) << endl;
		rc << "DEPTH "  << audio.getDepth() << endl;

		// store the lead-out value for the current mode
		if (lead >= 0) {
			string key = "LEADIN." + mode + ' ';
			rc << key << ((1000 * audio.getLead()) / rate) << endl;
		}

		// store the list of lead-out values for the other modes
		for (timings_t::const_iterator i = leads.begin(); i != leads.end(); ++i) {
			if ((i->first) != mode) {
				string key = "LEADIN." + (i->first) + ' ';
				rc << key << leads[i->first] << endl;
			}
		}

		// store the lead-out value for the current mode
		if (trail >= 0) {
			string key = "TRAIL." + mode + ' ';
			rc << key << (int)(1000 * audio.getTrail()) << endl;
		}

		// store the list of lead-out values for the other modes
		for (timings_t::const_iterator i = trails.begin(); i != trails.end(); ++i) {
			if ((i->first) != mode) {
				string key = "TRAIL." + (i->first) + ' ';
				rc << key << trails[i->first] << endl;
			}
		}
	}

	// done
	return 0;
}

// EOF
