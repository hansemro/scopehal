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
	@brief Main library include file
 */

#ifndef scopeprotocols_h
#define scopeprotocols_h

#include "../scopehal/scopehal.h"
#include "../scopehal/Filter.h"

#include "ACCoupleFilter.h"
#include "AddFilter.h"
#include "ACRMSMeasurement.h"
#include "ADL5205Decoder.h"
#include "AreaMeasurement.h"
#include "AutocorrelationFilter.h"
#include "BandwidthMeasurement.h"
#include "BaseMeasurement.h"
#include "BINImportFilter.h"
#include "BurstWidthMeasurement.h"
#include "CANDecoder.h"
#include "ChannelEmulationFilter.h"
#include "ClipFilter.h"
#include "ClockRecoveryFilter.h"
#include "ComplexImportFilter.h"
#include "ConstantFilter.h"
#include "CSVExportFilter.h"
#include "CSVImportFilter.h"
#include "CTLEFilter.h"
#include "CurrentShuntFilter.h"
#include "DCDMeasurement.h"
#include "DDJMeasurement.h"
#include "DDR1Decoder.h"
#include "DDR3Decoder.h"
#include "DeEmbedFilter.h"
#include "DeskewFilter.h"
#include "DigitalToPAM4Filter.h"
#include "DigitalToNRZFilter.h"
#include "DivideFilter.h"
#include "DownconvertFilter.h"
#include "DownsampleFilter.h"
#include "DPAuxChannelDecoder.h"
#include "DPhyDataDecoder.h"
#include "DPhyEscapeModeDecoder.h"
#include "DPhyHSClockRecoveryFilter.h"
#include "DPhySymbolDecoder.h"
#include "DramClockFilter.h"
#include "DramRefreshActivateMeasurement.h"
#include "DramRowColumnLatencyMeasurement.h"
#include "DSIFrameDecoder.h"
#include "DSIPacketDecoder.h"
#include "DutyCycleMeasurement.h"
#include "DVIDecoder.h"
#include "EmphasisFilter.h"
#include "EmphasisRemovalFilter.h"
#include "EnhancedResolutionFilter.h"
#include "EnvelopeFilter.h"
#include "ESPIDecoder.h"
#include "EthernetProtocolDecoder.h"		//must be before all other ethernet decodes
#include "EthernetAutonegotiationDecoder.h"
#include "EthernetAutonegotiationPageDecoder.h"
#include "EthernetBaseXAutonegotiationDecoder.h"
#include "EthernetGMIIDecoder.h"
#include "EthernetRGMIIDecoder.h"
#include "EthernetRMIIDecoder.h"
#include "EthernetSGMIIDecoder.h"
#include "Ethernet10BaseTDecoder.h"
#include "Ethernet100BaseTXDecoder.h"
#include "Ethernet1000BaseXDecoder.h"
#include "Ethernet10GBaseRDecoder.h"
#include "Ethernet64b66bDecoder.h"
#include "EyeBitRateMeasurement.h"
#include "EyePattern.h"
#include "EyeHeightMeasurement.h"
#include "EyeJitterMeasurement.h"
#include "EyePeriodMeasurement.h"
#include "EyeWidthMeasurement.h"
#include "FallMeasurement.h"
#include "FSKDecoder.h"
#include "FullWidthHalfMax.h"
#include "GlitchRemovalFilter.h"
#include "FFTFilter.h"
#include "FIRFilter.h"
#include "FrequencyMeasurement.h"
#include "GroupDelayFilter.h"
#include "HistogramFilter.h"
#include "HorizontalBathtub.h"
#include "HyperRAMDecoder.h"
#include "I2CDecoder.h"
#include "I2CEepromDecoder.h"
#include "I2CRegisterDecoder.h"
#include "IBM8b10bDecoder.h"
#include "IBISDriverFilter.h"
#include "InvertFilter.h"
#include "IPv4Decoder.h"
#include "IQSquelchFilter.h"
#include "ISIMeasurement.h"
#include "JitterFilter.h"
#include "JitterSpectrumFilter.h"
#include "JtagDecoder.h"
#include "MagnitudeFilter.h"
#include "MDIODecoder.h"
#include "MemoryFilter.h"
#include "MilStd1553Decoder.h"
#include "MovingAverageFilter.h"
#include "MultiplyFilter.h"
#include "NoiseFilter.h"
#include "OFDMDemodulator.h"
#include "OneWireDecoder.h"
#include "OvershootMeasurement.h"
#include "ParallelBus.h"
#include "PAM4DemodulatorFilter.h"
#include "PCIe128b130bDecoder.h"
#include "PCIeDataLinkDecoder.h"
#include "PCIeGen2LogicalDecoder.h"
#include "PCIeGen3LogicalDecoder.h"
#include "PCIeLinkTrainingDecoder.h"
#include "PCIeTransportDecoder.h"
#include "PeakHoldFilter.h"
#include "PeriodMeasurement.h"
#include "PhaseMeasurement.h"
#include "PhaseNonlinearityFilter.h"
#include "PkPkMeasurement.h"
#include "PRBSCheckerFilter.h"
#include "PRBSGeneratorFilter.h"
#include "PulseWidthMeasurement.h"
#include "ReferencePlaneExtensionFilter.h"
#include "RjBUjFilter.h"
#include "QSGMIIDecoder.h"
#include "QSPIDecoder.h"
#include "QuadratureDecoder.h"
#include "RiseMeasurement.h"
#include "ScalarStairstepFilter.h"
#include "ScaleFilter.h"
#include "SDCmdDecoder.h"
#include "SDDataDecoder.h"
#include "SiglentBINImportFilter.h"
#include "SNRFilter.h"
#include "SParameterCascadeFilter.h"
#include "SParameterDeEmbedFilter.h"
#include "SpectrogramFilter.h"
#include "SPIDecoder.h"
#include "SPIFlashDecoder.h"
#include "SquelchFilter.h"
#include "StepGeneratorFilter.h"
#include "SubtractFilter.h"
#include "SWDDecoder.h"
#include "SWDMemAPDecoder.h"
#include "TachometerFilter.h"
#include "TappedDelayLineFilter.h"
#include "TCPDecoder.h"
#include "TDRFilter.h"
#include "TDRStepDeEmbedFilter.h"
#include "ThermalDiodeFilter.h"
#include "ThresholdFilter.h"
#include "TIEMeasurement.h"
#include "TimeOutsideLevelMeasurement.h"
#include "TMDSDecoder.h"
#include "ToneGeneratorFilter.h"
#include "TopMeasurement.h"
#include "TouchstoneExportFilter.h"
#include "TouchstoneImportFilter.h"
#include "TRCImportFilter.h"
#include "TrendFilter.h"
#include "UARTDecoder.h"
#include "UartClockRecoveryFilter.h"
#include "UndershootMeasurement.h"
#include "UnwrappedPhaseFilter.h"
#include "UpsampleFilter.h"
#include "USB2ActivityDecoder.h"
#include "USB2PacketDecoder.h"
#include "USB2PCSDecoder.h"
#include "USB2PMADecoder.h"
#include "VCDImportFilter.h"
#include "VectorFrequencyFilter.h"
#include "VectorPhaseFilter.h"
#include "VerticalBathtub.h"
#include "VICPDecoder.h"
#include "Waterfall.h"
#include "WAVImportFilter.h"
#include "WFMImportFilter.h"
#include "WindowedAutocorrelationFilter.h"
#include "WindowFilter.h"

#include "AverageStatistic.h"
#include "MaximumStatistic.h"
#include "MinimumStatistic.h"

void ScopeProtocolStaticInit();

#endif
