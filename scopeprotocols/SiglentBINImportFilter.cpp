/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
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

#include "../scopehal/scopehal.h"
#include "SiglentBINImportFilter.h"

#ifdef __x86_64__
#include <immintrin.h>
#endif
#include <omp.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SiglentBINImportFilter::SiglentBINImportFilter(const string& color)
	: ImportFilter(color)
{
	m_fpname = "Siglent (V2/V4) BIN File";
	m_parameters[m_fpname] = FilterParameter(FilterParameter::TYPE_FILENAME, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_fpname].m_fileFilterMask = "*.bin";
	m_parameters[m_fpname].m_fileFilterName = "V2/V4 Siglent binary waveform files (*.bin)";
	m_parameters[m_fpname].signal_changed().connect(sigc::mem_fun(*this, &SiglentBINImportFilter::OnFileNameChanged));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string SiglentBINImportFilter::GetProtocolName()
{
	return "Siglent (V2/V4) BIN Import";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Convert 1-bit Digital Samples to Bool Array

void SiglentBINImportFilter::ConvertDigitalSamples(bool* pout, uint8_t* pin, size_t count)
{
	//Divide large waveforms (>1M points) into blocks and multithread them
	if(count > (1000000/8))
	{
		//Round blocks to multiples of 32 samples (4 bytes) for clean vectorization
		size_t numblocks = omp_get_max_threads();
		size_t lastblock = numblocks - 1;
		size_t blocksize = count / numblocks;
		blocksize = blocksize - (blocksize % 4);

		#pragma omp parallel for
		for(size_t i=0; i<numblocks; i++)
		{
			//Last block gets any extra that didn't divide evenly
			size_t nsamp = blocksize;
			if(i == lastblock)
				nsamp = count - i*blocksize;

			size_t off = i*blocksize;

			#ifdef __x86_64__
			if(g_hasAvx2)
			{
				ConvertDigitalSamplesAVX2(
					pout + off,
					pin + off,
					nsamp);
			}
			else
			#endif
			{
				ConvertDigitalSamplesGeneric(
					pout + off,
					pin + off,
					nsamp);
			}
		}
	}

	//Small waveforms get done single threaded to avoid overhead
	else
	{
		#ifdef __x86_64__
		if(g_hasAvx2)
			ConvertDigitalSamplesAVX2(pout, pin, count);
		else
		#endif
			ConvertDigitalSamplesGeneric(pout, pin, count);
	}
}

void SiglentBINImportFilter::ConvertDigitalSamplesGeneric(bool* pout, uint8_t* pin, size_t count)
{
	for(size_t i = 0; i < count; i++)
	{
		for(int j = 0; j < 8; j++)
		{
			pout[8*i + j] = (pin[i] >> j) & 0x1;
		}
	}
}

#ifdef __x86_64__
__attribute__((target("avx2")))
void SiglentBINImportFilter::ConvertDigitalSamplesAVX2(bool* pout, uint8_t* pin, size_t count)
{
	unsigned int end = count - (count % 4);

	//Mask to get n-th bit in n-th byte, where n is in 0..7.
	//Broadcast mask for 4 blocks (with 4x8=32 samples);
	const __m256i bitmask = _mm256_set1_epi64x(0x8040201008040201);

	//Mask to get first bit of each byte
	const __m256i ones = _mm256_set1_epi8(0x1);

	for(unsigned int k = 0; k < end; k += 4)
	{
		//Each block contains 8 samples
		uint8_t block0 = pin[k];
		uint8_t block1 = pin[k + 1];
		uint8_t block2 = pin[k + 2];
		uint8_t block3 = pin[k + 3];

		//Broadcast each block 8 times (such that each sample occupies its own byte)
		__m256i bc_samps = _mm256_set_epi8( block3, block3, block3, block3,
											block3, block3, block3, block3,
											block2, block2, block2, block2,
											block2, block2, block2, block2,
											block1, block1, block1, block1,
											block1, block1, block1, block1,
											block0, block0, block0, block0,
											block0, block0, block0, block0);

		//Extract nth bit of nth byte for each block
		__m256i result = _mm256_and_si256(bc_samps, bitmask);

		//Fills each byte with 1s if it matches bitmask
		result = _mm256_cmpeq_epi8(result, bitmask);

		//Mask to get first bit of each byte. This gives us our clean bool array!
		result = _mm256_and_si256(result, ones);

		//Store results
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(pout + (k*8)), result);
	}

	//Get any extras we didn't get in the SIMD loop
	for(size_t i = end; i < count; i++)
	{
		for(size_t j = 0; j < 8; j++)
		{
			pout[8*i + j] = (pin[i] >> j) & 0x1;
		}
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void SiglentBINImportFilter::OnFileNameChanged()
{
	//Wipe anything we may have had in the past
	ClearStreams();

	auto fname = m_parameters[m_fpname].ToString();
	if(fname.empty())
		return;

	//Set waveform timestamp to file timestamp
	time_t timestamp = 0;
	int64_t fs = 0;
	GetTimestampOfFile(fname, timestamp, fs);

	string f = ReadFile(fname);
	uint32_t fpos = 0;

	FileHeader fh;
	f.copy((char*)&fh, sizeof(FileHeader), fpos);
	fpos += sizeof(FileHeader);

	switch(fh.version)
	{
		case 2:
			break;
		case 4:
			fpos += 4;
			break;
		default:
			LogError("Unsupported version (%d) in file header\n", fh.version);
			return;
	}

	LogDebug("Version: %d\n", fh.version);

	//Parse waveform header
	WaveHeader wh;
	f.copy((char*)&wh, sizeof(WaveHeader), fpos);
	fpos += sizeof(WaveHeader);

	for(int i = 0; i < 4; i++)
	{
		LogDebug("ch%d_en: %d\n", i+1, wh.ch_en[i]);
		LogDebug("ch%d_v_gain: %f\n", i+1, wh.ch_v_gain[i].value);
		LogDebug("ch%d_v_offset: %f\n", i+1, wh.ch_v_offset[i].value);
		LogDebug("ch%d_probe: %f\n", i+1, wh.ch_probe[i]);
		LogDebug("ch%d_codes_per_div: %d\n", i+1, wh.ch_codes_per_div[i]);
	}

	LogDebug("digital_en: %d\n", wh.digital_en);
	for(int i = 0; i < 16; i++)
	{
		LogDebug("d%d_ch_en: %d\n", i, wh.d_ch_en[i]);
	}

	LogDebug("time_div: %f\n", wh.time_div);
	LogDebug("time_delay: %f\n", wh.time_delay);
	LogDebug("wave_length: %d\n", wh.wave_length);
	LogDebug("s_rate: %f\n", wh.s_rate);
	LogDebug("d_wave_length: %d\n", wh.d_wave_length);
	LogDebug("d_s_rate: %f\n", wh.d_s_rate);

	LogDebug("data_width: %d\n", wh.data_width);
	LogDebug("byte_order: %d\n", wh.byte_order);
	LogDebug("num_hori_div: %d\n", wh.num_hori_div);

	for(int i = 0; i < 4; i++)
	{
		LogDebug("math%d_en: %d\n", i+1, wh.math_en[i]);
		LogDebug("math%d_v_gain: %f\n", i+1, wh.math_v_gain[i].value);
		LogDebug("math%d_v_offset: %f\n", i+1, wh.math_v_offset[i].value);
		LogDebug("math%d_wave_length: %d\n", i+1, wh.math_wave_length[i]);
		LogDebug("math%d_s_interval: %f\n", i+1, wh.math_s_interval[i]);
	}
	LogDebug("math_codes_per_div: %d\n", wh.math_codes_per_div);

	switch(fh.version)
	{
		case 2:
			fpos = 0x800;
			break;
		case 4:
			fpos = 0x1000;
			break;
		default:
			LogError("Unsupported version (%d) in file header\n", fh.version);
			return;
	}

	//Process analog data
	uint32_t data_width = wh.data_width + 1; // number of bytes
	int32_t center_code = (1 << (8*data_width - 1)) - 1;

	uint32_t wave_idx = 0;
	for(int i = 0; i < 4; i++)
	{
		if(wh.ch_en[i] == 1)
		{
			string name = string("C") + to_string(i+1);
			AddStream(Unit(Unit::UNIT_VOLTS), name, Stream::STREAM_TYPE_ANALOG);
			auto wfm = new UniformAnalogWaveform;
			wfm->m_timescale = round(FS_PER_SECOND / wh.s_rate);
			wfm->m_startTimestamp = timestamp * FS_PER_SECOND;
			wfm->m_startFemtoseconds = fs;
			wfm->m_triggerPhase = 0;
			wfm->PrepareForCpuAccess();
			SetData(wfm, m_streams.size() - 1);
			wfm->Resize(wh.wave_length);

			LogDebug("Waveform[%d]: %s\n", wave_idx, name.c_str());
			double v_gain = wh.ch_v_gain[i].value * wh.ch_probe[i] / wh.ch_codes_per_div[i];
			LogDebug("\tv_gain: %f\n", v_gain);
			LogDebug("\tcenter: %d\n", center_code);

			if(data_width == 2)
			{
				Oscilloscope::ConvertUnsigned16BitSamples(
					wfm->m_samples.GetCpuPointer(),
					(uint16_t*)(f.c_str() + fpos),
					v_gain,
					v_gain * center_code + wh.ch_v_offset[i].value,
					wh.wave_length);
				fpos += 2 * wh.wave_length;
			}
			else
			{
				Oscilloscope::ConvertUnsigned8BitSamples(
					wfm->m_samples.GetCpuPointer(),
					(uint8_t*)(f.c_str() + fpos),
					v_gain,
					v_gain * center_code + wh.ch_v_offset[i].value,
					wh.wave_length);
				fpos += wh.wave_length;
			}

			wfm->MarkModifiedFromCpu();
			wave_idx += 1;
		}
	}

	//Process math data
	for(int i = 0; i < 4; i++)
	{
		if(wh.math_en[i] == 1)
		{
			string name = string("F") + to_string(i+1);
			AddStream(Unit(Unit::UNIT_VOLTS), name, Stream::STREAM_TYPE_ANALOG);
			auto wfm = new UniformAnalogWaveform;
			wfm->m_timescale = round(wh.math_s_interval[i] * FS_PER_SECOND);
			wfm->m_startTimestamp = timestamp * FS_PER_SECOND;
			wfm->m_startFemtoseconds = fs;
			wfm->m_triggerPhase = 0;
			wfm->PrepareForCpuAccess();
			SetData(wfm, m_streams.size() - 1);
			wfm->Resize(wh.math_wave_length[i]);

			LogDebug("Waveform[%d]: %s\n", wave_idx, name.c_str());
			double v_gain = wh.math_v_gain[i].value / wh.math_codes_per_div;
			LogDebug("\tv_gain: %f\n", v_gain);
			LogDebug("\tcenter: %d\n", center_code);

			if(data_width == 2)
			{
				Oscilloscope::ConvertUnsigned16BitSamples(
					wfm->m_samples.GetCpuPointer(),
					(uint16_t*)(f.c_str() + fpos),
					v_gain,
					v_gain * center_code + wh.math_v_offset[i].value,
					wh.math_wave_length[i]);
				fpos += 2 * wh.math_wave_length[i];
			}
			else
			{
				Oscilloscope::ConvertUnsigned8BitSamples(
					wfm->m_samples.GetCpuPointer(),
					(uint8_t*)(f.c_str() + fpos),
					v_gain,
					v_gain * center_code + wh.math_v_offset[i].value,
					wh.math_wave_length[i]);
				fpos += wh.math_wave_length[i];
			}

			wfm->MarkModifiedFromCpu();
			wave_idx += 1;
		}
	}

	//Process digital data
	if(wh.digital_en)
	{
		for(int i = 0; i < 16; i++)
		{
			if(wh.d_ch_en[i] == 1)
			{
				string name = string("D") + to_string(i);
				AddStream(Unit(Unit::UNIT_VOLTS), name, Stream::STREAM_TYPE_DIGITAL);
				auto wfm = new UniformDigitalWaveform;
				wfm->m_timescale = round(FS_PER_SECOND / wh.d_s_rate);
				wfm->m_startTimestamp = timestamp * FS_PER_SECOND;
				wfm->m_startFemtoseconds = fs;
				wfm->m_triggerPhase = 0;
				wfm->PrepareForCpuAccess();
				SetData(wfm, m_streams.size() - 1);
				wfm->Resize(wh.d_wave_length);

				LogDebug("Waveform[%d]: %s\n", wave_idx, name.c_str());
				ConvertDigitalSamples(
					wfm->m_samples.GetCpuPointer(),
					(uint8_t*)(f.c_str() + fpos),
					wh.d_wave_length / 8);
				fpos += wh.d_wave_length / 8;

				wfm->MarkModifiedFromCpu();
				wave_idx += 1;
			}
		}
	}

	m_outputsChangedSignal.emit();
}
