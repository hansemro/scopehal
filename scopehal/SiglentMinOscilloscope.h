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

#ifndef SiglentMinOscilloscope_h
#define SiglentMinOscilloscope_h

#include <mutex>
#include <chrono>

class EdgeTrigger;

/**
	@brief A Siglent new generation scope based on linux (SDS2000X+/SDS5000/SDS6000)

 */

#define MAX_ANALOG 4
#define WAVEDESC_SIZE 346

// These SDS2000/SDS5000 scopes will actually sample 200MPoints, but the maximum it can transfer in one
// chunk is 10MPoints
// TODO(dannas): Can the Siglent SDS1104x-e really transfer 14MPoints? Update comment and constant
#define WAVEFORM_SIZE (14 * 1000 * 1000)


class SiglentMinOscilloscope
	: public virtual SCPIOscilloscope
{
public:
	SiglentMinOscilloscope(SCPITransport* transport);
	virtual ~SiglentMinOscilloscope();

	//not copyable or assignable
	SiglentMinOscilloscope(const SiglentMinOscilloscope& rhs) = delete;
	SiglentMinOscilloscope& operator=(const SiglentMinOscilloscope& rhs) = delete;

private:
	std::string converse(const char* fmt, ...);
	void sendOnly(const char* fmt, ...);

protected:
	void IdentifyHardware();
	void SharedCtorInit();
	virtual void DetectAnalogChannels();

public:
	//Device information
	virtual unsigned int GetInstrumentTypes() const override;
	virtual uint32_t GetInstrumentTypesForChannel(size_t i) const override;

	virtual void FlushConfigCache();

	//Channel configuration
	virtual bool IsChannelEnabled(size_t i);
	virtual void EnableChannel(size_t i);
	virtual bool CanEnableChannel(size_t i);
	virtual void DisableChannel(size_t i);
	virtual OscilloscopeChannel::CouplingType GetChannelCoupling(size_t i);
	virtual void SetChannelCoupling(size_t i, OscilloscopeChannel::CouplingType type);
	virtual std::vector<OscilloscopeChannel::CouplingType> GetAvailableCouplings(size_t i);
	virtual double GetChannelAttenuation(size_t i);
	virtual void SetChannelAttenuation(size_t i, double atten);
	virtual unsigned int GetChannelBandwidthLimit(size_t i);
	virtual void SetChannelBandwidthLimit(size_t i, unsigned int limit_mhz);
	virtual OscilloscopeChannel* GetExternalTrigger();
	virtual float GetChannelVoltageRange(size_t i, size_t stream);
	virtual void SetChannelVoltageRange(size_t i, size_t stream, float range);
	virtual float GetChannelOffset(size_t i, size_t stream);
	virtual void SetChannelOffset(size_t i, size_t stream, float offset);
	virtual std::string GetChannelDisplayName(size_t i);
	virtual void SetChannelDisplayName(size_t i, std::string name);
	virtual std::vector<unsigned int> GetChannelBandwidthLimiters(size_t i);
	virtual bool CanInvert(size_t i);
	virtual void Invert(size_t i, bool invert);
	virtual bool IsInverted(size_t i);

	//Triggering
	virtual Oscilloscope::TriggerMode PollTrigger();
	virtual bool AcquireData();
	virtual void Start();
	virtual void StartSingleTrigger();
	virtual void Stop();
	virtual void ForceTrigger();
	virtual bool IsTriggerArmed();
	virtual void PushTrigger();
	virtual void PullTrigger();
	virtual void EnableTriggerOutput();
	virtual std::vector<std::string> GetTriggerTypes();

	//Scope models.
	//We only distinguish down to the series of scope, exact SKU is mostly irrelevant.
	enum Model
	{
		MODEL_SIGLENT_SDS2000XP,
		MODEL_UNKNOWN
	};

	Model GetModelID() { return m_modelid; }

	//Timebase
	virtual std::vector<uint64_t> GetSampleRatesNonInterleaved();
	virtual std::vector<uint64_t> GetSampleRatesInterleaved();
	virtual std::set<InterleaveConflict> GetInterleaveConflicts();
	virtual std::vector<uint64_t> GetSampleDepthsNonInterleaved();
	virtual std::vector<uint64_t> GetSampleDepthsInterleaved();
	virtual uint64_t GetSampleRate();
	virtual uint64_t GetSampleDepth();
	virtual void SetSampleDepth(uint64_t depth);
	virtual void SetSampleRate(uint64_t rate);
	virtual void SetUseExternalRefclk(bool external);
	virtual bool IsInterleaving();
	virtual bool SetInterleaving(bool combine);

	virtual void SetTriggerOffset(int64_t offset);
	virtual int64_t GetTriggerOffset();
	virtual void SetDeskewForChannel(size_t channel, int64_t skew);
	virtual int64_t GetDeskewForChannel(size_t channel);

protected:
	//Helper
	void PullEdgeTrigger();
	//Helper
	void PullTriggerSource(Trigger* trig, std::string triggerModeName);
	//Helper
	void GetTriggerSlope(EdgeTrigger* trig, std::string reply);

	//Helper
	void PushEdgeTrigger(EdgeTrigger* trig);

	//Helper
	void BulkCheckChannelEnableState();

	//Helper
	std::string GetPossiblyEmptyString(const std::string& property);

	//Helper
	int ReadWaveformBlock(uint32_t maxsize, char* data, bool hdSizeWorkaround = false);
	//Helper
	bool ReadWavedescs(
		char wavedescs[MAX_ANALOG][WAVEDESC_SIZE], bool* enabled, unsigned int& firstEnabledChannel, bool& any_enabled);

	//Helper
	void RequestWaveforms(bool* enabled, uint32_t num_sequences, bool denabled);
	//Helper
	time_t ExtractTimestamp(unsigned char* wavedesc, double& basetime);

	//Helper
	std::vector<WaveformBase*> ProcessAnalogWaveform(const char* data,
		size_t datalen,
		char* wavedesc,
		uint32_t num_sequences,
		time_t ttime,
		double basetime,
		double* wavetime,
		int i);

	//hardware analog channel count, independent of LA option etc
	unsigned int m_analogChannelCount;

	Model m_modelid;

	///Maximum bandwidth we support, in MHz
	unsigned int m_maxBandwidth;

	bool m_triggerArmed;
	bool m_triggerOneShot;
	bool m_triggerForced;

	// Transfer buffer. This is a bit hacky
	char m_analogWaveformData[MAX_ANALOG][WAVEFORM_SIZE];
	int m_analogWaveformDataSize[MAX_ANALOG];
	char m_wavedescs[MAX_ANALOG][WAVEDESC_SIZE];

	//Cached configuration
	std::map<size_t, float> m_channelVoltageRanges;
	std::map<size_t, float> m_channelOffsets;
	std::map<int, bool> m_channelsEnabled;
	bool m_sampleRateValid;
	int64_t m_sampleRate;
	bool m_memoryDepthValid;
	int64_t m_memoryDepth;
	bool m_triggerOffsetValid;
	int64_t m_triggerOffset;
	std::map<size_t, int64_t> m_channelDeskew;
	std::map<size_t, bool> m_probeIsActive;

	//True if we have >8bit capture depth
	bool m_highDefinition;
	//True if on SDS2000X+ fw 1.3.9R6 and older
	bool m_requireSizeWorkaround;

	//Other channels
	//OscilloscopeChannel* m_extTrigChannel;
	//FunctionGeneratorChannel* m_awgChannel;
	//std::vector<OscilloscopeChannel*> m_digitalChannels;

	//Mutexing for thread safety
	std::recursive_mutex m_cacheMutex;

public:
	static std::string GetDriverNameInternal();
	OSCILLOSCOPE_INITPROC(SiglentMinOscilloscope)
};
#endif
