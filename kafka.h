#include <opencv2/core/core.hpp>
#include<pthread.h>
#include <opencv2/highgui/highgui.hpp>
#include<iostream>
#include<fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include<time.h>
#include<sys/time.h>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include<chrono>
#include <cstring>
#include<vector>

#ifdef _MSC_VER
#include "../win32/wingetopt.h"
#elif _AIX
#include <unistd.h>
#else
#include <getopt.h>
#endif
#define byte uint8_t
#include "rdkafkacpp.h"


#define NUMBER 200

#include <string>
class CameraData
{
public:
   RdKafka::Producer *producer;

};

CameraData Cam[NUMBER];


std::string brokers = "localhost";
std::string errstr;
std::string topic_str;

std::string mode;
std::string debug;
//checkpoint

#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A



std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

#endif

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";


static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}
//*****************************************Encoding string*********************************************************
std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];
	
  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = ( char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';

  }

  return ret;

}


//***************************************************Decoding string******************************************************

std::string base64_decode(std::string const& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = ( char_array_4[0] << 2       ) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) +   char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = 0; j < i; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}

//*********************************************************************************************************************************

RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);


static bool run = true;
static bool exit_eof = false;

static void sigterm (int sig) {
    run = false;
}

//*********************************************************************************************************************************

class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb (RdKafka::Message &message) {
        std::string status_name;
        switch (message.status())
        {
            case RdKafka::Message::MSG_STATUS_NOT_PERSISTED:
                status_name = "NotPersisted";
                break;
            case RdKafka::Message::MSG_STATUS_POSSIBLY_PERSISTED:
                status_name = "PossiblyPersisted";
                break;
            case RdKafka::Message::MSG_STATUS_PERSISTED:
                status_name = "Persisted";
                break;
            default:
                status_name = "Unknown?";
                break;
        }
        std::cout << "Message delivery for (" << message.len() << " bytes): " <<
                  status_name << ": " << message.errstr() << std::endl;
        if (message.key())
            std::cout << "Key: " << *(message.key()) << ";" << std::endl;
    }
};

//*********************************************************************************************************************************

class ExampleEventCb : public RdKafka::EventCb {
public:
    void event_cb (RdKafka::Event &event) {
        switch (event.type())
        {
            case RdKafka::Event::EVENT_ERROR:
                if (event.fatal()) {
                    std::cerr << "FATAL ";
                    run = false;
                }
                std::cerr << "ERROR (" << RdKafka::err2str(event.err()) << "): " <<
                          event.str() << std::endl;
                break;

            case RdKafka::Event::EVENT_STATS:
                std::cerr << "\"STATS\": " << event.str() << std::endl;
                break;

            case RdKafka::Event::EVENT_LOG:
                fprintf(stderr, "LOG-%i-%s: %s\n",
                        event.severity(), event.fac().c_str(), event.str().c_str());
                break;

            default:
                std::cerr << "EVENT " << event.type() <<
                          " (" << RdKafka::err2str(event.err()) << "): " <<
                          event.str() << std::endl;
                break;
        }
    }
};

ExampleEventCb ex_event_cb;
ExampleDeliveryReportCb ex_dr_cb;



class MyHashPartitionerCb : public RdKafka::PartitionerCb {
public:
    int32_t partitioner_cb (const RdKafka::Topic *topic, const std::string *key,
                            int32_t partition_cnt, void *msg_opaque) {
        return djb_hash(key->c_str(), key->size()) % partition_cnt;
    }
private:
    static inline unsigned int djb_hash (const char *str, size_t len) {
        unsigned int hash = 5381;
        for (size_t i = 0 ; i < len ; i++)
            hash = ((hash << 5) + hash) + str[i];
        return hash;
    }
};

/**********************************************************************************************************************************
Initialising values
//********************************************************************************************************************************/

void init(int k)
{
	conf->set("metadata.broker.list", brokers, errstr);
	if (!debug.empty()) 
	{
		if (conf->set("debug", debug, errstr) != RdKafka::Conf::CONF_OK) 
		{
			std::cerr << errstr << std::endl;
			exit(1);
		}
	}
	if (!debug.empty()) 
	{
		if (conf->set("debug", debug, errstr) != RdKafka::Conf::CONF_OK) 
		{
			std::cerr << errstr << std::endl;
			exit(1);
		}
 	}
	conf->set("event_cb", &ex_event_cb, errstr);
	signal(SIGINT, sigterm);
	signal(SIGTERM, sigterm);
	conf->set("dr_cb", &ex_dr_cb, errstr);
	conf->set("message.max.bytes", std::to_string(20000000), errstr);
	conf->set("message.copy.max.bytes", std::to_string(20000000), errstr);
	for(int i=0;i<k;i++)
		Cam[i].producer = RdKafka::Producer::create(conf, errstr);   
}
/*****************************************************************************************************************************************************************************
This function accepts the matrix , camera link, camera number(this number will be used for choosing the producer),topic name and time stamp.
The function then sends the data to desired topic and partition
*****************************************************************************************************************************************************************************/
int production(cv::Mat passedMat,std::string cameraLink,int partitionNumber,unsigned long timeStmp)
{    
	std::vector<uchar> data_encode;
	cv::imencode(".jpg", passedMat, data_encode);
	const int ne = snprintf(NULL, 0, "%lu", timeStmp);
	assert(ne > 0);
	char buf[ne+1];
	int c = snprintf(buf, ne+1, "%lu", timeStmp);
	assert(buf[ne] == '\0');
	assert(c == ne);
	std::string str_encode(data_encode.begin(), data_encode.end());
	std::string payLoad= base64_encode(reinterpret_cast<const unsigned char*>(str_encode.c_str()), str_encode.length());
	size_t  len = sizeof(passedMat);
	std::string timeStamp(buf);
	RdKafka::Headers *headers = RdKafka::Headers::create();
	headers->add("theKey", timeStamp+","+"camera_192158140_N-1"/*cameraLink*/);
	RdKafka::ErrorCode resp =
	Cam[partitionNumber].producer->produce(topic_str,partitionNumber,
		RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
		/* Value */
		const_cast<char *>(payLoad.c_str()),strlen(payLoad.c_str()),
		/* Key */
		NULL, 0 ,
		/* Timestamp (defaults to now) */
		RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME,
		/* Message headers, if any */
		headers,         
		/* Per-message opaque value passed to
		* delivery report */
		NULL);
	if (resp != RdKafka::ERR_NO_ERROR) 
	{
	std::cerr << "% Produce failed: " <<
		RdKafka::err2str(resp) << std::endl;
			//delete headers; /* Headers are automatically deleted on produce()
			/*   * success. */
	}
	else
	{
		std::cerr << "Send"<<
		std::endl;
	}
	Cam[partitionNumber].producer->poll(0);
}
