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
#include "public/include/components/VideoEncoderVCE.h"
#include "public/include/components/VideoEncoderHEVC.h"
#include "public/common/Thread.h"
#include "public/common/AMFSTL.h"
#include "public/common/TraceAdapter.h"
#include <fstream>

#define AMF_FACILITY L"SimpleEncoder"

//#define ENABLE_4K
//#define ENABLE_EFC // color conversion inside encoder component. Will use GFX or HW if available
//#define ENABLE_10_BIT

#if defined(ENABLE_10_BIT)
static const wchar_t *pCodec = AMFVideoEncoder_HEVC;
static AMF_COLOR_BIT_DEPTH_ENUM eDepth = AMF_COLOR_BIT_DEPTH_10;
#else
static const wchar_t *pCodec = AMFVideoEncoderVCE_AVC;
//static const wchar_t *pCodec = AMFVideoEncoder_HEVC;
static AMF_COLOR_BIT_DEPTH_ENUM eDepth = AMF_COLOR_BIT_DEPTH_8;
#endif

#ifdef _WIN32
static amf::AMF_MEMORY_TYPE memoryTypeIn  = amf::AMF_MEMORY_DX11;
#else
static amf::AMF_MEMORY_TYPE memoryTypeIn  = amf::AMF_MEMORY_VULKAN;
#endif

#if defined ENABLE_EFC
static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_RGBA;
//static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_BGRA;
//static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_R10G10B10A2;
//static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_RGBA_F16;
#else
static amf::AMF_SURFACE_FORMAT formatIn   = amf::AMF_SURFACE_NV12;
#endif

#if defined(ENABLE_4K)
static amf_int32 widthIn                  = 1920*2;
static amf_int32 heightIn                 = 1080*2;

#else
static amf_int32 widthIn                  = 1920;
static amf_int32 heightIn                 = 1080;
#endif
static amf_int32 frameRateIn              = 30;
static amf_int64 bitRateIn                = 5000000L; // in bits, 5MBit
static amf_int32 rectSize                 = 50;
static amf_int32 frameCount               = 500;
static bool bMaximumSpeed = true;

#define START_TIME_PROPERTY L"StartTimeProperty" // custom property ID to store submission time in a frame - all custom properties are copied from input to output

static const wchar_t *fileNameOut_h264 = L"./output.h264";
static const wchar_t *fileNameOut_h265 = L"./output.h265";

static amf_int32 xPos = 0;
static amf_int32 yPos = 0;

#ifdef _WIN32
static void FillSurfaceDX9(amf::AMFContext *context, amf::AMFSurface *surface);
static void FillSurfaceDX11(amf::AMFContext *context, amf::AMFSurface *surface);
#endif
static void PrepareFillFromHost(amf::AMFContext *context);
static void FillSurfaceVulkan(amf::AMFContext *context, amf::AMFSurface *surface);

amf::AMFSurfacePtr pColor1;
amf::AMFSurfacePtr pColor2;

#define MILLISEC_TIME     10000

class PollingThread : public amf::AMFThread
{
protected:
    amf::AMFContextPtr      m_pContext;
    amf::AMFComponentPtr    m_pEncoder;
    std::ofstream           m_pFile;
public:
    PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const wchar_t *pFileName);
    ~PollingThread();
    virtual void Run();
};

