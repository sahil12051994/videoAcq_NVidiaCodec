extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
}
#include <cuda.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cudaProfiler.h>
#include <stdio.h>
#include<future>	
#include <iostream>
#include<algorithm>
#include <thread>
#include <chrono>
#include <string.h>
#include <memory>
#include<string>
#include "NvDecoder/NvDecoder.h"
#include "../Utils/NvCodecUtils.h"
#include "../Utils/FFmpegDemuxer.h"
#include "../Utils/ColorSpace.h"
#define N 2
std::vector<FFmpegDemuxer*> vDemuxer2;

class demuxing{
public:
	FFmpegDemuxer *demuxer;
       uint8_t * pVideo=NULL;
        int nVideoBytes;
       
       void setvalue(std::string line){
	char a[255];
        strcpy(a,line.c_str());
	demuxer=new FFmpegDemuxer(a);	
	}
      void resetFuture()
	{
	}

};
demuxing demuxers[2];
simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

/*  enum is to select the desired format(In our case bgra is used) */
enum OutputFormat
{
    native = 0, bgrp, rgbp, bgra, rgba, bgra64, rgba64
};


int anSize[] = { 0, 3, 3, 4, 4, 8, 8 };

CUdeviceptr pTmpImage = 0;


OutputFormat eOutputFormat = bgra;

class Camera{
private:
	std::string name;
	std::string id;
	int Width;
	int Height;
	std::string timestamp;
public:
	std::string get_name(void){
		return name;
	}
	std::string get_id(){
		return id;
	}
	std::string get_timestamp(){
        	return timestamp;
	}
};

/* This function below is used to get the image from the memory*/
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

void DecProc(NvDecoder  *pDec)
{
       
     int nHeight=demuxers[0].demuxer->GetHeight();
       int nWidth=demuxers[0].demuxer->GetWidth();
      int nFrameSize=nWidth*nHeight*anSize[eOutputFormat];
       cuMemAlloc(&pTmpImage, nWidth * nHeight * anSize[eOutputFormat]);
    try
    {
          int nFrameReturned = 0, nFrame = 0;
          uint8_t  **ppFrame = NULL;
         uint8_t *pImage= new uint8_t[nFrameSize];  
           int frames=0;
      do{ 
             
	    std::vector<std::future<bool>> fut(1); 
		for(int i=0;i<1;i++)	
			fut[i] =std::async(&FFmpegDemuxer::Demux,&(*(demuxers[0].demuxer)),&(demuxers[0].pVideo),&(demuxers[0].nVideoBytes));        

	  for(auto &e:fut)
	     { e.get();            
	       	    int loop=0;               

	            pDec->Decode(demuxers[loop].pVideo, demuxers[loop].nVideoBytes, &ppFrame, &nFrameReturned);
	            if (!nFrame && nFrameReturned)      
	         	LOG(INFO) << pDec->GetVideoInfo();
		    for (int i = 0; i < nFrameReturned; i++)
	            {
	            	Nv12ToColor32<BGRA32>((uint8_t *)ppFrame[i], pDec->GetWidth(), (uint8_t*)pTmpImage, 4 * pDec->GetWidth(), pDec->GetWidth(), pDec->GetHeight());
	            	GetImage(pTmpImage, reinterpret_cast<uint8_t*>(pImage), 4 * pDec->GetWidth(), pDec->GetHeight());                
            	     if(loop==0){cv::Mat image(nHeight,nWidth , CV_8UC4, reinterpret_cast<char*>(pImage));
            		cv::imwrite("image"+std::to_string(frames)+".jpg",image);
                                  frames++;
            		}
		  }
		loop++;
	            nFrame += nFrameReturned;
	    }
	}while(1);
	
	}
    catch (std::exception& ex)
    {
        std::cout<<ex.what();
    }
}
struct NvDecPerfData
{
    uint8_t *pBuf;
    std::vector<uint8_t *> *pvpPacketData; 
    std::vector<int> *pvpPacketDataSize;
};
int CUDAAPI HandleVideoData(void *pUserData, CUVIDSOURCEDATAPACKET *pPacket) {
    NvDecPerfData *p = (NvDecPerfData *)pUserData;
    memcpy(p->pBuf, pPacket->payload, pPacket->payload_size);
    p->pvpPacketData->push_back(p->pBuf);
    p->pvpPacketDataSize->push_back(pPacket->payload_size);
    p->pBuf += pPacket->payload_size;
    return 1;
}


int main(int argc, char **argv)
{
    char szInFilePath[255];
    int iGpu = 0;
    int nThread = 1; 
    bool bSingle = false;
    bool bHost = false;
    std::vector<std::exception_ptr> vExceptionPtrs;

    try
    {
        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        if (iGpu < 0 || iGpu >= nGpu) {
            std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
            return 1;
        }
        CUdevice cuDevice = 0;
        ck(cuDeviceGet(&cuDevice, iGpu));
        char szDeviceName[80];
        ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
        std::cout << "GPU in use: " << szDeviceName << std::endl;
	CUcontext cuContext = NULL;
        ck(cuCtxCreate(&cuContext, 0, cuDevice));
        std::ifstream file(argv[1]);
	std::string line;
        int i=2;
       while(std::getline(file,line)){

	 	demuxers[0].setvalue(line);
		 
    /*
                if(demuxer.GetHeight()==720 && demuxer.GetHeight()==1280 && demuxer.GetVideoCodec()==AV_CODEC_ID_H)
                           vDemuxer1.push_back(demuxer);
		else if(demuxer.GetHeight()==720 && demuxer.GetHeight()==1280 && demuxer.GetVideoCodec()==AV_CODEC_ID_HEVC)
			 vDemuxer3.push_back(demuxer);
		else if(demuxer.GetHeight()==1080 && demuxer.GetHeight()==1920 && demuxer.GetVideoCodec()==AV_CODEC_ID_H264)
                 	 vDemuxer2.push_back(demuxer);
		else if(demuxer.GetHeight()==1080 && demuxer.GetHeight()==1920 && demuxer.GetVideoCodec()==AV_CODEC_ID_HEVC)
			 vDemuxer4.push_back(demuxer);
		else 
			 vDemuxer5.push_back(demuxer);            */        
	}
  //     NvDecoder  dec1(new NvDecoder(cuContext, 1280, 720, true, FFmpeg2NvCodecId(AV_CODEC_ID_H264)));
       std::unique_ptr<NvDecoder> dec2(new NvDecoder(cuContext, 1920, 1080, true, (cudaVideoCodec)cudaVideoCodec_H264));
//        NvDecoder  dec3(new NvDecoder(cuContext, 1280, 720, true, FFmpeg2NvCodecId(AV_CODEC_ID_HEVC)));
//        NvDecoder  dec4(new NvDecoder(cuContext, 1920, 1080, true, FFmpeg2NvCodecId(AV_CODEC_ID_HEVC)));

       std::vector<NvThread> gpuThread;

//       gpuThread.push_back(NvThread(std::thread(DecProc, dec1, &vDemuxer1) ));
       DecProc( dec2.get());
//       gpuThread.push_back(NvThread(std::thread(DecProc, dec3, &vDemuxer3) ));
//       gpuThread.push_back(NvThread(std::thread(DecProc, dec4, &vDemuxer4) ));
//            
 //       gpuThread1.join();
//	gpuThread[0].join();
//	gpuThread3.join();
//	gpuThread4.join();
        
    }
    catch (const std::exception& ex)
    {
        std::cout << ex.what();
        exit(1);
    }
    return 0;
}
