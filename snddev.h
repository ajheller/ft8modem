/*
 *
 *
 *    snddev.h
 *
 *    Sound interface and decoding framework.
 *
 *    This is the core of the 'ft8modem' application.
 *
 *    Copyright (C) 2023-2024 by Matt Roberts.
 *    License: GNU GPL3 (www.gnu.org)
 *
 *
 */

#ifndef __KK5JY_FT8_SNDDEV_H
#define __KK5JY_FT8_SNDDEV_H

// other stuff
#include <ctype.h>

// sound card base class
#include "sc.h"

// modulator
#include "mfsk.h"

// demod/decoder interface
#include "decode.h"

// encoding support
#include "encode.h"

// clock helper
#include "clock.h"

// modulator
#include "mfsk.h"

// FIR filter
#include "FirFilter.h"

// mutex locking
#include "locker.h"

// decimator
#include "decimate.h"

// modulator presets
#include "presets.h"


//
//  enum TimeSlots
//
enum TimeSlots {
	NextSlot = 0,
	OddSlot = 1,
	EvenSlot = 2
};


//
//  class ModemSoundDevice
//
//  This is the main component of the ft8modem.  It manages the sound (or UDP)
//  audio source, stores WAV data, and manages the jt9(1) instances.  It also
//  runs the MFSK modulator during transmission.
//
class ModemSoundDevice : public SoundCard {
	private: // constants
		// lowest reportable input sound level
		const int minimum_db = -144; // 24-bit integer lsb is -144dB

		// decimation LPF cutoff frequency
		const int dec_cutoff = 4500; // Hz

		// default decimation LPF FIR tap count
		const int dec_taps = 25; // taps

	private: // instance variables
		// the clock
		KK5JY::FT8::FrameClock m_Clock;

		// the decimator clock
		KK5JY::DSP::FirFilter<float> *m_Filter;

		// the decimator; convert sound-card rate to 12kHz
		KK5JY::DSP::IDecimator<float> *m_Decimator;

		// decoders
		KK5JY::FT8::Decode<float> *m_Current;
		KK5JY::FT8::Decode<float> *m_Decoding;

		// the modulator
		KK5JY::DSP::MFSK::Modulator<float> *m_MFSK;

		std::string m_TempDir;   // path to temp folder
		std::string m_Mode;      // mode string (e.g., "FT8")
		std::string m_LastSent;  // most recently-sent message
		std::string m_Decoder;   // optional path to 'jt9'-like decoder
		std::string m_Extension; // optional profile name
		KK5JY::FT8::ShaperTypes m_Shaper; // the shaper type
		size_t m_Rate;   // sampling rate
		size_t m_Lead;   // number of samples of silence to emit before transmission
		double m_FrameStart, m_FrameEnd, m_FrameSize, m_TxWinStart, m_TxWinEnd;
		double m_Trail;  // extend the trailing window by this much (sec)
		double m_Fudge;  // adjust the frame clock by this many seconds
		double m_bps, m_shift; // MFSK parameters
		short m_Depth;   // decoding depth (1...3)
		short m_Threads; // the number of threads to use
		short m_TrimIdx; // where in the decodes to start reading
		float m_Volume;  // output volume (normalized)
		char m_ModeCode; // e.g., '+' or '~'
		volatile char m_NewMode; // used to indicate sound card mode change required
		volatile float m_MaxInput; // used to track input audio level
		volatile uint64_t m_Intervals; // counts number of decode intervals
		volatile bool m_Keep; // don't delete WAV files
		volatile double m_Start;
		volatile bool m_Sending;
		volatile bool m_Active;
		volatile bool m_Abort;
		volatile bool m_Purge;
		volatile bool m_Loop; // loopback transmissions into decoder
		float *m_LoopBuffer; // used for monitoring TX sigs
		float *m_NetBuffer; // used for UDP audio source
		size_t m_NetSize; // size of m_NetBuffer

		enum TimeSlots m_Slot;

		// critical section mutex
		my::mutex m_Mutex;

