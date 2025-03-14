// Notice Regarding Standards.  AMD does not provide a license or sublicense to
// any Intellectual Property Rights relating to any standards, including but not
// limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
// AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
// (collectively, the "Media Technologies"). For clarity, you will pay any
// royalties due for such third party technologies, which may include the Media
// Technologies that are owed as a result of AMD providing the Software to you.
//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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

New features are available in 
Driver: Radeon Software Adrenalin 2019 Edition; 19.7.1 Software: 19.30 and later 

1.4.26.0 (26.07.2022) version
--------------------------
   - New perceptual (PAQ) and temporal (TAQ) adaptive quantization modes for PreAnalysis
   - New high motion quality boost mode for PreAnalysis
   - New sharpness option for HQScaler
   - New low latency decoding option
   - New temporal SVC encoding options

1.4.24.0 (17.03.2022) version
--------------------------
   - New AMD Direct Capture component and updated DVR sample app
   - New HQScaler ( Bilinear / Bicubic / FSR )
   - Vulkan support for PreAnalysis component
   - Vulkan support for HEVC Encoder ( Navi+ )
   - Support for temporal layer query for AVC and HEVC Encoder
   - Support for consecutive B-frames and adaptive miniGOP for AVC Encoder
   - Improved H264 Encoding on Vulkan ( both Windows & Linux Navi+ )

1.4.23.0 (02.12.2021) version
--------------------------
   - New Auto LTR encoder mode
   - FFmpeg updated to version 4.3.2
   - Additional statistics and feedback in encoder
   - New encoder usage presets
   - Main 10 HEVC encoder profile support
   - Support for encoder instance selection

1.4.21.0 (05.07.2021) version
--------------------------
   - HDR color conversion updated
   - HDR color conversion in encoder
   - QVBR rate control mode in encoder 
   - Extended statistic and quality metrics in encoder
   - Frame rate parameter became dynamic
   - New LTR encoder mode
   - Optional VUI parameter "matrix_coefficients" now set in encoder

1.4.18.0 (16.09.2020) version
--------------------------
   - AV1 decoding
   - VP9 decoding
   - DX12 decoding and color conversion
   - Pre-analysis component  (PA)
   - Pre-processing filter 
   - Encoder: output timeout support, throughput information, picture management , statistic

1.4.14.0 (08.07.2019) version
--------------------------
   - Support for RX5700 (Navi) GPUs
	- FFmpeg updated to version 4.1.3
	- Added sample projects for Visual Studio 2019
	- Dropped support for Visual Studio 2013 and 1015

1.4.9.0 (23.08.2018) version
--------------------------
    - Vulkan support
    - Vulkan encoder supports H.264 only
    - Linux support (see public/make/common_defs.mak for packages and build parameters)

1.4.7.0 (23.04.2018) version
--------------------------
    - 360 stich sample

1.4.6.0 (11.30.2017) version
--------------------------
    - Game DVR sample

1.4.5.0 (07.10.2017) version
--------------------------
    - Internal version

1.4.4.0 (06.07.2017) version
--------------------------
    - FFmpeg updated to v3.3.1
    - added sample projects for VS2015 and VS2017


1.4.2.0 (11.04.2017) version
--------------------------
    - AMF "C" interfaces
    - "SimpleEncoderC" sample
    - Improvements in sample playback pipelines: Seek, Pause, AV sync

1.4.0.0 (07.12.2016) version
--------------------------
    - Update of AMF SDK for RX 480, 470, 460
    - AVC new Preanalysys parameter added
    - Full range for color space converter and AVC encoder added
    - HEVC encoder component added




Driver: Radeon Software Edition Crimson; 16.7.3 Software: 16.30 and later 

1.3.0.5 (19.07.2016) version
--------------------------
    - Initial version of AMF SDK
