

#include "common.h"
#include "deviceHandler.h"
#include "packetHandler.h"
#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#endif

#ifndef _WIN32
static bool isUsableEthernetInterface(const char* ifname)
{
	if (!ifname || ifname[0] == '\0') {
		return false;
	}

	char pathBuffer[256];
	snprintf(pathBuffer, sizeof(pathBuffer), "/sys/class/net/%s/wireless", ifname);
	if (access(pathBuffer, F_OK) == 0) {
		return false;
	}

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return false;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		close(fd);
		return false;
	}

	if ((ifr.ifr_flags & IFF_LOOPBACK) != 0 || (ifr.ifr_flags & IFF_UP) == 0) {
		close(fd);
		return false;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		close(fd);
		return false;
	}

	close(fd);
	return ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER;
}
#endif

// function which lists all pcap compatible adapters
// @param threadData -> stores the devicelist
int obtainDeviceList(threadData_t* threadData)
{
	pcap_if_t *alldevs = NULL;
	pcap_if_t *d;
	int adaperCount = 0;
	char errbuf[PCAP_ERRBUF_SIZE];


	/* Retrieve the device list from the local machine */
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs_ex: %s\n", errbuf);
		exit(1);
	}

	pcap_if_t* filteredHead = NULL;
	pcap_if_t* filteredTail = NULL;

	for (d = alldevs; d != NULL; )
	{
		pcap_if_t* next = d->next;
		d->next = NULL;

#ifndef _WIN32
		if (!isUsableEthernetInterface(d->name)) {
			pcap_freealldevs(d);
			d = next;
			continue;
		}
#endif

		if (!filteredHead) {
			filteredHead = d;
			filteredTail = d;
		} else {
			filteredTail->next = d;
			filteredTail = d;
		}

		adaperCount++;
		d = next;
	}

	alldevs = filteredHead;


	/*
	// print additional infos to the found adapters
	int count = 0;
	for (d = alldevs; d; d = d->next)
	{
	(count)++;
	ifprint(d, count);
	}
	*/


	if ((adaperCount) == 0)
	{
		printf("\nNo suitable Ethernet interfaces found.\n");

		// wait to see if it worked

		return -1;
	}
	if (!alldevs)
	{
		// list was empty
		// bail out
		printf_s("\nNo interfaces found! Make sure Npcap is installed.\n");
		return -1;
	}

	threadData->numberOfAdapters = adaperCount;
	threadData->alldevs = alldevs;

	return 0;
}

/*-----------------------------------------------------------------------------------------*/
// print one device with extra information
// @param d -> one device
// @param count -> adapternumber 
void ifprint(pcap_if_t *d, int count)
{
	pcap_addr_t *a;
	char ip6str[128];

	/* Name */
	printf("Adapternummer: %d\n", count);
	printf("%s\n", d->name);

	/* Description */
	if (d->description)
		printf("\tDescription: %s\n", d->description);



	/* Loopback Address*/
	printf("\tLoopback: %s\n", (d->flags & PCAP_IF_LOOPBACK) ? "yes" : "no");

	/* IP addresses */
	for (a = d->addresses; a; a = a->next) {
		printf("\tAddress Family: #%d\n", a->addr->sa_family);
		// 23 if ipv6 || 2 if ipv4
		switch (a->addr->sa_family)
		{
		case AF_INET:
			printf("\tAddress Family Name: AF_INET\n");
			if (a->addr)
				printf("\tAddress: %s\n", iptos(((struct sockaddr_in *)a->addr)->sin_addr.s_addr));
			if (a->netmask)
				printf("\tNetmask: %s\n", iptos(((struct sockaddr_in *)a->netmask)->sin_addr.s_addr));
			if (a->broadaddr)
				printf("\tBroadcast Address: %s\n", iptos(((struct sockaddr_in *)a->broadaddr)->sin_addr.s_addr));
			if (a->dstaddr)
				printf("\tDestination Address: %s\n", iptos(((struct sockaddr_in *)a->dstaddr)->sin_addr.s_addr));
			break;

		case AF_INET6:
			printf("\tAddress Family Name: AF_INET6\n");
			if (a->addr)
				printf("\tAddress: %s\n", ip6tos(a->addr, ip6str, sizeof(ip6str)));
			break;

		default:
			printf("\tAddress Family Name: Unknown\n");
			break;
		}
	}
	printf("\n\n");
}
/*-----------------------------------------------------------------------------------------*/

