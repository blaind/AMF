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

// This sample analyses NV12 frames using AMF Pre Analysis and writes the importance map to a file.
// In actual use the importance map can aid an encoder of some type.

#include <stdio.h>
#ifdef _WIN32
#include <tchar.h>
#include <d3d9.h>
#include <d3d11.h>
#endif
#include "public/common/AMFFactory.h"
#include "public/include/components/PreAnalysis.h"
#include "public/common/Thread.h"
#include "public/common/AMFSTL.h"
#include "public/common/TraceAdapter.h"
#include <fstream>

#define AMF_FACILITY L"AMFSamplePA"

static const wchar_t *pCodec = AMFPreAnalysis;

 //#define ENABLE_4K

#ifdef _WIN32
static amf::AMF_MEMORY_TYPE memoryTypeIn = amf::AMF_MEMORY_DX11;
#else
static amf::AMF_MEMORY_TYPE memoryTypeIn = amf::AMF_MEMORY_VULKAN;
#endif
static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_NV12;

#if defined(ENABLE_4K)
static amf_int32 widthIn = 1920 * 2;
static amf_int32 heightIn = 1080 * 2;
#else
static amf_int32 widthIn = 1920;
static amf_int32 heightIn = 1080;
#endif
static amf_int32 frameRateIn = 30;
static amf_int64 bitRateIn = 5000000L; // in bits, 5MBit
static amf_int32 rectSize = 50;
static amf_int32 frameCount = 500;
static bool bMaximumSpeed = true;

#define START_TIME_PROPERTY L"StartTimeProperty" // custom property ID to store submission time in a frame - all custom properties are copied from input to output

static const wchar_t *fileNameOut = L"./output.immp";

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
	amf::AMFComponentPtr    m_pPreAnalysis;
	std::ofstream           m_pFile;
