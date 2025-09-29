/*
 *
 *   sc.h - soundcard interface
 *
 *   This is a class library that wraps an RtAudio soundcard object
 *   and simplifies the interface.
 *
 *   Copyright (C) 2015-2024 by Matt Roberts, KK5JY,
 *   All rights reserved.
 *
 *   License: GNU GPL3 (www.gnu.org)
 *
 */

#ifndef __KK5JY_SC_H
#define __KK5JY_SC_H

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <rtaudio/RtAudio.h>


//
//  SoundCard - simple mono full-duplex interface to the sound card
//
class SoundCard {
	public:
		RtAudio adc;
		RtAudio::StreamParameters params;
		unsigned mCard;
		unsigned mRate;
		unsigned mChannels;
		unsigned mWin;
	
	public:
		SoundCard(unsigned id, unsigned rate, unsigned short channels = 2, unsigned short win = 256);
		virtual ~SoundCard() { };

	public:
		virtual bool start(bool play, bool rec);
		virtual void stop();

		// shortcuts for 'start(...)'
		bool startDuplex() { return start(true, true); };
		bool startPlayback() { return start(true, false); };
		bool startRecording() { return start(false, true); };

		// readonly accessors
		unsigned card() const { return mCard; }
		unsigned rate() const { return mRate; }
		unsigned channels() const { return mChannels; }
		unsigned window() const { return mWin; }

	public:
		static void showDevices(const std::string & linePrefix = "", bool showNoInput = true);
		static unsigned deviceCount();
	
	protected:
		virtual void event(float *inBuffer, float *outBuffer, size_t samples) = 0;

	private:
		static int handler(
			void *outputBuffer,
			void *inputBuffer,
			unsigned int nBufferFrames,
			double streamTime,
			RtAudioStreamStatus status,
			void *userData );
};

/*
 *
 *  SoundCard::ctor(...)
 *
 */
inline SoundCard::SoundCard(unsigned id, unsigned rate, unsigned short channels, unsigned short win)
	: //adc(RtAudio::LINUX_ALSA),
	  mCard(id),
	  mRate(rate),
	  mChannels(channels),
	  mWin(win) {
	// nop
}

/*
 *
 *  SoundCard::start()
 *
 */
inline bool SoundCard::start(bool play, bool rec) {
	stop();

	params.deviceId = mCard;
	params.nChannels = mChannels;
	params.firstChannel = 0;

	// open the sound card
	int result = adc.openStream(
		play ? &params : 0, // output
		rec ? &params : 0,  // input
		RTAUDIO_FLOAT32,    // format
		mRate,              // rate
		&mWin,       // buffer size
		&handler,    // callback
		this);       // callback user data
	if (result != RTAUDIO_NO_ERROR)
		return false;

	// start the stream running
	result = adc.startStream();
	if (result != RTAUDIO_NO_ERROR)
		return false;

	// happy
	return true;
}

/*
 *
 *  SoundCard::stop()
 *
 */
inline void SoundCard::stop() {
	if (adc.isStreamRunning())
		adc.stopStream();
	if (adc.isStreamOpen())
		adc.closeStream();
}

/*
 *
 *   SoundCard::handler(...)
 *
 */
inline int SoundCard::handler(
		void *outputBuffer,
		void *inputBuffer,
		unsigned int nBufferFrames,
		double streamTime,
		RtAudioStreamStatus status,
		void *sc) {
	#ifdef _DEBUG
	if (status) {
		std::cerr << "[sc:ov]";
	}
	#endif

	// extract appropriate pointers
	SoundCard *thisPtr = (SoundCard*)(sc);
	if (thisPtr == 0) return 0;
	float *inData = reinterpret_cast<float*>(inputBuffer);
	float *outData = reinterpret_cast<float*>(outputBuffer);

	// call the user's handler
	try {
		thisPtr->event(inData, outData, nBufferFrames);
	} catch (const std::exception &ex) {
		std::cerr << "SoundCard::handler(...) caught exception: " << ex.what() << std::endl;
		std::cerr.flush();
	} catch (...) {
		std::cerr << "SoundCard::handler(...) caught unknown exception." << std::endl;
		std::cerr.flush();
	}

	// return success
	return 0;
}


/*
 *
 *   channelsToString(...)
 *
 */
static std::string channelsToString(unsigned count) {
	switch(count) {
		case 0: return "None";
		case 1: return "Mono";
		case 2: return "Stereo";
		default: return "Multi";
	}
}

/*
 *
 *   ratesToString(...)
 *
 */
static std::string ratesToString(std::vector<unsigned> rates) {
	std::stringstream result;
	for (unsigned i = 0; i != rates.size(); ++i) {
		if (result.str().size() != 0)
			result << ", ";
		result << rates[i];
	}
	return result.str();
}

/*
 *
 *   showDevices()
 *
 */
inline void SoundCard::showDevices (const std::string &linePrefix, bool showNoInput) {
	RtAudio adc; //(RtAudio::LINUX_ALSA);

	// Determine the number of devices available
	unsigned int devices = adc.getDeviceCount();
	if (devices == 0) {
		std::cout << "No audio devices found." << std::endl;
		return;
	}

	// Scan through devices for various capabilities
	//std::cout << "Valid devices:" << std::endl;
	typedef std::vector<unsigned int> idlist_t;
	idlist_t ids = adc.getDeviceIds();
	for (idlist_t::const_iterator i = ids.begin(); i != ids.end(); ++i) {
		RtAudio::DeviceInfo info = adc.getDeviceInfo(*i);

		if (showNoInput || info.inputChannels > 0) {
			std::cout << linePrefix;
			std::cout << "ID = " << *i;
			std::cout << ": \"" << info.name << "\"";
			std::cout << ", inputs = " << channelsToString(info.inputChannels);
			std::cout << ", outputs = " << channelsToString(info.outputChannels);
			std::cout << ", rates = " << ratesToString(info.sampleRates);
			std::cout << "\n";
		}
	}
	std::cout.flush();
}

/*
 *
 *   deviceCount()
 *
 */
inline unsigned SoundCard::deviceCount() {
	RtAudio adc; //(RtAudio::LINUX_ALSA);
	return adc.getDeviceCount();
}

#endif // __KK5JY_SC_H