	private: // utility methods
		size_t decimate_buffer(float *input, size_t count, float *output);
		void update_volume(float *input, size_t count);

	public: // [cd]tors
		ModemSoundDevice(const std::string &mode, size_t id, size_t rate, size_t win, const std::string &ext = "");
		~ModemSoundDevice();

	public: // primary interface
		// perform background processing of decodes and sound-card direction changes
		//   poll this often to make sure things happen on time
		// TODO: this should probably be replaced by a background thread, since the
		//       sound card changes can't be done from the callback
		void run();

		// send a message
		bool transmit(const std::string &message, double f0, TimeSlots slot = NextSlot);

		// stop sending immediately
		bool cancelTransmit(void);

		// tell the decoder to discard one round of decodes
		void purge(void) { m_Purge = true; }

		// start running the receiver
		bool start();

		// stop running the receiver
		virtual void stop();

		// test whether sound card is running
		bool isActive(void) const volatile { return m_Active; }

		// feed UDP audio from a socket
		void udp_audio(uint8_t *raw_data, size_t bytes);

	public: // set/get methods
		// set the path to the decoder
		std::string setDecoder(std::string d) { return (m_Decoder = d); }

		// get the path to the decoder
		std::string getDecoder(void) const { return m_Decoder; }

		// get the profile name
		std::string getExtension(void) const { return m_Extension; }

		// set the modulator shaper type
		KK5JY::FT8::ShaperTypes setShaper(KK5JY::FT8::ShaperTypes m) { return (m_Shaper = m); }

		// get the modulator shaper type
		KK5JY::FT8::ShaperTypes getShaper(void) const { return m_Shaper; }

		// set the number of threads to use
		short setThreads(short t) { return (m_Threads = t); }

		// get the number of threads to use
		short getThreads(void) const { return m_Threads; }

		// set monitor mode
		bool setMonitor(bool m) { return (m_Loop = m); }

		// get monitor mode
		bool getMonitor(void) const { return m_Loop; }

		// set the decoding depth
		short setDepth(short depth);

		// get the decoding depth
		short getDepth(void) const { return m_Depth; }

		// set the lead-in silence (samples)
		size_t setLead(size_t newVal) { return (m_Lead = newVal); }

		// get the lead-in silence (samples)
		size_t getLead(void) const { return m_Lead; }

		// set the trail adjustment (seconds)
		double setTrail(double newVal);

		// get the trail adjustment (seconds)
		double getTrail(void) const { return m_Trail; }

		// set the clock adjustment (seconds)
		double setFudge(double newVal) { return (m_Fudge = newVal); }

		// get the clock adjustment (seconds)
		double getFudge(void) const { return m_Fudge; }

		// set the volume (normalized)
		float setVolume(float newVal) { return (m_Volume = newVal); }

		// get the volume (normalized)
		float getVolume(void) const { return m_Volume; }

		// set the temp folder
		std::string setTemp(const std::string &s);

		// get the temp folder
		std::string getTemp() const { return m_TempDir; }

		// set the temp folder
		bool setKeep(bool keep) { return (m_Keep = keep); }

		// get the temp folder
		bool getKeep() const { return m_Keep; }

		// set the decimation filter tap count
		void setFilter(int taps);

	protected: // event handlers
		void event(float *in, float *out, size_t count);
};


//
//  ModemSoundDevice::setTrail(newval)
//
inline double ModemSoundDevice::setTrail(double newVal) {
	// make sure at least 250ms gap remains between recordings
	const double maxVal = m_FrameStart - (m_FrameEnd + 0.25);
	if (newVal > maxVal)
		newVal = maxVal;
	return (m_Trail = newVal);
}


//
//  ModemSoundDevice::setDepth(...)
//
inline short ModemSoundDevice::setDepth(short depth) {
	if (depth >= 1 && depth <= 3) {
		m_Depth = depth;
	}
	return m_Depth;
}

#endif // __KK5JY_FT8_SNDDEV_H
