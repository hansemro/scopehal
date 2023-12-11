/***********************************************************************************************************************
*                                                                                                                      *
* libscopehal v0.1                                                                                                     *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/*
 * Minimized Siglent scope driver. Currently supports SDS2000X+.
 *
 * Current State
 * =============
 *
 * SDS2000XP
 *
 * - Basic functionality for analog channels works.
 * - Digital channels are not implemented
 * - Supports edge trigger only
 * - Sampling lengths up to 10MSamples are supported.
 */

#include "scopehal.h"
#include "SiglentMinOscilloscope.h"
#include "base64.h"
#include <locale>
#include <stdarg.h>
#include <omp.h>
#include <thread>
#include <chrono>

#include "EdgeTrigger.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SiglentMinOscilloscope::SiglentMinOscilloscope(SCPITransport* transport)
	: SCPIDevice(transport)
	, SCPIInstrument(transport)
	, m_maxBandwidth(10000)
	, m_triggerArmed(false)
	, m_triggerOneShot(false)
	, m_sampleRateValid(false)
	, m_sampleRate(1)
	, m_memoryDepthValid(false)
	, m_memoryDepth(1)
	, m_triggerOffsetValid(false)
	, m_triggerOffset(0)
	, m_highDefinition(false)
{
	//Enable command rate limiting
	//TODO: only for some firmware versions or instrument SKUs?
	transport->EnableRateLimiting(chrono::milliseconds(50));

	//standard initialization
	FlushConfigCache();
	IdentifyHardware();
	DetectAnalogChannels();
	SharedCtorInit();
}

string SiglentMinOscilloscope::converse(const char* fmt, ...)
{
	string ret;
	char opString[128];
	va_list va;
	va_start(va, fmt);
	vsnprintf(opString, sizeof(opString), fmt, va);
	va_end(va);

	ret = m_transport->SendCommandQueuedWithReply(opString, false);
	return ret;
}

void SiglentMinOscilloscope::sendOnly(const char* fmt, ...)
{
	char opString[128];
	va_list va;

	va_start(va, fmt);
	vsnprintf(opString, sizeof(opString), fmt, va);
	va_end(va);

	m_transport->SendCommandQueued(opString);
}

void SiglentMinOscilloscope::SharedCtorInit()
{
	switch(m_modelid)
	{
		// --------------------------------------------------
		case MODEL_SIGLENT_SDS2000XP:
			sendOnly("CHDR OFF");

			//Desired format for waveform data
			//Only use increased bit depth if the scope actually puts content there!
			sendOnly(":WAVEFORM:WIDTH BYTE");
			break;
		// --------------------------------------------------
		default:
			LogError("Unknown scope type\n");
			break;
			// --------------------------------------------------
	}

	//Clear the state-change register to we get rid of any history we don't care about
	PollTrigger();

	//Enable deduplication for vertical axis commands once we know what we're dealing with
	switch(m_modelid)
	{
		case MODEL_SIGLENT_SDS2000XP:
			m_transport->DeduplicateCommand("OFFSET");
			m_transport->DeduplicateCommand("SCALE");
			break;
		default:
			break;
	}
}

void SiglentMinOscilloscope::IdentifyHardware()
{
	//Ask for the ID
	string reply = converse("*IDN?");
	char vendor[128] = "";
	char model[128] = "";
	char serial[128] = "";
	char version[128] = "";
	//Parse IDN
	//Siglent Technologies,SDS2204X Plus,SDS2ABCDEFGHIJ,5.4.1.5.2R3
	if(4 != sscanf(reply.c_str(), "%127[^,],%127[^,],%127[^,],%127s", vendor, model, serial, version))
	{
		LogError("Bad IDN response %s\n", reply.c_str());
		return;
	}
	m_vendor = vendor;
	m_model = model;
	m_serial = serial;
	m_fwVersion = version;

	//Look up model info
	m_modelid = MODEL_UNKNOWN;
	m_maxBandwidth = 0;
	m_requireSizeWorkaround = false;

	if(m_vendor.compare("Siglent Technologies") == 0)
	{
		//SDS2NNNX Plus
		if(m_model.compare(0, 4, "SDS2") == 0 && m_model.back() == 's')
		{
			m_modelid = MODEL_SIGLENT_SDS2000XP;

			//SDS21NNX Plus
			m_maxBandwidth = 100;
			//SDS22NNX Plus
			if(m_model.compare(4, 1, "2") == 0)
				m_maxBandwidth = 200;
			//SDS23NNX Plus
			else if(m_model.compare(4, 1, "3") == 0)
				m_maxBandwidth = 350;
			//SDS25NNX Plus
			if(m_model.compare(4, 1, "5") == 0)
				m_maxBandwidth = 500;
		}
		else
		{
			LogWarning("Model \"%s\" is unknown, available sample rates/memory depths may not be properly detected\n",
				m_model.c_str());
		}
	}
	else
	{
		LogWarning("Vendor \"%s\" is unknown\n", m_vendor.c_str());
	}
}

/**
	@brief Figures out how many analog channels we have, and add them to the device

 */
void SiglentMinOscilloscope::DetectAnalogChannels()
{
	int nchans = 1;

	// Char 7 of the model name is the number of channels
	// SDS2104X Plus
	//       ^
	//       4 channels
	if(m_model.length() > 7)
	{
		switch(m_model[6])
		{
			case '2':
				nchans = 2;
				break;
			case '4':
				nchans = 4;
				break;
		}
	}

	for(int i = 0; i < nchans; i++)
	{
		//Hardware name of the channel
		string chname = string("C") + to_string(i+1);

		//Color the channels based on Siglents standard color sequence
		//yellow-pink-cyan-green-lightgreen
		string color = "#ffffff";
		switch(i % 4)
		{
			case 0:
				color = "#ffff00";
				break;

			case 1:
				color = "#ff6abc";
				break;

			case 2:
				color = "#00ffff";
				break;

			case 3:
				color = "#00c100";
				break;
		}

		//Create the channel
		m_channels.push_back(
			new OscilloscopeChannel(
				this,
				chname,
				color,
				Unit(Unit::UNIT_FS),
				Unit(Unit::UNIT_VOLTS),
				Stream::STREAM_TYPE_ANALOG,
				i));
	}
	m_analogChannelCount = nchans;
}

SiglentMinOscilloscope::~SiglentMinOscilloscope()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device information

string SiglentMinOscilloscope::GetDriverNameInternal()
{
	return "siglent_min";
}

