// @author Stefan Eiwanger


// main.c : Defines the entry point for the console application.
// get a list of adapters -> get user input for adapter, layer and if necessary ip address
// start listeners and send packets
// get user input for path to store the data
// free the data
// end



#include "common.h"

#include "deviceHandler.h"
#include "linkedList.h"
#include "packetHandler.h"
#include "remoteScan.h"
#include "threading.h"




// identification number of ip protocol
unsigned short identnmb = 0;

typedef struct cli_options {
	bool hasInterface;
	bool hasMode;
	bool hasTarget;
	bool hasDuration;
	int mode;
	long durationSeconds;
	char interfaceValue[256];
	char targetValue[32];
} cli_options_t;

static int shouldPrintDcpOutput(int mode)
{
	return mode == 0;
}

static int shouldPrintRpcOutput(int mode)
{
	return mode == 1;
}

static int shouldPrintTopologyOutput(int mode)
{
	return mode == 2;
}

typedef struct topology_edge {
	const char* leftLabel;
	const char* rightLabel;
	const char* leftPort;
	const char* rightPort;
	mac_address leftMac;
	mac_address rightMac;
} topology_edge_t;

typedef struct topology_node {
	const char* label;
	mac_address mac;
	int degree;
} topology_node_t;

static void formatMacAddress(mac_address mac, char* buffer, size_t bufferSize)
{
	if (!buffer || bufferSize == 0) {
		return;
	}

	snprintf(buffer, bufferSize, "%02x:%02x:%02x:%02x:%02x:%02x",
		mac.byte1 & 0xFF,
		mac.byte2 & 0xFF,
		mac.byte3 & 0xFF,
		mac.byte4 & 0xFF,
		mac.byte5 & 0xFF,
		mac.byte6 & 0xFF);
}

static linked_list_t* findDeviceByMac(linked_list_t* list, mac_address mac)
{
	for (linked_list_t* current = list; current != NULL; current = current->next) {
		if (current->device && compareMacAddress(current->device->deviceMACaddress, mac)) {
			return current;
		}
	}

	return NULL;
}

static const char* getDeviceLabel(const datasheet* device, char* fallbackBuffer, size_t fallbackBufferSize)
{
	if (!device) {
		return "unknown device";
	}

	if (device->nameOfStation && device->nameOfStation[0] != '\0') {
		return device->nameOfStation;
	}

	if (device->deviceType && device->deviceType[0] != '\0') {
		return device->deviceType;
	}

	formatMacAddress(device->deviceMACaddress, fallbackBuffer, fallbackBufferSize);
	return fallbackBuffer;
}

static int sameMacAddress(mac_address left, mac_address right)
{
	return compareMacAddress(left, right);
}

static int sameOptionalPort(const char* left, const char* right)
{
	if (!left && !right) {
		return 1;
	}

	if (!left || !right) {
		return 0;
	}

	return strcmp(left, right) == 0;
}

static int edgeMatches(const topology_edge_t* edge,
	mac_address leftMac,
	const char* leftPort,
	mac_address rightMac,
	const char* rightPort)
{
	if (!edge) {
		return 0;
	}

	if (sameMacAddress(edge->leftMac, leftMac) &&
		sameMacAddress(edge->rightMac, rightMac) &&
		sameOptionalPort(edge->leftPort, leftPort) &&
		sameOptionalPort(edge->rightPort, rightPort)) {
		return 1;
	}

	if (sameMacAddress(edge->leftMac, rightMac) &&
		sameMacAddress(edge->rightMac, leftMac) &&
		sameOptionalPort(edge->leftPort, rightPort) &&
		sameOptionalPort(edge->rightPort, leftPort)) {
		return 1;
	}

	return 0;
}

static int appendTopologyEdge(topology_edge_t* edges,
	int edgeCapacity,
	int edgeCount,
	const char* leftLabel,
	const char* rightLabel,
	const char* leftPort,
	const char* rightPort,
	mac_address leftMac,
	mac_address rightMac)
{
	if (edgeCount >= edgeCapacity) {
		return edgeCount;
	}

	for (int index = 0; index < edgeCount; index++) {
		if (edgeMatches(&edges[index], leftMac, leftPort, rightMac, rightPort)) {
			return edgeCount;
		}
	}

	edges[edgeCount].leftLabel = leftLabel;
	edges[edgeCount].rightLabel = rightLabel;
	edges[edgeCount].leftPort = leftPort;
	edges[edgeCount].rightPort = rightPort;
	edges[edgeCount].leftMac = leftMac;
	edges[edgeCount].rightMac = rightMac;
	return edgeCount + 1;
}

