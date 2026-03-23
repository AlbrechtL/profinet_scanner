#include "common.h"

#ifdef _WIN32

#include "threading.h"

typedef int (*pcap_findalldevs_fn)(pcap_if_t **, char *);
typedef void (*pcap_freealldevs_fn)(pcap_if_t *);
typedef pcap_t *(*pcap_open_live_fn)(const char *, int, int, int, char *);
typedef int (*pcap_datalink_fn)(pcap_t *);
typedef int (*pcap_compile_fn)(pcap_t *, struct bpf_program *, const char *, int, bpf_u_int32);
typedef int (*pcap_setfilter_fn)(pcap_t *, struct bpf_program *);
typedef int (*pcap_loop_fn)(pcap_t *, int, pcap_handler, u_char *);
typedef void (*pcap_close_fn)(pcap_t *);
typedef int (*pcap_sendpacket_fn)(pcap_t *, const u_char *, int);
typedef char *(*pcap_geterr_fn)(pcap_t *);
typedef void (*pcap_breakloop_fn)(pcap_t *);

static HMODULE g_packetModule = NULL;
static HMODULE g_wpcapModule = NULL;
static bool g_npcapInitialized = false;
static char g_npcapError[PCAP_ERRBUF_SIZE] = "Npcap is not initialized";

static pcap_findalldevs_fn g_pcap_findalldevs = NULL;
static pcap_freealldevs_fn g_pcap_freealldevs = NULL;
static pcap_open_live_fn g_pcap_open_live = NULL;
static pcap_datalink_fn g_pcap_datalink = NULL;
static pcap_compile_fn g_pcap_compile = NULL;
static pcap_setfilter_fn g_pcap_setfilter = NULL;
static pcap_loop_fn g_pcap_loop = NULL;
static pcap_close_fn g_pcap_close = NULL;
static pcap_sendpacket_fn g_pcap_sendpacket = NULL;
static pcap_geterr_fn g_pcap_geterr = NULL;
static pcap_breakloop_fn g_pcap_breakloop = NULL;

static void setNpcapError(const char* message)
{
	strncpy_s(g_npcapError, sizeof(g_npcapError), message, _TRUNCATE);
}

static void setNpcapErrorWithCode(const char* prefix, DWORD errorCode)
{
	_snprintf_s(g_npcapError, sizeof(g_npcapError), _TRUNCATE, "%s (error %lu)", prefix, (unsigned long)errorCode);
}

static bool loadNpcapSymbol(FARPROC* target, const char* symbolName)
{
	*target = GetProcAddress(g_wpcapModule, symbolName);
	if (*target == NULL) {
		char message[PCAP_ERRBUF_SIZE];
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Npcap symbol %s not found", symbolName);
		setNpcapError(message);
		return false;
	}

	return true;
}

static bool ensureNpcapLoaded(void)
{
	if (g_npcapInitialized) {
		return true;
	}

	return LoadNpcapDlls() == TRUE;
}

