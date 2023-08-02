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

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SiglentBINImportFilter::SiglentBINImportFilter(const string& color)
	: ImportFilter(color)
{
	m_fpname = "Siglent BIN File";
	m_parameters[m_fpname] = FilterParameter(FilterParameter::TYPE_FILENAME, Unit(Unit::UNIT_COUNTS));
	m_parameters[m_fpname].m_fileFilterMask = "*.bin";
	m_parameters[m_fpname].m_fileFilterName = "V2/V4 Siglent binary waveform files (*.bin)";
	m_parameters[m_fpname].signal_changed().connect(sigc::mem_fun(*this, &SiglentBINImportFilter::OnFileNameChanged));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string SiglentBINImportFilter::GetProtocolName()
{
	return "Siglent BIN Import";
}

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
			LogError("Unknown version in file header");
			return;
	}
	
	LogDebug("Version: %d\n", fh.version);
	LogDebug("fpos: %d\n", fpos);

	//Parse waveform header
	WaveHeader wh;
	f.copy((char*)&wh, sizeof(WaveHeader), fpos);
	fpos += sizeof(WaveHeader);

	SiglentAnalogChannel analog_ch[4] = {
		{
			1,
			wh.ch1_v_gain,
			wh.ch1_v_offset,
			wh.ch1_probe,
			wh.ch1_codes_per_div,
			wh.ch1_en == 1,
		},
		{
			2,
			wh.ch2_v_gain,
			wh.ch2_v_offset,
			wh.ch2_probe,
			wh.ch2_codes_per_div,
			wh.ch2_en == 1,
		},
		{
			3,
			wh.ch3_v_gain,
			wh.ch3_v_offset,
			wh.ch3_probe,
			wh.ch3_codes_per_div,
			wh.ch3_en == 1,
		},
		{
			4,
			wh.ch4_v_gain,
			wh.ch4_v_offset,
			wh.ch4_probe,
			wh.ch4_codes_per_div,
			wh.ch4_en == 1,
		}
	};

	SiglentMathChannel math_ch[4] = {
		{
			1,
			wh.math1_v_gain,
			wh.math1_v_offset,
			wh.math_codes_per_div,
			wh.math1_en == 1,
		},
		{
			2,
			wh.math2_v_gain,
			wh.math2_v_offset,
			wh.math_codes_per_div,
			wh.math2_en == 1,
		},
		{
			3,
			wh.math3_v_gain,
			wh.math3_v_offset,
			wh.math_codes_per_div,
			wh.math3_en == 1,
		},
		{
			4,
			wh.math4_v_gain,
			wh.math4_v_offset,
			wh.math_codes_per_div,
			wh.math4_en == 1,
		}
	};

	LogDebug("sizeof(WaveHeader): %ld\n", sizeof(WaveHeader));
	LogDebug("ch1_en: %d (%p)\n", wh.ch1_en, (void*)&wh.ch1_en);
	LogDebug("ch2_en: %d\n", wh.ch2_en);
	LogDebug("ch3_en: %d\n", wh.ch3_en);
	LogDebug("ch4_en: %d\n", wh.ch4_en);
	LogDebug("ch1_v_gain: %f\n", wh.ch1_v_gain);
	LogDebug("ch2_v_gain: %f\n", wh.ch2_v_gain);
	LogDebug("ch3_v_gain: %f\n", wh.ch3_v_gain);
	LogDebug("ch4_v_gain: %f\n", wh.ch4_v_gain);

	LogDebug("digital_en: %d\n", wh.digital_en);
	LogDebug("d_ch_en[ 0]: %d\n", wh.d_ch_en[ 0]);
	LogDebug("d_ch_en[ 1]: %d\n", wh.d_ch_en[ 1]);
	LogDebug("d_ch_en[ 2]: %d\n", wh.d_ch_en[ 2]);
	LogDebug("d_ch_en[ 3]: %d\n", wh.d_ch_en[ 3]);
	LogDebug("d_ch_en[ 4]: %d\n", wh.d_ch_en[ 4]);
	LogDebug("d_ch_en[ 5]: %d\n", wh.d_ch_en[ 5]);
	LogDebug("d_ch_en[ 6]: %d\n", wh.d_ch_en[ 6]);
	LogDebug("d_ch_en[ 7]: %d\n", wh.d_ch_en[ 7]);
	LogDebug("d_ch_en[ 8]: %d\n", wh.d_ch_en[ 8]);
	LogDebug("d_ch_en[ 9]: %d\n", wh.d_ch_en[ 9]);
	LogDebug("d_ch_en[10]: %d\n", wh.d_ch_en[10]);
	LogDebug("d_ch_en[11]: %d\n", wh.d_ch_en[11]);
	LogDebug("d_ch_en[12]: %d\n", wh.d_ch_en[12]);
	LogDebug("d_ch_en[13]: %d\n", wh.d_ch_en[13]);
	LogDebug("d_ch_en[14]: %d\n", wh.d_ch_en[14]);
	LogDebug("d_ch_en[15]: %d\n", wh.d_ch_en[15]);

	LogDebug("time_div: %f\n", wh.time_div);
	LogDebug("time_delay: %f\n", wh.time_delay);
	LogDebug("wave_length: %d\n", wh.wave_length);
	LogDebug("s_rate: %f\n", wh.s_rate);
	LogDebug("d_wave_length: %d\n", wh.d_wave_length);
	LogDebug("d_s_rate: %f\n", wh.d_s_rate);

	LogDebug("ch1_probe: %f\n", wh.ch1_probe);
	LogDebug("ch2_probe: %f\n", wh.ch2_probe);
	LogDebug("ch3_probe: %f\n", wh.ch3_probe);
	LogDebug("ch4_probe: %f\n", wh.ch4_probe);

	LogDebug("data_width: %d\n", wh.data_width);
	LogDebug("byte_order: %d\n", wh.byte_order);
	LogDebug("num_hori_div: %d\n", wh.num_hori_div);
	LogDebug("ch1_codes_per_div: %d\n", wh.ch1_codes_per_div);
	LogDebug("ch2_codes_per_div: %d\n", wh.ch2_codes_per_div);
	LogDebug("ch3_codes_per_div: %d\n", wh.ch3_codes_per_div);
	LogDebug("ch4_codes_per_div: %d\n", wh.ch4_codes_per_div);

	LogDebug("math1_en: %d\n", wh.math1_en);
	LogDebug("math2_en: %d\n", wh.math2_en);
	LogDebug("math3_en: %d\n", wh.math3_en);
	LogDebug("math4_en: %d\n", wh.math4_en);
	LogDebug("math1_v_gain: %f\n", wh.math1_v_gain);
	LogDebug("math2_v_gain: %f\n", wh.math2_v_gain);
	LogDebug("math3_v_gain: %f\n", wh.math3_v_gain);
	LogDebug("math4_v_gain: %f\n", wh.math4_v_gain);
	LogDebug("math1_v_offset: %f\n", wh.math1_v_offset);
	LogDebug("math2_v_offset: %f\n", wh.math2_v_offset);
	LogDebug("math3_v_offset: %f\n", wh.math3_v_offset);
	LogDebug("math4_v_offset: %f\n", wh.math4_v_offset);
	LogDebug("math1_wave_length: %d\n", wh.math1_wave_length);
	LogDebug("math2_wave_length: %d\n", wh.math2_wave_length);
	LogDebug("math3_wave_length: %d\n", wh.math3_wave_length);
	LogDebug("math4_wave_length: %d\n", wh.math4_wave_length);
	LogDebug("math1_s_period: %f\n", wh.math1_s_period);
	LogDebug("math2_s_period: %f\n", wh.math2_s_period);
	LogDebug("math3_s_period: %f\n", wh.math3_s_period);
	LogDebug("math4_s_period: %f\n", wh.math4_s_period);
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
			LogError("Unknown version in file header");
			return;
	}

	//Process analog data
	uint32_t data_width = wh.data_width + 1; // number of bytes
	uint32_t data_length = data_width * wh.wave_length; // length of waveform data in bytes
	uint32_t center_code = (1 << (8*data_width - 1)) - 1;

	uint32_t wave_idx = 0;
	for(int i = 0; i < 4; i++)
	{
		if(analog_ch[i].enabled)
		{
			string name = string("C") + to_string(i+1);

			AddStream(Unit(Unit::UNIT_VOLTS), name, Stream::STREAM_TYPE_ANALOG);
			auto wfm = new UniformAnalogWaveform;
			wfm->m_timescale = FS_PER_SECOND / wh.s_rate;
			wfm->m_startTimestamp = timestamp * FS_PER_SECOND;
			wfm->m_startFemtoseconds = fs;
			wfm->m_triggerPhase = 0;
			wfm->PrepareForCpuAccess();
			SetData(wfm, m_streams.size() - 1);

			LogDebug("timescale: %f\n", FS_PER_SECOND / wh.s_rate);
			LogDebug("timestamp : %f\n", timestamp * FS_PER_SECOND);

			LogDebug("%s Waveform\n", name.c_str());
			uint32_t start = wave_idx * data_length + fpos;
			uint32_t end = start + data_length;
			LogDebug("\tstart: %d\n", start);
			LogDebug("\tend: %d\n", end);
			double v_gain = analog_ch[i].v_gain * analog_ch[i].probe_factor / analog_ch[i].codes_per_div;
			LogDebug("\tv_gain: %f\n", v_gain);
			LogDebug("\tcenter: %d\n", center_code);

			if(data_width == 2)
			{
				for(size_t j = 0; j < wh.wave_length; j++)
				{
					uint16_t* sample = (uint16_t*)(f.c_str() + fpos);
					//LogDebug("\tfpos: %d\n", fpos);
					//LogDebug("\tsample[%ld]: %d\n", j, *sample);
					float value = (((int32_t)*sample - (int32_t)center_code)) * v_gain - analog_ch[i].v_offset;
					//LogDebug("\tvalue[%ld]: %f\n", j, value);
					wfm->m_samples.push_back(value);
					fpos += 2;
				}
			}
			else
			{
				for(size_t j = 0; j < wh.wave_length; j++)
				{
					uint8_t* sample = (uint8_t*)(f.c_str() + fpos);
					//LogDebug("\tfpos: %d\n", fpos);
					//LogDebug("\tsample[%ld]: %d\n", j, *sample);
					float value = ((int16_t)*sample - (int16_t)center_code) * v_gain - analog_ch[i].v_offset;
					//LogDebug("\tvalue[%ld]: %f\n", j, value);
					wfm->m_samples.push_back(value);
					fpos += 1;
				}
			}

			wfm->MarkModifiedFromCpu();
			AutoscaleVertical(wave_idx);

			wave_idx += 1;
		}
	}

	//TODO: Process math data
	wave_idx = 0;
	for(int i = 0; i < 4; i++)
	{
		if(math_ch[i].enabled)
		{
			wave_idx += 1;
		}
	}

	//TODO: Process digital data
	wave_idx = 0;
	if(wh.digital_en)
	{
		for(int i = 0; i < 16; i++)
		{
			wave_idx += 1;
		}
	}

	m_outputsChangedSignal.emit();
}
