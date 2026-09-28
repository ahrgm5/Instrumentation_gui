#pragma once

#include <instrument.hpp>
#include <string>
#include <cstdint>
#include <utility>

// Defines a specific motor target (Supports both MCA channels and DMQ standalone stages)
struct AxisTarget {
    uint8_t device = 1; // Device address on the daisy chain
    uint8_t axis = 0; // 0 for standalone (DMQ), 1..N for multi-axis controller (MCA)
};

// Example Configuration:
// - Motors X1, X2, Y driven by 3-channel MCA (Device 1, Axes 1, 2, 3)
// - Motor Z is a standalone DMQ stage (Device 2)
struct GantryConfig {
    AxisTarget x1{ 1, 1 }; // MCA Axis 1
    AxisTarget x2{ 1, 2 }; // MCA Axis 2
    AxisTarget y{ 1, 3 }; // MCA Axis 3
    AxisTarget z{ 2, 0 }; // DMQ Stage (Standalone Device 2)
};

class Zaber : public Instrument {
public:
    explicit Zaber(ViSession defaultRM);
    Zaber(ViSession defaultRM, const std::string& resourceName, ViUInt32 timeoutMs = 5000);
    Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings);

    bool open(const std::string& resourceName, ViUInt32 timeoutMs = 5000) override;
    bool open(const std::string& resourceName, const ChannelSettings& settings) override;

    // Target-based Commands (Works for MCA and DMQ seamlessly)
    void home(const AxisTarget& target);
    void moveAbsolute(int32_t microsteps, const AxisTarget& target);
    void moveRelative(int32_t microsteps, const AxisTarget& target);
    void stop(const AxisTarget& target);
    int32_t getPosition(const AxisTarget& target);

    // 4-Motor System High-Level API
    void setGantryConfig(const GantryConfig& config);
    void homeAll();
    void moveX(int32_t microsteps); // Synchronized dual X move (X1 + X2)
    void moveY(int32_t microsteps);
    void moveZ(int32_t microsteps);

    std::pair<int32_t, int32_t> getXPositions();

    // Raw ASCII Dispatcher
    std::string sendCommand(uint8_t deviceAddress, const std::string& command, uint8_t axis = 0);

private:
    GantryConfig m_cfg;
    static ChannelSettings getDefaultZaberSettings(ViUInt32 timeoutMs);
    std::string formatCommand(uint8_t deviceAddress, uint8_t axis, const std::string& command) const;
};