/* From tcptraceroute, convert a numeric IP address to a string */
char *iptos(u_long in)
{
	static char output[IPTOSBUFFERS][3 * 4 + 3 + 1];
	static short which;
	u_char *p;

	p = (u_char *)&in;
	which = (which + 1 == IPTOSBUFFERS ? 0 : which + 1);
	_snprintf_s(output[which], sizeof(output[which]), sizeof(output[which]), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return output[which];
}

/*-----------------------------------------------------------------------------------------*/
// convert ipv6 to string
char* ip6tos(struct sockaddr *sockaddr, char *address, int addrlen)
{
	socklen_t sockaddrlen;

#ifdef _WIN32
	sockaddrlen = sizeof(struct sockaddr_in6);
#else
	sockaddrlen = sizeof(struct sockaddr_storage);
#endif

	if (getnameinfo(sockaddr,
		sockaddrlen,
		address,
		addrlen,
		NULL,
		0,
		NI_NUMERICHOST) != 0) address = NULL;

	return address;
}


// get the own address of MAC and IP
// @param threadData -> data struct
void setOwnAddress(threadData_t* threadData)
{
	if (!threadData || !threadData->alldevs || netAdapterNmb < 1) {
		return;
	}

	pcap_if_t *d;
	int i;
	for (d = threadData->alldevs, i = 0; i < netAdapterNmb - 1 && d != NULL; d = d->next, i++);
	if (!d) {
		return;
	}

	getIP_SUB(d, threadData);

	char ip[16];
	//char dot = '.';

	sprintf_s(ip, sizeof(ip), "%d.%d.%d.%d", threadData->ownIp.byte1, threadData->ownIp.byte2, threadData->ownIp.byte3, threadData->ownIp.byte4);

	mac_address* m_address = getMAC(ip);
	if (!m_address) {
		return;
	}
	threadData->ownMac.byte1 = m_address->byte1;
	threadData->ownMac.byte2 = m_address->byte2;
	threadData->ownMac.byte3 = m_address->byte3;
	threadData->ownMac.byte4 = m_address->byte4;
	threadData->ownMac.byte5 = m_address->byte5;
	threadData->ownMac.byte6 = m_address->byte6;
	free(m_address);

}

// extract the ip address and subnetmask from a given device
void getIP_SUB(pcap_if_t *d, threadData_t* threadData)
{
	if (!d || !threadData) {
		return;
	}

	pcap_addr_t *a;

	/* IP addresses */
	for (a = d->addresses; a; a = a->next) {
		if (!a->addr) {
			continue;
		}

		//ipv4 equals 2, we only need ipv4
		switch (a->addr->sa_family)
		{
		case AF_INET:
			if (a->addr)
				extractIP(&threadData->ownIp, ((struct sockaddr_in *)a->addr)->sin_addr.s_addr);
			if (a->netmask)
				extractIP(&threadData->subnetmask, ((struct sockaddr_in *)a->netmask)->sin_addr.s_addr);
			break;
		default:
			break;
		}
	}
	printf("\n\n");
}

// convert the long ip address to a struct ip address
void extractIP(ip_address* ip, u_long address)
{
	u_char* p = (u_char *)&address;
	ip->byte1 = *p;
	ip->byte2 = *(p + 1);
	ip->byte3 = *(p + 2);
	ip->byte4 = *(p + 3);
}

static uint32_t ipAddressToUint32(ip_address address)
{
	return ((uint32_t)address.byte1 << 24) |
		((uint32_t)address.byte2 << 16) |
		((uint32_t)address.byte3 << 8) |
		(uint32_t)address.byte4;
}

static bool isTargetOnLocalSubnet(const threadData_t* threadData, const ip_address* targetIp)
{
	if (!threadData || !targetIp) {
		return false;
	}

	uint32_t localAddress = ipAddressToUint32(threadData->ownIp);
	uint32_t subnetMask = ipAddressToUint32(threadData->subnetmask);
	uint32_t targetAddress = ipAddressToUint32(*targetIp);

	return (localAddress & subnetMask) == (targetAddress & subnetMask);
}

static const char* getSelectedAdapterName(const threadData_t* threadData)
{
	pcap_if_t* device;
	int index;

	if (!threadData || !threadData->alldevs || netAdapterNmb < 1) {
		return NULL;
	}

	for (device = threadData->alldevs, index = 0; index < netAdapterNmb - 1 && device != NULL; device = device->next, index++);
	if (!device) {
		return NULL;
	}

	return device->name;
}

static void copyMacAddressValue(mac_address* destination, const mac_address* source)
{
	if (!destination || !source) {
		return;
	}

	destination->byte1 = source->byte1;
	destination->byte2 = source->byte2;
	destination->byte3 = source->byte3;
	destination->byte4 = source->byte4;
	destination->byte5 = source->byte5;
	destination->byte6 = source->byte6;
}

static int parseMacAddressString(const char* macString, mac_address* resolvedMac)
{
	unsigned int byte1;
	unsigned int byte2;
	unsigned int byte3;
	unsigned int byte4;
	unsigned int byte5;
	unsigned int byte6;

	if (!macString || !resolvedMac) {
		return -1;
	}

	if (sscanf(macString, "%x:%x:%x:%x:%x:%x", &byte1, &byte2, &byte3, &byte4, &byte5, &byte6) != 6) {
		return -1;
	}

	resolvedMac->byte1 = (u_char)byte1;
	resolvedMac->byte2 = (u_char)byte2;
	resolvedMac->byte3 = (u_char)byte3;
	resolvedMac->byte4 = (u_char)byte4;
	resolvedMac->byte5 = (u_char)byte5;
	resolvedMac->byte6 = (u_char)byte6;
	return 0;
}

#ifdef _WIN32
static void warmUpArpCacheForIp(const char* ipaddr)
{
	char pingbuf[64];
	FILE* fp;

	if (!ipaddr) {
		return;
	}

	sprintf_s(pingbuf, sizeof(pingbuf), "ping -n 1 -l 1 %s", ipaddr);
	fp = _popen(pingbuf, "r");
	if (!fp) {
		return;
	}

	while (fgetc(fp) != EOF) {}
	_pclose(fp);
}

static int lookupArpMacAddress(const char* ipaddr, const char* interfaceName, mac_address* resolvedMac)
{
	char arpbuf[64];
	char outbuf[BUFSIZE];
	FILE* fp;

	(void)interfaceName;

	if (!ipaddr || !resolvedMac) {
		return -1;
	}

	sprintf_s(arpbuf, sizeof(arpbuf), "arp -a %s", ipaddr);
	fp = _popen(arpbuf, "r");
	if (!fp) {
		return -1;
	}

	while (fgets(outbuf, sizeof(outbuf), fp) != NULL) {
		char foundIp[64];
		char mac[32];
		char type[32];

		if (sscanf(outbuf, "%63s %31s %31s", foundIp, mac, type) != 3) {
			continue;
		}

		if (strcmp(foundIp, ipaddr) == 0) {
			for (size_t index = 0; mac[index] != '\0'; index++) {
				if (mac[index] == '-') {
					mac[index] = ':';
				}
			}

			_pclose(fp);
			return parseMacAddressString(mac, resolvedMac);
		}
	}

	_pclose(fp);
	return -1;
}
#else
static void warmUpArpCacheForIp(const char* interfaceName, const char* ipaddr)
{
	char pingcmd[160];

	if (!ipaddr) {
		return;
	}

	if (interfaceName && interfaceName[0] != '\0') {
		snprintf(pingcmd, sizeof(pingcmd), "ping -I %s -c 1 -W 1 %s > /dev/null 2>&1", interfaceName, ipaddr);
	} else {
		snprintf(pingcmd, sizeof(pingcmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ipaddr);
	}

	(void)system(pingcmd);
}

static int lookupArpMacAddress(const char* ipaddr, const char* interfaceName, mac_address* resolvedMac)
{
	FILE* arpFile;
	char line[256];

	if (!ipaddr || !resolvedMac) {
		return -1;
	}

	arpFile = fopen("/proc/net/arp", "r");
	if (!arpFile) {
		return -1;
	}

	if (!fgets(line, sizeof(line), arpFile)) {
		fclose(arpFile);
		return -1;
	}

	while (fgets(line, sizeof(line), arpFile)) {
		char foundIp[64];
		char hwType[16];
		char flags[16];
		char mac[32];
		char mask[32];
		char device[IFNAMSIZ];

		if (sscanf(line, "%63s %15s %15s %31s %31s %15s", foundIp, hwType, flags, mac, mask, device) != 6) {
			continue;
		}

		if (strcmp(foundIp, ipaddr) != 0) {
			continue;
		}

		if (interfaceName && interfaceName[0] != '\0' && strcmp(device, interfaceName) != 0) {
			continue;
		}

		fclose(arpFile);
		return parseMacAddressString(mac, resolvedMac);
	}

	fclose(arpFile);
	return -1;
}
#endif




/**
* @return the mac address of the interface with the given ip
*/
mac_address* getMAC(const char *ip){
#ifdef _WIN32
	PIP_ADAPTER_INFO AdapterInfo;
	DWORD dwBufLen = sizeof(AdapterInfo);
	mac_address* macad;
	if ((macad = malloc(sizeof(mac_address))) == NULL){
		printf("Error allocating memory needed to store mac address\n");
		return NULL;
	}

	AdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
	if (AdapterInfo == NULL) {
		printf("Error allocating memory needed to call GetAdaptersinfo\n");
		return NULL;
	}

	// Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable
	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(AdapterInfo);
		AdapterInfo = (IP_ADAPTER_INFO *)malloc(dwBufLen);
		if (AdapterInfo == NULL) {
			printf("Error allocating memory needed to call GetAdaptersinfo\n");
			return NULL;
		}
	}

	if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) {
		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info
		do {
			macad->byte1 = pAdapterInfo->Address[0];
			macad->byte2 = pAdapterInfo->Address[1];
			macad->byte3 = pAdapterInfo->Address[2];
			macad->byte4 = pAdapterInfo->Address[3];
			macad->byte5 = pAdapterInfo->Address[4];
			macad->byte6 = pAdapterInfo->Address[5];

			char* tmp = pAdapterInfo->IpAddressList.IpAddress.String;

			if (strcmp(ip, pAdapterInfo->IpAddressList.IpAddress.String) == 0){
				free(AdapterInfo);
				return macad;
			}
			printf("\n");
			pAdapterInfo = pAdapterInfo->Next;
		} while (pAdapterInfo);
	}
	free(AdapterInfo);
	return NULL;
#else
	if (!ip) {
		return NULL;
	}

	struct ifaddrs* ifaddr = NULL;
	struct ifaddrs* ifa = NULL;
	char ipString[INET_ADDRSTRLEN];
	char selectedIfname[IFNAMSIZ];
	selectedIfname[0] = '\0';

	if (getifaddrs(&ifaddr) == -1) {
		return NULL;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
			continue;
		}

		if (inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ipString, sizeof(ipString)) == NULL) {
			continue;
		}

		if (strcmp(ipString, ip) == 0) {
			strncpy(selectedIfname, ifa->ifa_name, sizeof(selectedIfname) - 1);
			selectedIfname[sizeof(selectedIfname) - 1] = '\0';
			break;
		}
	}

	freeifaddrs(ifaddr);

	if (selectedIfname[0] == '\0') {
		return NULL;
	}

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return NULL;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, selectedIfname, IFNAMSIZ - 1);

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		close(fd);
		return NULL;
	}

	close(fd);

	mac_address* macad = (mac_address*)malloc(sizeof(mac_address));
	if (!macad) {
		return NULL;
	}

	unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
	macad->byte1 = mac[0];
	macad->byte2 = mac[1];
	macad->byte3 = mac[2];
	macad->byte4 = mac[3];
	macad->byte5 = mac[4];
	macad->byte6 = mac[5];

	return macad;