static int findTopologyNodeIndex(const topology_node_t* nodes, int nodeCount, mac_address mac)
{
	for (int index = 0; index < nodeCount; index++) {
		if (sameMacAddress(nodes[index].mac, mac)) {
			return index;
		}
	}

	return -1;
}

static int appendTopologyNode(topology_node_t* nodes,
	int nodeCapacity,
	int nodeCount,
	const char* label,
	mac_address mac)
{
	int existingIndex = findTopologyNodeIndex(nodes, nodeCount, mac);
	if (existingIndex >= 0) {
		return nodeCount;
	}

	if (nodeCount >= nodeCapacity) {
		return nodeCount;
	}

	nodes[nodeCount].label = label;
	nodes[nodeCount].mac = mac;
	nodes[nodeCount].degree = 0;
	return nodeCount + 1;
}

static int findEdgeForNode(const topology_edge_t* edges, int edgeCount, int nodeIndex, const topology_node_t* nodes, const int* visited)
{
	for (int edgeIndex = 0; edgeIndex < edgeCount; edgeIndex++) {
		if (visited[edgeIndex]) {
			continue;
		}

		if (sameMacAddress(edges[edgeIndex].leftMac, nodes[nodeIndex].mac) ||
			sameMacAddress(edges[edgeIndex].rightMac, nodes[nodeIndex].mac)) {
			return edgeIndex;
		}
	}

	return -1;
}

static void printTopologyEdgeList(const topology_edge_t* edges, int edgeCount)
{
	printf("Topology links:\n");
	for (int index = 0; index < edgeCount; index++) {
		printf("  %s", edges[index].leftLabel);
		if (edges[index].leftPort) {
			printf(" [%s]", edges[index].leftPort);
		}
		printf(" <-> %s", edges[index].rightLabel);
		if (edges[index].rightPort) {
			printf(" [%s]", edges[index].rightPort);
		}
		printf("\n");
	}
	printf("\n");
}

static void printResolvedTopology(const threadData_t* threadData)
{
	int deviceCount;
	int edgeCount = 0;
	int nodeCount = 0;
	topology_edge_t* edges;
	topology_node_t* nodes;
	int* visited;
	int startIndex = -1;
	int currentNodeIndex;
	int printedEdges = 0;

	if (!threadData || !threadData->first) {
		printf("No topology links resolved.\n\n");
		return;
	}

	deviceCount = linkedlist_status(threadData->first);
	if (deviceCount <= 0) {
		printf("No topology links resolved.\n\n");
		return;
	}

	edges = calloc((size_t)(deviceCount * 8), sizeof(topology_edge_t));
	nodes = calloc((size_t)(deviceCount * 2), sizeof(topology_node_t));
	visited = calloc((size_t)(deviceCount * 8), sizeof(int));
	if (!edges || !nodes || !visited) {
		printf("No topology links resolved.\n\n");
		free(edges);
		free(nodes);
		free(visited);
		return;
	}

	for (linked_list_t* current = threadData->first; current != NULL; current = current->next) {
		linkedList_slot* slot;
		char deviceFallback[18];

		if (!current->device) {
			continue;
		}

		for (slot = current->device->slotList; slot != NULL; slot = slot->next) {
			for (linkedList_subslot* subslot = slot->subslotList; subslot != NULL; subslot = subslot->next) {
				if (!subslot->ownPortID || !subslot->peerMacAddress) {
					continue;
				}

				linked_list_t* peerDevice = findDeviceByMac(threadData->first, *subslot->peerMacAddress);
				char peerFallback[18];
				char peerMacString[18];
				const char* peerLabel;
				const char* leftLabel = getDeviceLabel(current->device, deviceFallback, sizeof(deviceFallback));
				const char* peerPort = NULL;

				formatMacAddress(*subslot->peerMacAddress, peerMacString, sizeof(peerMacString));

				if (peerDevice && peerDevice->device) {
					peerLabel = getDeviceLabel(peerDevice->device, peerFallback, sizeof(peerFallback));
				} else if (subslot->peerChassisID && subslot->peerChassisID[0] != '\0') {
					peerLabel = (char*)subslot->peerChassisID;
				} else {
					peerLabel = peerMacString;
				}

				if (subslot->peerPortID && subslot->peerPortID[0] != '\0') {
					peerPort = (char*)subslot->peerPortID;
				}

				edgeCount = appendTopologyEdge(edges,
					deviceCount * 8,
					edgeCount,
					leftLabel,
					peerLabel,
					(char*)subslot->ownPortID,
					peerPort,
					current->device->deviceMACaddress,
					*subslot->peerMacAddress);

				nodeCount = appendTopologyNode(nodes,
					deviceCount * 2,
					nodeCount,
					leftLabel,
					current->device->deviceMACaddress);
				nodeCount = appendTopologyNode(nodes,
					deviceCount * 2,
					nodeCount,
					peerLabel,
					*subslot->peerMacAddress);
			}
		}
	}

	if (edgeCount == 0) {
		printf("  No port-to-port links could be resolved from the current scan.\n");
		printf("\n");
		free(edges);
		free(nodes);
		free(visited);
		return;
	}

	for (int edgeIndex = 0; edgeIndex < edgeCount; edgeIndex++) {
		int leftIndex = findTopologyNodeIndex(nodes, nodeCount, edges[edgeIndex].leftMac);
		int rightIndex = findTopologyNodeIndex(nodes, nodeCount, edges[edgeIndex].rightMac);
		if (leftIndex >= 0) {
			nodes[leftIndex].degree++;
		}
		if (rightIndex >= 0) {
			nodes[rightIndex].degree++;
		}
	}

	for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++) {
		if (nodes[nodeIndex].degree == 1) {
			startIndex = nodeIndex;
			break;
		}
	}

	if (startIndex < 0) {
		printTopologyEdgeList(edges, edgeCount);
		free(edges);
		free(nodes);
		free(visited);
		return;
	}

	currentNodeIndex = startIndex;
	printf("Topology chain:\n  %s", nodes[currentNodeIndex].label);
	while (printedEdges < edgeCount) {
		int edgeIndex = findEdgeForNode(edges, edgeCount, currentNodeIndex, nodes, visited);
		int nextNodeIndex;
		const char* currentPort;
		const char* nextPort;

		if (edgeIndex < 0) {
			break;
		}

		visited[edgeIndex] = 1;
		printedEdges++;

		if (sameMacAddress(edges[edgeIndex].leftMac, nodes[currentNodeIndex].mac)) {
			nextNodeIndex = findTopologyNodeIndex(nodes, nodeCount, edges[edgeIndex].rightMac);
			currentPort = edges[edgeIndex].leftPort;
			nextPort = edges[edgeIndex].rightPort;
		} else {
			nextNodeIndex = findTopologyNodeIndex(nodes, nodeCount, edges[edgeIndex].leftMac);
			currentPort = edges[edgeIndex].rightPort;
			nextPort = edges[edgeIndex].leftPort;
		}

		printf(" --[%s <-> %s]-- %s",
			currentPort ? currentPort : "?",
			nextPort ? nextPort : "?",
			nextNodeIndex >= 0 ? nodes[nextNodeIndex].label : "unknown device");

		if (nextNodeIndex < 0) {
			break;
		}

		currentNodeIndex = nextNodeIndex;
	}
	printf("\n");
	printf("\n");

	free(edges);
	free(nodes);
	free(visited);
}

