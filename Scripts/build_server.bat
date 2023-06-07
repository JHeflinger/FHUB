@echo off
cd ..
mkdir bin
cd FHUB
gcc -o ../bin/FHUB_server server.c utils.h -lWs2_32
PAUSE