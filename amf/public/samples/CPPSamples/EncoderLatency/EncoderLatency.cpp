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

// this sample encodes NV12 frames using AMF Encoder and writes them to H.264 elmentary stream

#include <stdio.h>
#ifdef _WIN32
#include <tchar.h>
#include <d3d9.h>
#include <d3d11.h>
#endif
#include "public/common/AMFFactory.h"
#include "public/common/Thread.h"
#include "public/common/AMFSTL.h"
#include "public/common/TraceAdapter.h"
#include "public/include/components/VideoEncoderVCE.h"
#include "public/include/components/VideoEncoderHEVC.h"
#include "public/samples/CPPSamples/common/EncoderParamsAVC.h"
#include "public/samples/CPPSamples/common/EncoderParamsHEVC.h"
#include "../common/ParametersStorage.h"
#include "../common/CmdLineParser.h"
#include "../common/CmdLogger.h"
#include "../common/PipelineDefines.h"
#include "SurfaceGenerator.h"

#include <fstream>
#include <iostream>

static const wchar_t* pCodec = AMFVideoEncoderVCE_AVC;
//static const wchar_t *pCodec = AMFVideoEncoder_HEVC;

static AMF_COLOR_BIT_DEPTH_ENUM eDepth = AMF_COLOR_BIT_DEPTH_8;
static amf_int32 frameCount = 500;
static amf_bool frameCountPassedIn = false;
static amf_int32 preRender = 0;
static amf_int32 vcnInstance = -1;
static bool bMaximumSpeed = true;
static float fFrameRate = 30.f;
static bool bRealTime = false;


#ifdef _WIN32
amf::AMF_MEMORY_TYPE    memoryTypeIn = amf::AMF_MEMORY_DX11;
#else
amf::AMF_MEMORY_TYPE    memoryTypeIn = amf::AMF_MEMORY_VULKAN;
#endif
amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_NV12;
amf_int32               widthIn = 1920;
amf_int32               heightIn = 1080;
amf_int32               rectSize = 50;

bool          writeToFileMode = false;
amf_int64     formatType = 0;
amf_int64     memoryIn = 0;
amf_wstring  codec = pCodec;
amf_wstring  workAlgorithm = L"ASAP";
amf_wstring   fileNameOut;


#define START_TIME_PROPERTY L"StartTimeProperty" // custom property ID to store submission time in a frame - all custom properties are copied from input to output

amf::AMFSurfacePtr pColor1;
amf::AMFSurfacePtr pColor2;

#define MILLISEC_TIME     10000

static const wchar_t*  PARAM_NAME_WORKALGORITHM = L"ALGORITHM";
static const wchar_t*  PARAM_NAME_PRERENDER     = L"PRERENDER";
static const wchar_t*  PARAM_NAME_VCN_INSTANCE  = L"VCNINSTANCE";
static const wchar_t*  PARAM_NAME_REALTIME      = L"REALTIME";


AMF_RESULT RegisterParams(ParametersStorage* pParams)
{
    pParams->SetParamDescription(PARAM_NAME_WORKALGORITHM, ParamCommon, L"'ASAP' or 'OneInOne' frames submission algorithm", NULL);
    pParams->SetParamDescription(PARAM_NAME_ENGINE, ParamCommon, L"Memory type: DX9Ex, DX11, Vulkan (h.264 only)", ParamConverterMemoryType);
    pParams->SetParamDescription(PARAM_NAME_CODEC, ParamCommon, L"Codec name (AVC or H264, HEVC or H265)", ParamConverterCodec);
    pParams->SetParamDescription(PARAM_NAME_INPUT_FORMAT, ParamCommon, L"Supported file formats: RGBA_F16, R10G10B10A2, NV12, P010", ParamConverterFormat);
    pParams->SetParamDescription(PARAM_NAME_INPUT_FRAMES, ParamCommon, L"Output number of frames", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_PRERENDER, ParamCommon, L"Pre-render number of frames", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_OUTPUT_WIDTH, ParamCommon, L"Output resolution, width", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_OUTPUT_HEIGHT, ParamCommon, L"Output resolution, height", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_VCN_INSTANCE, ParamCommon, L"VCN to test (0 or 1). Navi21 and up only", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_OUTPUT, ParamCommon, L"Output file name", NULL);
    pParams->SetParamDescription(PARAM_NAME_INPUT, ParamCommon, L"Input file name", NULL);
    pParams->SetParamDescription(PARAM_NAME_INPUT_WIDTH, ParamCommon, L"Input file width", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_INPUT_HEIGHT, ParamCommon, L"Input file height", ParamConverterInt64);
    pParams->SetParamDescription(PARAM_NAME_REALTIME, ParamCommon, L"Bool, Keep real-time framerate, default false", ParamConverterBoolean);

    return AMF_OK;
}