void SiglentMinOscilloscope::FlushConfigCache()
{
	lock_guard<recursive_mutex> lock(m_cacheMutex);

	if(m_trigger)
		delete m_trigger;
	m_trigger = NULL;

	m_channelVoltageRanges.clear();
	m_channelOffsets.clear();
	m_channelsEnabled.clear();
	m_channelDeskew.clear();
	m_probeIsActive.clear();
	m_sampleRateValid = false;
	m_memoryDepthValid = false;
	m_triggerOffsetValid = false;

	//Clear cached display name of all channels
	for(auto c : m_channels)
	{
		if(GetInstrumentTypesForChannel(c->GetIndex()) & Instrument::INST_OSCILLOSCOPE)
			c->ClearCachedDisplayName();
	}
}

OscilloscopeChannel* SiglentMinOscilloscope::GetExternalTrigger()
{
	return nullptr;
}

/**
	@brief See what features we have
 */
unsigned int SiglentMinOscilloscope::GetInstrumentTypes() const
{
	unsigned int type = INST_OSCILLOSCOPE;
	/*
	if(m_hasFunctionGen)
		type |= INST_FUNCTION;
	*/
	return type;
}

uint32_t SiglentMinOscilloscope::GetInstrumentTypesForChannel(size_t i) const
{
	/*
	if(m_awgChannel && (m_awgChannel->GetIndex() == i) )
		return Instrument::INST_FUNCTION;
	*/

	//If we get here, it's an oscilloscope channel
	return Instrument::INST_OSCILLOSCOPE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Channel configuration

bool SiglentMinOscilloscope::IsChannelEnabled(size_t i)
{
	//Early-out if status is in cache
	{
		lock_guard<recursive_mutex> lock2(m_cacheMutex);
		if(m_channelsEnabled.find(i) != m_channelsEnabled.end())
			return m_channelsEnabled[i];
	}

	//Analog
	if(i < m_analogChannelCount)
	{
		//See if the channel is enabled, hide it if not
		string reply = converse(":CHANNEL%d:SWITCH?", i + 1);
		{
			lock_guard<recursive_mutex> lock2(m_cacheMutex);
			m_channelsEnabled[i] = (reply.find("OFF") != 0);	//may have a trailing newline, ignore that
		}
	}

	lock_guard<recursive_mutex> lock2(m_cacheMutex);
	return m_channelsEnabled[i];
}

void SiglentMinOscilloscope::EnableChannel(size_t i)
{
	bool wasInterleaving = IsInterleaving();

	//No need to lock the main mutex since sendOnly now pushes to the queue

	//If this is an analog channel, just toggle it
	if(i < m_analogChannelCount)
	{
		sendOnly(":CHANNEL%d:SWITCH ON", i + 1);
	}
	else
	{
		LogError("Unsupported channel type\n");
	}

	lock_guard<recursive_mutex> lock(m_cacheMutex);
	m_channelsEnabled[i] = true;

	//Sample rate and memory depth can change if interleaving state changed
	if(IsInterleaving() != wasInterleaving)
	{
		m_memoryDepthValid = false;
		m_sampleRateValid = false;
	}
}

bool SiglentMinOscilloscope::CanEnableChannel(size_t i)
{
	return i < m_analogChannelCount;
}

void SiglentMinOscilloscope::DisableChannel(size_t i)
{
	bool wasInterleaving = IsInterleaving();

	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);
		m_channelsEnabled[i] = false;
	}

	//If this is an analog channel, just toggle it
	if(i < m_analogChannelCount)
		sendOnly(":CHANNEL%d:SWITCH OFF", i + 1);

	//Sample rate and memory depth can change if interleaving state changed
	if(IsInterleaving() != wasInterleaving)
	{
		m_memoryDepthValid = false;
		m_sampleRateValid = false;
	}
}

vector<OscilloscopeChannel::CouplingType> SiglentMinOscilloscope::GetAvailableCouplings(size_t /*i*/)
{
	vector<OscilloscopeChannel::CouplingType> ret;

	ret.push_back(OscilloscopeChannel::COUPLE_DC_1M);
	ret.push_back(OscilloscopeChannel::COUPLE_AC_1M);
	ret.push_back(OscilloscopeChannel::COUPLE_DC_50);
	ret.push_back(OscilloscopeChannel::COUPLE_AC_50);
	ret.push_back(OscilloscopeChannel::COUPLE_GND);
	return ret;
}

OscilloscopeChannel::CouplingType SiglentMinOscilloscope::GetChannelCoupling(size_t i)
{
	string replyType;
	string replyImp;

	m_probeIsActive[i] = false;

	replyType = Trim(converse(":CHANNEL%d:COUPLING?", i + 1).substr(0, 2));
	replyImp = Trim(converse(":CHANNEL%d:IMPEDANCE?", i + 1).substr(0, 3));

	if(replyType == "AC")
		return (replyImp.find("FIF") == 0) ? OscilloscopeChannel::COUPLE_AC_50 : OscilloscopeChannel::COUPLE_AC_1M;
	else if(replyType == "DC")
		return (replyImp.find("FIF") == 0) ? OscilloscopeChannel::COUPLE_DC_50 : OscilloscopeChannel::COUPLE_DC_1M;
	else if(replyType == "GN")
		return OscilloscopeChannel::COUPLE_GND;

	//invalid
	LogWarning("SiglentMinOscilloscope::GetChannelCoupling got invalid coupling [%s] [%s]\n",
		replyType.c_str(),
		replyImp.c_str());
	return OscilloscopeChannel::COUPLE_SYNTHETIC;
}

void SiglentMinOscilloscope::SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type)
{
	if(i >= m_analogChannelCount)
		return;

	//Get the old coupling value first.
	//This ensures that m_probeIsActive[i] is valid
	GetChannelCoupling(i);

	//If we have an active probe, don't touch the hardware config
	if(m_probeIsActive[i])
		return;

	switch(type)
	{
		case OscilloscopeChannel::COUPLE_AC_1M:
			sendOnly(":CHANNEL%d:COUPLING AC", i + 1);
			sendOnly(":CHANNEL%d:IMPEDANCE ONEMEG", i + 1);
			break;

		case OscilloscopeChannel::COUPLE_DC_1M:
			sendOnly(":CHANNEL%d:COUPLING DC", i + 1);
			sendOnly(":CHANNEL%d:IMPEDANCE ONEMEG", i + 1);
			break;

		case OscilloscopeChannel::COUPLE_DC_50:
			sendOnly(":CHANNEL%d:COUPLING DC", i + 1);
			sendOnly(":CHANNEL%d:IMPEDANCE FIFTY", i + 1);
			break;

		case OscilloscopeChannel::COUPLE_AC_50:
			sendOnly(":CHANNEL%d:COUPLING AC", i + 1);
			sendOnly(":CHANNEL%d:IMPEDANCE FIFTY", i + 1);
			break;

		//treat unrecognized as ground
		case OscilloscopeChannel::COUPLE_GND:
		default:
			sendOnly(":CHANNEL%d:COUPLING GND", i + 1);
			break;
	}
}