#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    AMF_RESULT res = AMF_OK; // error checking can be added later
    res = g_AMFFactory.Init();
    if(res != AMF_OK)
    {
        wprintf(L"AMF Failed to initialize");
        return 1;
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

    if(memoryTypeIn == amf::AMF_MEMORY_VULKAN)
    {
        res = amf::AMFContext1Ptr(context)->InitVulkan(NULL);
        AMF_RETURN_IF_FAILED(res, L"InitVulkan(NULL) failed");
        PrepareFillFromHost(context);
    }
#ifdef _WIN32
    else if(memoryTypeIn == amf::AMF_MEMORY_DX9)
    {

        res = context->InitDX9(NULL); // can be DX9 or DX9Ex device
        AMF_RETURN_IF_FAILED(res, L"InitDX9(NULL) failed");
    }
    else if(memoryTypeIn == amf::AMF_MEMORY_DX11)
    {
        res = context->InitDX11(NULL); // can be DX11 device
        AMF_RETURN_IF_FAILED(res, L"InitDX11(NULL) failed");
        PrepareFillFromHost(context);
    }
#endif

    // component: encoder
    res = g_AMFFactory.GetFactory()->CreateComponent(context, pCodec, &encoder);
    AMF_RETURN_IF_FAILED(res, L"CreateComponent(%s) failed", pCodec);

    if(amf_wstring(pCodec) == amf_wstring(AMFVideoEncoderVCE_AVC))
    {
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCODING);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCODING) failed");

        if(bMaximumSpeed)
        {
            res = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
            // do not check error for AMF_VIDEO_ENCODER_B_PIC_PATTERN - can be not supported - check Capability Manager sample
            res = encoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED);
            AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED) failed");
        }

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitRateIn);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, %" LPRId64 L") failed", bitRateIn);
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, %dx%d) failed", frameRateIn, 1);

#if defined(ENABLE_4K)
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH) failed");

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51)");
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0)");
#endif
    }
    else
    {
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCODING);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCODING)");

        if(bMaximumSpeed)
        {
            res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED);
            AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED)");
        }

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitRateIn);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, %" LPRId64 L") failed", bitRateIn);
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, %dx%d) failed", frameRateIn, 1);

        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH, eDepth);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_COLOR_BIT_DEPTH, %d) failed", eDepth);


#if defined(ENABLE_4K)
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH) failed");
        res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5_1);
        AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5_1) failed");
#endif
    }
    res = encoder->Init(formatIn, widthIn, heightIn);
    AMF_RETURN_IF_FAILED(res, L"encoder->Init() failed");


    PollingThread thread(context, encoder, amf_wstring(pCodec) == amf_wstring(AMFVideoEncoderVCE_AVC) ? fileNameOut_h264 : fileNameOut_h265);
    thread.Start();

    // encode some frames
    amf_int32 submitted = 0;
    while(submitted < frameCount)
    {
        if(surfaceIn == NULL)
        {
            surfaceIn = NULL;
            res = context->AllocSurface(memoryTypeIn, formatIn, widthIn, heightIn, &surfaceIn);
            AMF_RETURN_IF_FAILED(res, L"AllocSurface() failed");

            if(memoryTypeIn == amf::AMF_MEMORY_VULKAN)
            {
                FillSurfaceVulkan(context, surfaceIn);
            }
#ifdef _WIN32
            else if(memoryTypeIn  == amf::AMF_MEMORY_DX9)
            {
                FillSurfaceDX9(context, surfaceIn);
            }
            else
            {
                FillSurfaceDX11(context, surfaceIn);
            }
#endif
        }
        // encode
        amf_pts start_time = amf_high_precision_clock();
        surfaceIn->SetProperty(START_TIME_PROPERTY, start_time);

        res = encoder->SubmitInput(surfaceIn);
        if(res == AMF_NEED_MORE_INPUT) // handle full queue
        {
            // do nothing
        }
		else if (res == AMF_INPUT_FULL || res == AMF_DECODER_NO_FREE_SURFACES)
		{
			amf_sleep(1); // input queue is full: wait, poll and submit again
		}
		else
        {
            AMF_RETURN_IF_FAILED(res, L"SubmitInput() failed");
            surfaceIn = NULL;
            submitted++;
        }
    }
    // drain encoder; input queue can be full
    while(true)
    {
        res = encoder->Drain();
        if(res != AMF_INPUT_FULL) // handle full queue
        {
            break;
        }
        amf_sleep(1); // input queue is full: wait and try again
    }
    thread.WaitForStop();

    pColor1 = NULL;
    pColor2 = NULL;

    // cleanup in this order
    surfaceIn = NULL;
    encoder->Terminate();
    encoder = NULL;
    context->Terminate();
    context = NULL; // context is the last

    g_AMFFactory.Terminate();
    return 0;
}