#if defined(_WIN32)
AMF_RESULT ReadParams(ParametersStorage* params)
#else
AMF_RESULT ReadParams(ParametersStorage* params, int argc, char* argv[])
#endif
{
    // register all parameters first
    RegisterParams(params);
    RegisterEncoderParamsAVC(params);
    RegisterEncoderParamsHEVC(params);

#if defined(_WIN32)
    if (!parseCmdLineParameters(params))
#else
    if (!parseCmdLineParameters(params, argc, argv))
#endif
    {
        LOG_INFO(L"+++ standard +++");
        ParametersStorage paramsCommon;
        RegisterParams(&paramsCommon);
        LOG_INFO(paramsCommon.GetParamUsage());

        LOG_INFO(L"+++ AVC codec +++");
        ParametersStorage paramsAVC;
        RegisterEncoderParamsAVC(&paramsAVC);
        LOG_INFO(paramsAVC.GetParamUsage());

        LOG_INFO(L"+++ HEVC codec +++");
        ParametersStorage paramsHEVC;
        RegisterEncoderParamsHEVC(&paramsHEVC);
        LOG_INFO(paramsHEVC.GetParamUsage());

        return AMF_FAIL;
    }

    // read the codec
    params->GetParamWString(PARAM_NAME_CODEC, codec);

    // clear existing parameters
    params->Clear();

    // update the proper parameters for the correct codec
    RegisterParams(params);
    if (codec == amf_wstring(AMFVideoEncoderVCE_AVC))
    {
        RegisterEncoderParamsAVC(params);
    }
    else if (codec == amf_wstring(AMFVideoEncoder_HEVC))
    {
        RegisterEncoderParamsHEVC(params);
    }
    else
    {
        LOG_ERROR(L"Invalid codec ID");
        return AMF_FAIL;
    }

    // parse parameters for a final time
#if defined(_WIN32)
    if (!parseCmdLineParameters(params))
#else
    if (!parseCmdLineParameters(params, argc, argv))
#endif
    {
        return AMF_FAIL;
    }


    // load paramters
    if (params->GetParam(PARAM_NAME_INPUT_FRAMES, frameCount) == AMF_OK)
    {
        frameCountPassedIn = true;
    }
    if (params->GetParam(PARAM_NAME_INPUT_HEIGHT, NULL) == AMF_OK &&
        params->GetParam(PARAM_NAME_INPUT_WIDTH, NULL) == AMF_OK)
    {
        params->GetParam(PARAM_NAME_INPUT_WIDTH, widthIn);
        params->GetParam(PARAM_NAME_INPUT_HEIGHT, heightIn);
    }
    else
    {
        params->GetParam(PARAM_NAME_OUTPUT_WIDTH, widthIn);
        params->GetParam(PARAM_NAME_OUTPUT_HEIGHT, heightIn);
    }
    params->GetParam(PARAM_NAME_PRERENDER, preRender);
    params->GetParam(PARAM_NAME_VCN_INSTANCE, vcnInstance);

    params->GetParamWString(PARAM_NAME_WORKALGORITHM, workAlgorithm);
    workAlgorithm = amf::amf_string_to_upper(workAlgorithm);
    params->GetParamWString(PARAM_NAME_OUTPUT, fileNameOut);

    params->GetParam(PARAM_NAME_REALTIME, bRealTime);

    if (codec == amf_wstring(AMFVideoEncoder_HEVC))
    {
        amf_int64 colorDepth;
        if (params->GetParam(AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH, colorDepth) == AMF_OK)
        {
            eDepth = colorDepth == 10 ? AMF_COLOR_BIT_DEPTH_10 : AMF_COLOR_BIT_DEPTH_8;
        }
        AMFRate fps = {};
        if (params->GetParam(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, fps) == AMF_OK)
        {
            fFrameRate = float(fps.num) / fps.den;
        }
    }
    else
    {
        AMFRate fps = {};
        if (params->GetParam(AMF_VIDEO_ENCODER_FRAMERATE, fps) == AMF_OK)
        {
            fFrameRate = float(fps.num) / fps.den;
        }

    }

    if (params->GetParam(PARAM_NAME_ENGINE, memoryIn) == AMF_OK)
    {
        memoryTypeIn = (amf::AMF_MEMORY_TYPE)memoryIn;
    }

    if (params->GetParam(PARAM_NAME_INPUT_FORMAT, formatType) == AMF_OK)
    {
        formatIn = (amf::AMF_SURFACE_FORMAT)formatType;
    }

    writeToFileMode = (fileNameOut.empty() != true);
    return AMF_OK;
}