double SiglentMinOscilloscope::GetChannelAttenuation(size_t i)
{
	string reply;

	reply = converse(":CHANNEL%d:PROBE?", i + 1);

	double d;
	sscanf(reply.c_str(), "%lf", &d);
	return d;
}

void SiglentMinOscilloscope::SetChannelAttenuation(size_t i, double atten)
{
	//Get the old coupling value first.
	//This ensures that m_probeIsActive[i] is valid
	GetChannelCoupling(i);

	//Don't allow changing attenuation on active probes
	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);
		if(m_probeIsActive[i])
			return;
	}

	sendOnly(":CHANNEL%d:PROBE VALUE,%1.2E", i + 1, atten);
}

vector<unsigned int> SiglentMinOscilloscope::GetChannelBandwidthLimiters(size_t /*i*/)
{
	vector<unsigned int> ret;

	//"no limit"
	ret.push_back(0);

	//20M
	ret.push_back(20);

	return ret;
}

unsigned int SiglentMinOscilloscope::GetChannelBandwidthLimit(size_t i)
{
	string reply = converse(":CHANNEL%d:BWLIMIT?", i + 1);
	if(reply == "FULL")
		return 0;
	else if(reply == "20M")
		return 20;

	LogWarning("SiglentMinOscilloscope::GetChannelCoupling got invalid bwlimit %s\n", reply.c_str());
	return 0;
}

void SiglentMinOscilloscope::SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz)
{
	switch(limit_mhz)
	{
		case 0:
			sendOnly(":CHANNEL%d:BWLIMIT FULL", i + 1);
			break;

		case 20:
			sendOnly(":CHANNEL%d:BWLIMIT 20M", i + 1);
			break;

		case 200:
			sendOnly(":CHANNEL%d:BWLIMIT 200M", i + 1);
			break;

		default:
			LogWarning("SiglentMinOscilloscope::invalid bwlimit set request (%dMhz)\n", limit_mhz);
	}
}

bool SiglentMinOscilloscope::CanInvert(size_t i)
{
	//All analog channels, and only analog channels, can be inverted
	return (i < m_analogChannelCount);
}

void SiglentMinOscilloscope::Invert(size_t i, bool invert)
{
	if(i >= m_analogChannelCount)
		return;

	sendOnly(":CHANNEL%d:INVERT %s", i + 1, invert ? "ON" : "OFF");
}

bool SiglentMinOscilloscope::IsInverted(size_t i)
{
	if(i >= m_analogChannelCount)
		return false;

	string reply = Trim(converse(":CHANNEL%d:INVERT?", i + 1));
	return (reply == "ON");
}

void SiglentMinOscilloscope::SetChannelDisplayName(size_t i, string name)
{
	auto chan = GetOscilloscopeChannel(i);
	if(!chan)
		return;

	if(i < m_analogChannelCount)
	{
		sendOnly(":CHANNEL%ld:LABEL:TEXT \"%s\"", i + 1, name.c_str());
		sendOnly(":CHANNEL%ld:LABEL ON", i + 1);
	}
	//else
	//{
	//	sendOnly(":DIGITAL:LABEL%ld \"%s\"", i - (m_analogChannelCount + 1), name.c_str());
	//}
}

string SiglentMinOscilloscope::GetChannelDisplayName(size_t i)
{
	auto chan = GetOscilloscopeChannel(i);
	if(!chan)
		return "";

	//Analog and digital channels use completely different namespaces, as usual.
	//Because clean, orthogonal APIs are apparently for losers?
	string name = "";

	if(i < m_analogChannelCount)
	{
		name = converse(":CHANNEL%d:LABEL:TEXT?", i + 1);
		// Remove "'s around the name
		if(name.length() > 2)
			name = name.substr(1, name.length() - 2);
	}
	//else
	//{
	//	name = converse(":DIGITAL:LABEL%d?", i - (m_analogChannelCount + 1));
	//	// Remove "'s around the name
	//	if(name.length() > 2)
	//		name = name.substr(1, name.length() - 2);
	//}

	//Default to using hwname if no alias defined
	if(name == "")
		name = chan->GetHwname();

	return name;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triggering

bool SiglentMinOscilloscope::IsTriggerArmed()
{
	return m_triggerArmed;
}

Oscilloscope::TriggerMode SiglentMinOscilloscope::PollTrigger()
{
	//Read the Internal State Change Register
	string sinr = "";

	if(m_triggerForced)
	{
		// The force trigger completed, return the sample set
		m_triggerForced = false;
		m_triggerArmed = false;
		return TRIGGER_MODE_TRIGGERED;
	}

	sinr = converse(":TRIGGER:STATUS?");

	//No waveform, but ready for one?
	if((sinr == "Arm") || (sinr == "Ready"))
	{
		m_triggerArmed = true;
		return TRIGGER_MODE_RUN;
	}

	//Stopped, no data available
	if(sinr == "Stop")
	{
		//For single mode: the trigger stops when data is ready to be sent
		if(m_triggerArmed)
		{
			//Only mark the trigger as disarmed if this was a one-shot trigger.
			//If this is a repeating trigger, we're still armed from the client's perspective,
			//since AcquireData() will reset the trigger for the next acquisition.
			if(m_triggerOneShot)
				m_triggerArmed = false;

			return TRIGGER_MODE_TRIGGERED;
		}
		else
			return TRIGGER_MODE_STOP;
	}
	return TRIGGER_MODE_RUN;
}

int SiglentMinOscilloscope::ReadWaveformBlock(uint32_t maxsize, char* data, bool hdSizeWorkaround)
{
	char packetSizeSequence[17];
	uint32_t getLength;

	// Get size of this sequence
	m_transport->ReadRawData(7, (unsigned char*)packetSizeSequence);

	// This is an aweful cludge, but the response can be in different formats depending on
	// if this was a direct trigger or a forced trigger. This is the report format for a direct trigger
	if((!strncmp(packetSizeSequence, "DESC,#9", 7)) || (!strncmp(packetSizeSequence, "DAT2,#9", 7)))
	{
		m_transport->ReadRawData(9, (unsigned char*)packetSizeSequence);
	}

	// This is the report format for a forced trigger
	else if(!strncmp(&packetSizeSequence[2], ":WF D", 5))
	{
		// Read the front end junk, then the actually number we're looking for
		m_transport->ReadRawData(6, (unsigned char*)packetSizeSequence);
		m_transport->ReadRawData(9, (unsigned char*)packetSizeSequence);
	}

	//Some scopes (observed on SDS2000X HD running firmware 1.1.7.0)
	//have no prefix at all and just have the #9... directly.
	else if(!strncmp(packetSizeSequence, "#9", 2))
	{
		//Trim off the #9
		memmove(packetSizeSequence, packetSizeSequence+2, 5);

		//Read the last 4 bytes of the length
		m_transport->ReadRawData(4, (unsigned char*)packetSizeSequence+5);
	}

	else
	{
		LogError("ReadWaveformBlock: invalid length format\n");
		return 0;
	}

	packetSizeSequence[9] = 0;
	LogTrace("INITIAL PACKET [%s]\n", packetSizeSequence);
	getLength = atoi(packetSizeSequence);

	uint32_t len = getLength;
	if(hdSizeWorkaround)
		len *= 2;
	len = min(len, maxsize);

	// Now get the data
	m_transport->ReadRawData(len, (unsigned char*)data);

	if(hdSizeWorkaround)
		return getLength*2;
	return getLength;
}

/**
	@brief Optimized function for checking channel enable status en masse with less round trips to the scope
 */
void SiglentMinOscilloscope::BulkCheckChannelEnableState()
{
	vector<int> uncached;

	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);

		//Check enable state in the cache.
		for(unsigned int i = 0; i < m_analogChannelCount; i++)
		{
			if(m_channelsEnabled.find(i) == m_channelsEnabled.end())
				uncached.push_back(i);
		}
	}

	for(auto i : uncached)
	{
		string reply = converse(":CHANNEL%d:SWITCH?", i + 1);
		if(reply == "OFF")
			m_channelsEnabled[i] = false;
		else if(reply == "ON")
			m_channelsEnabled[i] = true;
		else
			LogWarning("BulkCheckChannelEnableState: Unrecognised reply [%s]\n", reply.c_str());
	}

	//TODO: Check digital status
}

