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
#include "PlaybackPipelineBase.h"
#include "public/common/AMFFactory.h"
#include "public/include/components/MediaSource.h"
#include "public/common/TraceAdapter.h"

#pragma warning(disable:4355)

#define AMF_FACILITY L"PlaybackPipelineBase"

const wchar_t* PlaybackPipelineBase::PARAM_NAME_INPUT      = L"INPUT";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_URL_VIDEO  = L"UrlVideo";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_URL_AUDIO  = L"UrlAudio";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_PRESENTER  = L"PRESENTER";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_FRAMERATE = L"FRAMERATE";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_LOOP = L"LOOP";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_LISTEN_FOR_CONNECTION = L"LISTEN";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_DOTIMING = L"DOTIMING";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_LOWLATENCY = L"LOWLATENCY";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_FULLSCREEN = L"FULLSCREEN";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_SW_DECODER = L"swdecoder";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_HQ_SCALER = L"HQSCALER";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_PIP = L"PIP";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_PIP_ZOOM_FACTOR = L"PIPZOOMFACTOR";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_PIP_FOCUS_X = L"PIPFOCUSX";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_PIP_FOCUS_Y = L"PIPFOCUSY";
const wchar_t* PlaybackPipelineBase::PARAM_NAME_SIDE_BY_SIDE = L"SIDEBYSIDE";

PlaybackPipelineBase::PlaybackPipelineBase() :
    m_iVideoWidth(0),
    m_iVideoHeight(0),
    m_bVideoPresenterDirectConnect(false),
    m_nFfmpegRefCount(0),
    m_bURL(false),
    m_eDecoderFormat(amf::AMF_SURFACE_NV12),
    m_bCPUDecoder(false),
    m_bEnableSideBySide(false)
{
    g_AMFFactory.Init();
    SetParamDescription(PARAM_NAME_INPUT, ParamCommon,  L"Input file name", NULL);
    SetParamDescription(PARAM_NAME_URL_VIDEO, ParamCommon,  L"Input stream URL Video", NULL);
    SetParamDescription(PARAM_NAME_URL_AUDIO, ParamCommon,  L"Input stream URL Audio", NULL);
    SetParamDescription(PARAM_NAME_PRESENTER, ParamCommon,  L"Specifies presenter engine type (DX9, DX11, DX12, OPENGL)", ParamConverterVideoPresenter);
    SetParamDescription(PARAM_NAME_FRAMERATE, ParamCommon,  L"Forces Video Frame Rate (double)", ParamConverterDouble);
    SetParamDescription(PARAM_NAME_LOOP, ParamCommon,  L"Loop Video, boolean, default = true", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_LISTEN_FOR_CONNECTION, ParamCommon,  L"LIsten for connection, boolean, default = true", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_DOTIMING, ParamCommon,  L"Play Video and Audio using timestamps, boolean, default = true", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_LOWLATENCY, ParamCommon, L"Low latency mode, boolean, default = false", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_FULLSCREEN, ParamCommon, L"Specifies fullscreen mode, true, false, default false", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_SW_DECODER, ParamCommon, L"Forces sw decoder, true, false, default false", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_HQ_SCALER, ParamCommon,  L"Use HQ Scaler (OFF, Bilinear, Bicubic, FSR, default = OFF)", ParamConverterHQScalerAlgorithm);
    SetParamDescription(PARAM_NAME_PIP, ParamCommon, L"Specifies Picture in Picture mode, true, false, default false", ParamConverterBoolean);
    SetParamDescription(PARAM_NAME_PIP_ZOOM_FACTOR, ParamCommon, L"Specifies magnification ratio of PIP image, int, default 8", ParamConverterDouble);
    SetParamDescription(PARAM_NAME_PIP_FOCUS_X, ParamCommon, L"H position of foreground image, int, default 0", ParamConverterDouble);
    SetParamDescription(PARAM_NAME_PIP_FOCUS_Y, ParamCommon, L"V position of foreground image, int, default 0", ParamConverterDouble);
    SetParamDescription(PARAM_NAME_SIDE_BY_SIDE, ParamCommon, L"Specifies Side-by-Side mode, true, false, default false", ParamConverterBoolean);

    SetParam(PARAM_NAME_LISTEN_FOR_CONNECTION, false);
    SetParam(PARAM_NAME_FULLSCREEN, false);
    SetParam(PARAM_NAME_PIP, false);
    SetParam(PARAM_NAME_PIP_ZOOM_FACTOR, 0.1f);
    SetParam(PARAM_NAME_PIP_FOCUS_X, 0.45f);
    SetParam(PARAM_NAME_PIP_FOCUS_Y, 0.45f);
    SetParam(PARAM_NAME_SIDE_BY_SIDE, false);

#if defined(_WIN32)
    SetParam(PlaybackPipelineBase::PARAM_NAME_PRESENTER, amf::AMF_MEMORY_DX11);
#elif defined(__linux)
    SetParam(PlaybackPipelineBase::PARAM_NAME_PRESENTER, amf::AMF_MEMORY_VULKAN);
#endif
}

PlaybackPipelineBase::~PlaybackPipelineBase()
{
    Terminate();

    for (; m_nFfmpegRefCount > 0; --m_nFfmpegRefCount)
    {
        g_AMFFactory.UnLoadExternalComponent(FFMPEG_DLL_NAME);
    }
    g_AMFFactory.Terminate();
}

void PlaybackPipelineBase::Terminate()
{
    Stop();
    if (m_pContext != nullptr)
    {
        m_pContext->Terminate();
        m_pContext = NULL;
    }
}

AMF_RESULT PlaybackPipelineBase::GetDuration(amf_pts& duration) const
{
    if (m_pDemuxerVideo != NULL)
    {
        amf::AMFMediaSourcePtr pSource(m_pDemuxerVideo);
        if (pSource != NULL)
        {
            duration = pSource->GetDuration();
            return AMF_OK;
        }
    }

    return AMF_NOT_SUPPORTED;
}

AMF_RESULT PlaybackPipelineBase::GetCurrentPts(amf_pts& pts) const
{
	if (m_pDemuxerVideo != NULL)
	{
		amf::AMFMediaSourcePtr pSource(m_pDemuxerVideo);
		if (pSource != NULL)
		{
			pts = pSource->GetPosition();
			return AMF_OK;
		}
	}

	return AMF_NOT_SUPPORTED;
}

double PlaybackPipelineBase::GetProgressSize() const
{
    if (m_pVideoStream != NULL)
    {
        amf_int64 size = 0;
        AMF_RESULT res = m_pVideoStream->GetSize(&size);
        if (res == AMF_OK)
        {
            return static_cast<double>(size);
        }
    }
    else
    {
        amf_pts duration = 0;
        if (GetDuration(duration) == AMF_OK)
        {
            return static_cast<double>(duration);
        }
    }

    return 100.0;
}