#ifdef _WIN32
static void FillSurfaceDX9(amf::AMFContext *context, amf::AMFSurface *surface)
{
    HRESULT hr = S_OK;
    // fill surface with something something useful. We fill with color and color rect
    D3DCOLOR color1 = D3DCOLOR_XYUV (128, 255, 128);
    D3DCOLOR color2 = D3DCOLOR_XYUV (128, 0, 128);
    // get native DX objects
    IDirect3DDevice9 *deviceDX9 = (IDirect3DDevice9 *)context->GetDX9Device(); // no reference counting - do not Release()
    IDirect3DSurface9* surfaceDX9 = (IDirect3DSurface9*)surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()
    hr = deviceDX9->ColorFill(surfaceDX9, NULL, color1);

    if(xPos + rectSize > widthIn)
    {
        xPos = 0;
    }
    if(yPos + rectSize > heightIn)
    {
        yPos = 0;
    }
    RECT rect = {xPos, yPos, xPos + rectSize, yPos + rectSize};
    hr = deviceDX9->ColorFill(surfaceDX9, &rect, color2);

    xPos+=2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
    yPos+=2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
}




static void FillSurfaceDX11(amf::AMFContext *context, amf::AMFSurface *surface)
{
    HRESULT hr = S_OK;
    // fill surface with something something useful. We fill with color and color rect
    // get native DX objects
    ID3D11Device *deviceDX11 = (ID3D11Device *)context->GetDX11Device(); // no reference counting - do not Release()
    ID3D11Texture2D* surfaceDX11 = (ID3D11Texture2D*)surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()

    ID3D11DeviceContext *deviceContextDX11 = NULL;
    deviceDX11->GetImmediateContext(&deviceContextDX11);

    ID3D11Texture2D* surfaceDX11Color1 = (ID3D11Texture2D*)pColor1->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()
    deviceContextDX11->CopyResource(surfaceDX11, surfaceDX11Color1);

    if(xPos + rectSize > widthIn)
    {
        xPos = 0;
    }
    if(yPos + rectSize > heightIn)
    {
        yPos = 0;
    }
    D3D11_BOX rect = {0, 0, 0, (UINT) rectSize, (UINT) rectSize, 1};

    ID3D11Texture2D* surfaceDX11Color2 = (ID3D11Texture2D*)pColor2->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()

    deviceContextDX11->CopySubresourceRegion(surfaceDX11, 0, xPos, yPos, 0, surfaceDX11Color2, 0, &rect);
    deviceContextDX11->Flush();

    xPos+=2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
    yPos+=2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
    deviceContextDX11->Release();
}
#endif

static void FillNV12SurfaceWithColor(amf::AMFSurface* surface, amf_uint8 Y, amf_uint8 U, amf_uint8 V)
{
    amf::AMFPlane *pPlaneY =  surface->GetPlaneAt(0);
    amf::AMFPlane *pPlaneUV =  surface->GetPlaneAt(1);

    amf_int32 widthY = pPlaneY->GetWidth();
    amf_int32 heightY = pPlaneY->GetHeight();
    amf_int32 lineY = pPlaneY->GetHPitch();

    amf_uint8 *pDataY = (amf_uint8 *)pPlaneY->GetNative();

    for(amf_int32 y = 0; y < heightY; y++)
    {
        amf_uint8 *pDataLine = pDataY + y * lineY;
        memset(pDataLine, Y, widthY);
    }

    amf_int32 widthUV = pPlaneUV->GetWidth();
    amf_int32 heightUV = pPlaneUV->GetHeight();
    amf_int32 lineUV = pPlaneUV->GetHPitch();

    amf_uint8 *pDataUV = (amf_uint8 *)pPlaneUV->GetNative();

    for(amf_int32 y = 0; y < heightUV; y++)
    {
        amf_uint8 *pDataLine = pDataUV + y * lineUV;
        for(amf_int32 x = 0; x < widthUV; x++)
        {
            *pDataLine++ = U;
            *pDataLine++ = V;
        }
    }
}
static void FillRGBASurfaceWithColor(amf::AMFSurface* surface, amf_uint8 R, amf_uint8 G, amf_uint8 B)
{
	amf::AMFPlane *pPlane = surface->GetPlaneAt(0);

	amf_int32 width = pPlane->GetWidth();
	amf_int32 height = pPlane->GetHeight();
	amf_int32 line = pPlane->GetHPitch();

	amf_uint8 *pData = (amf_uint8 *)pPlane->GetNative();

	for (amf_int32 y = 0; y < height; y++)
	{
		amf_uint8 *pDataLine = pData + y * line;
		for (amf_int32 x = 0; x < width; x++)
		{
			*pDataLine++ = R;
			*pDataLine++ = G;
			*pDataLine++ = B;
			*pDataLine++ = 255; //A
		}
	}
}
static void FillBGRASurfaceWithColor(amf::AMFSurface* surface, amf_uint8 R, amf_uint8 G, amf_uint8 B)
{
    amf::AMFPlane *pPlane = surface->GetPlaneAt(0);

    amf_int32 width = pPlane->GetWidth();
    amf_int32 height = pPlane->GetHeight();
    amf_int32 line = pPlane->GetHPitch();

    amf_uint8 *pData = (amf_uint8 *)pPlane->GetNative();

    for (amf_int32 y = 0; y < height; y++)
    {
        amf_uint8 *pDataLine = pData + y * line;
        for (amf_int32 x = 0; x < width; x++)
        {
            *pDataLine++ = B;
            *pDataLine++ = G;
            *pDataLine++ = R;
            *pDataLine++ = 255; //A
        }
    }
}
class AMFHalfFloat
{
public:
    AMFHalfFloat();
    AMF_FORCEINLINE static amf_uint16 ToHalfFloat(amf_float value)
    {
        union FloatBits
        {
            amf_float f;
            amf_uint32 u;
        };

        FloatBits val;
        val.f = value;

        return amf_uint16(m_basetable[(val.u >> 23) & 0x1ff] + ((val.u & 0x007fffff) >> m_shifttable[(val.u >> 23) & 0x1ff]));
    }