bool SiglentMinOscilloscope::ReadWavedescs(
	char wavedescs[MAX_ANALOG][WAVEDESC_SIZE], bool* enabled, unsigned int& firstEnabledChannel, bool& any_enabled)
{
	BulkCheckChannelEnableState();
	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		enabled[i] = IsChannelEnabled(i);
		any_enabled |= enabled[i];
	}

	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		if(enabled[i] || (!any_enabled && i == 0))
		{
			if(firstEnabledChannel == UINT_MAX)
				firstEnabledChannel = i;

			m_transport->SendCommandQueued(":WAVEFORM:SOURCE C" + to_string(i + 1) + ";:WAVEFORM:PREAMBLE?");
			m_transport->FlushCommandQueue();
			if(WAVEDESC_SIZE != ReadWaveformBlock(WAVEDESC_SIZE, wavedescs[i]))
				LogError("ReadWaveformBlock for wavedesc %u failed\n", i);

			//This is the 0x0a at the end
			m_transport->ReadReply();
		}
	}

	return true;
}

time_t SiglentMinOscilloscope::ExtractTimestamp(unsigned char* wavedesc, double& basetime)
{
	/*
                TIMESTAMP is shown as Reserved In Siglent data format.
                This information is from LeCroy which uses the same wavedesc header.
		Timestamp is a somewhat complex format that needs some shuffling around.
		Timestamp starts at offset 296 bytes in the wavedesc
		(296-303)	double seconds
		(304)		byte minutes
		(305)		byte hours
		(306)		byte days
		(307)		byte months
		(308-309)	uint16 year

		TODO: during startup, query instrument for its current time zone
		since the wavedesc reports instment local time
	 */
	//Yes, this cast is intentional.
	//It assumes you're on a little endian system using IEEE754 64-bit float, but that applies to everything we support.
	//cppcheck-suppress invalidPointerCast
	double fseconds = *reinterpret_cast<const double*>(wavedesc + 296);
	uint8_t seconds = floor(fseconds);
	basetime = fseconds - seconds;
	time_t tnow = time(NULL);
	struct tm tstruc;

#ifdef _WIN32
	localtime_s(&tstruc, &tnow);
#else
	localtime_r(&tnow, &tstruc);
#endif

	//Convert the instrument time to a string, then back to a tm
	//Is there a better way to do this???
	//Naively poking "struct tm" fields gives incorrect results (scopehal-apps:#52)
	//Maybe because tm_yday is inconsistent?
	char tblock[64] = {0};
	snprintf(tblock,
		sizeof(tblock),
		"%d-%d-%d %d:%02d:%02d",
		*reinterpret_cast<uint16_t*>(wavedesc + 308),
		wavedesc[307],
		wavedesc[306],
		wavedesc[305],
		wavedesc[304],
		seconds);
	locale cur_locale;
	auto& tget = use_facet<time_get<char>>(cur_locale);
	istringstream stream(tblock);
	ios::iostate state;
	char format[] = "%F %T";
	tget.get(stream, time_get<char>::iter_type(), stream, state, &tstruc, format, format + strlen(format));
	return mktime(&tstruc);
}

