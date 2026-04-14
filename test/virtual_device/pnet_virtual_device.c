#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pnet_api.h>

typedef struct device_config
{
	char station_name[256];
	char ip_address[32];
	char mac_address[32];
	char port_name_1[64];
	char port_name_2[64];
	char peer_station_1[256];
	char peer_station_2[256];
	char peer_port_1[64];
	char peer_port_2[64];
} device_config_t;

typedef struct app_runtime
{
	uint32_t last_arep;
	uint8_t input_data;
	uint8_t output_data;
} app_runtime_t;

enum
{
	TEST_MOD_8_8_IDENT = 0x00000032,
	TEST_SUBMOD_CUSTOM_IDENT = 0x00000001,
	TEST_SLOT_IO = 1,
	TEST_SUBSLOT_IO = 1,
};

static volatile sig_atomic_t g_should_stop = 0;

static void on_signal(int signum)
{
	(void)signum;
	g_should_stop = 1;
}

static void copy_env(char * destination, size_t destination_size, const char * name, const char * fallback)
{
	const char * value = getenv(name);
	if (value == NULL || value[0] == '\0')
	{
		value = fallback;
	}

	snprintf(destination, destination_size, "%s", value);
}

static void load_config(device_config_t * config)
{
	memset(config, 0, sizeof(*config));
	copy_env(config->station_name, sizeof(config->station_name), "DEVICE_STATION_NAME", "pn-device");
	copy_env(config->ip_address, sizeof(config->ip_address), "DEVICE_IP_ADDRESS", "0.0.0.0");
	copy_env(config->mac_address, sizeof(config->mac_address), "DEVICE_MAC_ADDRESS", "00:00:00:00:00:00");
	copy_env(config->port_name_1, sizeof(config->port_name_1), "DEVICE_PORT_NAME_1", "port-001");
	copy_env(config->port_name_2, sizeof(config->port_name_2), "DEVICE_PORT_NAME_2", "port-002");
	copy_env(config->peer_station_1, sizeof(config->peer_station_1), "DEVICE_PEER_STATION_1", "");
	copy_env(config->peer_station_2, sizeof(config->peer_station_2), "DEVICE_PEER_STATION_2", "");
	copy_env(config->peer_port_1, sizeof(config->peer_port_1), "DEVICE_PEER_PORT_1", "");
	copy_env(config->peer_port_2, sizeof(config->peer_port_2), "DEVICE_PEER_PORT_2", "");
}

static void set_ipv4(pnet_cfg_ip_addr_t * target, const char * value)
{
	unsigned int byte1 = 0;
	unsigned int byte2 = 0;
	unsigned int byte3 = 0;
	unsigned int byte4 = 0;

	if (sscanf(value, "%u.%u.%u.%u", &byte1, &byte2, &byte3, &byte4) != 4)
	{
		return;
	}

	target->a = (uint8_t)byte1;
	target->b = (uint8_t)byte2;
	target->c = (uint8_t)byte3;
	target->d = (uint8_t)byte4;
}

static int plug_dap(pnet_t * net)
{
	if (pnet_plug_module(net, PNET_API_NO_APPLICATION_PROFILE, PNET_SLOT_DAP_IDENT, PNET_MOD_DAP_IDENT) != 0)
	{
		return -1;
	}

	if (pnet_plug_submodule(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			PNET_SLOT_DAP_IDENT,
			PNET_SUBSLOT_DAP_IDENT,
			PNET_MOD_DAP_IDENT,
			PNET_SUBMOD_DAP_IDENT,
			PNET_DIR_NO_IO,
			0,
			0) != 0)
	{
		return -1;
	}

	#if PNET_MAX_PHYSICAL_PORTS > 1
	if (pnet_plug_submodule(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			PNET_SLOT_DAP_IDENT,
			PNET_SUBSLOT_DAP_INTERFACE_1_IDENT,
			PNET_MOD_DAP_IDENT,
			PNET_SUBMOD_DAP_INTERFACE_1_IDENT,
			PNET_DIR_NO_IO,
			0,
			0) != 0)
	{
		return -1;
	}

	if (pnet_plug_submodule(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			PNET_SLOT_DAP_IDENT,
			PNET_SUBSLOT_DAP_INTERFACE_1_PORT_1_IDENT,
			PNET_MOD_DAP_IDENT,
			PNET_SUBMOD_DAP_INTERFACE_1_PORT_1_IDENT,
			PNET_DIR_NO_IO,
			0,
			0) != 0)
	{
		return -1;
	}

	if (pnet_plug_submodule(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			PNET_SLOT_DAP_IDENT,
			PNET_SUBSLOT_DAP_INTERFACE_1_PORT_2_IDENT,
			PNET_MOD_DAP_IDENT,
			PNET_SUBMOD_DAP_INTERFACE_1_PORT_2_IDENT,
			PNET_DIR_NO_IO,
			0,
			0) != 0)
	{
		return -1;
	}
	#endif

	return 0;
}