    AMF_FORCEINLINE static float FromHalfFloat(amf_uint16 value)
    {
        uint32_t mantissa = (uint32_t)(value & 0x03FF);

        uint32_t exponent = (value & 0x7C00);
        if (exponent == 0x7C00) // INF/NAN
        {
            exponent = (uint32_t)0x8f;
        }
        else if (exponent != 0)  // The value is normalized
        {
            exponent = (uint32_t)((value >> 10) & 0x1F);
        }
        else if (mantissa != 0)     // The value is denormalized
        {
            // Normalize the value in the resulting float
            exponent = 1;

            do
            {
                exponent--;
                mantissa <<= 1;
            } while ((mantissa & 0x0400) == 0);

            mantissa &= 0x03FF;
        }
        else                        // The value is zero
        {
            exponent = (uint32_t)-112;
        }

        uint32_t result = ((value & 0x8000) << 16) | // Sign
            ((exponent + 112) << 23) | // exponent
            (mantissa << 13);          // mantissa

        return reinterpret_cast<float*>(&result)[0];
    }
private:

    static amf_uint16 m_basetable[512];
    static amf_uint8 m_shifttable[512];

    static void GenerateHalfFloatConversionTables();

};
AMFHalfFloat::AMFHalfFloat()
{
    GenerateHalfFloatConversionTables();
}

static AMFHalfFloat s_InitHalfFLoat;
void AMFHalfFloat::GenerateHalfFloatConversionTables()
{
    for (unsigned int i = 0; i < 256; i++)
    {
        int e = i - 127;

        // map very small numbers to 0
        if (e < -24)
        {
            m_basetable[i | 0x000] = 0x0000;
            m_basetable[i | 0x100] = 0x8000;
            m_shifttable[i | 0x000] = 24;
            m_shifttable[i | 0x100] = 24;
        }
        // map small numbers to denorms
        else if (e < -14)
        {
            m_basetable[i | 0x000] = (0x0400 >> (-e - 14));
            m_basetable[i | 0x100] = (0x0400 >> (-e - 14)) | 0x8000;
            m_shifttable[i | 0x000] = amf_uint8(-e - 1);
            m_shifttable[i | 0x100] = amf_uint8(-e - 1);
        }
        // normal numbers lose precision
        else if (e <= 15)
        {
            m_basetable[i | 0x000] = amf_uint16((e + 15) << 10);
            m_basetable[i | 0x100] = amf_uint16(((e + 15) << 10) | 0x8000);
            m_shifttable[i | 0x000] = 13;
            m_shifttable[i | 0x100] = 13;
        }
        // large numbers map to infinity
        else if (e < 128)
        {
            m_basetable[i | 0x000] = 0x7C00;
            m_basetable[i | 0x100] = 0xFC00;
            m_shifttable[i | 0x000] = 24;
            m_shifttable[i | 0x100] = 24;
        }
        // infinity an NaN stay so
        else
        {
            m_basetable[i | 0x000] = 0x7C00;
            m_basetable[i | 0x100] = 0xFC00;
            m_shifttable[i | 0x000] = 13;
            m_shifttable[i | 0x100] = 13;
        }
    }
}
amf_uint16 AMFHalfFloat::m_basetable[512];
amf_uint8 AMFHalfFloat::m_shifttable[512];

