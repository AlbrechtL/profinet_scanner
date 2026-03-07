// @author Stefan Eiwanger


// main.c : Defines the entry point for the console application.
// get a list of adapters -> get user input for adapter, layer and if necessary ip address
// start listeners and send packets
// get user input for path to store the data
// free the data
// end



#include "stdafx.h"




// identification number of ip protocol
unsigned short identnmb = 0;

typedef struct cli_options {
	bool interactive;
	bool listInterfaces;
	bool hasInterface;
	bool hasMode;
	bool hasTarget;
	bool hasDuration;
	int mode;
	long durationSeconds;
	char interfaceValue[256];
	char targetValue[32];
} cli_options_t;

static void printHelp(const char* programName)
{
	printf("Usage:\n");
	printf("  %s --help\n", programName);
	printf("  %s --list-interfaces\n", programName);
	printf("  %s --interface <index|name> --mode <local|remote> [--target <a.b.c.d[-e]>]\n", programName);
	printf("  %s --interface <index|name> --mode <local|remote> [--target <a.b.c.d[-e]>] [--duration <seconds>]\n", programName);
	printf("  %s --interactive\n\n", programName);

	printf("Options:\n");
	printf("  --help               Show this help message and exit.\n");
	printf("  --list-interfaces    List available capture interfaces and exit.\n");
	printf("  --interface VALUE    Interface index (1-based) or interface name from --list-interfaces.\n");
	printf("  --mode VALUE         Scan mode: local (DCP) or remote (DCE/RPC).\n");
	printf("  --target VALUE       Remote target IP or range in the form a.b.c.d or a.b.c.d-e.\n");
	printf("                       Required when --mode remote is used.\n");
	printf("  --duration SECONDS   Stop capture after the given number of seconds.\n");
	printf("  --interactive        Run prompt-based mode (default when no arguments are given).\n\n");

	printf("Examples:\n");
	printf("  %s --list-interfaces\n", programName);
	printf("  %s --interface 1 --mode local\n", programName);
	printf("  %s --interface eth0 --mode remote --target 192.168.0.10-20\n", programName);
	printf("  %s --interface eth0 --mode remote --target 192.168.0.10 --duration 10\n", programName);
}

static void printDeviceSummary(const datasheet* device)
{
	if (!device) {
		return;
	}

	printf("Device\n");
	printf("  IP: %d.%d.%d.%d\n",
		device->deviceIp.byte1,
		device->deviceIp.byte2,
		device->deviceIp.byte3,
		device->deviceIp.byte4);
	printf("  MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
		device->deviceMACaddress.byte1 & 0xFF,
		device->deviceMACaddress.byte2 & 0xFF,
		device->deviceMACaddress.byte3 & 0xFF,
		device->deviceMACaddress.byte4 & 0xFF,
		device->deviceMACaddress.byte5 & 0xFF,
		device->deviceMACaddress.byte6 & 0xFF);
	printf("  Name: %s\n", device->nameOfStation ? device->nameOfStation : "");
	printf("  Type: %s\n", device->deviceType ? device->deviceType : "");
	if (device->annotation && device->annotation[0] != '\0') {
		printf("  Annotation: %s\n", device->annotation);
	}
	printf("  Order ID: %s\n", device->orderId ? device->orderId : "");
	printf("  SW Version: %s\n", device->version ? device->version : "");
	printf("  HW Revision: %s\n", device->hardwareRevison ? device->hardwareRevison : "");
	printf("  Vendor ID: 0x%04x\n", device->vendorId);
	printf("  Device ID: 0x%04x\n", device->deviceId);
	if (device->udpPort != 0) {
		printf("  UDP Port: %u\n", device->udpPort);
	}
	printf("\n");
}

static void printResultsToStdout(linked_list_t* list)
{
	if (!list) {
		printf("No devices found.\n");
		return;
	}

	printf("\nScan results (stdout):\n\n");
	for (linked_list_t* current = list; current != NULL; current = current->next) {
		printDeviceSummary(current->device);
	}
}