vector<WaveformBase*> SiglentMinOscilloscope::ProcessAnalogWaveform(const char* data,
	size_t datalen,
	char* wavedesc,
	uint32_t num_sequences,
	time_t ttime,
	double basetime,
	double* wavetime,
	int ch)
{
	vector<WaveformBase*> ret;

	//Parse the wavedesc headers
	auto pdesc = wavedesc;

	//cppcheck-suppress invalidPointerCast
	float v_gain = *reinterpret_cast<float*>(pdesc + 156);

	//cppcheck-suppress invalidPointerCast
	float v_off = *reinterpret_cast<float*>(pdesc + 160);

	//cppcheck-suppress invalidPointerCast
	float v_probefactor = *reinterpret_cast<float*>(pdesc + 328);

	//cppcheck-suppress invalidPointerCast
	float interval = *reinterpret_cast<float*>(pdesc + 176) * FS_PER_SECOND;

	//cppcheck-suppress invalidPointerCast
	double h_off = *reinterpret_cast<double*>(pdesc + 180) * FS_PER_SECOND;	   //fs from start of waveform to trigger

	//double h_off_frac = fmodf(h_off, interval);	   //fractional sample position, in fs

	double h_off_frac = 0;	  //((interval*datalen)/2)+h_off;

	if(h_off_frac < 0)
		h_off_frac = h_off;	   //interval + h_off_frac;	   //double h_unit = *reinterpret_cast<double*>(pdesc + 244);

	//Raw waveform data
	size_t num_samples;
	if(m_highDefinition)
		num_samples = datalen / 2;
	else
		num_samples = datalen;
	size_t num_per_segment = num_samples / num_sequences;
	int16_t* wdata = (int16_t*)&data[0];
	int8_t* bdata = (int8_t*)&data[0];

	float codes_per_div;

	//SDS2000X+ and SDS5000X have 30 codes per div.
	codes_per_div = 30;

	v_gain = v_gain * v_probefactor / codes_per_div;

	//in word mode, we have 256x as many codes
	if(m_highDefinition)
		v_gain /= 256;

	// Vertical offset is also scaled by the probefactor
	v_off = v_off * v_probefactor;

	// Update channel voltages and offsets based on what is in this wavedesc
	// m_channelVoltageRanges[ch] = v_gain * v_probefactor * 30 * 8;
	// m_channelOffsets[ch] = v_off;
	// m_triggerOffset = ((interval * datalen) / 2) + h_off;
	// m_triggerOffsetValid = true;

	LogTrace("\nV_Gain=%f, V_Off=%f, interval=%f, h_off=%f, h_off_frac=%f, datalen=%ld\n",
		v_gain,
		v_off,
		interval,
		h_off,
		h_off_frac,
		datalen);

	for(size_t j = 0; j < num_sequences; j++)
	{
		//Set up the capture we're going to store our data into
		auto cap = new UniformAnalogWaveform;
		cap->m_timescale = round(interval);

		cap->m_triggerPhase = h_off_frac;
		cap->m_startTimestamp = ttime;

		//Parse the time
		if(num_sequences > 1)
			cap->m_startFemtoseconds = static_cast<int64_t>((basetime + wavetime[j * 2]) * FS_PER_SECOND);
		else
			cap->m_startFemtoseconds = static_cast<int64_t>(basetime * FS_PER_SECOND);

		cap->Resize(num_per_segment);
		cap->PrepareForCpuAccess();

		//Convert raw ADC samples to volts
		if(m_highDefinition)
		{
			Convert16BitSamples(
				cap->m_samples.GetCpuPointer(),
				wdata + j * num_per_segment,
				v_gain,
				v_off,
				num_per_segment);
		}
		else
		{
			Convert8BitSamples(
				cap->m_samples.GetCpuPointer(),
				bdata + j * num_per_segment,
				v_gain,
				v_off,
				num_per_segment);
		}

		cap->MarkSamplesModifiedFromCpu();
		ret.push_back(cap);
	}

	return ret;
}

bool SiglentMinOscilloscope::AcquireData()
{
	//State for this acquisition (may be more than one waveform)
	uint32_t num_sequences = 1;
	map<int, vector<WaveformBase*>> pending_waveforms;
	double start = GetTime();
	time_t ttime = 0;
	double basetime = 0;
	double h_off_frac = 0;
	vector<vector<WaveformBase*>> waveforms;
	unsigned char* pdesc = NULL;
	bool denabled = false;
	string wavetime;
	bool enabled[8] = {false};
	double* pwtime = NULL;
	char tmp[128];

	//Acquire the data (but don't parse it)

	lock_guard<recursive_mutex> lock(m_transport->GetMutex());
	start = GetTime();
	//Get the wavedescs for all channels
	unsigned int firstEnabledChannel = UINT_MAX;
	bool any_enabled = true;

	if(!ReadWavedescs(m_wavedescs, enabled, firstEnabledChannel, any_enabled))
		return false;

	//Grab the WAVEDESC from the first enabled channel
	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		if(enabled[i] || (!any_enabled && i == 0))
		{
			pdesc = (unsigned char*)(&m_wavedescs[i][0]);
			break;
		}
	}

	//TODO: See if any digital channels are enabled

	//Pull sequence count out of the WAVEDESC if we have analog channels active
	if(pdesc)
	{
		uint32_t trigtime_len = *reinterpret_cast<uint32_t*>(pdesc + 48);
		if(trigtime_len > 0)
			num_sequences = trigtime_len / 16;
	}

	//No WAVEDESCs, look at digital channels
	else
	{
		//TODO: support sequence capture of digital channels if the instrument supports this
		//(need to look into it)
		if(denabled)
			num_sequences = 1;

		//no enabled channels. abort
		else
			return false;
	}

	//Request waveforms for all analog channels
	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		if(enabled[i])
		{
			m_transport->SendCommandQueued(":WAVEFORM:SOURCE C" + to_string(i + 1) + ";:WAVEFORM:DATA?");
		}
	}
	m_transport->FlushCommandQueue();

	if(pdesc)
	{
		//Figure out when the first trigger happened.
		//Read the timestamps if we're doing segmented capture
		ttime = ExtractTimestamp(pdesc, basetime);
		if(num_sequences > 1)
			wavetime = m_transport->ReadReply();
		pwtime = reinterpret_cast<double*>(&wavetime[16]);	  //skip 16-byte SCPI header

		//QUIRK: On SDS2000X+ with firmware 1.3.9R6 and older, the SCPI length header reports the
		//sample count rather than size in bytes! Firmware 1.3.9R10 and newer reports size in bytes.
		//2000X+ HD running firmware 1.1.7.0 seems to report size in bytes.
		bool hdWorkaround = m_requireSizeWorkaround && m_highDefinition;

		//Read the data from each analog waveform
		for(unsigned int i = 0; i < m_analogChannelCount; i++)
		{
			if(enabled[i])
			{
				m_analogWaveformDataSize[i] = ReadWaveformBlock(WAVEFORM_SIZE, m_analogWaveformData[i], hdWorkaround);
				// This is the 0x0a0a at the end
				m_transport->ReadRawData(2, (unsigned char*)tmp);
			}
		}
	}

	//TODO: Read the data from the digital waveforms, if enabled
	//if(denabled)
	//{
	//	if(!ReadWaveformBlock(WAVEFORM_SIZE, m_digitalWaveformDataBytes))
	//	{
	//		LogDebug("failed to download digital waveform\n");
	//		return false;
	//	}
	//}

	//At this point all data has been read so the scope is free to go do its thing while we crunch the results.
	//Re-arm the trigger if not in one-shot mode
	if(!m_triggerOneShot)
	{
		sendOnly(":TRIGGER:MODE SINGLE");
		m_transport->FlushCommandQueue();
		m_triggerArmed = true;
	}

	//Process analog waveforms
	waveforms.resize(m_analogChannelCount);
	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		if(enabled[i])
		{
			waveforms[i] = ProcessAnalogWaveform(&m_analogWaveformData[i][0],
				m_analogWaveformDataSize[i],
				&m_wavedescs[i][0],
				num_sequences,
				ttime,
				basetime,
				pwtime,
				i);
		}
	}

	//Save analog waveform data
	for(unsigned int i = 0; i < m_analogChannelCount; i++)
	{
		if(!enabled[i])
			continue;

		//Done, update the data
		for(size_t j = 0; j < num_sequences; j++)
			pending_waveforms[i].push_back(waveforms[i][j]);
	}

	//TODO: proper support for sequenced capture when digital channels are active
	// if(denabled)
	// {
	// 	map<int, DigitalWaveform*> digwaves = ProcessDigitalWaveform(m_digitalWaveformData);

	// 	//Done, update the data
	// 	for(auto it : digwaves)
	// 		pending_waveforms[it.first].push_back(it.second);
	// }

	//Now that we have all of the pending waveforms, save them in sets across all channels
	m_pendingWaveformsMutex.lock();
	for(size_t i = 0; i < num_sequences; i++)
	{
		SequenceSet s;
		for(size_t j = 0; j < m_channels.size(); j++)
		{
			if(pending_waveforms.find(j) != pending_waveforms.end())
				s[GetOscilloscopeChannel(j)] = pending_waveforms[j][i];
		}
		m_pendingWaveforms.push_back(s);
	}
	m_pendingWaveformsMutex.unlock();

	double dt = GetTime() - start;
	LogTrace("Waveform download and processing took %.3f ms\n", dt * 1000);
	return true;
}