public:
	PollingThread(amf::AMFContext *context, amf::AMFComponent *preAnalysis, const wchar_t *pFileName);
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
	if (res != AMF_OK)
	{
		wprintf(L"AMF Failed to initialize");
		return 1;
	}

	::amf_increase_timer_precision();

	amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_CONSOLE, true);
	amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_DEBUG_OUTPUT, true);

	// initialize AMF
	amf::AMFContextPtr context;
	amf::AMFComponentPtr pre_analysis;
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

	// Load the PreAnalysis component
	res = g_AMFFactory.GetFactory()->CreateComponent(context, AMFPreAnalysis, &pre_analysis);
	AMF_RETURN_IF_FAILED(res, L"Create PreAnalysis failed");

	// Enable Scene change detection and set sensitivity to medium
    res = pre_analysis->SetProperty(AMF_PA_SCENE_CHANGE_DETECTION_ENABLE, true);
    AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_PA_SCENE_CHANGE_DETECTION_ENABLE, true) failed");
    res = pre_analysis->SetProperty(AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY, AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_MEDIUM);
    AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY, %d) failed", AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_MEDIUM);

	res = pre_analysis->Init(formatIn, widthIn, heightIn);
	AMF_RETURN_IF_FAILED(res, L"pre_analysis->Init() failed");


	PollingThread thread(context, pre_analysis, fileNameOut);
	thread.Start();

	// Analyze some frames
	amf_int32 submitted = 0;
	while (submitted < frameCount)
	{
		if (surfaceIn == NULL)
		{
			res = context->AllocSurface(memoryTypeIn, formatIn, widthIn, heightIn, &surfaceIn);
			AMF_RETURN_IF_FAILED(res, L"AllocSurface() failed");

			if (memoryTypeIn == amf::AMF_MEMORY_VULKAN)
			{
				FillSurfaceVulkan(context, surfaceIn);
			}
#ifdef _WIN32
			else if (memoryTypeIn == amf::AMF_MEMORY_DX9)
			{
				FillSurfaceDX9(context, surfaceIn);
			}
			else
			{
				FillSurfaceDX11(context, surfaceIn);
			}
#endif
		}
		// Analyze
		amf_pts start_time = amf_high_precision_clock();
		surfaceIn->SetProperty(START_TIME_PROPERTY, start_time);

		res = pre_analysis->SubmitInput(surfaceIn);
		if (res == AMF_NEED_MORE_INPUT) // handle full queue
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
	// drain Pre Analysis; input queue can be full
	while (true)
	{
		res = pre_analysis->Drain();
		if (res != AMF_INPUT_FULL) // handle full queue
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
	pre_analysis->Terminate();
	pre_analysis = NULL;
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
    D3DCOLOR color1 = D3DCOLOR_XYUV(0, 255, 128);
    D3DCOLOR color2 = D3DCOLOR_XYUV(128, 0, 128);
	// get native DX objects
	IDirect3DDevice9 *deviceDX9 = (IDirect3DDevice9 *)context->GetDX9Device(); // no reference counting - do not Release()
	IDirect3DSurface9* surfaceDX9 = (IDirect3DSurface9*)surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()
	hr = deviceDX9->ColorFill(surfaceDX9, NULL, color1);

	if (xPos + rectSize > widthIn)
	{
		xPos = 0;
	}
	if (yPos + rectSize > heightIn)
	{
		yPos = 0;
	}
	RECT rect = { xPos, yPos, xPos + rectSize, yPos + rectSize };
	hr = deviceDX9->ColorFill(surfaceDX9, &rect, color2);

	xPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
	yPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
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

	if (xPos + rectSize > widthIn)
	{
		xPos = 0;
	}
	if (yPos + rectSize > heightIn)
	{
		yPos = 0;
	}
	D3D11_BOX rect = { 0, 0, 0, (UINT)rectSize, (UINT)rectSize, 1 };

	ID3D11Texture2D* surfaceDX11Color2 = (ID3D11Texture2D*)pColor2->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()

	deviceContextDX11->CopySubresourceRegion(surfaceDX11, 0, xPos, yPos, 0, surfaceDX11Color2, 0, &rect);
	deviceContextDX11->Flush();

	xPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
	yPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++

	deviceContextDX11->Release();
}
#endif

static void FillNV12SurfaceWithColor(amf::AMFSurface* surface, amf_uint8 Y, amf_uint8 U, amf_uint8 V)
{
	amf::AMFPlane *pPlaneY = surface->GetPlaneAt(0);
	amf::AMFPlane *pPlaneUV = surface->GetPlaneAt(1);

	amf_int32 widthY = pPlaneY->GetWidth();
	amf_int32 heightY = pPlaneY->GetHeight();
	amf_int32 lineY = pPlaneY->GetHPitch();

	amf_uint8 *pDataY = (amf_uint8 *)pPlaneY->GetNative();

	for (amf_int32 y = 0; y < heightY; y++)
	{
		amf_uint8 *pDataLine = pDataY + y * lineY;
		memset(pDataLine, Y, widthY);
	}

	amf_int32 widthUV = pPlaneUV->GetWidth();
	amf_int32 heightUV = pPlaneUV->GetHeight();
	amf_int32 lineUV = pPlaneUV->GetHPitch();

	amf_uint8 *pDataUV = (amf_uint8 *)pPlaneUV->GetNative();

	for (amf_int32 y = 0; y < heightUV; y++)
	{
		amf_uint8 *pDataLine = pDataUV + y * lineUV;
		for (amf_int32 x = 0; x < widthUV; x++)
		{
			*pDataLine++ = U;
			*pDataLine++ = V;
		}
	}

}
static void PrepareFillFromHost(amf::AMFContext *context)
{
	AMF_RESULT res = AMF_OK; // error checking can be added later
	res = context->AllocSurface(amf::AMF_MEMORY_HOST, formatIn, widthIn, heightIn, &pColor1);
	res = context->AllocSurface(amf::AMF_MEMORY_HOST, formatIn, rectSize, rectSize, &pColor2);

	FillNV12SurfaceWithColor(pColor2, 128, 0, 128);
	FillNV12SurfaceWithColor(pColor1, 0, 255, 128);

	pColor1->Convert(memoryTypeIn);
	pColor2->Convert(memoryTypeIn);
}

static void FillSurfaceVulkan(amf::AMFContext *context, amf::AMFSurface *surface)
{
	amf::AMFComputePtr compute;
	context->GetCompute(amf::AMF_MEMORY_VULKAN, &compute);

	if (xPos + rectSize > widthIn)
	{
		xPos = 0;
	}
	if (yPos + rectSize > heightIn)
	{
		yPos = 0;
	}

	for (int p = 0; p < 2; p++)
	{
		amf::AMFPlane *plane = pColor1->GetPlaneAt(p);
		amf_size origin1[3] = { 0, 0 , 0 };
		amf_size region1[3] = { (amf_size)plane->GetWidth() , (amf_size)plane->GetHeight(), (amf_size)1 };
		compute->CopyPlane(plane, origin1, region1, surface->GetPlaneAt(p), origin1);


		plane = pColor2->GetPlaneAt(p);

		amf_size region2[3] = { (amf_size)plane->GetWidth(), (amf_size)plane->GetHeight(), (amf_size)1 };
		amf_size origin2[3] = { (amf_size)xPos / (p + 1), (amf_size)yPos / (p + 1) ,     0 };

		compute->CopyPlane(plane, origin1, region2, surface->GetPlaneAt(0), origin2);

	}
	xPos += 2; // NV12 surfaces do not accept odd positions - do not use ++
	yPos += 2; // NV12 surfaces do not accept odd positions - do not use ++

}

PollingThread::PollingThread(amf::AMFContext *context, amf::AMFComponent *pre_analysis, const wchar_t *pFileName) : m_pContext(context), m_pPreAnalysis(pre_analysis)
{
	std::wstring wStr(pFileName);
	std::string str(wStr.begin(), wStr.end());
	m_pFile.open(str, std::ios::binary);
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

	amf_pts latency_time = 0;
	amf_pts write_duration = 0;
	amf_pts pa_duration = 0;
	amf_pts last_poll_time = 0;

	AMF_RESULT res = AMF_OK; // error checking can be added later
	while (true)
	{
		amf::AMFDataPtr frame;
		res = m_pPreAnalysis->QueryOutput(&frame);

		if (res == AMF_EOF)
		{
			break; // Drain complete
		}
		if ((res != AMF_OK) && (res != AMF_REPEAT))
		{
			// trace possible error message
			break; // Drain complete
		}
		if (frame != NULL)
		{
			amf_pts poll_time = amf_high_precision_clock();
			amf_pts start_time = 0;
			frame->GetProperty(START_TIME_PROPERTY, &start_time);
			if (start_time < last_poll_time) // remove wait time if submission was faster then PA
			{
				start_time = last_poll_time;
			}
			last_poll_time = poll_time;

			pa_duration += poll_time - start_time;

			if (latency_time == 0)
			{
				latency_time = poll_time - start_time;
			}

			amf::AMFVariant  activityMapVar;
			res = frame->GetProperty(AMF_PA_ACTIVITY_MAP, &activityMapVar);

			// Get Activity Map out of data
			amf::AMFSurfacePtr spSurface(activityMapVar.ToInterface());
			amf::AMFPlane*     pPlane  = spSurface->GetPlaneAt(0);
			const amf_int32    xBlocks = pPlane->GetWidth();
			const amf_int32    yBlocks = pPlane->GetHeight();
			const amf_uint32*  pData   = static_cast<amf_uint32*>(pPlane->GetNative());
            const amf_int32    hPitch  = pPlane->GetHPitch() / pPlane->GetPixelSizeInBytes();
            const amf_int32    hWidth  = xBlocks * sizeof(amf_uint32);

			for (amf_int32 y = 0; y < yBlocks; y++)
			{
				m_pFile.write(reinterpret_cast<const char*>(pData), hWidth);
                pData += hPitch;
			}

			write_duration += amf_high_precision_clock() - poll_time;
		}
		else
		{
			amf_sleep(1);
		}
	}
	printf("latency           = %.4fms\nPA  per frame = %.4fms\nwrite per frame   = %.4fms\n",
		double(latency_time) / MILLISEC_TIME,
		double(pa_duration) / MILLISEC_TIME / frameCount,
		double(write_duration) / MILLISEC_TIME / frameCount);

	m_pPreAnalysis = NULL;
	m_pContext = NULL;
}