static void printInterfaces(threadData_t* threadData)
{
	pcap_if_t* device;
	int index = 1;

	printf("Available interfaces:\n");
	for (device = threadData->alldevs; device != NULL; device = device->next, index++) {
		printf("  [%d] %s", index, device->name ? device->name : "(unknown)");
		if (device->description) {
			printf(" - %s", device->description);
		}
		printf("\n");
	}
}

static int resolveInterfaceIndex(threadData_t* threadData, const char* interfaceValue)
{
	if (!interfaceValue || interfaceValue[0] == '\0') {
		return -1;
	}

	char* endptr = NULL;
	long parsedIndex = strtol(interfaceValue, &endptr, 10);
	if (endptr && *endptr == '\0') {
		if (parsedIndex >= 1 && parsedIndex <= threadData->numberOfAdapters) {
			return (int)parsedIndex;
		}
		return -1;
	}

	pcap_if_t* device;
	int index = 1;
	for (device = threadData->alldevs; device != NULL; device = device->next, index++) {
		if (device->name && strcmp(device->name, interfaceValue) == 0) {
			return index;
		}
	}

	return -1;
}

int main(int argc, char **argv) {
	cli_options_t options;
	int inum = 0;
	int mode = -1;

	memset(&options, 0, sizeof(options));
	options.mode = -1;
	options.interactive = (argc == 1);

	for (int argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
		if (strcmp(argv[argumentIndex], "--help") == 0) {
			printHelp(argv[0]);
			return 0;
		}
		if (strcmp(argv[argumentIndex], "--interactive") == 0) {
			options.interactive = true;
			continue;
		}
		if (strcmp(argv[argumentIndex], "--list-interfaces") == 0) {
			options.listInterfaces = true;
			continue;
		}
		if (strcmp(argv[argumentIndex], "--interface") == 0) {
			if (argumentIndex + 1 >= argc) {
				printf("Missing value for --interface\n");
				return -1;
			}
			strcpy_s(options.interfaceValue, sizeof(options.interfaceValue), argv[++argumentIndex]);
			options.hasInterface = true;
			continue;
		}
		if (strcmp(argv[argumentIndex], "--mode") == 0) {
			if (argumentIndex + 1 >= argc) {
				printf("Missing value for --mode\n");
				return -1;
			}

			char* modeArg = argv[++argumentIndex];
			if (strcmp(modeArg, "local") == 0) {
				options.mode = 0;
			} else if (strcmp(modeArg, "remote") == 0) {
				options.mode = 1;
			} else {
				printf("Invalid --mode value: %s (use local or remote)\n", modeArg);
				return -1;
			}
			options.hasMode = true;
			continue;
		}
		if (strcmp(argv[argumentIndex], "--target") == 0) {
			if (argumentIndex + 1 >= argc) {
				printf("Missing value for --target\n");
				return -1;
			}
			strcpy_s(options.targetValue, sizeof(options.targetValue), argv[++argumentIndex]);
			options.hasTarget = true;
			continue;
		}
		if (strcmp(argv[argumentIndex], "--duration") == 0) {
			if (argumentIndex + 1 >= argc) {
				printf("Missing value for --duration\n");
				return -1;
			}
			char* durationArg = argv[++argumentIndex];
			char* endptr = NULL;
			long parsedSeconds = strtol(durationArg, &endptr, 10);
			if (!endptr || *endptr != '\0' || parsedSeconds <= 0) {
				printf("Invalid --duration value: %s (use a positive integer)\n", durationArg);
				return -1;
			}
			options.durationSeconds = parsedSeconds;
			options.hasDuration = true;
			continue;
		}

		printf("Unknown argument: %s\n", argv[argumentIndex]);
		printf("Use --help to see available options.\n");
		return -1;
	}

	if (!options.interactive && !options.listInterfaces) {
		if (!options.hasInterface) {
			printf("Missing required option --interface in non-interactive mode\n");
			return -1;
		}
		if (!options.hasMode) {
			printf("Missing required option --mode in non-interactive mode\n");
			return -1;
		}
		if (options.mode == 1 && !options.hasTarget) {
			printf("Missing required option --target for remote mode\n");
			return -1;
		}
	}

	// check if on windows, if so then load the npcap library
#ifdef WIN32
	/* Load Npcap and its functions. */
	if (!LoadNpcapDlls()) {
		fprintf(stderr, "Couldn't load Npcap\n");
		exit(1);
	}
#endif

	// allocate memory for temporary storage for device data and the data the functions need
	threadData_t* threadData = createDataStruct();
	g_scanStopRequested = false;

	// Pointer to the Exitcode of a thread
	LPDWORD lpExitCode = NULL;

	// get the list of possible devices
	if (obtainDeviceList(threadData) != 0)
	{
		printf_s("Error at obtainDeviceList\n");
		return -1;
	}

	if (options.listInterfaces)
	{
		printInterfaces(threadData);
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return 0;
	}

	if (options.hasInterface) {
		inum = resolveInterfaceIndex(threadData, options.interfaceValue);
		if (inum < 1 || inum > threadData->numberOfAdapters) {
			printf_s("\nInterface value out of range or unknown: %s\n", options.interfaceValue);
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			return -1;
		}
	} else if (options.interactive) {
		// get interfacenumber to scan and send
		printInterfaces(threadData);
		printf_s("Enter the interface number (1-%d):", threadData->numberOfAdapters);
		scanf_s("%d", &inum);
		getchar(); // flush buffer
	} else {
		printf("Missing required option --interface in non-interactive mode\n");
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}



	if (inum < 1 || inum > threadData->numberOfAdapters)
	{
		printf_s("\nInterface number out of range.\n");

		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}


	netAdapterNmb = inum;
	setOwnAddress(threadData);
	//---------------------------------------------------------------------------------------------------------------------


	// get mac addres of def gateway
	ip_address* defaultGatewayIP = getAdapterDefaultGateway_IP(threadData);
	getAdapterDefaultGateway_MAC(threadData, defaultGatewayIP);
	if (defaultGatewayIP) {
		free(defaultGatewayIP);
	}

	g_scanDurationMs = options.hasDuration ? (options.durationSeconds * 1000L) : 0;
	g_scanStartMs = g_scanDurationMs > 0 ? get_monotonic_ms() : 0;




	if (options.hasMode) {
		mode = options.mode;
	} else if (options.interactive) {
		printf("\nScan local (0) or remote (1): \n");
		scanf_s("%d", &mode);
		getchar(); // flush buffer
	} else {
		printf("Missing required option --mode in non-interactive mode\n");
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}

	if (mode != 0 && mode != 1)
	{
		printf("Invalid mode value. Use 0/1 in interactive mode or --mode local|remote in CLI mode.\n");
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}


	if (mode == 1)
	{
		// insert target ip address   
		char targetIP[4 * 3 + 3 + 1 + 1 + 3]; // 4*3 numbers, 3 dots and 1 \0
		targetIP[0] = 0;	// first init to zero

		if (options.hasTarget) {
			strcpy_s(targetIP, sizeof(targetIP), options.targetValue);
		} else if (options.interactive) {
			printf_s("\nTarget IP address form xxx.xxx.xxx.xxx-xxx: \n");
			fgets(targetIP, sizeof(targetIP), stdin);
		} else {
			printf("Missing required option --target for remote mode\n");
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			return -1;
		}

		int range;
		if ((range = checkIP(targetIP, threadData)) == -1){
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			if (options.interactive) {
				#ifdef WIN32
				system("pause");
				#else
				printf("Press Enter to continue...");
				getchar();
				#endif
			}
			return -1; // false IP
		}

		if (!threadData->defaultGatewayMAC) {
			printf("Default gateway MAC could not be resolved for remote scan.\n");
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			if (options.interactive) {
				#ifdef WIN32
				system("pause");
				#else
				printf("Press Enter to continue...");
				getchar();
				#endif
			}
			return -1;
		}

		int l = 0;

		// we have all necessary information, start the scanning

		HANDLE sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);

		printf_s("\nSend RPC lookup endpointmapper first call \n");
		for (l = 0; l <= range; l++)
		{
			threadData->numberOfIPDev = l;
			sendPacket_RPC_rem(threadData, true);

		}

		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}


		int defcount = linkedlist_status(threadData->first);
		if (defcount == -1)
		{
			printf_s("No devices found with given ip\n\n");
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			if (options.interactive) {
				#ifdef WIN32
				system("pause");
				#else
				printf("Press Enter to continue...");
				getchar();
				#endif
			}
			return -1;
		}
		printf_s("\nRPC lookup first call finished \n\n");
		printf_s("\nSend RPC lookup endpointmapper second call \n");
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);



		for (int k = 0; k < defcount; k++){
			threadData->numberOfIPDev = k;
			sendPacket_RPC_rem(threadData, false);
		}
		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}


		printf_s("\nRPC lookup second call finished \n\n");

		printf_s("\nSend RPC implicit read PDRealData \n");
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);

		for (int k = 0; k < defcount; k++){
			threadData->numberOfIPDev = k;

			sendpacket_IM_rem(threadData, PDREALDATA, NULL, 3);
		}
		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}


		printf_s("\nPDRealData call finished \n\n");

		printf_s("\nSend RPC implicit read realidentificationdata \n");
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		for (int k = 0; k < defcount; k++){
			threadData->numberOfIPDev = k;

			sendpacket_IM_rem(threadData, REALIDENTIFICATIONDATA, NULL, 3);  // get all slots of the device
		}
		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}

		printf_s("\nRealIdentificationData call finished \n\n");


		// send a request for each slot and subslot to get the data out of them
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		linked_list_t* list = threadData->first;
		slotParameter slotpara;
		printf_s("\nSend RPC implicit read PDRealData for one subslot \n");

		for (int k = 0; k < defcount; k++, list = list->next){
			linkedList_slot* slot = list->device->slotList;
			threadData->numberOfIPDev = k;

			slotpara.posSlot = 0;
			while (slot){
				linkedList_subslot* subslot = slot->subslotList;
				slotpara.posSubslot = 0;
				while (subslot){
					sendpacket_IM_rem(threadData, PDREALDATASUBMODUL, &slotpara, 3);  // get the data of each submodul
					Sleep(200); // ddos if the request are to fast, try slowing it down
					subslot = subslot->next;
					slotpara.posSubslot++;
				}
				slot = slot->next;
				slotpara.posSlot++;
			}

		}
		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}
		printf_s("\nPDRealData for each subslot call finished \n\n");

		printf_s("\nSend RPC implicit read I&M data for each module \n");
		slotpara.posSubslot = -1; // set to non reachable value
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		list = threadData->first;
		for (int k = 0; k < defcount; k++, list = list->next){
			threadData->numberOfIPDev = k;

			linkedList_slot* slot = list->device->slotList;
			slotpara.posSlot = 0;
			while (slot){
				sendpacket_IM_rem(threadData, IM0, &slotpara, 3);  // get all slots of the device
				Sleep(200); // ddos if the request are to fast, try slowing it down
				slot = slot->next;
				slotpara.posSlot++;

			}
		}
		WaitForSingleObject(sniffThreadrem, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}

		printf_s("\nI&M data call for each module finished \n\n");

	}
	else
	{

		HANDLE sniffThread = CreateThread(NULL, 0, sniffer_thread_DCP, threadData, 0, lpExitCode);


		printf_s("\nSend pn_dcp \n");
		sendPacket_DCP(threadData);

		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}
		// every time a packet is recieved the timer of pcap_next_ex is restored to TIMEOUT seconds, if the TIMEOUT seconds are over the function returns 0
		printf_s("\npn_dcp finished\n\n");
		// so far so good it works

		sniffThread = CreateThread(NULL, 0, sniffer_thread_IP, threadData, 0, lpExitCode);

		// open new thread for sniffing of rcp packets
		int deviceCount = linkedlist_status(threadData->first);
		if (deviceCount == -1)
		{
			WaitForSingleObject(sniffThread, INFINITE);
			printf_s("List empty; no profinet devices in the subnet!");
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			return -1;
		}

		// for each device in the linkedlist send a rpc call
		printf_s("\nSend RPC lookup endpointmapper first call\n");

		for (int k = 0; k < deviceCount; k++){
			threadData->numberOfIPDev = k;
			sendPacket_RPC(threadData);
		}

		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}

		printf_s("\nRPC lookup first call finished \n\n");

		// again sniff against IP-RPC
		// this time the intern break should stop the loop
		printf_s("\nSend RPC lookup endpointmapper second call \n");
		sniffThread = CreateThread(NULL, 0, sniffer_thread_IP, threadData, 0, lpExitCode);
		for (int k = 0; k < deviceCount; k++){
			threadData->numberOfIPDev = k;
			sendPacket_RPC(threadData);
		}
		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}
		printf_s("\nRPC lookup second call finished \n\n");


		// reuse remote handler
		printf_s("\nSend RPC implicit read realidentificationdata \n");
		sniffThread = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		for (int k = 0; k < deviceCount; k++){

			threadData->numberOfIPDev = k;
			sendpacket_IM_rem(threadData, REALIDENTIFICATIONDATA, NULL, 2);  // get all slots of the device
		}
		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}


		// send a request for each slot and subslot to get the data out of them
		sniffThread = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		linked_list_t* list = threadData->first;
		slotParameter slotpara;
		printf_s("\nSend RPC implicit read PDRealData for one subslot \n");

		for (int k = 0; k < deviceCount; k++, list = list->next){
			linkedList_slot* slot = list->device->slotList;
			threadData->numberOfIPDev = k;

			slotpara.posSlot = 0;
			while (slot){
				linkedList_subslot* subslot = slot->subslotList;
				slotpara.posSubslot = 0;
				while (subslot){
					sendpacket_IM_rem(threadData, PDREALDATASUBMODUL, &slotpara, 2);  // get the data of each submodul
					Sleep(200); // ddos if the request are to fast, try slowing it down
					subslot = subslot->next;
					slotpara.posSubslot++;
				}
				slot = slot->next;
				slotpara.posSlot++;
			}

		}
		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}
		printf_s("\nPDRealData for each subslot call finished \n\n");


		printf_s("\nSend RPC implicit read I&M data for each module \n");
		slotpara.posSubslot = -1; // set to non reachable value
		sniffThread = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		list = threadData->first;

		for (int k = 0; k < deviceCount; k++, list = list->next){
			threadData->numberOfIPDev = k;

			linkedList_slot* slot = list->device->slotList;
			slotpara.posSlot = 0;
			while (slot){
				sendpacket_IM_rem(threadData, IM0, &slotpara, 2);  // get all slots of the device
				Sleep(200); // ddos if the request are to fast, try slowing it down
				slot = slot->next;
				slotpara.posSlot++;
			}
		}
		WaitForSingleObject(sniffThread, INFINITE);

		printf_s("\nI&M data call for each module finished \n\n");


	}



	// free list of devices