double PlaybackPipelineBase::GetProgressPosition() const
{
    if(m_pVideoStream != NULL)
    {
		amf_int64 pos = 0;
		AMF_RESULT res = m_pVideoStream->GetPosition(&pos);
		if (res == AMF_OK)
		{
			return static_cast<double>(pos);
		}
    }
    else
    {
		amf_pts pts = 0;
		if (GetCurrentPts(pts) == AMF_OK)
		{
			return static_cast<double>(pts);
		}
    }

    return 0.0;
}

AMF_RESULT PlaybackPipelineBase::Seek(amf_pts pts)
{
    if(m_pVideoStream)
    {
//        m_pVideoStream->SetPosition( (amf_int64)pos);
    }
    else if(m_pDemuxerVideo != NULL)
    {
        amf::AMFMediaSourcePtr pSource(m_pDemuxerVideo);
        if(pSource != NULL)
        {
            Freeze();
            Flush();
            //MM temporarely - till provide better information that pipeline frozen
            amf_sleep(200);
            pSource->Seek(pts, amf::AMF_SEEK_PREV_KEYFRAME, -1);

            if(m_pDemuxerAudio != NULL)
            {

                amf::AMFMediaSourcePtr pSourceAudio(m_pDemuxerAudio);
                if(pSourceAudio != NULL)
                {
                    pSourceAudio->Seek(pts, amf::AMF_SEEK_PREV_KEYFRAME, -1);
                }
            }
            if(m_pAudioPresenter != NULL)
            {
                m_pAudioPresenter->Seek(pts);
            }

            UnFreeze();

            return AMF_OK;
        }
    }
    return AMF_NOT_SUPPORTED;
}

AMF_RESULT PlaybackPipelineBase::Init()
{
    Terminate();
    AMF_RESULT res = AMF_OK;
    //---------------------------------------------------------------------------------------------
    // Read Options
    std::wstring inputPath = L"";
    std::wstring inputUrlAudio = L"";
#if !defined(METRO_APP)
    res = GetParamWString(PARAM_NAME_INPUT, inputPath);
#else
    inputPath = path;
#endif
    if(inputPath.length() == 0)
    {
        res = GetParamWString(PARAM_NAME_URL_VIDEO, inputPath);
        if(inputPath.length() == 0)
        {
            return AMF_NOT_FOUND;
        }
        m_bURL = true;

        res = GetParamWString(PARAM_NAME_URL_AUDIO, inputUrlAudio);
    }

	amf::AMF_MEMORY_TYPE presenterEngine = amf::AMF_MEMORY_UNKNOWN;
    {
        amf_int64 engineInt = amf::AMF_MEMORY_UNKNOWN;
        if(GetParam(PARAM_NAME_PRESENTER, engineInt) == AMF_OK)
        {
            if(amf::AMF_MEMORY_UNKNOWN != engineInt)
            {
                presenterEngine = (amf::AMF_MEMORY_TYPE)engineInt;
            }
        }
    }

    //---------------------------------------------------------------------------------------------
    // Init context and devices
    res = InitContext(presenterEngine);
    CHECK_AMF_ERROR_RETURN(res, "Create AMF context");

    //---------------------------------------------------------------------------------------------
    // Init Video Stream Parser or demuxer
    BitStreamType streamType = BitStreamUnknown;
    if(!m_bURL)
    {
        streamType = GetStreamType(inputPath.c_str());
    }

    bool bLowlatency = false;
    GetParam(PARAM_NAME_LOWLATENCY, bLowlatency);

    amf_int32 iVideoStreamIndex = -1;
    amf_int32 iAudioStreamIndex = -1;

    amf::AMFOutputPtr       pAudioOutput;
    amf::AMFOutputPtr       pVideoOutput;
    if( streamType != BitStreamUnknown)
    {
#if !defined(METRO_APP)
        amf::AMFDataStream::OpenDataStream(inputPath.c_str(), amf::AMFSO_READ, amf::AMFFS_SHARE_READ, &m_pVideoStream);
#else
        amf::AMFDataStream::OpenDataStream(inputPath.c_str(), amf::AMFSO_READ, amf::AMFFS_SHARE_READ, &m_pVideoStream);
#endif
        CHECK_RETURN(m_pVideoStream != NULL, AMF_FILE_NOT_OPEN, "Open File");

        m_pVideoStreamParser = BitStreamParser::Create(m_pVideoStream, streamType, m_pContext);
        CHECK_RETURN(m_pVideoStreamParser != NULL, AMF_FILE_NOT_OPEN, "BitStreamParser::Create");

        double framerate = 0;
        if(GetParam(PARAM_NAME_FRAMERATE, framerate) == AMF_OK && framerate != 0)
        {
            m_pVideoStreamParser->SetFrameRate(framerate);
        }
        iVideoStreamIndex = 0;
    }
    else
    {
        amf::AMFComponentPtr  pDemuxer;

        res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", (void*)FFMPEG_DEMUXER, &pDemuxer);
        CHECK_AMF_ERROR_RETURN(res, L"AMFCreateComponent(" << FFMPEG_DEMUXER << L") failed");
        ++m_nFfmpegRefCount;
        m_pDemuxerVideo = amf::AMFComponentExPtr(pDemuxer);

        bool bListen = false;
        GetParam(PlaybackPipelineBase::PARAM_NAME_LISTEN_FOR_CONNECTION, bListen);

        if(m_bURL)
        {
            bool bListen = false;
            GetParam(PlaybackPipelineBase::PARAM_NAME_LISTEN_FOR_CONNECTION, bListen);
            m_pDemuxerVideo->SetProperty(FFMPEG_DEMUXER_LISTEN, bListen);
        }
        res = m_pDemuxerVideo->SetProperty(m_bURL ? FFMPEG_DEMUXER_URL : FFMPEG_DEMUXER_PATH, inputPath.c_str());
        CHECK_AMF_ERROR_RETURN(res, L"m_pDemuxerVideo->SetProperty() failed" );
        res = m_pDemuxerVideo->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
        CHECK_AMF_ERROR_RETURN(res, L"m_pDemuxerVideo->Init(" << inputPath << ") failed");

        amf_int32 outputs = m_pDemuxerVideo->GetOutputCount();
        for(amf_int32 output = 0; output < outputs; output++)
        {
            amf::AMFOutputPtr pOutput;
            res = m_pDemuxerVideo->GetOutput(output, &pOutput);
            CHECK_AMF_ERROR_RETURN(res, L"m_pDemuxerVideo->GetOutput() failed");

            amf_int64 eStreamType = AMF_STREAM_UNKNOWN;
            pOutput->GetProperty(AMF_STREAM_TYPE, &eStreamType);


            if(iVideoStreamIndex < 0 && eStreamType == AMF_STREAM_VIDEO)
            {
                iVideoStreamIndex = output;
                pVideoOutput = pOutput;
            }

            if(iAudioStreamIndex < 0 && eStreamType == AMF_STREAM_AUDIO)
            {
                iAudioStreamIndex = output;
                pAudioOutput = pOutput;
            }
        }
        if(inputUrlAudio.length() > 0)
        {
            amf::AMFComponentPtr  pDemuxerAudio;
            res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", (void*)FFMPEG_DEMUXER, &pDemuxerAudio);
            CHECK_AMF_ERROR_RETURN(res, L"AMFCreateComponent(" << FFMPEG_DEMUXER << L") failed");
            ++m_nFfmpegRefCount;
            m_pDemuxerAudio = amf::AMFComponentExPtr(pDemuxerAudio);

            m_pDemuxerAudio->SetProperty(m_bURL ? FFMPEG_DEMUXER_URL : FFMPEG_DEMUXER_PATH, inputUrlAudio.c_str());
            res = m_pDemuxerAudio->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
            CHECK_AMF_ERROR_RETURN(res, L"m_pDemuxerAudio->Init() failed");

            amf_int32 outputs = m_pDemuxerAudio->GetOutputCount();
            for(amf_int32 output = 0; output < outputs; output++)
            {
                amf::AMFOutputPtr pOutput;
                res = m_pDemuxerAudio->GetOutput(output, &pOutput);
                CHECK_AMF_ERROR_RETURN(res, L"m_pDemuxerAudio->GetOutput() failed");

                amf_int64 eStreamType = AMF_STREAM_UNKNOWN;
                pOutput->GetProperty(AMF_STREAM_TYPE, &eStreamType);


                if(iVideoStreamIndex < 0 && eStreamType == AMF_STREAM_VIDEO)
                {
                    iVideoStreamIndex = output;
                    pVideoOutput = pOutput;
                }

                if(iAudioStreamIndex < 0 && eStreamType == AMF_STREAM_AUDIO)
                {
                    iAudioStreamIndex = output;
                    pAudioOutput = pOutput;
                }
            }

        }


    }
    //---------------------------------------------------------------------------------------------
    // init streams
    if(iVideoStreamIndex >= 0)
    {
		res = InitVideo(pVideoOutput, presenterEngine);
        CHECK_AMF_ERROR_RETURN(res, L"InitVideo() failed");
    }
    if(iAudioStreamIndex >= 0)
    {
        res = InitAudio(pAudioOutput);
        if (res != AMF_OK)
        {
            LOG_AMF_ERROR(res, L"InitAudio() failed");
            iAudioStreamIndex = -1;
        }
//        CHECK_AMF_ERROR_RETURN(res, L"InitAudio() failed");
    }

    //---------------------------------------------------------------------------------------------
    // Connect pipeline
	//-------------------------- Connect parser/ Demuxer
	PipelineElementPtr pPipelineElementDemuxerVideo = nullptr;
	PipelineElementPtr pPipelineElementDemuxerAudio = nullptr;
	if (m_pVideoStreamParser != NULL)
	{
		pPipelineElementDemuxerVideo = m_pVideoStreamParser;
	}
	else
	{
		pPipelineElementDemuxerVideo = PipelineElementPtr(new AMFComponentExElement(m_pDemuxerVideo));
	}
    Connect(pPipelineElementDemuxerVideo, 10);

    if(m_pDemuxerAudio != NULL)
    {
        pPipelineElementDemuxerAudio = PipelineElementPtr(new AMFComponentExElement(m_pDemuxerAudio));
	    Connect(pPipelineElementDemuxerAudio, 0, PipelineElementPtr(), 0, 10);
    }
    else
    {
        pPipelineElementDemuxerAudio = pPipelineElementDemuxerVideo;
    }
	Connect(PipelineElementPtr(new AMFComponentElement(m_pVideoDecoder)), 0, pPipelineElementDemuxerVideo, iVideoStreamIndex, m_bURL && bLowlatency == false ? 100 : 4, CT_ThreadQueue);
	// Initialize pipeline for both video and audio
	InitVideoPipeline(iVideoStreamIndex, pPipelineElementDemuxerVideo);
	InitAudioPipeline(iAudioStreamIndex, pPipelineElementDemuxerAudio);

    if(iVideoStreamIndex >= 0 && m_pVideoPresenter != NULL && m_pAudioPresenter != NULL)
    {
        m_AVSync.Reset();
        m_pVideoPresenter->SetAVSyncObject(&m_AVSync);
        m_pAudioPresenter->SetAVSyncObject(&m_AVSync);
    }

    bool bTiming = true;
    GetParam(PARAM_NAME_DOTIMING,  bTiming);
    if (bLowlatency)
    {
        bTiming = false;
    }
    if(m_pVideoPresenter != NULL)
    {
        m_pVideoPresenter->DoActualWait(bTiming);
    }
    if(m_pAudioPresenter != NULL)
    {
        m_pAudioPresenter->DoActualWait(bTiming);
    }


    return AMF_OK;
}