static void FillRGBA_F16SurfaceWithColor(amf::AMFSurface* surface, amf_uint8 R, amf_uint8 G, amf_uint8 B)
{
    amf::AMFPlane *pPlane = surface->GetPlaneAt(0);

    amf_int32 width = pPlane->GetWidth();
    amf_int32 height = pPlane->GetHeight();
    amf_int32 line = pPlane->GetHPitch();

    amf_uint8 *pData = (amf_uint8 *)pPlane->GetNative();

    amf_uint16 fR = AMFHalfFloat::ToHalfFloat((float)R / 255.f);
    amf_uint16 fG = AMFHalfFloat::ToHalfFloat((float)G / 255.f);
    amf_uint16 fB = AMFHalfFloat::ToHalfFloat((float)B / 255.f);
    amf_uint16 fA = AMFHalfFloat::ToHalfFloat((float)255.f / 255.f);

    for (amf_int32 y = 0; y < height; y++)
    {
        amf_uint16* pDataLine = (amf_uint16*)(pData + y * line);
        for (amf_int32 x = 0; x < width; x++)
        {
            *pDataLine++ = fR;
            *pDataLine++ = fG;
            *pDataLine++ = fB;
            *pDataLine++ = fA; //A
        }
    }
}
static void FillR10G10B10A2SurfaceWithColor(amf::AMFSurface* surface, amf_uint8 R, amf_uint8 G, amf_uint8 B)
{
    amf::AMFPlane *pPlane = surface->GetPlaneAt(0);

    amf_int32 width = pPlane->GetWidth();
    amf_int32 height = pPlane->GetHeight();
    amf_int32 line = pPlane->GetHPitch();

    amf_uint8 *pData = (amf_uint8 *)pPlane->GetNative();

    amf_uint32 r10 = ((amf_uint32)R * 0x3FF / 0xFF) << 2;
    amf_uint32 g10 = ((amf_uint32)G * 0x3FF / 0xFF) << 12;
    amf_uint32 b10 = ((amf_uint32)B * 0x3FF / 0xFF) << 22;
    amf_uint32 a2 = 0x3;

    amf_uint32 color = r10 | g10 | b10 | a2;

    for (amf_int32 y = 0; y < height; y++)
    {
        amf_uint32* pDataLine = (amf_uint32*)(pData + y * line);
        for (amf_int32 x = 0; x < width; x++)
        {
            *pDataLine++ = color;
        }
    }
}
static void PrepareFillFromHost(amf::AMFContext *context)
{
    AMF_RESULT res = AMF_OK; // error checking can be added later
    res = context->AllocSurface(amf::AMF_MEMORY_HOST, formatIn, widthIn, heightIn, &pColor1);
    res = context->AllocSurface(amf::AMF_MEMORY_HOST, formatIn, rectSize, rectSize, &pColor2);

    switch (formatIn)
    {
    case amf::AMF_SURFACE_NV12:
        FillNV12SurfaceWithColor(pColor2, 128, 0, 128);
        FillNV12SurfaceWithColor(pColor1, 128, 255, 128);
        break;
    case amf::AMF_SURFACE_RGBA:
        FillRGBASurfaceWithColor(pColor2, 255, 0, 0);
        FillRGBASurfaceWithColor(pColor1, 0, 0, 255);
        break;
    case amf::AMF_SURFACE_BGRA:
        FillBGRASurfaceWithColor(pColor2, 255, 0, 0);
        FillBGRASurfaceWithColor(pColor1, 0, 0, 255);
        break;
    case amf::AMF_SURFACE_R10G10B10A2:
        FillR10G10B10A2SurfaceWithColor(pColor2, 255, 0, 0);
        FillR10G10B10A2SurfaceWithColor(pColor1, 0, 0, 255);
        break;
    case amf::AMF_SURFACE_RGBA_F16:
        FillRGBA_F16SurfaceWithColor(pColor2, 255, 0, 0);
        FillRGBA_F16SurfaceWithColor(pColor1, 0, 0, 255);
        break;
    default:
        break;
    }
    pColor1->Convert(memoryTypeIn);
    pColor2->Convert(memoryTypeIn);
}