static void printTopologyToStdout(const threadData_t* threadData)
{
	printf("\nTopology results (stdout):\n\n");
	printf("Topology source: PROFINET RPC peer data.\n\n");
	printResolvedTopology(threadData);
}

static void printHelp(const char* programName)
{
	printf("Usage:\n");
	printf("  %s --help\n", programName);
	printf("  %s --interface <name> --mode <local|remote|topology> [--target <a.b.c.d[-e]>]\n", programName);
	printf("  %s --interface <name> --mode <local|remote|topology> [--target <a.b.c.d[-e]>] [--duration <seconds>]\n", programName);
	printf("\n");

	printf("Options:\n");
	printf("  --help               Show this help message and exit.\n");
	printf("  --interface VALUE    Interface name (for example eth0 or enp0s31f6).\n");
	printf("  --mode VALUE         Scan mode: local (DCP), remote (DCE/RPC), or topology (DCP + RPC peer links).\n");
	printf("  --target VALUE       Remote target IP or range in the form a.b.c.d or a.b.c.d-e.\n");
	printf("                       Required when --mode remote is used.\n");
	printf("  --duration SECONDS   Stop capture after the given number of seconds.\n");
	printf("\n");

	printf("Examples:\n");
	printf("  %s --interface eth0 --mode local\n", programName);
	printf("  %s --interface eth0 --mode remote --target 192.168.0.10-20\n", programName);
	printf("  %s --interface eth0 --mode topology --duration 10\n", programName);
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

static int resolveInterfaceIndex(threadData_t* threadData, const char* interfaceValue)
{
	if (!interfaceValue || interfaceValue[0] == '\0') {
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

	if (argc == 1) {
		printHelp(argv[0]);
		return 0;
	}

	for (int argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
		if (strcmp(argv[argumentIndex], "--help") == 0) {
			printHelp(argv[0]);
			return 0;
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
			} else if (strcmp(modeArg, "topology") == 0) {
				options.mode = 2;
			} else {
				printf("Invalid --mode value: %s (use local, remote, or topology)\n", modeArg);
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

	if (!options.hasInterface) {
		printf("Missing required option --interface\n");
		return -1;
	}
	if (!options.hasMode) {
		printf("Missing required option --mode\n");
		return -1;
	}
	if (options.mode == 1 && !options.hasTarget) {
		printf("Missing required option --target for remote mode\n");
		return -1;
	}

	// check if on windows, if so then load the npcap library
#ifdef _WIN32
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

	if (options.hasInterface) {
		inum = resolveInterfaceIndex(threadData, options.interfaceValue);
		if (inum < 1 || inum > threadData->numberOfAdapters) {
			printf_s("\nUnknown interface name: %s\n", options.interfaceValue);
			pcap_freealldevs(threadData->alldevs);
			free(threadData);
			return -1;
		}
	} else {
		printf("Missing required option --interface\n");
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
	} else {
		printf("Missing required option --mode\n");
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}

	if (mode != 0 && mode != 1 && mode != 2)
	{
		printf("Invalid mode value. Use --mode local|remote|topology.\n");
		pcap_freealldevs(threadData->alldevs);
		free(threadData);
		return -1;
	}

	g_scanOutputMode = mode;


	if (mode == 1)
	{
		// insert target ip address   
		char targetIP[4 * 3 + 3 + 1 + 1 + 3]; // 4*3 numbers, 3 dots and 1 \0
		targetIP[0] = 0;	// first init to zero

		if (options.hasTarget) {
			strcpy_s(targetIP, sizeof(targetIP), options.targetValue);
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
			return -1; // false IP
		}

		int l = 0;

		// we have all necessary information, start the scanning

		HANDLE sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC lookup endpointmapper first call \n");
		}
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
			return -1;
		}
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nRPC lookup first call finished \n\n");
			printf_s("\nSend RPC lookup endpointmapper second call \n");
		}
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


		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nRPC lookup second call finished \n\n");
			printf_s("\nSend RPC implicit read PDRealData \n");
		}
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


		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nPDRealData call finished \n\n");
			printf_s("\nSend RPC implicit read realidentificationdata \n");
		}
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

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nRealIdentificationData call finished \n\n");
		}


		// send a request for each slot and subslot to get the data out of them
		sniffThreadrem = CreateThread(NULL, 0, sniffer_thread_remote, threadData, 0, lpExitCode);
		linked_list_t* list = threadData->first;
		slotParameter slotpara;
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC implicit read PDRealData for one subslot \n");
		}

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
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nPDRealData for each subslot call finished \n\n");
		}

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC implicit read I&M data for each module \n");
		}
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

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nI&M data call for each module finished \n\n");
		}

	}
	else
	{

		HANDLE sniffThread = CreateThread(NULL, 0, sniffer_thread_DCP, threadData, 0, lpExitCode);


		if (shouldPrintDcpOutput(mode)) {
			printf_s("\nSend pn_dcp \n");
		}
		sendPacket_DCP(threadData);

		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}
		// every time a packet is recieved the timer of pcap_next_ex is restored to TIMEOUT seconds, if the TIMEOUT seconds are over the function returns 0
		if (shouldPrintDcpOutput(mode)) {
			printf_s("\npn_dcp finished\n\n");
		}
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
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC lookup endpointmapper first call\n");
		}

		for (int k = 0; k < deviceCount; k++){
			threadData->numberOfIPDev = k;
			sendPacket_RPC(threadData);
		}

		WaitForSingleObject(sniffThread, INFINITE);
		if (g_scanStopRequested) {
			printf_s("\nScan duration reached; stopping early.\n");
			goto finalize;
		}

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nRPC lookup first call finished \n\n");
		}

		// again sniff against IP-RPC
		// this time the intern break should stop the loop
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC lookup endpointmapper second call \n");
		}
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
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nRPC lookup second call finished \n\n");
		}


		// reuse remote handler
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC implicit read realidentificationdata \n");
		}
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
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC implicit read PDRealData for one subslot \n");
		}

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
		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nPDRealData for each subslot call finished \n\n");
		}


		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nSend RPC implicit read I&M data for each module \n");
		}
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

		if (shouldPrintRpcOutput(mode)) {
			printf_s("\nI&M data call for each module finished \n\n");
		}


	}



	// free list of devices
finalize:
	pcap_freealldevs(threadData->alldevs);

	if (shouldPrintTopologyOutput(mode)) {
		printTopologyToStdout(threadData);
	}
	else {
		printResultsToStdout(threadData->first);
	}

	empty_list(threadData->first);
	free(threadData);

	return 0;
}

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