void SiglentMinOscilloscope::Start()
{
	sendOnly(":TRIGGER:MODE STOP");
	sendOnly(":TRIGGER:MODE SINGLE");	 //always do single captures, just re-trigger
	m_transport->FlushCommandQueue();

	m_triggerArmed = true;
	m_triggerOneShot = false;
}

void SiglentMinOscilloscope::StartSingleTrigger()
{
	sendOnly(":TRIGGER:MODE STOP");
	sendOnly(":TRIGGER:MODE SINGLE");
	m_transport->FlushCommandQueue();

	m_triggerArmed = true;
	m_triggerOneShot = true;
}

void SiglentMinOscilloscope::Stop()
{
	m_transport->SendCommandQueued(":TRIGGER:MODE STOP");
	m_transport->FlushCommandQueue();

	m_triggerArmed = false;
	m_triggerOneShot = true;
}

void SiglentMinOscilloscope::ForceTrigger()
{
	// Don't allow more than one force at a time
	if(m_triggerForced)
		return;

	m_triggerForced = true;

	sendOnly(":TRIGGER:MODE FTRIG");
	m_transport->FlushCommandQueue();

	m_triggerArmed = true;
	m_triggerOneShot = true;
}

float SiglentMinOscilloscope::GetChannelOffset(size_t i, size_t /*stream*/)
{
	//not meaningful for trigger or digital channels
	if(i > m_analogChannelCount)
		return 0;

	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);

		if(m_channelOffsets.find(i) != m_channelOffsets.end())
			return m_channelOffsets[i];
	}

	string reply = converse(":CHANNEL%ld:OFFSET?", i + 1);

	float offset;
	sscanf(reply.c_str(), "%f", &offset);

	lock_guard<recursive_mutex> lock(m_cacheMutex);
	m_channelOffsets[i] = offset;
	return offset;
}

void SiglentMinOscilloscope::SetChannelOffset(size_t i, size_t /*stream*/, float offset)
{
	//not meaningful for trigger or digital channels
	if(i > m_analogChannelCount)
		return;

	sendOnly(":CHANNEL%ld:OFFSET %1.2E", i + 1, offset);

	lock_guard<recursive_mutex> lock(m_cacheMutex);
	m_channelOffsets[i] = offset;
}

float SiglentMinOscilloscope::GetChannelVoltageRange(size_t i, size_t /*stream*/)
{
	//not meaningful for trigger or digital channels
	if(i > m_analogChannelCount)
		return 1;

	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);
		if(m_channelVoltageRanges.find(i) != m_channelVoltageRanges.end())
			return m_channelVoltageRanges[i];
	}

	string reply = converse(":CHANNEL%d:SCALE?", i + 1);

	float volts_per_div;
	sscanf(reply.c_str(), "%f", &volts_per_div);

	float v = volts_per_div * 8;	//plot is 8 divisions high
	lock_guard<recursive_mutex> lock(m_cacheMutex);
	m_channelVoltageRanges[i] = v;
	return v;
}

void SiglentMinOscilloscope::SetChannelVoltageRange(size_t i, size_t /*stream*/, float range)
{
	float vdiv = range / 8;
	m_channelVoltageRanges[i] = range;

	sendOnly(":CHANNEL%ld:SCALE %.4f", i + 1, vdiv);
}

vector<uint64_t> SiglentMinOscilloscope::GetSampleRatesNonInterleaved()
{
	vector<uint64_t> ret;
	
	//SDS2000X+
	ret =
	{
		10 * 1000,
		20 * 1000,
		50 * 1000,
		100 * 1000,
		200 * 1000,
		500 * 1000,
		1 * 1000 * 1000,
		2 * 1000 * 1000,
		5 * 1000 * 1000,
		10 * 1000 * 1000,
		20 * 1000 * 1000,
		50 * 1000 * 1000,
		100 * 1000 * 1000,
		200 * 1000 * 1000,
		500 * 1000 * 1000,
		1 * 1000 * 1000 * 1000
	};

	return ret;
}

vector<uint64_t> SiglentMinOscilloscope::GetSampleRatesInterleaved()
{
	vector<uint64_t> ret = GetSampleRatesNonInterleaved();
	for(size_t i=0; i<ret.size(); i++)
		ret[i] *= 2;
	return ret;
}