AMF_RESULT ValidateParams(ParametersStorage * pParams)
{
    amf::AMFVariant tmp;
    if (eDepth == AMF_COLOR_BIT_DEPTH_10 && (formatIn == amf::AMF_SURFACE_NV12 || formatIn == amf::AMF_SURFACE_YV12 || formatIn == amf::AMF_SURFACE_BGRA
        || formatIn == amf::AMF_SURFACE_ARGB || formatIn == amf::AMF_SURFACE_RGBA || formatIn == amf::AMF_SURFACE_GRAY8 || formatIn == amf::AMF_SURFACE_YUV420P
        || formatIn == amf::AMF_SURFACE_U8V8 || formatIn == amf::AMF_SURFACE_YUY2))
    {
        if (pParams->GetParam(PARAM_NAME_INPUT_FORMAT, tmp) == AMF_OK)
        {
            printf("[ERROR] Selected surface format is not a 10-bit format, requested parameters combination can't be applied. Program will terminate\n");
            return AMF_INVALID_ARG;
        }

        printf("[WARNING] Default surface format NV12 is an 8-bit format. Program will use P010 (10-bit) format instead.\n");
        formatIn = amf::AMF_SURFACE_P010;
    }
    else if (eDepth == AMF_COLOR_BIT_DEPTH_8 && (formatIn == amf::AMF_SURFACE_P010 || formatIn == amf::AMF_SURFACE_R10G10B10A2 || formatIn == amf::AMF_SURFACE_RGBA_F16
        || formatIn == amf::AMF_SURFACE_UYVY || formatIn == amf::AMF_SURFACE_Y210 || formatIn == amf::AMF_SURFACE_Y410 || formatIn == amf::AMF_SURFACE_Y416 || formatIn == amf::AMF_SURFACE_GRAY32))
    {
        if (pParams->GetParam(AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH, tmp) == AMF_OK)
        {
            printf("[ERROR] Selected surface format is not a 10-bit format, requested parameters combination can't be applied. Program will terminate\n");
            return AMF_INVALID_ARG;
        }

        printf("[WARNING] Default bit depth is 8, but selected surface format is not an 8-bit format. Color depth will be changed to 10 bits\n");
        eDepth = AMF_COLOR_BIT_DEPTH_10;
    }

    if ( ((pParams->GetParam(PARAM_NAME_INPUT, NULL) == AMF_OK) || (pParams->GetParam(PARAM_NAME_INPUT_WIDTH, NULL) == AMF_OK) || (pParams->GetParam(PARAM_NAME_INPUT_HEIGHT, NULL) == AMF_OK)) &&
         ((pParams->GetParam(PARAM_NAME_OUTPUT_WIDTH, NULL) == AMF_OK) || (pParams->GetParam(PARAM_NAME_OUTPUT_HEIGHT, NULL) == AMF_OK)) )
    {
        printf("[WARNING] Input and output dimensions are exclusive - output values ignored and input values used\n");
    }

    return AMF_OK;
};


