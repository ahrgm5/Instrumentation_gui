#pragma once

#include "instrument.hpp"
#include <string>
#include <cstdint>
#include <vector>

struct ZaberAxisLimits {
    int index;
    int32_t minPosition;
    int32_t maxPosition;
    int32_t resolution;
};

class Zaber : public Instrument {
public:
    static ChannelSettings defaultZaberSettings();

    explicit Zaber(ViSession defaultRM);
    Zaber(
        ViSession defaultRM,
        const std::string& resourceName,
        const ChannelSettings& settings = defaultZaberSettings()
    );

    ~Zaber() override = default;

    ChannelSettings defaultSettings() const override {
        return defaultZaberSettings();
    }

    virtual bool open(const std::string& resourceName, const ChannelSettings& settings = defaultZaberSettings()) {
        return Instrument::open(resourceName, settings);
    }

    // Direct Motion & Query Commands
    void home(int index);
    void moveAbsolute(int index, int32_t microsteps);
    void moveRelative(int index, int32_t microsteps);
    void stop(int index);

    // Queries position for all devices (index 0) and displays output in terminal
    int32_t getPosition(int numDevices = 4);

    // Queries position for a specific device index
    int32_t getPositionForDevice(int index);

    // Motion Synchronization Helpers
    bool isIdle(int index);
    void waitUntilIdle(int index, int pollIntervalMs = 100, int timeoutSeconds = 30);

    // Settings & Limits Queries
    int32_t getLimitMax(int index);
    int32_t getLimitMin(int index);
    int32_t getResolution(int index);

    std::vector<ZaberAxisLimits> queryLimitsForAllDevices(int numDevices);

private:
    std::string sendCommand(int index, const std::string& command);
    int32_t parseSettingResponse(const std::string& response);
};