extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}
#include"kafka.h"
#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
#include<iostream>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include<thread>
#include<vector>
#include<stdlib.h>
#include<fstream>
#include<stdio.h>
#include<sstream>
#include<string>
#include<sys/time.h>
#include<ctime>


/*===================================================================================================================================================================================
                    |___STRUCTURE CONFIG: CONTAINS ,KAFKA PATH, CAMERA LINK,TOPIC NAME.FPS___|
===================================================================================================================================================================================*/

struct Config {
	std::string path;
	std::string readFromFile    ;
	std::string topicName;
	double fps;
};

/*===================================================================================================================================================================================
                    |___STRUCTURE PACKETDATA:CONTAINS CAMERA ID(INTEGER),WIDTH,HEIGHT,PIXEL FORMAT AND DATA OF THE DECODED FRAME___|
===================================================================================================================================================================================*/

struct packetData
{
	int id;
	AVFrame * tempFrame=av_frame_alloc();
	int width;
	int height;
	AVPixelFormat pxl_fmt;
	unsigned long timeStmp;
	packetData(AVFrame *frame,int h,int w, AVPixelFormat p,int d,unsigned long time)
	{
		id=d;
		av_frame_move_ref(tempFrame,frame);
		width=w;
		height=h;
		pxl_fmt=p;
		timeStmp=time;
	}
	packetData()
	{
	}
};

/*===================================================================================================================================================================================
                    |___DEMUXPROC ACCEPT CAMERA ID WITH CAMERA LINK,DECODE EACH FRAME AND THEN INSERT THE DECODED FRAME INTO QUEUE AT A FIXED RATE___|
===================================================================================================================================================================================*/
Config configuration;
std::string cameraLinks[200];
boost::lockfree::queue<packetData> queue(4000);


void ComsumeData()
{	int ll=0;	
	packetData pt;
	while (1) {
		while(queue.pop(pt))
		{	
    			AVFrame *pFrameRGB;
			pFrameRGB = av_frame_alloc();
		   	uint8_t *buffer;
   			int numBytes; 
 			AVPixelFormat  pFormat = AV_PIX_FMT_BGR24;
   			numBytes = avpicture_get_size(pFormat,pt.width,pt.height) ; 
    			buffer = (uint8_t *) av_malloc(numBytes*sizeof(uint8_t));
    			avpicture_fill((AVPicture *) pFrameRGB,buffer,pFormat,pt.width,pt.height);	
			struct SwsContext * img_convert_ctx;
        	        img_convert_ctx = sws_getCachedContext(NULL,pt.width, pt.height, AV_PIX_FMT_YUV420P,   pt.width, pt.height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL,NULL);
        	        sws_scale(img_convert_ctx, ((AVPicture*)(pt.tempFrame))->data, ((AVPicture*)pt.tempFrame)->linesize, 0, pt.height, ((AVPicture *)pFrameRGB)->data, ((AVPicture *)pFrameRGB)->linesize); 
        	        	cv::Mat M(pt.tempFrame->height,pt.tempFrame->width,CV_8UC3,pFrameRGB->data[0]); 
			production(M,cameraLinks[pt.id],pt.id,pt.timeStmp);
			ll++;
			delete buffer;
 		        sws_freeContext(img_convert_ctx); 
 			av_free(pFrameRGB);
 			av_free(pt.tempFrame);
 	       
 		}

    	}
}

/*===================================================================================================================================================================================
                    |___DEMUXPROC ACCEPT CAMERA ID WITH CAMERA LINK,DECODE EACH FRAME AND THEN INSERT THE DECODED FRAME INTO QUEUE AT A FIXED RATE___|
===================================================================================================================================================================================*/

int DemuxProc(int k,std::string s)
{
	unsigned long initialTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();//time at which function starts
	const char  *filenameSrc =s.c_str();;
	AVCodecContext  *pCodecCtx;
	AVFormatContext *pFormatCtx = avformat_alloc_context();
	AVDictionary* options = NULL;
	av_dict_set(&options, "rtsp_transport", "tcp", 0);
	AVCodec * pCodec;
	AVFrame *pFrame;
	if(avformat_open_input(&pFormatCtx,filenameSrc,NULL,&options) != 0) return -12;
	if(avformat_find_stream_info(pFormatCtx,NULL) < 0)   return -13;
	av_dump_format(pFormatCtx, 0, filenameSrc, 0);
	int videoStream = 1;
	for(int i=0; i < pFormatCtx->nb_streams; i++)
	{
		if(pFormatCtx->streams[i]->codec->coder_type==AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			break;
		}
	}
	if(videoStream == -1) return -14;
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;
	pCodec =avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) return -15; //codec not found
	if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0) return -16;
	pFrame    = av_frame_alloc(); 
	int res;
	int frameFinished;
	AVPacket packet;
	while(res = av_read_frame(pFormatCtx,&packet)>=0)
	{
		if(packet.stream_index == videoStream)
		{
			avcodec_decode_video2(pCodecCtx,pFrame,&frameFinished,&packet);//decoding step
			if(frameFinished)
			{
				unsigned long finalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();//current frame timestamp
				if((finalTime-initialTime)>configuration.fps*1000)//SEND THE DATA  ONCE AFTER EACH value(FPS)seconds
				{	
					queue.push(packetData(pFrame,pCodecCtx->height,pCodecCtx->width,pCodecCtx->pix_fmt,k,finalTime));   //pushing into queue 
					initialTime=finalTime;//set the initial time to finaltime so new frame will be enqueued after value(FPS) seconds
				}
				av_free_packet(&packet);
			}
		}
	}
	av_free_packet(&packet);
	avcodec_close(pCodecCtx);
	av_free(pFrame);
	avformat_close_input(&pFormatCtx);
}
/*===================================================================================================================================================================================
                    |___FUNCTION TO READ CONFIGURATION FILE(config.txt).THE CONFIGURATION HAS FOUR VALUES (KAFKA PATH(/home/jbm/kafka),FILE NAME THAT CONTAINS LIST OF CAMERAS,TOPICNAME,FPS)___|
===================================================================================================================================================================================*/