AMF_RESULT SetEncoderDefaults(ParametersStorage* pParams, amf::AMFComponent* encoder, const  amf_wstring& codec)
{
    AMF_RESULT res;
    if (codec == amf_wstring(AMFVideoEncoderVCE_AVC))
    {
        // usage parameters come first
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderUsage, encoder));

        // AMF_VIDEO_ENCODER_USAGE needs to be set before the rest
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCODING);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCODING) failed");

        // initialize command line parameters
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderStatic, encoder));
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderDynamic, encoder));

        // if we requested to run a specific VCN instance, check
        // if it's available, otherwise we can't run the test...
        if (vcnInstance != -1)
        {
            amf::AMFCapsPtr encoderCaps;
            if (encoder->GetCaps(&encoderCaps) == AMF_OK)
            {
                amf_uint64  vcnInstCount = 0;
                AMF_RETURN_IF_FAILED(encoderCaps->GetProperty(AMF_VIDEO_ENCODER_CAP_NUM_OF_HW_INSTANCES, &vcnInstCount), L"Multiple VCN instances not supported");
                AMF_RETURN_IF_FALSE((vcnInstance >= 0) && (vcnInstance < vcnInstCount), AMF_OUT_OF_RANGE, L"Invalid VCN instance %d, requested.  Only %d instances supported.", vcnInstance, vcnInstCount);

                res = encoder->SetProperty(AMF_VIDEO_ENCODER_INSTANCE_INDEX, vcnInstance);
                AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_INSTANCE_INDEX, %d) failed", vcnInstance);
            }
        }

        if (bMaximumSpeed)
        {

             encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
            // do not check error for AMF_VIDEO_ENCODER_B_PIC_PATTERN - can be not supported - check Capability Manager sample
            res = encoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED);
            AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED) failed");
        }

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, true);
        AMF_RETURN_IF_FAILED(res, L"encoder->SetProperty(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, true) failed");

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_QUERY_TIMEOUT, 50); //ms
        AMF_RETURN_IF_FAILED(res, L"encoder->SetProperty(AMF_VIDEO_ENCODER_QUERY_TIMEOUT, 50) failed");
    }
    else
    {
        // usage parameters come first
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderUsage, encoder));

        // AMF_VIDEO_ENCODER_HEVC_USAGE needs to be set before the rest
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCODING);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCODING)");

        // initialize command line parameters
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderStatic, encoder));
        AMF_RETURN_IF_FAILED(PushParamsToPropertyStorage(pParams, ParamEncoderDynamic, encoder));

        // if we requested to run a specific VCN instance, check
        // if it's available, otherwise we can't run the test...
        if (vcnInstance != -1)
        {
            amf::AMFCapsPtr encoderCaps;
            if (encoder->GetCaps(&encoderCaps) == AMF_OK)
            {
                amf_uint64  vcnInstCount = 0;
                AMF_RETURN_IF_FAILED(encoderCaps->GetProperty(AMF_VIDEO_ENCODER_HEVC_CAP_NUM_OF_HW_INSTANCES, &vcnInstCount), L"Multiple VCN instances not supported");
                AMF_RETURN_IF_FALSE((vcnInstance >= 0) && (vcnInstance < vcnInstCount), AMF_OUT_OF_RANGE, L"Invalid VCN instance %d, requested.  Only %d instances supported.", vcnInstance, vcnInstCount);

                res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_INSTANCE_INDEX, vcnInstance);
                AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_INSTANCE_INDEX, %d) failed", vcnInstance);
            }
        }

        if (bMaximumSpeed)
        {
            res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED);
            AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED)");
        }

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_LOWLATENCY_MODE, true);
        AMF_RETURN_IF_FAILED(res, L"encoder->SetProperty(AMF_VIDEO_ENCODER_LOWLATENCY_MODE, true) failed");

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUERY_TIMEOUT, 50); //ms
        AMF_RETURN_IF_FAILED(res, L"encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUERY_TIMEOUT, 50) failed");
    }
    return AMF_OK;
}


static void printTime(amf_pts total_time, amf_pts latency_time, amf_pts write_duration, amf_pts first_frame, amf_pts min_latency, amf_pts max_latency)
{
    fprintf(stderr, "Total  : Frames = %i Duration = %.2fms FPS = %.2fframes\n" \
           "Latency: First,Min,Max = %.2fms, %.2fms, %.2fms\n" \
           "Latency: Average = %.2fms\n",
        frameCount,
        double(total_time) / MILLISEC_TIME,
        double(AMF_SECOND) * double(frameCount) / double(total_time),
        double(first_frame) / MILLISEC_TIME,
        double(min_latency) / MILLISEC_TIME,
        double(max_latency) / MILLISEC_TIME,
        double(latency_time) / MILLISEC_TIME / frameCount
    );
    fflush(stderr);
}


