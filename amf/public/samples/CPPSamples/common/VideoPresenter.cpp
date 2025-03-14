// 
// Notice Regarding Standards.  AMD does not provide a license or sublicense to
// any Intellectual Property Rights relating to any standards, including but not
// limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
// AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
// (collectively, the "Media Technologies"). For clarity, you will pay any
// royalties due for such third party technologies, which may include the Media
// Technologies that are owed as a result of AMD providing the Software to you.
// 
// MIT license 
// 
// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "VideoPresenter.h"
#include "public/include/components/VideoConverter.h"
#include "public/include/components/HQScaler.h"
#include "public/common/TraceAdapter.h"

#define AMF_FACILITY L"VideoPresenter"

#define WAIT_THRESHOLD 5 * AMF_SECOND / 1000LL // 5 ms

#define DROP_THRESHOLD 10 * AMF_SECOND / 1000LL // 10 ms

VideoPresenter::VideoPresenter() :
    m_startTime(-1LL),
    m_startPts(-1LL),
    m_state(ModePlaying),
    m_iFrameCount(0),
    m_FpsStatStartTime(0),
    m_iFramesDropped(0),
    m_dLastFPS(0),
    m_instance(0),
    m_pProcessor(NULL),
    m_pHQScaler(NULL),
    m_currentTime(0),
    m_bDoWait(true),
    m_pAVSync(NULL),
    m_iSubresourceIndex(0),
    m_sourceVertexRect(AMFConstructRect(0, 0, 0, 0)),
    m_destVertexRect(AMFConstructRect(0, 0, 0, 0)),
    m_InputFrameSize(AMFConstructSize(0, 0)),
    m_ptsDropThreshold(DROP_THRESHOLD),
    m_bFullScreen(false),
	m_bWaitForVSync(false),
    m_bEnablePIP(false),
    m_fPIPZoomFactor(0.1f),
    m_fPIPFocusPos({ 0.45f, 0.45f })
{
    amf_increase_timer_precision();
}
VideoPresenter::~VideoPresenter()
{
}

AMF_RESULT VideoPresenter::Reset()
{
    m_iFrameCount = 0;
    m_FpsStatStartTime = 0;
    m_iFramesDropped = 0;
    m_startTime = -1LL;
    m_startPts = -1LL;

    return AMF_OK;
}
AMF_RESULT VideoPresenter::Init(amf_int32 width, amf_int32 height, amf::AMFSurface* /*pSurface*/)
{
    m_iFrameCount = 0;
    m_FpsStatStartTime = 0;
    m_iFramesDropped = 0;
    m_InputFrameSize.width = width;
    m_InputFrameSize.height = height;
    return AMF_OK;
}

AMF_RESULT VideoPresenter::Terminate()
{
    SetProcessor(NULL, NULL);
    return AMF_OK;
}

AMF_RESULT VideoPresenter::SubmitInput(amf::AMFData* pData)
{
    if(m_bFrozen)
    {
        return AMF_OK;
    }

    if(pData)
    {
        m_currentTime = pData->GetPts();
        if(m_pAVSync != NULL)
        {
             //AMFTraceWarning(AMF_FACILITY, L"First PTS=%5.2f", (double)pData->GetPts() / 10000);
            if (m_pAVSync->IsVideoStarted() == false)
            {
                m_pAVSync->VideoStarted();
            }
            m_pAVSync->SetVideoPts(pData->GetPts());
        }

        switch(m_state)
        {
        case ModeStep:
            m_state = ModePaused;
            return Present(amf::AMFSurfacePtr(pData));
        case ModePlaying:
            return Present(amf::AMFSurfacePtr(pData));
        case ModePaused:
            return AMF_INPUT_FULL;
        }
        return AMF_WRONG_STATE;
    }
    return AMF_OK;
}

AMF_RESULT VideoPresenter::CalcOutputRect(const AMFRect* pSrcRect, const AMFRect* pScreenRect, AMFRect* pOutputRect)
{
    amf::AMFLock lock(&m_cs);

    amf_double dDstRatio = pScreenRect->Height() / (amf_double)pScreenRect->Width();
    amf_double dSrcRatio = pSrcRect->Height() / (amf_double)pSrcRect->Width();

    // TODO : add aspect ratio from frame
    //dSrcRatio /= frameApectRatioFromDecoder;

    if(dDstRatio > dSrcRatio)
    {   // empty areas on top and bottom
        pOutputRect->left = 0;
        pOutputRect->right = pScreenRect->Width();
        amf_int lViewHeight = amf_int(pScreenRect->Width() * dSrcRatio);
        pOutputRect->top = (pScreenRect->Height() - lViewHeight) / 2;
        pOutputRect->bottom = pOutputRect->top + lViewHeight;
    }
    else
    {   // empty areas on left and right
        pOutputRect->bottom = pScreenRect->Height();
        pOutputRect->top = 0;
        amf_int lViewWidth = amf_int(pScreenRect->Height() / dSrcRatio);
        pOutputRect->left = (pScreenRect->Width() - lViewWidth) / 2;
        pOutputRect->right = pOutputRect->left + lViewWidth;
    }
    return AMF_OK;
}