finalize:
	pcap_freealldevs(threadData->alldevs);



	printResultsToStdout(threadData->first);
	if (options.interactive) {
		#ifdef WIN32
        system("pause");
        #else
        printf("Press Enter to continue...");
        getchar();
        #endif
	}

	empty_list(threadData->first);
	free(threadData);

	return 0;
}





/*-----------------------------------------------------------------------------------------*/
// function to load the Npcap library
#ifdef WIN32
BOOL LoadNpcapDlls() {
	_TCHAR npcap_dir[512];
	UINT len;
	len = GetSystemDirectory(npcap_dir, 480);
	if (!len) {
		fprintf(stderr, "Error in GetSystemDirectory: %x", GetLastError());
		return FALSE;
	}
	_tcscat_s(npcap_dir, 512, _T("\\Npcap"));
	if (SetDllDirectory(npcap_dir) == 0) {
		fprintf(stderr, "Error in SetDllDirectory: %x", GetLastError());
		return FALSE;
	}
	return TRUE;
}
#endif

/*-----------------------------------------------------------------------------------------*/
// threadfuncitons for multithreading -> sniffing pn_packets (layer2)
DWORD WINAPI sniffer_thread_DCP(LPVOID lpParameter)
{
	captureDCPPackets(lpParameter);
	return 0;
}

/*-----------------------------------------------------------------------------------------*/

// threadfuncitons for multithreading -> sniffing IP packets (layer3)
DWORD WINAPI sniffer_thread_IP(LPVOID lpParameter)
{
	captureIPPackets(lpParameter);
	return 0;
}

/*-----------------------------------------------------------------------------------------*/