void loadConfig(Config& config,std::string configFilePath) {
	std::ifstream fin(configFilePath);
	std::string line;
	while (getline(fin, line)) 
	{
		std::istringstream sin(line.substr(line.find("=") + 1));
		if (line.find("File") != -1)
			sin >> config.readFromFile;
		else if (line.find("PATH_KAFKA") != -1)
			sin >> config.path;
		else if (line.find("Topic") != -1)
			sin >> config.topicName;
		else if (line.find("FPS") != -1)   
			sin >> config.fps;
	}
}
/*===================================================================================================================================================================================
                  |___COMMANDRUNNER: TAKE THE COMMAND(STRING) AS INPUT AND GIVE THE OUTPUT OF COMMAND AS STRING___|
===================================================================================================================================================================================*/
std::string CommandRunner(std::string cmd) {

	std::string data;
	FILE * stream;
	const int max_buffer = 256;
	char buffer[max_buffer];
	cmd.append(" 2>&1");
	stream = popen(cmd.c_str(), "r");
	if (stream) {
	while (!feof(stream))
	if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
	pclose(stream);
	}
	return data;
}

/*===================================================================================================================================================================================
                    |___MAIN FUNCTION: TAKE config.txt as ARGUMENT , MAKE THE TOPIC IF NOT PRESENT AND CHANGE THE NUMBER OF PARTITIONS AS PER NUMBER OF CAMERA LINKS,___|
                    |___SPAWN THREADS FOR EACH CAMERA FOR DECODING AND DEMUXING, AND MAKE A THREAD FOR SENDING THE FRAME TO KAFKA_______________________________| 
===================================================================================================================================================================================*/

int main(int argc, char *argv[])
{
     	
	av_register_all();
	avdevice_register_all();
	avcodec_register_all();
	avformat_network_init();
	int filenumber=0;
	std::string line;
	std::vector<std::thread> th;
	loadConfig(configuration,argv[1]);//populating the configuration variable using the file data
	topic_str=configuration.topicName;
	int numOfLines= std::stoi(CommandRunner("wc -l "+configuration.readFromFile+ " | awk '{print $1}' "));//number of lines in the links file
	std::string topicCreationChecker=CommandRunner(configuration.path+"/bin/./kafka-topics.sh --list --zookeeper localhost:2181| grep ^"+configuration.topicName+"$");//checking the topic present or not
	if(std::strlen(topicCreationChecker.c_str())>0)
	{       
		int partitionNow=std::stoi(CommandRunner(configuration.path+"/bin/./kafka-topics.sh --describe --zookeeper localhost:2181 --topic "+configuration.topicName+"|grep Count | awk '{print $2}'|sed 's/PartitionCount://'"));//getting partition count
		if(numOfLines<partitionNow)
			CommandRunner(configuration.path+"/bin/./kafka-topics.sh --alter --zookeeper localhost:2181 "+"--partitions "+std::to_string(numOfLines)+"--topic "+configuration.topicName);//changing partition count
		std::cout<<numOfLines;
	}
	else
	{
		std::cout<<CommandRunner(configuration.path+"/bin/./kafka-topics.sh --create --zookeeper localhost:2181 --replication-factor 1 --partitions " +std::to_string(numOfLines)+ " --topic "+configuration.topicName);//making topic
	}
	std::ifstream file(configuration.readFromFile);//reading file containing camera links
	while(std::getline(file,line))
	{
		th.push_back(std::thread(DemuxProc,filenumber,line));//thread for decoding
		cameraLinks[filenumber]=line;
		++filenumber;
	}
	init(filenumber);
	th.push_back(std::thread(ComsumeData));//thread for pushing data into kafka
	for(int i =0;i<filenumber+1;i++)
	{
		th[i].join();
	}
}
