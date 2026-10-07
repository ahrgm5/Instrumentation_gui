#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include <visa.h>
#include <instrument.hpp>
#include <imotoraxis.hpp>

class VelmexVXM : public Instrument {
public:
    static ChannelSettings defaultVelmexSettings();

    explicit VelmexVXM(ViSession defaultRM);
    VelmexVXM(ViSession defaultRM,  const std::string& resourceName,    const ChannelSettings& settings = defaultVelmexSettings() );

    ~VelmexVXM() override = default;

    ChannelSettings defaultSettings() const override { return defaultVelmexSettings();    }

    bool open(const std::string& resourceName, const ChannelSettings& settings = defaultVelmexSettings()) override;

    void writeCommand(const std::string& cmd);
    std::string readResponse();

    void write(const std::string& command) override;

    void clearMemory();
    void setSpeed(int motor, int speed, bool fullPower = false);
    void indexIncremental(int motor, int steps);
    std::string run();

    // Query motor position (Added for VelmexVXM)
    int32_t getPosition(int motor);
};

// --- Velmex Motor Adapter ---
class VelmexAdapter : public IMotorAxis {
public:
    explicit VelmexAdapter(std::shared_ptr<VelmexVXM> velmex);

    void moveRelative(int axis, int distance) override;
    void moveAbsolute(int axis, int position) override;
    void stop(int axis) override;
    void setSpeed(int axis, int speed) override;
    void indexIncremental(int axis, int steps) override;

    // Must be explicitly declared here to satisfy IMotorAxis interface
    int32_t getPosition(int axis) override;

private:
    std::shared_ptr<VelmexVXM> m_velmex;
};