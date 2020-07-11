#!/bin/bash

BLACK=`tput setaf 0`
RED=`tput setaf 1`
GREEN=`tput setaf 2`
YELLOW=`tput setaf 3`
BLUE=`tput setaf 4`
MAGENTA=`tput setaf 5`
CYAN=`tput setaf 6`
WHITE=`tput setaf 7`
BOLD=`tput bold`
BLINK=`tput blink`
RESET=`tput sgr0`
tput civis
echo "${RED}${BOLD}|********************************STARTED SCRIPT********************************|${RESET}"
echo -e ""
echo -n "${CYAN}${BOLD}>>>>${BLINK}COMPILING${RESET}"
g++ -g -Ilibrdkafka/src-cpp -w  CVMATFFMPEG.cpp -o Stream  librdkafka/src-cpp/librdkafka++.a librdkafka/src/librdkafka.a -lm /home/jbm/librdkafka/mklove/deps/dest/zstd/usr/lib/libzstd.a -lssl -lcrypto -lz -ldl -lpthread -lrt  -lstdc++  -std=c++11 -fopenmp `pkg-config --cflags --libs opencv libavcodec libavutil libavformat libavdevice` -lswscale -lpthread -lboost_system -lboost_thread -std=c++11
echo  -e "\b\b\b\b\b\b\b\b\b${CYAN}${BOLD}COMPILATION COMPLETE !!!!!!!${RESET}"
echo -e ""
echo -n "${CYAN}${BOLD}>>>>${BLINK}CREATING A SCREEN SESSION AND RUNNING THE CODE ON IT${BLINK}${RESET}"
screen -S StreamAcquisition -d -m ./Stream config.txt> log.txt 
sleep 3
echo  -e -n "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b${CYAN}${BOLD}CREATED THE SCREEN WITH ${RESET}${MAGENTA}${BOLD}SCREEN NAME: ${RESET}${YELLOW}${BOLD}StreamAcquisition ${RESET}${CYAN}${BOLD}AND ${RESET}${MAGENTA}${BOLD}PID:${RESET}${YELLOW}${BOLD}"
echo " $(screen -ls | grep StreamAcquisition | awk '{print $1}' | sed 's/.StreamAcquisition//') !!"
echo -e ""
filename=$(cat config.txt | grep File | awk '{print $3}')
m1=$(md5sum "$filename")
while true; do
  sleep 10
  m2=$(md5sum "$filename")
  if [ "$m1" != "$m2" ] ; then
   echo "${RED}${BOLD}CAMERA LINK FILE CHANGED."
   echo -e ""
    echo -n "${CYAN}${BOLD}>>>>${BLINK}WIPING THE SCREEN${BLINK}${RESET}"
    kill -9 $(screen -ls | grep StreamAcquisition| awk '{print $1}' | sed 's/.StreamAcquisition//')
   sleep 3
    echo  -e "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b${CYAN}${BOLD}SCREEN WIPE COMPLETE!!!!!!!!${RESET}"
    screen -wipe  StreamAcquisition| grep ASDFVCXZ
    echo -n "${CYAN}${BOLD}>>>>${BLINK}RECREATING A SCREEN SESSION AND RUNNING THE CODE ON IT${BLINK}${RESET}"
    screen -S StreamAcquisition -d -m ./Stream config.txt >log.txt
    sleep 3
    echo  -e -n "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b${CYAN}${BOLD}RECREATED THE SCREEN WITH ${RESET}${MAGENTA}${BOLD}SCREEN NAME: ${RESET}${YELLOW}${BOLD}StreamAcquisition ${RESET}${CYAN}${BOLD}AND ${RESET}${MAGENTA}${BOLD}PID:${RESET}${YELLOW}${BOLD}"
    echo " $(screen -ls | grep StreamAcquisition | awk '{print $1}' | sed 's/.StreamAcquisition//') !!"
    echo -e ""
    m1=$m2
  fi
done