#endif
}

// fuction returns the default gateway (ip) of a lokal interface with help of the mac address
ip_address* getAdapterDefaultGateway_IP(threadData_t* threadData)
{
#ifdef _WIN32
	ULONG outBufLen = 15000;
	ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_ALL_INTERFACES | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST;
	ULONG family = AF_INET;

	DWORD dwRetVal = 0;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;


	do{
		if ((pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen)) == NULL)
		{
			printf("Error allocating memory for AdaperAdresses");
			return NULL;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			free(pAddresses);
			pAddresses = NULL;
			outBufLen += 5000;
		}
		else{
			break;
		}

	} while (1);

	bool end = false;
	while (pAddresses->Next != NULL && !end)
	{
		mac_address local;
		local.byte1 = pAddresses->PhysicalAddress[0];
		local.byte2 = pAddresses->PhysicalAddress[1];
		local.byte3 = pAddresses->PhysicalAddress[2];
		local.byte4 = pAddresses->PhysicalAddress[3];
		local.byte5 = pAddresses->PhysicalAddress[4];
		local.byte6 = pAddresses->PhysicalAddress[5];

		if (compareMacAddress(local, threadData->ownMac)){
			ip_address* def_gateway = malloc(sizeof(ip_address));
			if (!def_gateway)
				return NULL;

			int i;
			if (!pAddresses->FirstGatewayAddress){
				printf("Error no default Gateway address for this adapter\n");
				return NULL;

			}
			for (i = 0; pAddresses->FirstGatewayAddress->Address.lpSockaddr->sa_data[i] == 0; i++);
			u_char* dg = (((u_char*)pAddresses->FirstGatewayAddress->Address.lpSockaddr->sa_data)+i);
			def_gateway->byte1 = *dg;
			def_gateway->byte2 = *(dg + 1);
			def_gateway->byte3 = *(dg + 2);
			def_gateway->byte4 = *(dg + 3);


			//free(pAddresses);
			return def_gateway;


		}
		else{
			pAddresses = pAddresses->Next;
		}

	}


	return NULL;
