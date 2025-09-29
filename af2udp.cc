/*
 *
 *
 *    af2udp.cc
 *
 *    Read pipe data from standard input, and write as sequenced UDP frames.
 *    Intended for piping rtl_fm data to ft8modem.
 *
 *    Copyright (C) 2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#include <iostream>
#include <string>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

// for socket/UDP support
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "version.h"

using namespace std;

// default read window size
#define DEFAULT_WINDOW (256)


//
//  usage(...)
//
static void usage(const std::string &argv0) {
	cerr << endl;
	cerr << "Usage: " << argv0 << " <port> [<win>]" << endl;
	cerr << endl;
	cerr << "    Read S16_LE single-channel audio data from standard input," << endl;
	cerr << "    packetize it into UDP frames, and send them to ft8modem." << endl;
	cerr << endl;
	cerr << "    Options include:" << endl;
	cerr << "         <port> is the local UDP port where 'ft8modem' is listening for audio." << endl;
	cerr << "         <win>  is the read window size; default is " << DEFAULT_WINDOW << " bytes." << endl;
	cerr << endl;
	exit(1);
}


//
//  main(...)
//
int main (int argc, char ** argv) {
	if (argc == 1 || argc > 3) {
		usage(argv[0]);
	}

	if (strcmp(argv[1], "-v") == 0) {
		cerr << "af2udp version " << KK5JY::FT8::GetModemVersion() << endl;
		return 0;
	}

	// defaults
	int win = DEFAULT_WINDOW;
	int port = 0;

	// read options
	if (argc >= 2)
		port = atoi(argv[1]);
	if (argc == 3)
		win = atoi(argv[2]);
	
	if (win <= 0) {
		cerr << "Error: Window must be greater than zero." << endl;
		return 1;
	}

	if (port <= 0 || port > 65535) {
		cerr << "Error: POrt must be greater than zero, and less than 65536." << endl;
		return 1;
	}

	// build the target address structure
	struct sockaddr_in addr;
	::memset(&addr, 0, sizeof(addr)); // shouldn't be needed for C++, but...
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);

	char buffer[win + 2];
	uint16_t seq = 0;
	int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	while (true) {
		int ct = ::read(0, buffer, win);
		if (ct <= 0)
			return 0;
		buffer[  ct  ] = seq & 0xFF;
		buffer[ct + 1] = (seq & 0xFF00) >> 8;
		++seq;

		// send the frame
		int result = ::sendto(sock, buffer, ct + 2, 0, reinterpret_cast<sockaddr*>( & addr), sizeof(sockaddr_in));
		if (result != ct + 2) {
			cerr << "Warning: Tried to send " << (ct + 2) << " bytes, but only sent " << result << " bytes." << endl;
			cerr.flush();
		}
	}
}

// EOF: af2udp.cc