static void FillSurfaceVulkan(amf::AMFContext *context, amf::AMFSurface *surface)
{
        amf::AMFComputePtr compute;
        context->GetCompute(amf::AMF_MEMORY_VULKAN, &compute);

        if(xPos + rectSize > widthIn)
        {
            xPos = 0;
        }
        if(yPos + rectSize > heightIn)
        {
            yPos = 0;
        }

        for(int p = 0; p < pColor1->GetPlanesCount(); p++)
        {
            amf::AMFPlane *plane = pColor1->GetPlaneAt(p);
            amf_size origin1[3] = {0, 0 , 0};
            amf_size region1[3] = {(amf_size)plane->GetWidth() , (amf_size)plane->GetHeight(), (amf_size)1};
            compute->CopyPlane(plane, origin1,region1, surface->GetPlaneAt(p), origin1);


            plane = pColor2->GetPlaneAt(p);

            amf_size region2[3] = {(amf_size)plane->GetWidth(), (amf_size)plane->GetHeight(), (amf_size)1};
            amf_size origin2[3] = {(amf_size)xPos / ( p+1), (amf_size)yPos / ( p+1) ,     0};

            compute->CopyPlane(plane, origin1,region2, surface->GetPlaneAt(0), origin2);

        }
        xPos+=2; // NV12 surfaces do not accept odd positions - do not use ++
        yPos+=2; // NV12 surfaces do not accept odd positions - do not use ++

}

PollingThread::PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const wchar_t *pFileName) : m_pContext(context), m_pEncoder(encoder)
{
    std::wstring wStr(pFileName);
    std::string str(wStr.begin(), wStr.end());
    m_pFile.open(str, std::ios::binary);
}
PollingThread::~PollingThread()
{
    if(m_pFile)
    {
        m_pFile.close();
    }
}
void PollingThread::Run()
{
    RequestStop();

    amf_pts latency_time = 0;
    amf_pts write_duration = 0;
    amf_pts encode_duration = 0;
    amf_pts last_poll_time = 0;

    AMF_RESULT res = AMF_OK; // error checking can be added later
    while(true)
    {
        amf::AMFDataPtr data;
        res = m_pEncoder->QueryOutput(&data);
        if(res == AMF_EOF)
        {
            break; // Drain complete
        }
		if ((res != AMF_OK) && (res != AMF_REPEAT))
		{
			// trace possible error message
			break; // Drain complete
		}
        if(data != NULL)
        {
            amf_pts poll_time = amf_high_precision_clock();
            amf_pts start_time = 0;
            data->GetProperty(START_TIME_PROPERTY, &start_time);
            if(start_time < last_poll_time ) // remove wait time if submission was faster then encode
            {
                start_time = last_poll_time;
            }
            last_poll_time = poll_time;

            encode_duration += poll_time - start_time;

            if(latency_time == 0)
            {
                latency_time = poll_time - start_time;
            }

            amf::AMFBufferPtr buffer(data); // query for buffer interface
            m_pFile.write(reinterpret_cast<char*>(buffer->GetNative()), buffer->GetSize());

            write_duration += amf_high_precision_clock() - poll_time;
        }
        else
        {
            amf_sleep(1);
        }
    }
    printf("latency           = %.4fms\nencode  per frame = %.4fms\nwrite per frame   = %.4fms\n",
        double(latency_time)/MILLISEC_TIME,
        double(encode_duration )/MILLISEC_TIME/frameCount,
        double(write_duration)/MILLISEC_TIME/frameCount);

    m_pEncoder = NULL;
    m_pContext = NULL;
}