AMF_RESULT PlaybackPipelineBase::InitAudioPipeline(amf_uint32 iAudioStreamIndex, PipelineElementPtr pAudioSourceStream)
{
	if (iAudioStreamIndex >= 0 && m_pAudioPresenter != NULL && pAudioSourceStream != NULL)
	{
		Connect(PipelineElementPtr(new AMFComponentElement(m_pAudioDecoder)), 0, pAudioSourceStream, iAudioStreamIndex, m_bURL ? 1000 : 100, CT_ThreadQueue);
		Connect(PipelineElementPtr(new AMFComponentElement(m_pAudioConverter)), 10);
        if(m_pAudioPresenter != nullptr)
        {
		    Connect(m_pAudioPresenter, 10);
        }
	}
	return AMF_OK;
}

AMF_RESULT PlaybackPipelineBase::InitVideoPipeline(amf_uint32 iVideoStreamIndex, PipelineElementPtr pVideoSourceStream)
{
    bool bLowlatency = false;
    GetParam(PARAM_NAME_LOWLATENCY, bLowlatency);

    if (m_pVideoProcessor != NULL)
	{
        if (bLowlatency)
        {
            Connect(PipelineElementPtr(new AMFComponentElement(m_pVideoProcessor)), 1, CT_Direct);
        }
        else
        {
            Connect(PipelineElementPtr(new AMFComponentElement(m_pVideoProcessor)), m_bCPUDecoder ? 4 : 1, CT_ThreadQueue);
        }
    }
    if (m_pScaler != NULL)
    {
        if (m_pHQScaler2 == NULL)
        {
            Connect(PipelineElementPtr(new AMFComponentElement(m_pScaler)), 1, CT_Direct);
        }
        else
        {
            if (m_pSplitter == NULL)
            {
                m_pSplitter = SplitterPtr(new Splitter(true, 2));
            }
            if (m_pCombiner == NULL)
            {
                m_pCombiner = CombinerPtr(new Combiner(m_pContext));
            }

            if (m_pSplitter != NULL && m_pCombiner != NULL)
            {
                PipelineElementPtr upstreamConnector = GetLastElement();

                PipelineElementPtr hqScalerElement1(new AMFComponentElement(m_pScaler));
                PipelineElementPtr hqScalerElement2(new AMFComponentElement(m_pHQScaler2));

                Connect(m_pSplitter, 0, upstreamConnector, 0, 4);

                Connect(hqScalerElement1, 0, m_pSplitter, 0, 2);
                Connect(hqScalerElement2, 0, m_pSplitter, 1, 2);

                Connect(m_pCombiner, 0, hqScalerElement1, 0, 4);
                Connect(m_pCombiner, 1, hqScalerElement2, 0, 4);
            }
        }
    }
    if(m_bVideoPresenterDirectConnect)
    {
        Connect(m_pVideoPresenter, 4, CT_ThreadPoll); // if back buffers are used in multithreading the video memory changes every Present() call so pointer returned by GetBackBuffer() points to wrong memory
    }
    else
    {
        Connect(m_pVideoPresenter, 4, CT_ThreadQueue);
    }
	return AMF_OK;
}

