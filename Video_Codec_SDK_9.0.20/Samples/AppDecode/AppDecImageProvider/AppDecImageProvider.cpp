/*
* Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/
#include <opencv2/core/core.hpp>
#include <cuda.h>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <algorithm>
#include <memory>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../Utils/ColorSpace.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();
enum OutputFormat
{
    native = 0, bgrp, rgbp, bgra, rgba, bgra64, rgba64
};

void GetImage(CUdeviceptr dpSrc, uint8_t *pDst, int nWidth, int nHeight)
{
    CUDA_MEMCPY2D m = { 0 };
    m.WidthInBytes = nWidth;
    m.Height = nHeight;
    m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    m.srcDevice = (CUdeviceptr)dpSrc;
    m.srcPitch = m.WidthInBytes;
    m.dstMemoryType = CU_MEMORYTYPE_HOST;
    m.dstDevice = (CUdeviceptr)(m.dstHost = pDst);
    m.dstPitch = m.WidthInBytes;
    cuMemcpy2D(&m);
}
/**
*  This sample application illustrates the decoding of a media file in a desired color format.
*  The application supports native (nv12 or p016), bgra, bgrp and bgra64 output formats.
*/

int main(int argc, char **argv)
{
    char szInFilePath[256] = "rtsp://admin:password%40123@192.1.10.80/Streaming/Channels/101";
    OutputFormat eOutputFormat = bgra;
    int iGpu = 0;
    bool bReturn = 1;
    CUdeviceptr pTmpImage = 0;

    try
    {
        //ParseCommandLine(argc, argv, szInFilePath, eOutputFormat, szOutFilePath, iGpu);
        //CheckInputFile(szInFilePath);

        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu)
        {
            std::ostringstream err;
            err << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]";
            throw std::invalid_argument(err.str());
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        LOG(INFO) << "GPU in use: " << szDeviceName;
        CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));

        FFmpegDemuxer demuxer(szInFilePath);
        NvDecoder dec(cuContext, demuxer.GetWidth(), demuxer.GetHeight(), true, FFmpeg2NvCodecId(demuxer.GetVideoCodec()));
        int nWidth = demuxer.GetWidth(), nHeight = demuxer.GetHeight();
        int anSize[] = { 0, 3, 3, 4, 4, 8, 8 };
        int nFrameSize = eOutputFormat == native ? demuxer.GetFrameSize() : nWidth * nHeight * anSize[eOutputFormat];
        std::unique_ptr<uint8_t[]> pImage(new uint8_t[nFrameSize]);

        int nVideoBytes = 0, nFrameReturned = 0, nFrame = 0;
        uint8_t *pVideo = NULL;
        uint8_t **ppFrame;

        cuMemAlloc(&pTmpImage, nWidth * nHeight * anSize[eOutputFormat]);
       int ff=0;
        do {
            demuxer.Demux(&pVideo, &nVideoBytes);
            dec.Decode(pVideo, nVideoBytes, &ppFrame, &nFrameReturned);
            if (!nFrame && nFrameReturned)
                LOG(INFO) << dec.GetVideoInfo();
              std::cout<<"\n"<<"Nframe"<<nFrameReturned<<"\n";
            for (int i = 0; i < nFrameReturned; i++)
            {
                            Nv12ToColor32<BGRA32>((uint8_t *)ppFrame[i], dec.GetWidth(), (uint8_t*)pTmpImage, 4 * dec.GetWidth(), dec.GetWidth(), dec.GetHeight());
                        GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage.get()), 4 * dec.GetWidth(), dec.GetHeight());
                
                cv::Mat image(1080,1920 , CV_8UC4, reinterpret_cast<char*>(pImage.get()));
               cv::imwrite("image"+std::to_string(ff)+".jpg",image);
               cv::waitKey(30); 

            }
            ff++;
            nFrame += nFrameReturned;
        } while (nVideoBytes);

        if (pTmpImage) {
            cuMemFree(pTmpImage);
        }
   }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
