@echo off
cd ..
mkdir bin
cd FHUB
gcc -o ../bin/FHUB_chat chat.c utils.h -lWs2_32
PAUSE