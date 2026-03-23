#include "pcap.h"
#include "platform_compat.h"

#ifndef THREADING_H
#define THREADING_H
// headerfile for necessary functions for multithreading

//extern DWORD WINAPI sniffer_thread(LPVOID lpParameter); // 
extern DWORD WINAPI sniffer_thread_DCP(LPVOID lpParameter); // ->main
extern DWORD WINAPI sniffer_thread_IP(LPVOID lpParameter); // ->main
extern DWORD WINAPI loopTimerThread(LPVOID lpParameter); //-> packet capturer

#ifdef _WIN32
extern BOOL LoadNpcapDlls();
#endif


#endif