#else
	(void)threadData;
	FILE* routeFile = fopen("/proc/net/route", "r");
	if (!routeFile) {
		return NULL;
	}

	char line[256];
	if (!fgets(line, sizeof(line), routeFile)) {
		fclose(routeFile);
		return NULL;
	}

	while (fgets(line, sizeof(line), routeFile)) {
		char iface[IFNAMSIZ];
		unsigned long destination = 0;
		unsigned long gateway = 0;
		unsigned int flags = 0;

		if (sscanf(line, "%15s %lx %lx %x", iface, &destination, &gateway, &flags) != 4) {
			continue;
		}

		if (destination == 0 && (flags & 0x2)) {
			ip_address* defGateway = (ip_address*)malloc(sizeof(ip_address));
			if (!defGateway) {
				fclose(routeFile);
				return NULL;
			}

			defGateway->byte1 = (u_char)(gateway & 0xFF);
			defGateway->byte2 = (u_char)((gateway >> 8) & 0xFF);
			defGateway->byte3 = (u_char)((gateway >> 16) & 0xFF);
			defGateway->byte4 = (u_char)((gateway >> 24) & 0xFF);

			fclose(routeFile);
			return defGateway;
		}
	}

	fclose(routeFile);
	return NULL;
#endif
}

#define BUFSIZE 256
// send a ping to the given ip and read the arp table with the cmd to get the MAC address of the default gateway 
void getAdapterDefaultGateway_MAC(threadData_t* threadData, ip_address* defaultGateway)
{
	if (!defaultGateway) {
		return;
	}

	if (!threadData->defaultGatewayMAC) {
		threadData->defaultGatewayMAC = malloc(sizeof(mac_address));
		if (!threadData->defaultGatewayMAC) {
			printf("Error allocating memory for defaultGatewayMac address");
			return;
		}
	}

#ifdef _WIN32
	char ipaddr[16];
	sprintf_s(ipaddr, sizeof(ipaddr), "%d.%d.%d.%d", defaultGateway->byte1, defaultGateway->byte2, defaultGateway->byte3, defaultGateway->byte4);
	warmUpArpCacheForIp(ipaddr);
	if (lookupArpMacAddress(ipaddr, NULL, threadData->defaultGatewayMAC) != 0) {
		memset(threadData->defaultGatewayMAC, 0, sizeof(*threadData->defaultGatewayMAC));
	}

#else
	char ipaddr[16];
	snprintf(ipaddr, sizeof(ipaddr), "%d.%d.%d.%d", defaultGateway->byte1, defaultGateway->byte2, defaultGateway->byte3, defaultGateway->byte4);
	warmUpArpCacheForIp(getSelectedAdapterName(threadData), ipaddr);
	if (lookupArpMacAddress(ipaddr, getSelectedAdapterName(threadData), threadData->defaultGatewayMAC) != 0) {
		memset(threadData->defaultGatewayMAC, 0, sizeof(*threadData->defaultGatewayMAC));
	}
#endif


}

