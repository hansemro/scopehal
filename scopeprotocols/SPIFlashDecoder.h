/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of SPIFlashDecoder
 */
#ifndef SPIFlashDecoder_h
#define SPIFlashDecoder_h

class SPIFlashSymbol
{
public:

	enum FlashType
	{
		TYPE_COMMAND,
		TYPE_ADDRESS,
		TYPE_DATA,

		TYPE_DUMMY,

		//Winbond W25N specific
		TYPE_W25N_BLOCK_ADDR,
		TYPE_W25N_SR_ADDR,			//address of a status register
		TYPE_W25N_SR_STATUS,
		TYPE_W25N_SR_CONFIG,
		TYPE_W25N_SR_PROT

	} m_type;

	enum FlashCommand
	{
		CMD_READ_STATUS_REGISTER,
		CMD_WRITE_STATUS_REGISTER,
		CMD_READ_JEDEC_ID,
		CMD_READ,			//Read, SPI address, SPI data
		CMD_READ_1_1_4,		//Fast read, SPI address, QSPI data
		CMD_READ_1_4_4,		//Fast read, QSPI address, QSPI data
		CMD_RESET,

		//Winbond W25N specific
		CMD_W25N_READ_PAGE,

		CMD_UNKNOWN
	} m_cmd;

	uint32_t m_data;

	SPIFlashSymbol()
	{}

	SPIFlashSymbol(FlashType type, FlashCommand cmd, uint32_t data)
	 : m_type(type)
	 , m_cmd(cmd)
	 , m_data(data)
	{}

	bool operator== (const SPIFlashSymbol& s) const
	{
		return (m_type == s.m_type) && (m_cmd == s.m_cmd) && (m_data == s.m_data);
	}
};

typedef Waveform<SPIFlashSymbol> SPIFlashWaveform;

class SPIFlashDecoder : public Filter
{
public:
	SPIFlashDecoder(std::string color);

	virtual std::string GetText(int i);
	virtual Gdk::Color GetColor(int i);

	virtual void Refresh();

	virtual bool NeedsConfig();
	virtual bool IsOverlay();

	static std::string GetProtocolName();
	virtual void SetDefaultName();

	virtual bool ValidateChannel(size_t i, StreamDescriptor stream);

	PROTOCOL_DECODER_INITPROC(SPIFlashDecoder)
};

#endif