vector<uint64_t> SiglentMinOscilloscope::GetSampleDepthsNonInterleaved()
{
	vector<uint64_t> ret = {};

	//SDS2000X+
	ret =
	{
		10 * 1000,			//10k
		100 * 1000,			//100k
		1000 * 1000,		//1M
		10 * 1000 * 1000	//10M
	};
	return ret;
}

vector<uint64_t> SiglentMinOscilloscope::GetSampleDepthsInterleaved()
{
	vector<uint64_t> ret = GetSampleDepthsNonInterleaved();
	for(size_t i=0; i<ret.size(); i++)
		ret[i] *= 2;
	return ret;
}

set<SiglentMinOscilloscope::InterleaveConflict> SiglentMinOscilloscope::GetInterleaveConflicts()
{
	set<InterleaveConflict> ret;

	//All scopes normally interleave channels 1/2 and 3/4.
	//If both channels in either pair is in use, that's a problem.
	ret.emplace(InterleaveConflict(GetOscilloscopeChannel(0), GetOscilloscopeChannel(1)));
	if(m_analogChannelCount > 2)
		ret.emplace(InterleaveConflict(GetOscilloscopeChannel(2), GetOscilloscopeChannel(3)));

	return ret;
}

uint64_t SiglentMinOscilloscope::GetSampleRate()
{
	double f;
	if(!m_sampleRateValid)
	{
		string reply = converse(":ACQUIRE:SRATE?");
		sscanf(reply.c_str(), "%lf", &f);
		m_sampleRate = static_cast<int64_t>(f);
		m_sampleRateValid = true;
	}
	return m_sampleRate;
}

uint64_t SiglentMinOscilloscope::GetSampleDepth()
{
	double f;
	if(!m_memoryDepthValid)
	{
		//:ACQUIRE:MDEPTH can sometimes return incorrect values! It returns the *cap* on memory depth,
		// not the *actual* memory depth....we don't know that until we've collected samples

		// What you see below is the only observed method that seems to reliably get the *actual* memory depth.
		string reply = converse(":ACQUIRE:MDEPTH?");
		f = Unit(Unit::UNIT_SAMPLEDEPTH).ParseString(reply);
		m_memoryDepth = static_cast<int64_t>(f);
		m_memoryDepthValid = true;
	}
	return m_memoryDepth;
}

void SiglentMinOscilloscope::SetSampleDepth(uint64_t depth)
{
	//Need to lock the mutex when setting depth because of the quirks around needing to change trigger mode too
	lock_guard<recursive_mutex> lock(m_transport->GetMutex());

	//save original sample rate (scope often changes sample rate when adjusting memory depth)
	uint64_t rate = GetSampleRate();

	// we can not change memory size in Run/Stop mode
	sendOnly("TRIG_MODE AUTO");

	switch(depth)
	{
		case 10000:
			sendOnly("ACQUIRE:MDEPTH 10k");
			break;
		case 20000:
			sendOnly("ACQUIRE:MDEPTH 20k");
			break;
		case 100000:
			sendOnly("ACQUIRE:MDEPTH 100k");
			break;
		case 200000:
			sendOnly("ACQUIRE:MDEPTH 200k");
			break;
		case 1000000:
			sendOnly("ACQUIRE:MDEPTH 1M");
			break;
		case 2000000:
			sendOnly("ACQUIRE:MDEPTH 2M");
			break;
		case 10000000:
			sendOnly("ACQUIRE:MDEPTH 10M");
			break;

		// We don't yet support memory depths that need to be transferred in chunks
		case 20000000:
		//sendOnly("ACQUIRE:MDEPTH 20M");
		//	break;
		case 50000000:
		//	sendOnly("ACQUIRE:MDEPTH 50M");
		//	break;
		case 100000000:
		//	sendOnly("ACQUIRE:MDEPTH 100M");
		//	break;
		case 200000000:
		//	sendOnly("ACQUIRE:MDEPTH 200M");
		//	break;
		default:
			LogError("Invalid memory depth for channel: %lu\n", depth);
	}

	if(IsTriggerArmed())
	{
		// restart trigger
		sendOnly("TRIG_MODE SINGLE");
	}
	else
	{
		// change to stop mode
		sendOnly("TRIG_MODE STOP");
	}

	m_memoryDepthValid = false;

	//restore old sample rate
	SetSampleRate(rate);
}

void SiglentMinOscilloscope::SetSampleRate(uint64_t rate)
{
	m_sampleRate = rate;
	m_sampleRateValid = false;

	m_memoryDepthValid = false;
	double sampletime = GetSampleDepth() / (double)rate;
	double scale = sampletime / 10;

	sendOnly(":TIMEBASE:SCALE %1.2E", scale);
	m_memoryDepthValid = false;
}

void SiglentMinOscilloscope::EnableTriggerOutput()
{
	LogWarning("EnableTriggerOutput not implemented\n");
}

void SiglentMinOscilloscope::SetUseExternalRefclk(bool /*external*/)
{
    LogWarning("SetUseExternalRefclk not implemented\n");
}

void SiglentMinOscilloscope::SetTriggerOffset(int64_t offset)
{
	//Siglents standard has the offset being from the midpoint of the capture.
	//Scopehal has offset from the start.
	int64_t rate = GetSampleRate();
	int64_t halfdepth = GetSampleDepth() / 2;
	int64_t halfwidth = static_cast<int64_t>(round(FS_PER_SECOND * halfdepth / rate));

	sendOnly(":TIMEBASE:DELAY %1.2E", (halfwidth - offset) * SECONDS_PER_FS);

	//Don't update the cache because the scope is likely to round the offset we ask for.
	//If we query the instrument later, the cache will be updated then.
	lock_guard<recursive_mutex> lock2(m_cacheMutex);
	m_triggerOffsetValid = false;
}

int64_t SiglentMinOscilloscope::GetTriggerOffset()
{
	//Early out if the value is in cache
	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);
		if(m_triggerOffsetValid)
			return m_triggerOffset;
	}
	string reply = converse(":TIMEBASE:DELAY?");

	lock_guard<recursive_mutex> lock(m_cacheMutex);

	//Result comes back in scientific notation
	double sec;
	sscanf(reply.c_str(), "%le", &sec);
	m_triggerOffset = static_cast<int64_t>(round(sec * FS_PER_SECOND));

	//Convert from midpoint to start point
	int64_t rate = GetSampleRate();
	int64_t halfdepth = GetSampleDepth() / 2;
	int64_t halfwidth = static_cast<int64_t>(round(FS_PER_SECOND * halfdepth / rate));
	m_triggerOffset = halfwidth - m_triggerOffset;

	m_triggerOffsetValid = true;

	return m_triggerOffset;
}