AMF_RESULT  PlaybackPipelineBase::InitVideoProcessor()
{
    // check if we need to create the HQ scaler
    amf_int64  hqScalerMode = -1;

    AMF_RESULT res = GetParam(PARAM_NAME_HQ_SCALER, hqScalerMode);

    if ((res == AMF_OK) && (hqScalerMode != -1))
    {
        res = g_AMFFactory.GetFactory()->CreateComponent(m_pContext, AMFHQScaler, &m_pScaler);
        CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.GetFactory()->CreateComponent(" << AMFHQScaler << L") failed");

        m_pScaler->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_iVideoWidth, m_iVideoHeight));
        m_pScaler->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, m_pVideoPresenter->GetMemoryType());
        m_pScaler->SetProperty(AMF_HQ_SCALER_ALGORITHM, hqScalerMode);

        /*
        // If enabled side-by-side, create a separate HQ Scaler
        if (m_bEnableSideBySide)
        {
            res = g_AMFFactory.GetFactory()->CreateComponent(m_pContext, AMFHQScaler, &m_pHQScaler2);
            CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.GetFactory()->CreateComponent(" << AMFHQScaler << L") failed");

            m_pHQScaler2->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_iVideoWidth, m_iVideoHeight));
            m_pHQScaler2->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, m_pVideoPresenter->GetMemoryType());
            m_pHQScaler2->SetProperty(AMF_HQ_SCALER_ALGORITHM, AMF_HQ_SCALER_ALGORITHM_BILINEAR);

            res = m_pHQScaler2->Init(m_pVideoPresenter->GetInputFormat(), m_iVideoWidth, m_iVideoHeight);
            CHECK_AMF_ERROR_RETURN(res, L"m_pHQScaler2->Init() failed");
        }
        */

    }

    // initialize scaler
    if (m_pScaler != nullptr)
    {
        res = m_pScaler->Init(m_pVideoPresenter->GetInputFormat(), m_iVideoWidth, m_iVideoHeight);
        CHECK_AMF_ERROR_RETURN(res, L"m_pScaler->Init() failed with err=%d");
    }

    // create CSC - this one is needed all the time,
    // even if we use HQ scaler
    res = g_AMFFactory.GetFactory()->CreateComponent(m_pContext, AMFVideoConverter, &m_pVideoProcessor);
    CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.GetFactory()->CreateComponent(" << AMFVideoConverter << L") failed");

    if (m_eDecoderFormat == amf::AMF_SURFACE_P010) //HDR support
    {
//        m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_2020);
//        m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_LINEAR_RGB, true);
    }
    else
    {
//        m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_709);
    }
    m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, m_pVideoPresenter->GetMemoryType());
    m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, m_pVideoPresenter->GetInputFormat());

    res = m_pVideoProcessor->Init(m_eDecoderFormat, m_iVideoWidth, m_iVideoHeight);
    CHECK_AMF_ERROR_RETURN(res, L"m_pVideoProcessor->Init() failed");


    // set-up the processor for presenter - if we use HQ scaler,
    // we need to set it, otherwise it will be CSC

    if (m_pContext->GetOpenCLContext() == NULL)
    {
        m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_KEEP_ASPECT_RATIO, true);
        m_pVideoProcessor->SetProperty(AMF_VIDEO_CONVERTER_FILL, true);

        if (m_pScaler != nullptr)
        {
            m_pScaler->SetProperty(AMF_HQ_SCALER_KEEP_ASPECT_RATIO, true);
            m_pScaler->SetProperty(AMF_HQ_SCALER_FILL, true);
        }

        m_pVideoPresenter->SetProcessor(m_pVideoProcessor, m_pScaler);
    }

    return res;
}

