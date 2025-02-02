/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the
"Apache License"); you may not use this file except in compliance with the
Apache License. You may obtain a copy of the Apache License at
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Copyright (c) 2022 Audiokinetic Inc.
*******************************************************************************/

#include "ToneFilterFX.h"
#include "../ToneFilterConfig.h"

#include <AK/AkWwiseSDKVersion.h>

AK::IAkPlugin *CreateToneFilterFX(AK::IAkPluginMemAlloc *in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, ToneFilterFX());
}

AK::IAkPluginParam *CreateToneFilterFXParams(AK::IAkPluginMemAlloc *in_pAllocator)
{
    return AK_PLUGIN_NEW(in_pAllocator, ToneFilterFXParams());
}

AK_IMPLEMENT_PLUGIN_FACTORY(ToneFilterFX, AkPluginTypeEffect, ToneFilterConfig::CompanyID, ToneFilterConfig::PluginID)

ToneFilterFX::ToneFilterFX()
    : m_pParams(nullptr), m_pAllocator(nullptr), m_pContext(nullptr)
{
}

ToneFilterFX::~ToneFilterFX()
{
}

AKRESULT ToneFilterFX::Init(AK::IAkPluginMemAlloc *in_pAllocator, AK::IAkEffectPluginContext *in_pContext, AK::IAkPluginParam *in_pParams, AkAudioFormat &in_rFormat)
{
    m_pParams = (ToneFilterFXParams *)in_pParams;
    m_pAllocator = in_pAllocator;
    m_pContext = in_pContext;

    const auto numChannels = in_rFormat.GetNumChannels();
    for (size_t ch = 0; ch < numChannels; ch++)
    {
        filterMatrix.emplace_back(FilterArray());
        for (size_t i = 0; i < 48; i++)
        {
            filterMatrix[ch][i].setCoefficients(in_rFormat.uSampleRate, freqList[i], 40.0f);
        }
    }

    return AK_Success;
}

AKRESULT ToneFilterFX::Term(AK::IAkPluginMemAlloc *in_pAllocator)
{
    AK_PLUGIN_DELETE(in_pAllocator, this);
    return AK_Success;
}

AKRESULT ToneFilterFX::Reset()
{
    return AK_Success;
}

AKRESULT ToneFilterFX::GetPluginInfo(AkPluginInfo &out_rPluginInfo)
{
    out_rPluginInfo.eType = AkPluginTypeEffect;
    out_rPluginInfo.bIsInPlace = true;
    out_rPluginInfo.bCanProcessObjects = false;
    out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
    return AK_Success;
}

void ToneFilterFX::Execute(AkAudioBuffer *io_pBuffer)
{
    const auto numChannels = io_pBuffer->NumChannels();
    const auto numSamples = io_pBuffer->uValidFrames;
    const auto mix = m_pParams->RTPC.fMix;

    for (AkUInt32 ch = 0; ch < io_pBuffer->NumChannels(); ch++)
    {
        auto *data = io_pBuffer->GetChannel(ch);

        // Pass to tone filter array.
        auto *outData = static_cast<AkSampleType *>(AK_PLUGIN_ALLOC(m_pAllocator, sizeof(AkSampleType) * numSamples));
        for (auto &&filter : filterMatrix[ch])
        {
            auto *tempData = static_cast<AkSampleType *>(AK_PLUGIN_ALLOC(m_pAllocator, sizeof(AkSampleType) * numSamples));
            filter.process(data, tempData, numSamples);

            // Clip sine wave into square.
            auto rms = DSP::Utils::calculateRMS(tempData, numSamples);
            for (size_t i = 0; i < numSamples; ++i)
            {
                tempData[i] = tempData[i] > 0.0f ? rms : -rms;
            }

            for (size_t i = 0; i < numSamples; ++i)
            {
                outData[i] += static_cast<AkSampleType>(tempData[i]);
            }
            AK_PLUGIN_FREE(m_pAllocator, tempData);
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            data[i] = outData[i] * mix + data[i] * (1 - mix);
        }
        AK_PLUGIN_FREE(m_pAllocator, outData);
    }
}

AKRESULT ToneFilterFX::TimeSkip(AkUInt32 in_uFrames)
{
    return AK_DataReady;
}