BOOL LoadNpcapDlls(void)
{
	char systemDirectory[MAX_PATH];
	char packetPath[MAX_PATH];
	char wpcapPath[MAX_PATH];
	UINT len;

	if (g_npcapInitialized) {
		return TRUE;
	}

	len = GetSystemDirectoryA(systemDirectory, MAX_PATH);
	if (len == 0 || len >= MAX_PATH) {
		setNpcapErrorWithCode("GetSystemDirectoryA failed", GetLastError());
		return FALSE;
	}

	_snprintf_s(packetPath, sizeof(packetPath), _TRUNCATE, "%s\\Npcap\\Packet.dll", systemDirectory);
	_snprintf_s(wpcapPath, sizeof(wpcapPath), _TRUNCATE, "%s\\Npcap\\wpcap.dll", systemDirectory);

	g_packetModule = LoadLibraryA(packetPath);
	if (g_packetModule == NULL) {
		setNpcapErrorWithCode("Could not load Npcap Packet.dll", GetLastError());
		return FALSE;
	}

	g_wpcapModule = LoadLibraryA(wpcapPath);
	if (g_wpcapModule == NULL) {
		setNpcapErrorWithCode("Could not load Npcap wpcap.dll", GetLastError());
		FreeLibrary(g_packetModule);
		g_packetModule = NULL;
		return FALSE;
	}

	if (!loadNpcapSymbol((FARPROC*)&g_pcap_findalldevs, "pcap_findalldevs") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_freealldevs, "pcap_freealldevs") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_open_live, "pcap_open_live") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_datalink, "pcap_datalink") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_compile, "pcap_compile") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_setfilter, "pcap_setfilter") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_loop, "pcap_loop") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_close, "pcap_close") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_sendpacket, "pcap_sendpacket") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_geterr, "pcap_geterr") ||
		!loadNpcapSymbol((FARPROC*)&g_pcap_breakloop, "pcap_breakloop")) {
		FreeLibrary(g_wpcapModule);
		FreeLibrary(g_packetModule);
		g_wpcapModule = NULL;
		g_packetModule = NULL;
		return FALSE;
	}

	g_npcapInitialized = true;
	return TRUE;
}

int pcap_findalldevs(pcap_if_t **alldevs, char *errbuf)
{
	if (!ensureNpcapLoaded()) {
		if (alldevs) {
			*alldevs = NULL;
		}
		if (errbuf) {
			strncpy_s(errbuf, PCAP_ERRBUF_SIZE, g_npcapError, _TRUNCATE);
		}
		return -1;
	}

	return g_pcap_findalldevs(alldevs, errbuf);
}

void pcap_freealldevs(pcap_if_t *alldevs)
{
	if (!ensureNpcapLoaded() || alldevs == NULL) {
		return;
	}

	g_pcap_freealldevs(alldevs);
}

pcap_t *pcap_open_live(const char *device, int snaplen, int promisc, int to_ms, char *errbuf)
{
	if (!ensureNpcapLoaded()) {
		if (errbuf) {
			strncpy_s(errbuf, PCAP_ERRBUF_SIZE, g_npcapError, _TRUNCATE);
		}
		return NULL;
	}

	return g_pcap_open_live(device, snaplen, promisc, to_ms, errbuf);
}

int pcap_datalink(pcap_t *handle)
{
	if (!ensureNpcapLoaded()) {
		return -1;
	}

	return g_pcap_datalink(handle);
}

int pcap_compile(pcap_t *handle, struct bpf_program *program, const char *buffer, int optimize, bpf_u_int32 mask)
{
	if (!ensureNpcapLoaded()) {
		return -1;
	}

	return g_pcap_compile(handle, program, buffer, optimize, mask);
}

int pcap_setfilter(pcap_t *handle, struct bpf_program *program)
{
	if (!ensureNpcapLoaded()) {
		return -1;
	}

	return g_pcap_setfilter(handle, program);
}

int pcap_loop(pcap_t *handle, int count, pcap_handler callback, u_char *user)
{
	if (!ensureNpcapLoaded()) {
		return -1;
	}

	return g_pcap_loop(handle, count, callback, user);
}

void pcap_close(pcap_t *handle)
{
	if (!ensureNpcapLoaded() || handle == NULL) {
		return;
	}

	g_pcap_close(handle);
}

int pcap_sendpacket(pcap_t *handle, const u_char *buffer, int size)
{
	if (!ensureNpcapLoaded()) {
		return -1;
	}

	return g_pcap_sendpacket(handle, buffer, size);
}

char *pcap_geterr(pcap_t *handle)
{
	if (!ensureNpcapLoaded()) {
		return g_npcapError;
	}

	return g_pcap_geterr(handle);
}

void pcap_breakloop(pcap_t *handle)
{
	if (!ensureNpcapLoaded() || handle == NULL) {
		return;
	}

	g_pcap_breakloop(handle);
}

#endif