AMF_RESULT  PlaybackPipelineBase::InitVideoDecoder(const wchar_t *pDecoderID, amf_int64 codecID, amf::AMF_SURFACE_FORMAT surfaceFormat, amf_int64 bitrate, AMFRate frameRate, amf::AMFBuffer* pExtraData)
{
    bool bLowlatency = false;
    GetParam(PARAM_NAME_LOWLATENCY, bLowlatency);

    AMF_VIDEO_DECODER_MODE_ENUM   decoderMode = bLowlatency ? AMF_VIDEO_DECODER_MODE_LOW_LATENCY : AMF_VIDEO_DECODER_MODE_COMPLIANT; // AMF_VIDEO_DECODER_MODE_REGULAR , AMF_VIDEO_DECODER_MODE_LOW_LATENCY;

    bool bSWDecoder = false;
    GetParam(PARAM_NAME_SW_DECODER, bSWDecoder);

    if (bSWDecoder == false)
    {
        AMF_RESULT res = g_AMFFactory.GetFactory()->CreateComponent(m_pContext, pDecoderID, &m_pVideoDecoder);
        CHECK_AMF_ERROR_RETURN(res, L"g_AMFFactory.GetFactory()->CreateComponent(" << pDecoderID << L") failed");
    }

    // switch to SW Decoder when the HW Decoder does not support this config
    amf::AMFCapsPtr pCaps;
    if (m_pVideoDecoder != nullptr && m_pVideoDecoder->GetCaps(&pCaps) != AMF_OK)
    {
        m_pVideoDecoder = NULL;
    }

    if (m_pVideoDecoder != nullptr)
    {
        amf::AMFCapsPtr pCaps;
        AMF_RESULT res = m_pVideoDecoder->GetCaps(&pCaps);
        CHECK_AMF_ERROR_RETURN(res, L"failed to get decoder caps");

        amf::AMFIOCapsPtr pInputCaps;
        res = pCaps->GetInputCaps(&pInputCaps);
        CHECK_AMF_ERROR_RETURN(res, L"failed to get decoder input caps");

        // check resolution
        amf_int32 minWidth = 0;
        amf_int32 maxWidth = 0;
        amf_int32 minHeight = 0;
        amf_int32 maxHeight = 0;
        pInputCaps->GetWidthRange(&minWidth, &maxWidth);
        pInputCaps->GetHeightRange(&minHeight, &maxHeight);
        if (minWidth <= m_iVideoWidth && m_iVideoWidth <= maxWidth && minHeight <= m_iVideoHeight && m_iVideoHeight <= maxHeight &&
            surfaceFormat != amf::AMF_SURFACE_P012 && surfaceFormat != amf::AMF_SURFACE_P016) //MM TODO Update caps
        {
            m_pVideoDecoder->SetProperty(AMF_VIDEO_DECODER_REORDER_MODE, amf_int64(decoderMode));
            m_pVideoDecoder->SetProperty(AMF_VIDEO_DECODER_EXTRADATA, amf::AMFVariant(pExtraData));

            m_pVideoDecoder->SetProperty(AMF_TIMESTAMP_MODE, amf_int64(AMF_TS_DECODE)); // our sample H264 parser provides decode order timestamps- change depend on demuxer
            m_eDecoderFormat = amf::AMF_SURFACE_NV12;
            if (std::wstring(pDecoderID) == AMFVideoDecoderHW_H265_MAIN10)
            {
                m_eDecoderFormat = amf::AMF_SURFACE_P010;
            }
            res = m_pVideoDecoder->Init(m_eDecoderFormat, m_iVideoWidth, m_iVideoHeight);
            CHECK_AMF_ERROR_RETURN(res, L"m_pVideoDecoder->Init(" << m_iVideoWidth << L", " << m_iVideoHeight << L") failed " << pDecoderID);
		    if (std::wstring(pDecoderID) == AMFVideoDecoderHW_AV1)
		    {
			    //Only up to above m_pVideoDecoder->Init(), got/updated actual eDecoderFormat, then also update m_eDecoderFormat here.
			    amf_int64 format = 0;
			    if (m_pVideoDecoder->GetProperty(L"AV1StreamFormat", &format) == AMF_OK)
			    {
                    m_eDecoderFormat = (amf::AMF_SURFACE_FORMAT)format;
			    }
		    }
            m_bCPUDecoder = false;
        }
        else
        {
            // fall back to software decoder if resolution not supported
            m_pVideoDecoder = nullptr;
        }
    }

    // use software decoder
    //   - if specifically requested
    //   - if resolution not supported
    //   - if codec not supported
    if (m_pVideoDecoder == nullptr)
    {
        AMF_RESULT res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", (void*)FFMPEG_VIDEO_DECODER, &m_pVideoDecoder);
        CHECK_AMF_ERROR_RETURN(res, L"LoadExternalComponent(" << FFMPEG_VIDEO_DECODER << L") failed");
        ++m_nFfmpegRefCount;

        if (codecID == 0)
        {
            if (std::wstring(pDecoderID) == AMFVideoDecoderUVD_H264_AVC)
            {
                codecID = AMF_STREAM_CODEC_ID_H264_AVC;
            }
            else if (std::wstring(pDecoderID) == AMFVideoDecoderHW_H265_MAIN10)
            {
                codecID = AMF_STREAM_CODEC_ID_H265_MAIN10;
            }
            else if (std::wstring(pDecoderID) == AMFVideoDecoderHW_H265_HEVC)
            {
                codecID = AMF_STREAM_CODEC_ID_H265_HEVC;
            }
			else if (std::wstring(pDecoderID) == AMFVideoDecoderHW_AV1)
			{
				codecID = AMF_STREAM_CODEC_ID_AV1;
			}
		}
        m_pVideoDecoder->SetProperty(VIDEO_DECODER_CODEC_ID, codecID);
        m_pVideoDecoder->SetProperty(VIDEO_DECODER_BITRATE, bitrate);
        m_pVideoDecoder->SetProperty(VIDEO_DECODER_FRAMERATE, frameRate);

//      m_pVideoDecoder->SetProperty(AMF_VIDEO_DECODER_REORDER_MODE, amf_int64(decoderMode));
        m_pVideoDecoder->SetProperty(VIDEO_DECODER_EXTRA_DATA, amf::AMFVariant(pExtraData));

        if(surfaceFormat != amf::AMF_SURFACE_UNKNOWN)
        {
            m_eDecoderFormat = surfaceFormat;
        }
        else
        {
            m_eDecoderFormat = amf::AMF_SURFACE_NV12;
        }
/*
        if(std::wstring(pDecoderID) == AMFVideoDecoderHW_H265_MAIN10)
        {
            m_eDecoderFormat = amf::AMF_SURFACE_P010;
        }
        if (std::wstring(pDecoderID) == AMFVideoDecoderHW_AV1)
        {
            // can be 10 or 12 bits per sample
            amf_int32 outputCount = m_pDemuxerVideo->GetOutputCount();
            AMF_RETURN_IF_FALSE(outputCount > 0, AMF_FAIL, L"InitVideoDecoder() - not enough output streams");

            amf::AMFOutputPtr pOutput;
            m_pDemuxerVideo->GetOutput(0, &pOutput);

            amf_int64 surfaceFormat = amf::AMF_SURFACE_UNKNOWN;
            pOutput->GetProperty(AMF_STREAM_VIDEO_FORMAT, &surfaceFormat);

            if (surfaceFormat != static_cast<amf::AMF_SURFACE_FORMAT>(amf::AMF_SURFACE_UNKNOWN))
            {
                m_eDecoderFormat = static_cast<amf::AMF_SURFACE_FORMAT>(surfaceFormat);
            }
        }
*/
        res = m_pVideoDecoder->Init(m_eDecoderFormat, m_iVideoWidth, m_iVideoHeight);
        CHECK_AMF_ERROR_RETURN(res, L"m_pVideoDecoder->Init("<< m_iVideoWidth << m_iVideoHeight << L") failed " << pDecoderID );
        m_bCPUDecoder = true;
    }

    return AMF_OK;
}

AMF_RESULT PlaybackPipelineBase::Play()
{
    switch(GetState())
    {
    case PipelineStateRunning:
        if(m_pAudioPresenter != NULL)
        {
            m_pAudioPresenter->Resume(m_pVideoPresenter->GetCurrentTime());
        }
        return m_pVideoPresenter->Resume();
    case PipelineStateReady:
        return Start();
    case PipelineStateNotReady:
    case PipelineStateEof:
    case PipelineStateFrozen:
        return UnFreeze();
    default:
        break;
    }
    return AMF_WRONG_STATE;
}

AMF_RESULT PlaybackPipelineBase::Pause()
{
    switch(GetState())
    {
    case PipelineStateRunning:
    {
        if(m_pAudioPresenter != NULL)
        {
            m_pAudioPresenter->Pause();
        }
        return m_pVideoPresenter->Pause();
    }
    case PipelineStateReady:
    case PipelineStateNotReady:
    case PipelineStateEof:
    case PipelineStateFrozen:
    default:
        break;
    }
    return AMF_WRONG_STATE;
}

