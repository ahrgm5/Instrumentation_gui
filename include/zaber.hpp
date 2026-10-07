#pragma once

#include "instrument.hpp"
#include "imotoraxis.hpp"
#include <string>
#include <cstdint>
#include <memory>

class Zaber : public Instrument {
public:
    static ChannelSettings defaultZaberSettings();

    explicit Zaber(ViSession defaultRM);
        Zaber(ViSession defaultRM,  const std::string& resourceName,    const ChannelSettings& settings = defaultZaberSettings()
        );

    ~Zaber() override = default;

    ChannelSettings defaultSettings() const override {
        return defaultZaberSettings();
    }

    bool open(const std::string& resourceName, const ChannelSettings& settings = defaultZaberSettings()) override;

    // Direct Motion & Query Commands
    void home(int deviceAddress, int axisNumber = 1);
    void moveAbsolute(int deviceAddress, int axisNumber, int32_t microsteps);
    void moveRelative(int deviceAddress, int axisNumber, int32_t microsteps);
    void stop(int deviceAddress, int axisNumber = 1);

    // Queries position for all devices (index 0) and displays output in terminal
    int32_t getPosition(int numDevices = 4);

    // Queries position for a specific device index
    int32_t getPositionForDevice(int index);

    // Queries position for a specific device address and explicit axis number
    int32_t getPositionForAxis(int deviceAddress, int axisNumber);

    // Motion Synchronization Helpers
    bool isIdle(int deviceAddress, int axisNumber = 1);
    void waitUntilIdle(int deviceAddress, int axisNumber = 1, int pollIntervalMs = 100, int timeoutSeconds = 30);

    // Settings & Limits Queries
    int32_t getLimitMax(int deviceAddress, int axisNumber = 1);
    int32_t getLimitMin(int deviceAddress, int axisNumber = 1);
    int32_t getResolution(int deviceAddress, int axisNumber = 1);

private:
    std::string sendCommand(int deviceAddress, const std::string& command);
    void writeCommand(int deviceAddress, const std::string& command);
    int32_t parseSettingResponse(const std::string& response);
};

// --- Zaber Motor Adapter ---
class ZaberAdapter : public IMotorAxis {
public:
    explicit ZaberAdapter(std::shared_ptr<Zaber> zaber);

    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    void stop(int axis) override;
    void setSpeed(int axis, int speed) override;
    void indexIncremental(int axis, int steps) override;
    int32_t getPosition(int axis) override;

private:
    std::shared_ptr<Zaber> m_zaber;
};