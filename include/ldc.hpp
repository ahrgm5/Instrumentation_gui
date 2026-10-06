#pragma once
#include "instrument.hpp"
#include <string>

class Ldc502 : public Instrument {
public:
    // Override the base class virtual defaultSettings() function
    ChannelSettings defaultSettings() const override;

    explicit Ldc502(ViSession defaultRM);
    Ldc502(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = {});

    ~Ldc502() override = default;

    // SCPI Identification
    std::string getIdentity();

    // Laser Diode Current / Power Controls
    void setLaserOutput(bool enable);
    bool isLaserOutputEnabled();

    void setCurrentSetpoint(double currentmA);
    double getCurrentSetpoint();
    double getActualCurrent();

    // Status / Error Handling
    std::string getStatus();
    std::string getError();
};