bool  PlaybackPipelineBase::IsPaused() const
{
    if (m_pVideoPresenter == nullptr)
    {
        return false;
    }
    return GetState() == PipelineStateRunning && m_pVideoPresenter->GetMode() == VideoPresenter::ModePaused;
}


AMF_RESULT PlaybackPipelineBase::Step()
{
    switch(GetState())
    {
    case PipelineStateRunning:
        if(m_pAudioPresenter != NULL)
        {
            m_pAudioPresenter->Pause();
        }
        m_pVideoPresenter->Pause();
        return m_pVideoPresenter->Step();
    case PipelineStateReady:
    case PipelineStateNotReady:
    case PipelineStateEof:
    case PipelineStateFrozen:
    default:
        break;
    }
    return AMF_WRONG_STATE;
}

AMF_RESULT PlaybackPipelineBase::Stop()
{
    if(m_pAudioPresenter != NULL)
    {
        m_pAudioPresenter->Flush();
    }

    Pipeline::Stop();

    m_AVSync.Reset();

    if(m_pAudioDecoder != NULL)
    {
        m_pAudioDecoder->Terminate();
        m_pAudioDecoder = NULL;
    }

    if(m_pAudioConverter != NULL)
    {
        m_pAudioConverter->Terminate();
        m_pAudioConverter = NULL;
    }

    if(m_pAudioPresenter != NULL)
    {
        m_pAudioPresenter = NULL;
    }

    if(m_pVideoDecoder != NULL)
    {
        m_pVideoDecoder->Terminate();
        m_pVideoDecoder = NULL;
    }

    if(m_pVideoProcessor != NULL)
    {
        m_pVideoProcessor->Terminate();
        m_pVideoProcessor = NULL;
    }

    if (m_pScaler != NULL)
    {
        m_pScaler->Terminate();
        m_pScaler = NULL;
    }

    if (m_pHQScaler2 != NULL)
    {
        m_pHQScaler2->Terminate();
        m_pHQScaler2 = NULL;
    }

    if(m_pVideoPresenter != NULL)
    {
        m_pVideoPresenter->Terminate();
        m_pVideoPresenter = NULL;
    }

    m_pVideoStreamParser = NULL;

    if(m_pDemuxerVideo != NULL)
    {
        m_pDemuxerVideo->Terminate();
        m_pDemuxerVideo = NULL;
    }
    if(m_pDemuxerAudio != NULL)
    {
        m_pDemuxerAudio->Terminate();
        m_pDemuxerAudio = NULL;
    }


    if(m_pContext != NULL)
    {
        m_pContext->Terminate();
        m_pContext = NULL;
    }

    return AMF_OK;
}

void PlaybackPipelineBase::OnParamChanged(const wchar_t* name)
{
    //check if HQScaler will need to be re-created in the pipeline
    if (std::wstring(name) == std::wstring(PARAM_NAME_HQ_SCALER))
    {
        amf_int64  modeHQScaler = -1;

        if (m_pScaler != nullptr)
        {
            m_pScaler->GetProperty(AMF_HQ_SCALER_ALGORITHM, &modeHQScaler);
        }

        amf_int64  modeHQScalerNew = -1;
        GetParam(PARAM_NAME_HQ_SCALER, modeHQScalerNew);

        if ((modeHQScaler == -1 && modeHQScalerNew != -1)  || 
            (modeHQScaler != -1 && modeHQScalerNew == -1)) //need to re-build the pipeline
        {
            if (GetState() == PipelineStateRunning)
            {
                Init();
                Play();
            }
        }

        if (m_pScaler != nullptr && modeHQScalerNew != -1)
        {
            m_pScaler->SetProperty(AMF_HQ_SCALER_ALGORITHM, modeHQScalerNew);
        }
    }

    if (m_pVideoPresenter != nullptr)
    {
        if (std::wstring(name) == std::wstring(PARAM_NAME_FULLSCREEN))
        {
            bool bFullScreen = false;
            GetParam(PARAM_NAME_FULLSCREEN, bFullScreen);
            m_pVideoPresenter->SetFullScreen(bFullScreen);
        }
        else if (std::wstring(name) == std::wstring(PARAM_NAME_DOTIMING))
        {
            bool bDoTiming = true;
            GetParam(PARAM_NAME_DOTIMING, bDoTiming);
            m_pVideoPresenter->DoActualWait(bDoTiming);
        }
        else if (std::wstring(name) == std::wstring(PARAM_NAME_PIP))
        {
            bool bEnablePIP = false;
            GetParam(PARAM_NAME_PIP, bEnablePIP);
            m_pVideoPresenter->SetEnablePIP(bEnablePIP);
        }
        else if (std::wstring(name) == std::wstring(PARAM_NAME_PIP_ZOOM_FACTOR))
        {
            amf_float fPIPZoomFactor;
            GetParam(PARAM_NAME_PIP_ZOOM_FACTOR, fPIPZoomFactor);
            m_pVideoPresenter->SetPIPZoomFactor(fPIPZoomFactor);
        }
        else if (std::wstring(name) == std::wstring(PARAM_NAME_PIP_FOCUS_X) || std::wstring(name) == std::wstring(PARAM_NAME_PIP_FOCUS_Y))
        {
            AMFFloatPoint2D fPIPFocusPos;
            GetParam(PARAM_NAME_PIP_FOCUS_X, fPIPFocusPos.x);
            GetParam(PARAM_NAME_PIP_FOCUS_Y, fPIPFocusPos.y);

            m_pVideoPresenter->SetPIPFocusPositions(fPIPFocusPos);
        }
        else if (std::wstring(name) == std::wstring(PARAM_NAME_SIDE_BY_SIDE))
        {
            bool bEnableSideBySide = false;
            GetParam(PARAM_NAME_SIDE_BY_SIDE, bEnableSideBySide);
            m_bEnableSideBySide = bEnableSideBySide;
        }
    }
    if(m_pVideoProcessor == NULL)
    {
        return;
    }
    UpdateVideoProcessorProperties(name);
}

