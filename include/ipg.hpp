#pragma once
#include <instrument.hpp>
#include <string>

class ipgYLR : public Instrument {
public:
    static ChannelSettings defaultSettings();

    explicit ipgYLR(ViSession defaultRM);
    ipgYLR(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = defaultSettings());

    ~ipgYLR() override = default;

    // Override write to enforce Carriage Return (\r) termination for YLR sockets
    void write(const std::string& command) override;

    // YLR-Series Specific Commands
    std::string getIdentity();
    std::string setIpAddress(const std::string& newIp);

    void setEmission(bool enable);
    bool isEmissionEnabled();
    std::string getStatus();
};