void SiglentMinOscilloscope::SetDeskewForChannel(size_t channel, int64_t skew)
{
	//Cannot deskew digital/trigger channels
	if(channel >= m_analogChannelCount)
		return;

	sendOnly(":CHANNEL%ld:SKEW %1.2E", channel, skew * SECONDS_PER_FS);

	//Update cache
	lock_guard<recursive_mutex> lock2(m_cacheMutex);
	m_channelDeskew[channel] = skew;
}

int64_t SiglentMinOscilloscope::GetDeskewForChannel(size_t channel)
{
	//Cannot deskew digital/trigger channels
	if(channel >= m_analogChannelCount)
		return 0;

	//Early out if the value is in cache
	{
		lock_guard<recursive_mutex> lock(m_cacheMutex);
		if(m_channelDeskew.find(channel) != m_channelDeskew.end())
			return m_channelDeskew[channel];
	}

	//Read the deskew
	string reply = converse(":CHANNEL%ld:SKEW?", channel + 1);

	//Value comes back as floating point ps
	float skew;
	sscanf(reply.c_str(), "%f", &skew);
	int64_t skew_ps = round(skew * FS_PER_SECOND);

	lock_guard<recursive_mutex> lock2(m_cacheMutex);
	m_channelDeskew[channel] = skew_ps;

	return skew_ps;
}

bool SiglentMinOscilloscope::IsInterleaving()
{
	if((m_channelsEnabled[0] == true) && (m_channelsEnabled[1] == true))
	{
		// Channel 1 and 2
		return false;
	}
	else if((m_channelsEnabled[3] == true) && (m_channelsEnabled[4] == true))
	{
		// Channel 3 and 4
		return false;
	}
	return true;
}

bool SiglentMinOscilloscope::SetInterleaving(bool /* combine*/)
{
	//Setting interleaving is not supported, it's always hardware managed
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triggering

void SiglentMinOscilloscope::PullTrigger()
{
	std::string reply;

	//Figure out what kind of trigger is active.
	reply = Trim(converse(":TRIGGER:TYPE?"));
	if(reply == "EDGE")
		PullEdgeTrigger();
	else
	{
		LogWarning("Unsupported trigger type \"%s\"\n", reply.c_str());
		m_trigger = NULL;
		return;
	}

	//Pull the source
	PullTriggerSource(m_trigger, reply);

	//TODO: holdoff
}

/**
	@brief Reads the source of a trigger from the instrument
 */
void SiglentMinOscilloscope::PullTriggerSource(Trigger* trig, string triggerModeName)
{
	string reply = Trim(converse(":TRIGGER:%s:SOURCE?", triggerModeName.c_str()));
	auto chan = GetOscilloscopeChannelByHwName(reply);
	trig->SetInput(0, StreamDescriptor(chan, 0), true);
	if(!chan)
		LogWarning("Unknown trigger source \"%s\"\n", reply.c_str());
}

/**
	@brief Reads settings for an edge trigger from the instrument
 */
void SiglentMinOscilloscope::PullEdgeTrigger()
{
	//Clear out any triggers of the wrong type
	if((m_trigger != NULL) && (dynamic_cast<EdgeTrigger*>(m_trigger) != NULL))
	{
		delete m_trigger;
		m_trigger = NULL;
	}

	//Create a new trigger if necessary
	if(m_trigger == NULL)
		m_trigger = new EdgeTrigger(this);
	EdgeTrigger* et = dynamic_cast<EdgeTrigger*>(m_trigger);

	//Level
	et->SetLevel(stof(converse(":TRIGGER:EDGE:LEVEL?")));

	//TODO: OptimizeForHF (changes hysteresis for fast signals)

	//Slope
	GetTriggerSlope(et, Trim(converse(":TRIGGER:EDGE:SLOPE?")));
}

/**
	@brief Processes the slope for an edge or edge-derived trigger
 */
void SiglentMinOscilloscope::GetTriggerSlope(EdgeTrigger* trig, string reply)

{
	reply = Trim(reply);

	if(reply == "RISing")
		trig->SetType(EdgeTrigger::EDGE_RISING);
	else if(reply == "FALLing")
		trig->SetType(EdgeTrigger::EDGE_FALLING);
	else if(reply == "ALTernate")
		trig->SetType(EdgeTrigger::EDGE_ANY);
	else
		LogWarning("Unknown trigger slope %s\n", reply.c_str());
}

/**
	@brief Pushes changes made to m_trigger to the instrument
 */
void SiglentMinOscilloscope::PushTrigger()
{
	auto et = dynamic_cast<EdgeTrigger*>(m_trigger);
	//auto pt = dynamic_cast<PulseWidthTrigger*>(m_trigger);
	//if(pt) ...
	if(et)	   //must be last
	{
		sendOnly(":TRIGGER:TYPE EDGE");
		sendOnly(":TRIGGER:EDGE:SOURCE %s", m_trigger->GetInput(0).m_channel->GetHwname().c_str());
		PushEdgeTrigger(et);
	}
}

/**
	@brief Pushes settings for an edge trigger to the instrument
 */
void SiglentMinOscilloscope::PushEdgeTrigger(EdgeTrigger* trig)
{
	if(m_modelid != MODEL_SIGLENT_SDS2000XP)
		return;

	switch(trig->GetType())
	{
		case EdgeTrigger::EDGE_RISING:
			sendOnly(":TRIGGER:EDGE:SLOPE RISING");
			break;

		case EdgeTrigger::EDGE_FALLING:
			sendOnly(":TRIGGER:EDGE:SLOPE FALLING");
			break;

		case EdgeTrigger::EDGE_ANY:
			sendOnly(":TRIGGER:EDGE:SLOPE ALTERNATE");
			break;

		default:
			LogWarning("Invalid trigger type %d\n", trig->GetType());
			break;
	}
	//Level
	sendOnly(":TRIGGER:EDGE:LEVEL %1.2E", trig->GetLevel());
}

/**
	@brief Gets a list of triggers this instrument supports
 */
vector<string> SiglentMinOscilloscope::GetTriggerTypes()
{
	vector<string> ret;
	ret.push_back(EdgeTrigger::GetTriggerName());
	return ret;
}