void PlaybackPipelineBase::UpdateVideoProcessorProperties(const wchar_t* name)
{
    if (m_pVideoProcessor == NULL)
    {
        return;
    }
    if(name != NULL)
    {
        ParamDescription description;
        GetParamDescription(name, description);

        if(description.m_Type == ParamVideoProcessor)
        {
            amf::AMFVariant value;
            GetParam(name, &value);
            m_pVideoProcessor->SetProperty(description.m_Name.c_str(), value);
        }
    }
    else
    {
        for(amf_size i = 0; i < GetParamCount(); ++i)
        {
            std::wstring name;
            amf::AMFVariant value;
            GetParamAt(i, name, &value);
            ParamDescription description;
            GetParamDescription(name.c_str(), description);
            if(description.m_Type == ParamVideoProcessor)
            {
                m_pVideoProcessor->SetProperty(description.m_Name.c_str(), value);
            }
        }
    }
}
double PlaybackPipelineBase::GetFPS()
{
    if(m_pVideoPresenter != NULL)
    {
        return m_pVideoPresenter->GetFPS();
    }
    return 0;
}
amf_int64 PlaybackPipelineBase::GetFramesDropped() const
{
    if(m_pVideoPresenter != NULL)
    {
        return m_pVideoPresenter->GetFramesDropped();
    }
    return 0;
}
AMF_RESULT  PlaybackPipelineBase::InitAudio(amf::AMFOutput* pOutput)
{
    AMF_RESULT res = AMF_OK;

    amf_int64 codecID = 0;
    amf_int64 streamBitRate = 0;
    amf_int64 streamSampleRate = 0;
    amf_int64 streamChannels = 0;
    amf_int64 streamFormat = 0;
    amf_int64 streamLayout = 0;
    amf_int64 streamBlockAlign = 0;
    amf_int64 streamFrameSize = 0;
    amf::AMFInterfacePtr pExtradata;

    pOutput->GetProperty(AMF_STREAM_CODEC_ID, &codecID);

    pOutput->GetProperty(AMF_STREAM_BIT_RATE, &streamBitRate);
    pOutput->GetProperty(AMF_STREAM_EXTRA_DATA, &pExtradata);

    pOutput->GetProperty(AMF_STREAM_AUDIO_SAMPLE_RATE, &streamSampleRate);
    pOutput->GetProperty(AMF_STREAM_AUDIO_CHANNELS, &streamChannels);
    pOutput->GetProperty(AMF_STREAM_AUDIO_FORMAT, &streamFormat);
    pOutput->GetProperty(AMF_STREAM_AUDIO_CHANNEL_LAYOUT, &streamLayout);
    pOutput->GetProperty(AMF_STREAM_AUDIO_BLOCK_ALIGN, &streamBlockAlign);
    pOutput->GetProperty(AMF_STREAM_AUDIO_FRAME_SIZE, &streamFrameSize);

    res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", (void*)FFMPEG_AUDIO_DECODER, &m_pAudioDecoder);
    CHECK_AMF_ERROR_RETURN(res, L"LoadExternalComponent(" << FFMPEG_AUDIO_DECODER << L") failed");
    ++m_nFfmpegRefCount;

    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CODEC_ID, codecID);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_BIT_RATE, streamBitRate);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_EXTRA_DATA, pExtradata);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_SAMPLE_RATE, streamSampleRate);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CHANNELS, streamChannels);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_SAMPLE_FORMAT, streamFormat);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_CHANNEL_LAYOUT, streamLayout);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_BLOCK_ALIGN, streamBlockAlign);
    m_pAudioDecoder->SetProperty(AUDIO_DECODER_IN_AUDIO_FRAME_SIZE, streamFrameSize);

    res = m_pAudioDecoder->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
   CHECK_AMF_ERROR_RETURN(res, L"m_pAudioDecoder->Init() failed");

    res = g_AMFFactory.LoadExternalComponent(m_pContext, FFMPEG_DLL_NAME, "AMFCreateComponentInt", (void*)FFMPEG_AUDIO_CONVERTER, &m_pAudioConverter);
    CHECK_AMF_ERROR_RETURN(res, L"LoadExternalComponent(" << FFMPEG_AUDIO_CONVERTER << L") failed");
    ++m_nFfmpegRefCount;

    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_BIT_RATE, &streamBitRate);
    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_SAMPLE_RATE, &streamSampleRate);
    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_CHANNELS, &streamChannels);
    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_SAMPLE_FORMAT, &streamFormat);
    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_CHANNEL_LAYOUT, &streamLayout);
    m_pAudioDecoder->GetProperty(AUDIO_DECODER_OUT_AUDIO_BLOCK_ALIGN, &streamBlockAlign);


    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_BIT_RATE, streamBitRate);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_SAMPLE_RATE, streamSampleRate);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_CHANNELS, streamChannels);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_SAMPLE_FORMAT, streamFormat);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_CHANNEL_LAYOUT, streamLayout);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_IN_AUDIO_BLOCK_ALIGN, streamBlockAlign);

    res = CreateAudioPresenter();
    if(res == AMF_NOT_IMPLEMENTED)
    {
        return AMF_OK;
    }
    CHECK_AMF_ERROR_RETURN(res, "Failed to create an audio presenter");

    res = m_pAudioPresenter->Init();
    CHECK_AMF_ERROR_RETURN(res, L"m_pAudioPresenter->Init() failed");

    m_pAudioPresenter->GetDescription(
        streamBitRate,
        streamSampleRate,
        streamChannels,
        streamFormat,
        streamLayout,
        streamBlockAlign
    );

    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_BIT_RATE, streamBitRate);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_SAMPLE_RATE, streamSampleRate);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_CHANNELS, streamChannels);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_SAMPLE_FORMAT, streamFormat);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_CHANNEL_LAYOUT, streamLayout);
    m_pAudioConverter->SetProperty(AUDIO_CONVERTER_OUT_AUDIO_BLOCK_ALIGN, streamBlockAlign);

    res = m_pAudioConverter->Init(amf::AMF_SURFACE_UNKNOWN, 0, 0);
    CHECK_AMF_ERROR_RETURN(res, L"m_pAudioConverter->Init() failed");

    pOutput->SetProperty(AMF_STREAM_ENABLED, true);

   return AMF_OK;
}
AMF_RESULT  PlaybackPipelineBase::InitVideo(amf::AMFOutput* pOutput, amf::AMF_MEMORY_TYPE presenterEngine)
{
    bool decodeAsAnnexBStream = false; // switches between Annex B and AVCC types of decode input.

    AMF_RESULT res = AMF_OK;
    amf::AMFBufferPtr pExtraData;
    std::wstring pVideoDecoderID;

	amf_int64 bitRate = 0;
	double dFps = 0.0;

    amf_int64 codecID = 0;
    amf_int64 surfaceFormat = amf::AMF_SURFACE_NV12;

    AMFRate frameRate = {};

    if(m_pVideoStreamParser != NULL)
    {
        if(!decodeAsAnnexBStream) // need to provide SPS/PPS if input stream will be AVCC ( not Annex B)
        {
            const unsigned char* extraData = m_pVideoStreamParser->GetExtraData();
            size_t extraDataSize = m_pVideoStreamParser->GetExtraDataSize();
            if (extraData && extraDataSize)
            {
                m_pContext->AllocBuffer(amf::AMF_MEMORY_HOST, extraDataSize, &pExtraData);

                memcpy(pExtraData->GetNative(), extraData, extraDataSize);
            }
        }
        m_iVideoWidth = m_pVideoStreamParser->GetPictureWidth();
        m_iVideoHeight = m_pVideoStreamParser->GetPictureHeight();

        pVideoDecoderID = m_pVideoStreamParser->GetCodecComponent();

		dFps = m_pVideoStreamParser->GetFrameRate();
        frameRate.num = amf_int32(dFps * 1000);
        frameRate.den = 1000;
    }
    else if(pOutput != NULL)
    {
        amf::AMFInterfacePtr pInterface;
        pOutput->GetProperty(AMF_STREAM_EXTRA_DATA, &pInterface);
        pExtraData = amf::AMFBufferPtr(pInterface);

        AMFSize frameSize;
        pOutput->GetProperty(AMF_STREAM_VIDEO_FRAME_SIZE, &frameSize);
        m_iVideoWidth = frameSize.width;
        m_iVideoHeight = frameSize.height;

        res= pOutput->GetProperty(AMF_STREAM_CODEC_ID, &codecID);
        pVideoDecoderID = StreamCodecIDtoDecoderID(AMF_STREAM_CODEC_ID_ENUM(codecID));

        pOutput->SetProperty(AMF_STREAM_ENABLED, true);

		pOutput->GetProperty(AMF_STREAM_BIT_RATE, &bitRate);

		pOutput->GetProperty(AMF_STREAM_VIDEO_FRAME_RATE, &frameRate);

        pOutput->GetProperty(AMF_STREAM_VIDEO_FORMAT, &surfaceFormat);

		dFps = frameRate.den == 0 ? 0.0 : (static_cast<double>(frameRate.num) / static_cast<double>(frameRate.den));
    }
    //---------------------------------------------------------------------------------------------
    // Init Video Decoder
    res = InitVideoDecoder(pVideoDecoderID.c_str(), codecID, (amf::AMF_SURFACE_FORMAT)surfaceFormat, bitRate, frameRate, pExtraData);
    CHECK_AMF_ERROR_RETURN(res, L"InitVideoDecoder() failed");

    if(m_pVideoDecoder != NULL)
    {
        m_pVideoDecoder->SetProperty(AMF_TIMESTAMP_MODE, amf_int64(m_pVideoStreamParser != NULL ? AMF_TS_DECODE : AMF_TS_SORT)); // our sample H264 parser provides decode order timestamps- change depend on demuxer
    }
    //---------------------------------------------------------------------------------------------
    // Init Presenter
    res = CreateVideoPresenter(presenterEngine, bitRate, dFps);
    CHECK_AMF_ERROR_RETURN(res, L"CreatePresenter() failed");

    bool bFullScreen = false;
    GetParam(PARAM_NAME_FULLSCREEN, bFullScreen);
    m_pVideoPresenter->SetFullScreen(bFullScreen);

    bool bEnablePIP = false;
    GetParam(PARAM_NAME_PIP, bEnablePIP);
    m_pVideoPresenter->SetEnablePIP(bEnablePIP);

    GetParam(PARAM_NAME_SIDE_BY_SIDE, m_bEnableSideBySide);

    if (m_eDecoderFormat == amf::AMF_SURFACE_P010 ||
        m_eDecoderFormat == amf::AMF_SURFACE_P012 ||
        m_eDecoderFormat == amf::AMF_SURFACE_P016) //HDR support
    {
        m_pVideoPresenter->SetInputFormat(amf::AMF_SURFACE_RGBA_F16);
// can be used if needed
//        m_pVideoPresenter->SetInputFormat(amf::AMF_SURFACE_R10G10B10A2);
    }

    res = m_pVideoPresenter->Init(m_iVideoWidth, m_iVideoHeight);
    CHECK_AMF_ERROR_RETURN(res, L"m_pVideoPresenter->Init(" << m_iVideoWidth << m_iVideoHeight << ") failed");

    //---------------------------------------------------------------------------------------------
    // Init Video Converter/Processor
    res = InitVideoProcessor();
    CHECK_AMF_ERROR_RETURN(res, L"InitVideoProcessor() failed");

    return AMF_OK;
}

