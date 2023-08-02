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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Declaration of SiglentBINImportFilter
 */
#ifndef SiglentBINImportFilter_h
#define SiglentBINImportFilter_h

class SiglentBINImportFilter : public ImportFilter
{
public:
	SiglentBINImportFilter(const std::string& color);

	static std::string GetProtocolName();

	PROTOCOL_DECODER_INITPROC(SiglentBINImportFilter)

	//Siglent binary capture structs
	#pragma pack(push, 1)
	struct FileHeader
	{
		uint32_t version;			//File format version
	};

	// V2/V4 wave header
	struct WaveHeader
	{
		int32_t ch1_en;				//Channel1 enable
		int32_t ch2_en;				//Channel1 enable
		int32_t ch3_en;				//Channel1 enable
		int32_t ch4_en;				//Channel1 enable
		double ch1_v_gain;			//Channel 1 vertical scale
		char reserved1[32];
		double ch2_v_gain;			//Channel 2 vertical scale
		char reserved2[32];
		double ch3_v_gain;			//Channel 3 vertical scale
		char reserved3[32];
		double ch4_v_gain;			//Channel 4 vertical scale
		char reserved4[32];
		double ch1_v_offset;		//Channel 1 vertical offset
		char reserved5[32];
		double ch2_v_offset;		//Channel 2 vertical offset
		char reserved6[32];
		double ch3_v_offset;		//Channel 3 vertical offset
		char reserved7[32];
		double ch4_v_offset;		//Channel 4 vertical offset
		char reserved8[32];
		int32_t digital_en;			//Digital enable
		int32_t d_ch_en[16];		//D0-D15 enable
		double time_div;			//Time base
		char reserved9[32];
		double time_delay;			//Trigger delay
		char reserved10[32];
		uint32_t wave_length;		//Number of samples per analog channel
		double s_rate;				//Sampling rate of analog channel
		char reserved11[32];
		uint32_t d_wave_length;		//Number of samples per digital channel
		double d_s_rate;			//Sampling rate of digital channel
		char reserved12[32];
		double ch1_probe;			//Channel 1 probe factor
		double ch2_probe;			//Channel 2 probe factor
		double ch3_probe;			//Channel 3 probe factor
		double ch4_probe;			//Channel 4 probe factor
		int8_t data_width;			//0:1 Byte, 1:2 Bytes
		int8_t byte_order;			//0:LSB, 1:MSB
		char reserved13[6];
		int32_t num_hori_div;		//Number of horizontal divisions
		int32_t ch1_codes_per_div;	//Channel 1 codes per division
		int32_t ch2_codes_per_div;	//Channel 2 codes per division
		int32_t ch3_codes_per_div;	//Channel 3 codes per division
		int32_t ch4_codes_per_div;	//Channel 4 codes per division
		int32_t math1_en;			//Math 1 channel enable
		int32_t math2_en;			//Math 2 channel enable
		int32_t math3_en;			//Math 3 channel enable
		int32_t math4_en;			//Math 4 channel enable
		double math1_v_gain;		//Math 1 vertical gain
		char reserved14[32];
		double math2_v_gain;		//Math 2 vertical gain
		char reserved15[32];
		double math3_v_gain;		//Math 3 vertical gain
		char reserved16[32];
		double math4_v_gain;		//Math 4 vertical gain
		char reserved17[32];
		double math1_v_offset;		//Math 1 vertical offset
		char reserved18[32];
		double math2_v_offset;		//Math 2 vertical offset
		char reserved19[32];
		double math3_v_offset;		//Math 3 vertical offset
		char reserved20[32];
		double math4_v_offset;		//Math 4 vertical offset
		char reserved21[32];
		uint32_t math1_wave_length;	//Number of Math 1 channel samples
		uint32_t math2_wave_length;	//Number of Math 2 channel samples
		uint32_t math3_wave_length;	//Number of Math 3 channel samples
		uint32_t math4_wave_length;	//Number of Math 4 channel samples
		double math1_s_period;		//Sampling interval of Math 1
		double math2_s_period;		//Sampling interval of Math 2
		double math3_s_period;		//Sampling interval of Math 3
		double math4_s_period;		//Sampling interval of Math 4
		int32_t math_codes_per_div;	//Math 1 codes per division
	};

	struct SiglentAnalogChannel
	{
		int32_t ch_num;
		double v_gain;
		double v_offset;
		double probe_factor;
		int32_t codes_per_div;
		bool enabled;
	};

	struct SiglentMathChannel
	{
		int32_t ch_num;
		double v_gain;
		double v_offset;
		double s_period;
		int32_t codes_per_div;
		bool enabled;
	};
	#pragma pack(pop)

protected:
	void OnFileNameChanged();
};

#endif