static int plug_application_profile(pnet_t * net)
{
	if (pnet_plug_module(net, PNET_API_NO_APPLICATION_PROFILE, TEST_SLOT_IO, TEST_MOD_8_8_IDENT) != 0)
	{
		return -1;
	}

	if (pnet_plug_submodule(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			TEST_SLOT_IO,
			TEST_SUBSLOT_IO,
			TEST_MOD_8_8_IDENT,
			TEST_SUBMOD_CUSTOM_IDENT,
			PNET_DIR_IO,
			1,
			1) != 0)
	{
		return -1;
	}

	return 0;
}

static int publish_initial_data(pnet_t * net, app_runtime_t * runtime)
{
	if (pnet_input_set_data_and_iops(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			TEST_SLOT_IO,
			TEST_SUBSLOT_IO,
			&runtime->input_data,
			1,
			PNET_IOXS_GOOD) != 0)
	{
		return -1;
	}

	if (pnet_output_set_iocs(
			net,
			PNET_API_NO_APPLICATION_PROFILE,
			TEST_SLOT_IO,
			TEST_SUBSLOT_IO,
			PNET_IOXS_GOOD) != 0)
	{
		return -1;
	}

	return 0;
}

static void ensure_storage_directory(void)
{
	(void)mkdir("/var/lib/pn-device", 0777);
}

static int signal_led_ind(pnet_t * net, void * arg, bool led_state)
{
	(void)net;
	(void)arg;
	fprintf(stdout, "Signal LED: %s\n", led_state ? "on" : "off");
	fflush(stdout);
	return 0;
}

static int connect_ind(pnet_t * net, void * arg, uint32_t arep, pnet_result_t * p_result)
{
	(void)net;
	app_runtime_t * runtime = (app_runtime_t *)arg;
	if (runtime != NULL)
	{
		runtime->last_arep = arep;
	}
	(void)p_result;
	return 0;
}

static int release_ind(pnet_t * net, void * arg, uint32_t arep, pnet_result_t * p_result)
{
	(void)net;
	app_runtime_t * runtime = (app_runtime_t *)arg;
	if (runtime != NULL && runtime->last_arep == arep)
	{
		runtime->last_arep = 0;
	}
	(void)arep;
	(void)p_result;
	return 0;
}

static int dcontrol_ind(
	pnet_t * net,
	void * arg,
	uint32_t arep,
	pnet_control_command_t control_command,
	pnet_result_t * p_result)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)control_command;
	(void)p_result;
	return 0;
}

static int ccontrol_cnf(pnet_t * net, void * arg, uint32_t arep, pnet_result_t * p_result)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)p_result;
	return 0;
}

static int read_ind(
	pnet_t * net,
	void * arg,
	uint32_t arep,
	uint32_t api,
	uint16_t slot,
	uint16_t subslot,
	uint16_t idx,
	uint16_t sequence_number,
	uint8_t ** pp_read_data,
	uint16_t * p_read_length,
	pnet_result_t * p_result)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)api;
	(void)slot;
	(void)subslot;
	(void)idx;
	(void)sequence_number;
	(void)pp_read_data;
	(void)p_read_length;
	(void)p_result;
	fprintf(stdout, "Read request api=%u slot=%u subslot=%u idx=%u\n", api, slot, subslot, idx);
	fflush(stdout);
	return 0;
}

static int write_ind(
	pnet_t * net,
	void * arg,
	uint32_t arep,
	uint32_t api,
	uint16_t slot,
	uint16_t subslot,
	uint16_t idx,
	uint16_t sequence_number,
	uint16_t write_length,
	const uint8_t * p_write_data,
	pnet_result_t * p_result)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)api;
	(void)slot;
	(void)subslot;
	(void)idx;
	(void)sequence_number;
	if (arg != NULL && slot == TEST_SLOT_IO && subslot == TEST_SUBSLOT_IO && write_length > 0)
	{
		((app_runtime_t *)arg)->output_data = p_write_data[0];
	}
	(void)p_result;
	return 0;
}

static int exp_module_ind(pnet_t * net, void * arg, uint32_t api, uint16_t slot, uint32_t module_ident)
{
	(void)arg;
	if (api == PNET_API_NO_APPLICATION_PROFILE && slot == TEST_SLOT_IO && module_ident == TEST_MOD_8_8_IDENT)
	{
		return pnet_plug_module(net, api, slot, module_ident);
	}

	return -1;
}

static int exp_submodule_ind(
	pnet_t * net,
	void * arg,
	uint32_t api,
	uint16_t slot,
	uint16_t subslot,
	uint32_t module_ident,
	uint32_t submodule_ident,
	const pnet_data_cfg_t * p_exp_data)
{
	(void)arg;
	if (
		api == PNET_API_NO_APPLICATION_PROFILE &&
		slot == TEST_SLOT_IO &&
		subslot == TEST_SUBSLOT_IO &&
		module_ident == TEST_MOD_8_8_IDENT &&
		submodule_ident == TEST_SUBMOD_CUSTOM_IDENT)
	{
		return pnet_plug_submodule(
			net,
			api,
			slot,
			subslot,
			module_ident,
			submodule_ident,
			p_exp_data != NULL ? p_exp_data->data_dir : PNET_DIR_IO,
			p_exp_data != NULL ? p_exp_data->insize : 1,
			p_exp_data != NULL ? p_exp_data->outsize : 1);
	}

	return -1;
}

static int new_data_status_ind(
	pnet_t * net,
	void * arg,
	uint32_t arep,
	uint32_t crep,
	uint8_t changes,
	uint8_t data_status)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)crep;
	(void)changes;
	(void)data_status;
	fprintf(stdout, "Data status update arep=%u status=0x%02x\n", arep, data_status);
	fflush(stdout);
	return 0;
}

static int alarm_ind(
	pnet_t * net,
	void * arg,
	uint32_t arep,
	const pnet_alarm_argument_t * p_alarm_argument,
	uint16_t data_len,
	uint16_t data_usi,
	const uint8_t * p_data)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)p_alarm_argument;
	(void)data_len;
	(void)data_usi;
	(void)p_data;
	return 0;
}

static int alarm_ack_cnf(pnet_t * net, void * arg, uint32_t arep, int res)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)res;
	return 0;
}

static int alarm_cnf(pnet_t * net, void * arg, uint32_t arep, const pnet_pnio_status_t * p_pnio_status)
{
	(void)net;
	(void)arg;
	(void)arep;
	(void)p_pnio_status;
	return 0;
}

static int state_ind(pnet_t * net, void * arg, uint32_t arep, pnet_event_values_t state)
{
	app_runtime_t * runtime = (app_runtime_t *)arg;

	if (runtime != NULL)
	{
		runtime->last_arep = arep;
	}

	if (state == PNET_EVENT_PRMEND)
	{
		if (runtime != NULL)
		{
			(void)publish_initial_data(net, runtime);
		}
		(void)pnet_application_ready(net, arep);
	}

	return 0;
}

int main(void)
{
	device_config_t config;
	app_runtime_t runtime = {0};
	pnet_cfg_t pnet_cfg = {0};
	pnet_t * net;
	struct timespec sleep_interval = {
		.tv_sec = 0,
		.tv_nsec = 1000 * 1000,
	};

	load_config(&config);
	ensure_storage_directory();

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	runtime.input_data = 0x11;
	runtime.output_data = 0x00;

	pnet_cfg.tick_us = 1000;
	pnet_cfg.state_cb = state_ind;
	pnet_cfg.connect_cb = connect_ind;
	pnet_cfg.release_cb = release_ind;
	pnet_cfg.dcontrol_cb = dcontrol_ind;
	pnet_cfg.ccontrol_cb = ccontrol_cnf;
	pnet_cfg.read_cb = read_ind;
	pnet_cfg.write_cb = write_ind;
	pnet_cfg.exp_module_cb = exp_module_ind;
	pnet_cfg.exp_submodule_cb = exp_submodule_ind;
	pnet_cfg.new_data_status_cb = new_data_status_ind;
	pnet_cfg.alarm_ind_cb = alarm_ind;
	pnet_cfg.alarm_cnf_cb = alarm_cnf;
	pnet_cfg.alarm_ack_cnf_cb = alarm_ack_cnf;
	pnet_cfg.signal_led_cb = signal_led_ind;
	pnet_cfg.cb_arg = &runtime;
	pnet_cfg.device_id.vendor_id_hi = 0xfe;
	pnet_cfg.device_id.vendor_id_lo = 0xed;
	pnet_cfg.device_id.device_id_hi = 0xbe;
	pnet_cfg.device_id.device_id_lo = 0xef;
	pnet_cfg.oem_device_id = pnet_cfg.device_id;
	pnet_cfg.if_cfg.main_netif_name = "eth0";
	pnet_cfg.num_physical_ports = (PNET_MAX_PHYSICAL_PORTS > 1) ? 2 : 1;
	pnet_cfg.min_device_interval = 32;
	pnet_cfg.send_hello = true;

	snprintf(pnet_cfg.station_name, sizeof(pnet_cfg.station_name), "%s", config.station_name);
	snprintf(pnet_cfg.product_name, sizeof(pnet_cfg.product_name), "%s", "pn-scanner virtual test device");
	snprintf(pnet_cfg.file_directory, sizeof(pnet_cfg.file_directory), "%s", "/var/lib/pn-device");
	pnet_cfg.if_cfg.physical_ports[0].netif_name = "eth0";
	#if PNET_MAX_PHYSICAL_PORTS > 1
	pnet_cfg.if_cfg.physical_ports[1].netif_name = "eth0";
	#endif
	pnet_cfg.if_cfg.ip_cfg.dhcp_enable = false;
	set_ipv4(&pnet_cfg.if_cfg.ip_cfg.ip_addr, config.ip_address);
	set_ipv4(&pnet_cfg.if_cfg.ip_cfg.ip_mask, "255.255.255.0");
	set_ipv4(&pnet_cfg.if_cfg.ip_cfg.ip_gateway, "0.0.0.0");
	pnet_cfg.im_0_data.im_vendor_id_hi = 0xfe;
	pnet_cfg.im_0_data.im_vendor_id_lo = 0xed;
	snprintf(pnet_cfg.im_0_data.im_order_id, sizeof(pnet_cfg.im_0_data.im_order_id), "%s", "PN-SCANNER-VIRT");
	snprintf(pnet_cfg.im_0_data.im_serial_number, sizeof(pnet_cfg.im_0_data.im_serial_number), "%s", config.station_name);
	pnet_cfg.im_0_data.im_hardware_revision = 1;
	pnet_cfg.im_0_data.im_sw_revision_prefix = 'V';
	pnet_cfg.im_0_data.im_sw_revision_functional_enhancement = 1;
	pnet_cfg.im_0_data.im_sw_revision_bug_fix = 0;
	pnet_cfg.im_0_data.im_sw_revision_internal_change = 0;
	pnet_cfg.im_0_data.im_revision_counter = 1;
	pnet_cfg.im_0_data.im_profile_id = 0;
	pnet_cfg.im_0_data.im_profile_specific_type = 0;
	pnet_cfg.im_0_data.im_version_major = 1;
	pnet_cfg.im_0_data.im_version_minor = 0;

	fprintf(stdout, "Starting virtual PROFINET device %s on %s (%s)\n", config.station_name, config.ip_address, config.mac_address);
	fprintf(stdout, "Port 1 peer: %s/%s\n", config.peer_station_1, config.peer_port_1);
	fprintf(stdout, "Port 2 peer: %s/%s\n", config.peer_station_2, config.peer_port_2);
	fflush(stdout);

	net = pnet_init(&pnet_cfg);
	if (net == NULL)
	{
		fprintf(stderr, "pnet_init() failed\n");
		return 1;
	}

	if (plug_dap(net) != 0)
	{
		fprintf(stderr, "failed to plug the default DAP structure\n");
		return 1;
	}

	if (plug_application_profile(net) != 0)
	{
		fprintf(stderr, "failed to plug the virtual IO module\n");
		return 1;
	}

	if (publish_initial_data(net, &runtime) != 0)
	{
		fprintf(stderr, "initial IO data not accepted yet; waiting for controller connection\n");
	}

	while (!g_should_stop)
	{
		runtime.input_data++;
		(void)publish_initial_data(net, &runtime);
		pnet_handle_periodic(net);
		nanosleep(&sleep_interval, NULL);
	}

	return 0;
}