class PollingThread : public amf::AMFThread
{
protected:
    amf::AMFContextPtr      m_pContext;
    amf::AMFComponentPtr    m_pEncoder;
    std::ofstream           m_pFile;
    bool                    isWritingToFile;
public:
    PollingThread(amf::AMFContext* context, amf::AMFComponent* encoder, const wchar_t* pFileName, bool writeToFile);
    ~PollingThread();
    virtual void Run();
};

PollingThread::PollingThread(amf::AMFContext * context, amf::AMFComponent * encoder, const wchar_t* pFileName, bool writeToFile) : m_pContext(context), m_pEncoder(encoder)
{
#ifdef _WIN32
    m_pFile.open(pFileName, std::ios::binary);
#else
    m_pFile.open( amf::amf_from_unicode_to_utf8(pFileName).c_str(), std::ios::binary);
#endif
    isWritingToFile = writeToFile;
}
PollingThread::~PollingThread()
{
    if (m_pFile)
    {
        m_pFile.close();
    }
}
void PollingThread::Run()
{
    RequestStop();

    amf_pts begin_time = amf_high_precision_clock();
    amf_pts latency_time = 0;
    amf_pts write_duration = 0;
    amf_pts last_poll_time = 0;
    amf_pts first_frame = 0;
    amf_pts min_latency = INT64_MAX;
    amf_pts max_latency = 0;

    while (true)
    {
        amf::AMFDataPtr data;
        AMF_RESULT res = m_pEncoder->QueryOutput(&data);
        if (res == AMF_EOF)
        {
            break; // Drain complete
        }
        if ((res != AMF_OK) && (res != AMF_REPEAT))
        {
            // trace possible error message
            break; // Drain complete
        }
        if (data != NULL)
        {
            amf_pts poll_time = amf_high_precision_clock();
            amf_pts start_time = 0;
            data->GetProperty(START_TIME_PROPERTY, &start_time);
            if (start_time < last_poll_time) // remove wait time if submission was faster then encode
            {
                start_time = last_poll_time;
            }
            last_poll_time = poll_time;

            amf_pts tmp_time = poll_time - start_time;
            if (first_frame == 0)
            {
                first_frame = tmp_time;
            }
            else
            {
                min_latency = min_latency < tmp_time ? min_latency : tmp_time;
                max_latency = max_latency > tmp_time ? max_latency : tmp_time;
            }

            latency_time += tmp_time;

            amf::AMFBufferPtr buffer(data); // query for buffer interface
            if (isWritingToFile)
            {
                m_pFile.write(reinterpret_cast<char*>(buffer->GetNative()), buffer->GetSize());
                write_duration += amf_high_precision_clock() - poll_time;
            }
        }
        else
        {
            amf_sleep(1);
        }
    }
    amf_pts end_time = amf_high_precision_clock();
    printTime(end_time - begin_time, latency_time, write_duration, first_frame, min_latency, max_latency);

    m_pEncoder = NULL;
    m_pContext = NULL;
}

void CheckAndRestartReader(RawStreamReader *pRawStreamReader)
{
    if ((frameCountPassedIn == true) && (pRawStreamReader->GetPosition() == 1.0))
    {
        pRawStreamReader->RestartReader();
    }
}

#if defined(_WIN32)
int _tmain(int argc, _TCHAR * argv[])
#else
int main(int argc, char* argv[])
#endif
{
    ParametersStorage params;
#if defined(_WIN32)
    AMF_RESULT res = ReadParams(&params);
#else
    AMF_RESULT res = ReadParams(&params, argc, argv);
#endif
    if (res != AMF_OK)
    {
        wprintf(L"Command line arguments couldn't be parsed");
        return -1;
    }

    if (ValidateParams(&params) == AMF_INVALID_ARG)
    {
        return -1;
    }

    res = g_AMFFactory.Init();
    if (res != AMF_OK)
    {
        wprintf(L"AMF Failed to initialize");
        return -1;
    }

    ::amf_increase_timer_precision();

    amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_CONSOLE, true);
    amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_DEBUG_OUTPUT, true);

    // initialize AMF
    amf::AMFContextPtr context;
    amf::AMFComponentPtr encoder;
    amf::AMFSurfacePtr surfaceIn;

    // context
    res = g_AMFFactory.GetFactory()->CreateContext(&context);
    AMF_RETURN_IF_FAILED(res, L"CreateContext() failed");

    if (memoryTypeIn == amf::AMF_MEMORY_VULKAN)
    {
        res = amf::AMFContext1Ptr(context)->InitVulkan(NULL);
        AMF_RETURN_IF_FAILED(res, L"InitVulkan(NULL) failed");
        PrepareFillFromHost(context);
    }
