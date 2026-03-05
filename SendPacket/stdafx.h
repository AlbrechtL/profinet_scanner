// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef _WIN32
#include "targetver.h"
#include <tchar.h>
#endif

#include "platform_compat.h"




// TODO: reference additional headers your program requires here
#include "pcap.h"
// own headerfiles
#include "protocols.h"
#include "threading.h"

#include "linkedList.h"
#include "packetHandler.h"
#include "deviceHandler.h"
#include "filehandler.h"

#include "remoteScan.h"

#ifdef _WIN32
#include <Iphlpapi.h>
#include <Assert.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <WinBase.h>
#include <processthreadsapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif


