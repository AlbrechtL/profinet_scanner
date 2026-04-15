#include "Profinet.h"
#include "gsdmltools.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace
{
constexpr uint32_t kModuleId = 0x00000040;
constexpr uint16_t kSubmoduleId = 0x00000140;
constexpr uint8_t kOutputIncrement = 1;

std::string get_env_or(const char * name, const std::string & fallback)
{
    const char * value = std::getenv(name);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }
    return value;
}

class VirtualDeviceApp
{
public:
    bool Initialize()
    {
        auto & device = profinet_.GetDevice();
        auto & device_props = device.properties;

        station_name_ = get_env_or("DEVICE_STATION_NAME", "pn-device");
        main_interface_ = get_env_or("DEVICE_INTERFACE", "eth0");
        storage_dir_ = get_env_or("DEVICE_STORAGE_DIR", "/var/lib/pn-device");

        std::filesystem::create_directories(storage_dir_);

        device_props.vendorName = "profinet_scanner";
        device_props.vendorID = 0x0493;
        device_props.deviceID = 0x1001;
        device_props.deviceName = "VirtualProfippDevice";
        device_props.deviceInfoText = "Virtual PROFINET device for profinet_scanner integration tests.";
        device_props.deviceProductFamily = "test";
        device_props.stationName = station_name_;
        device_props.numSlots = 1;
        device_props.serialNumber = station_name_;
        device_props.orderID = "pn-scanner-profipp";
        device_props.productName = "PN Scanner Virtual Device";
        device_props.minDeviceInterval = 32;
        device_props.defaultMautype = 0x10;
        device_props.swRevMajor = 0;
        device_props.swRevMinor = 1;
        device_props.swRevPatch = 0;
        device_props.hwRevMajor = 1;
        device_props.hwRevMinor = 0;

        auto module_with_plug_info = device.modules.Create(kModuleId, 1);
        if (module_with_plug_info == nullptr)
        {
            std::cerr << "failed to create profipp test module\n";
            return false;
        }

        auto & module = module_with_plug_info->module;
        module.properties.name = "Scanner Test Module";
        module.properties.infoText = "Single-slot module for scanner integration tests.";

        auto * submodule = module.submodules.Create(kSubmoduleId);
        if (submodule == nullptr)
        {
            std::cerr << "failed to create profipp test submodule\n";
            return false;
        }

        submodule->properties.name = "Scanner Test Submodule";
        submodule->properties.infoText = "Exposes one byte of cyclic IO for baseline controller compatibility.";
        submodule->inputs.Create<uint8_t, sizeof(uint8_t)>(
            [this](uint8_t value)
            {
                last_input_ = value;
                last_output_ = static_cast<uint8_t>(value + kOutputIncrement);
            });
        submodule->outputs.Create<uint8_t, sizeof(uint8_t)>(
            [this]() -> uint8_t
            {
                return last_output_;
            });

        auto & profinet_props = profinet_.GetProperties();
        profinet_props.pathStorageDirectory = storage_dir_;
        profinet_props.mainNetworkInterface = main_interface_;
        profinet_props.networkInterfaces = {main_interface_};

        return true;
    }

    bool Start()
    {
        profinet_instance_ = profinet_.Initialize();
        if (!profinet_instance_)
        {
            std::cerr << "failed to initialize profipp runtime\n";
            return false;
        }

        return profinet_instance_->Start();
    }

    [[noreturn]] void RunForever() const
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    profinet::Profinet profinet_;
    std::unique_ptr<profinet::ProfinetControl> profinet_instance_;
    std::string station_name_;
    std::string main_interface_;
    std::string storage_dir_;
    uint8_t last_input_ = 0;
    uint8_t last_output_ = 0;
};

} // namespace

int main()
{
    VirtualDeviceApp app;
    if (!app.Initialize())
    {
        return 1;
    }

    if (!app.Start())
    {
        return 1;
    }

    app.RunForever();
}