#ifdef _WIN32
    else if (memoryTypeIn == amf::AMF_MEMORY_DX9)
    {
        res = context->InitDX9(NULL); // can be DX9 or DX9Ex device
        AMF_RETURN_IF_FAILED(res, L"InitDX9(NULL) failed");
    }
    else if (memoryTypeIn == amf::AMF_MEMORY_DX11)
    {
        res = context->InitDX11(NULL); // can be DX11 device
        AMF_RETURN_IF_FAILED(res, L"InitDX11(NULL) failed");
        PrepareFillFromHost(context);
    }
#endif

    // file reader if needed
    RawStreamReaderPtr fileReader;
    if (params.GetParam(PARAM_NAME_INPUT, NULL) == AMF_OK)
    {
        fileReader = RawStreamReaderPtr(new RawStreamReader());
        res = fileReader->Init(&params, context);
        AMF_RETURN_IF_FAILED(res, L"fileReader->Init() failed");

        // if width/height/format are not provided, reader will
        // try to figure out from the name of the file
        widthIn = fileReader->GetWidth();
        heightIn = fileReader->GetHeight();
        formatIn = fileReader->GetFormat();
    }
    PipelineElementPtr pipelineElPtr(fileReader);

    // component: encoder
    res = g_AMFFactory.GetFactory()->CreateComponent(context, codec.c_str(), &encoder);
    AMF_RETURN_IF_FAILED(res, L"CreateComponent(%s) failed", pCodec);

    res = SetEncoderDefaults(&params, encoder, codec);
    AMF_RETURN_IF_FAILED(res, L"Could not set default values in encoder.");

    res = encoder->Init(formatIn, widthIn, heightIn);
    AMF_RETURN_IF_FAILED(res, L"encoder->Init() failed");

    // if we want to use pre-rendered frames
    // start filling them now
    std::vector<amf::AMFSurfacePtr>  preRenderedSurf;
    for (amf_int i = 0; i < preRender; i++)
    {
        amf::AMFSurfacePtr  surfacePreRender;
        if (pipelineElPtr != NULL)
        {
            CheckAndRestartReader(fileReader.get());
            res = ReadSurface(pipelineElPtr, &surfacePreRender, memoryTypeIn);
            if (res == AMF_EOF)
            {
                break;
            }
        }
        else
        {
            FillSurface(context, &surfacePreRender, true);
        }
        preRenderedSurf.push_back(surfacePreRender);
    }

    if (workAlgorithm == L"ASAP")
    {
        PollingThread thread(context, encoder, fileNameOut.c_str(), writeToFileMode);
        thread.Start();

        // encode some frames
        amf_int32 submitted = 0;
        while (submitted < frameCount)
        {
            if (preRenderedSurf.empty() == false)
            {
                surfaceIn = preRenderedSurf[submitted % preRenderedSurf.size()];
            }
            else
            {
                if (pipelineElPtr != NULL)
                {
                    CheckAndRestartReader(fileReader.get());
                    res = ReadSurface(pipelineElPtr, &surfaceIn, memoryTypeIn);
                    if (res == AMF_EOF)
                    {
                        frameCount = submitted;
                        continue;
                    }
                }
                else
                {
                    FillSurface(context, &surfaceIn, false);
                }
            }

            // encode
            amf_pts start_time = amf_high_precision_clock();
            surfaceIn->SetProperty(START_TIME_PROPERTY, start_time);

            res = encoder->SubmitInput(surfaceIn);
            if (res == AMF_NEED_MORE_INPUT)
            {
                // do nothing
            }
            else if (res == AMF_INPUT_FULL || res == AMF_DECODER_NO_FREE_SURFACES)
            {   // queue is full; sleep, try to get ready surfaces  in polling thread and repeat submission
                amf_sleep(1);
            }
            else
            {
                AMF_RETURN_IF_FAILED(res, L"SubmitInput() failed");

                surfaceIn = NULL;
                submitted++;
            }
        }

        // drain encoder; input queue can be full
        while (true)
        {
            res = encoder->Drain();
            if (res != AMF_INPUT_FULL) // handle full queue
            {
                break;
            }
            amf_sleep(1); // input queue is full: wait and try again
        }
        thread.WaitForStop();
    }
    else
    {
        // encode some frames
        amf_int32 submitted = 0;
        amf_pts   first_frame = 0;
        amf_pts   min_latency = INT64_MAX;
        amf_pts   max_latency = 0;
        amf_pts   latency_time = 0;
        amf_pts   write_duration = 0;
        amf::AMFPreciseWaiter waiter;

        // output file, if we have one
#ifdef _WIN32
        std::ofstream  logFile(fileNameOut.c_str(), std::ios::binary);
#else
        std::ofstream  logFile( amf::amf_from_unicode_to_utf8(fileNameOut.c_str()).c_str(), std::ios::binary);
#endif
        amf_pts begin_time = amf_high_precision_clock();
        while (submitted < frameCount)
        {
            amf_pts begin_frame = amf_high_precision_clock();

            if (preRenderedSurf.empty() == false)
            {
                surfaceIn = preRenderedSurf[submitted % preRenderedSurf.size()];
            }
            else
            {
                if (pipelineElPtr != NULL)
                {
                    CheckAndRestartReader(fileReader.get());
                    res = ReadSurface(pipelineElPtr, &surfaceIn, memoryTypeIn);
                    if (res == AMF_EOF)
                    {
                        frameCount = submitted;
                        continue;
                    }
                }
                else
                {
                    FillSurface(context, &surfaceIn, true);
                }
            }

            // encode
            amf_pts start_time = amf_high_precision_clock();

            // we're doing frame-in/frame-out so the input
            // should never be full
            res = encoder->SubmitInput(surfaceIn);
            AMF_RETURN_IF_FAILED(res, L"SubmitInput() failed");

            surfaceIn = NULL;
            submitted++;

            amf::AMFDataPtr data;
            do
            {
                res = encoder->QueryOutput(&data);
                if (res == AMF_REPEAT)
                {
                    amf_sleep(1);
                }
            } while (res == AMF_REPEAT);

            amf_pts poll_time = amf_high_precision_clock();
            amf_pts tmp_time = poll_time - start_time;

            if (first_frame == 0)
            {
                first_frame = tmp_time;
            }
            else
            {
                min_latency = (tmp_time < min_latency) ? tmp_time : min_latency;
                max_latency = (tmp_time > max_latency) ? tmp_time : max_latency;
            }
            latency_time += tmp_time;

            if ((data != NULL) && (writeToFileMode == true))
            {
                amf::AMFBufferPtr buffer(data); // query for buffer interface
                logFile.write(reinterpret_cast<char*>(buffer->GetNative()), buffer->GetSize());
                write_duration += amf_high_precision_clock() - poll_time;
            }
            if (bRealTime)
            {
                amf_pts end_frame = amf_high_precision_clock();
                amf_pts time_to_sleep = amf_pts(AMF_SECOND / fFrameRate) - (end_frame - begin_frame);
                if (time_to_sleep > 0)
                {
                    waiter.Wait(time_to_sleep);
                }
            }
        }
        amf_pts end_time = amf_high_precision_clock();
        printTime(end_time - begin_time, latency_time, write_duration, first_frame, min_latency, max_latency);
    }

    // clear any pre-rendered frames
    preRenderedSurf.clear();

    pColor1 = NULL;
    pColor2 = NULL;

    // cleanup in this order
    surfaceIn = NULL;
    encoder->Terminate();
    encoder = NULL;
    fileReader = NULL;
    pipelineElPtr = NULL;
    context->Terminate();
    context = NULL; // context is the last

    g_AMFFactory.Terminate();

    return 0;
}