int resolveRemoteDestinationMac(threadData_t* threadData, const ip_address* targetIp, mac_address* resolvedMac)
{
	const char* selectedAdapterName;
	char ipaddr[16];
	ip_address* defaultGatewayIP;

	if (!threadData || !targetIp || !resolvedMac) {
		return -1;
	}

	if (!isTargetOnLocalSubnet(threadData, targetIp)) {
		if (!threadData->defaultGatewayMAC ||
			(threadData->defaultGatewayMAC->byte1 == 0 &&
			threadData->defaultGatewayMAC->byte2 == 0 &&
			threadData->defaultGatewayMAC->byte3 == 0 &&
			threadData->defaultGatewayMAC->byte4 == 0 &&
			threadData->defaultGatewayMAC->byte5 == 0 &&
			threadData->defaultGatewayMAC->byte6 == 0)) {
			defaultGatewayIP = getAdapterDefaultGateway_IP(threadData);
			if (!defaultGatewayIP) {
				return -1;
			}

			getAdapterDefaultGateway_MAC(threadData, defaultGatewayIP);
			free(defaultGatewayIP);
		}

		if (!threadData->defaultGatewayMAC) {
			return -1;
		}

		copyMacAddressValue(resolvedMac, threadData->defaultGatewayMAC);
		return 0;
	}

	selectedAdapterName = getSelectedAdapterName(threadData);
	snprintf(ipaddr, sizeof(ipaddr), "%d.%d.%d.%d", targetIp->byte1, targetIp->byte2, targetIp->byte3, targetIp->byte4);
#ifdef _WIN32
	warmUpArpCacheForIp(ipaddr);
#else
	warmUpArpCacheForIp(selectedAdapterName, ipaddr);
#endif
	return lookupArpMacAddress(ipaddr, selectedAdapterName, resolvedMac);
}

// create a threadData struct and set all pointers to NULL
threadData_t* createDataStruct()
{
	threadData_t* tD = malloc(sizeof(threadData_t));
	if (!tD)
	{
		printf("Error allocating memory for data struct");
		return NULL;
	}

	tD->first = NULL;
	tD->targetIP = NULL;
	tD->defaultGatewayMAC = NULL;
	tD->devCount = 0;

	return tD;
}