void PlaybackPipelineBase::OnEof()
{
    Pipeline::OnEof();
    if(GetState() == PipelineStateEof)
    {
        bool bLoop = true;
        GetParam(PARAM_NAME_LOOP, bLoop);
        if(bLoop)
        {
            if(m_pDemuxerVideo != nullptr)
            {
                m_pDemuxerVideo->ReInit(0, 0);
            }
            if(m_pDemuxerAudio != nullptr)
            {
                m_pDemuxerAudio->ReInit(0, 0);
            }
            if(m_pVideoStreamParser != NULL)
            {
                m_pVideoStreamParser->ReInit();
            }
            if(m_pVideoDecoder != NULL)
            {
                m_pVideoDecoder->ReInit(m_iVideoWidth, m_iVideoHeight);
            }
            if(m_pVideoProcessor != NULL)
            {
                m_pVideoProcessor->ReInit(m_iVideoWidth, m_iVideoHeight);
            }
            if (m_pScaler != NULL)
            {
                m_pScaler->ReInit(m_iVideoWidth, m_iVideoHeight);
            }
            if (m_pHQScaler2 != NULL)
            {
                m_pHQScaler2->ReInit(m_iVideoWidth, m_iVideoHeight);
            }
            if(m_pVideoPresenter != NULL)
            {
                m_pVideoPresenter->Reset();
            }
            if(m_pAudioDecoder != NULL)
            {
                m_pAudioDecoder->ReInit(0, 0);
            }
            if(m_pAudioConverter != NULL)
            {
                m_pAudioConverter->ReInit(0, 0);
            }
            Restart();

        }
    }
}
AMF_RESULT PlaybackPipelineBase::OnActivate(bool bActivated)
{
    if (m_pVideoPresenter != nullptr)
    {
        if (bActivated)
        {
            bool bFullScreen = false;
            GetParam(PARAM_NAME_FULLSCREEN, bFullScreen);
            AMFTraceDebug(AMF_FACILITY, L"ProcessMessage() Window Activated");
            m_pVideoPresenter->SetFullScreen(bFullScreen);

            bool bEnablePIP = false;
            GetParam(PARAM_NAME_PIP, bEnablePIP);
            m_pVideoPresenter->SetEnablePIP(bEnablePIP);

            AMFFloatPoint2D fPIPFocusPos;
            GetParam(PARAM_NAME_PIP_FOCUS_X, fPIPFocusPos.x);
            GetParam(PARAM_NAME_PIP_FOCUS_Y, fPIPFocusPos.y);
            m_pVideoPresenter->SetPIPFocusPositions(fPIPFocusPos);
        }
        else
        {
            m_pVideoPresenter->SetFullScreen(false);
            m_pVideoPresenter->SetEnablePIP(false);
            AMFTraceDebug(AMF_FACILITY, L"ProcessMessage() Window Deactivated");
        }
    }
    return AMF_OK;
}