bool VideoPresenter::WaitForPTS(amf_pts pts, bool bRealWait)
{

    bool bRet = true;
    amf_pts currTime = amf_high_precision_clock();

    if(m_startTime != -1LL)
    {
        currTime -= m_startTime;
        pts -= m_startPts;

        amf_pts diff = pts - currTime;
        bool bWaited = false;
        if(diff >  WAIT_THRESHOLD && m_bDoWait && bRealWait) // ignore delays < 5 ms 
        {
            m_waiter.Wait(diff);
            bWaited = true;
        } 
//      AMFTraceWarning(AMF_FACILITY, L"+++ Present Frame #%d pts=%5.2f time=%5.2f diff=%5.2f %s", (int)m_iFrameCount, (float)pts / 10000., (float)currTime / 10000., float(diff) / 10000., bRealWait ? L"R" : L"");
        if(diff < -m_ptsDropThreshold) // ignore lags < 10 ms 
        {
            if(m_iFrameCount == 1)
            {
//                m_startTime += currTime;
            }
            else
            {
                m_iFramesDropped++;

//                AMFTraceWarning(AMF_FACILITY, L"+++ Drop Frame #%d pts=%5.2f time=%5.2f diff=%5.2f %s", (int)m_iFrameCount, (double)pts / 10000., (double)currTime / 10000., (double)diff / 10000., bRealWait ? L"R" : L"");
                bRet = false;
            }
        }

            // update FPS = alpha filter
        if( (m_iFrameCount % 100) == 0)
        {
            amf_pts presentTime = amf_high_precision_clock()- m_startTime;
            double currFPS = double(AMF_SECOND) / ((presentTime - m_FpsStatStartTime) / 100.);
            m_dLastFPS = currFPS;
            m_FpsStatStartTime = presentTime;
        }
    }
    else
    {
        m_startTime = currTime;
        m_startPts = pts;
        m_FpsStatStartTime = 0;
    }
    m_iFrameCount++;

    return bRet;
}

AMF_RESULT VideoPresenter::SetProcessor(amf::AMFComponent *processor, amf::AMFComponent* pHQScaler)
{
    amf::AMFLock lock(&m_cs);
    if(m_pProcessor != NULL)
    {
        m_pProcessor->SetOutputDataAllocatorCB(NULL);
    }

    if (m_pHQScaler != NULL)
    {
        m_pHQScaler->SetOutputDataAllocatorCB(NULL);
    }

    m_pProcessor = processor;
    m_pHQScaler = pHQScaler;

    if (SupportAllocator())
    {
        amf::AMFComponent* pAllocatorUser = (m_pHQScaler != NULL) ? m_pHQScaler : m_pProcessor;
        if (pAllocatorUser != nullptr)
        {
            pAllocatorUser->SetOutputDataAllocatorCB(this);
        }
    }

    UpdateProcessor();
    return AMF_OK;
}

void  VideoPresenter::UpdateProcessor()
{
    amf::AMFLock lock(&m_cs);

    if (m_pProcessor != NULL || m_pHQScaler != NULL)
    {
        AMFRect srcRect = { 0, 0, m_InputFrameSize.width, m_InputFrameSize.height };
        AMFRect outputRect;
        CalcOutputRect(&srcRect, &m_rectClient, &outputRect);

        if (m_pProcessor != NULL && (m_pHQScaler == NULL))
        {
            // what we want to do here is check for the properties if they exist
            // as the HQ scaler has different property names than CSC
            const amf::AMFPropertyInfo* pParamInfo = nullptr;
            if ((m_pProcessor->GetPropertyInfo(AMF_VIDEO_CONVERTER_OUTPUT_SIZE, &pParamInfo) == AMF_OK) && pParamInfo)
            {
                m_pProcessor->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_SIZE, ::AMFConstructSize(m_rectClient.Width(), m_rectClient.Height()));
                m_pProcessor->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_RECT, outputRect);
            }
        }

        if (m_pHQScaler != NULL)
        {
            const amf::AMFPropertyInfo* pParamInfo = nullptr;
            if ((m_pHQScaler->GetPropertyInfo(AMF_HQ_SCALER_OUTPUT_SIZE, &pParamInfo) == AMF_OK) && pParamInfo)
            {
                m_pHQScaler->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_rectClient.Width(), m_rectClient.Height()));
            }
        }
    }
}

AMFRect VideoPresenter::GetSourceRect()
{
    AMFRect out = {};
    if (m_pProcessor == nullptr)
    {
        out = m_sourceVertexRect;
    }
    else
    {
        m_pProcessor->GetProperty(AMF_VIDEO_CONVERTER_OUTPUT_SIZE, &out);
    }
    return out;
}

AMF_RESULT VideoPresenter::Freeze()
{
    amf::AMFLock lock(&m_cs);
    return PipelineElement::Freeze();
}

AMF_RESULT VideoPresenter::UnFreeze()
{
    amf::AMFLock lock(&m_cs);
    m_startTime = -1LL;
    m_startPts = -1LL;
    return PipelineElement::UnFreeze();
}

AMF_RESULT VideoPresenter::Resume() 
{ 
    amf::AMFLock lock(&m_cs);
    m_state = ModePlaying; 
    m_startTime = -1LL;
    m_startPts = -1LL;
    return AMF_OK;
}

AMF_RESULT              VideoPresenter::SetSubresourceIndex(amf_int32 index)
{
    m_iSubresourceIndex = index;
    return AMF_OK;
}

amf_int32               VideoPresenter::GetSubresourceIndex()
{
    return m_iSubresourceIndex;
}
