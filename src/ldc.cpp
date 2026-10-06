#include <ldc.hpp>
#include <visa.h>


ChannelSettings Ldc502::defaultSettings() const {
    ChannelSettings settings;
    settings.timeoutMs = 3000;
    settings.terminationCharacter = '\n'; // Standard SCPI terminator
    return settings;
}

Ldc502::Ldc502(ViSession defaultRM)
    : Instrument(defaultRM) {
}

Ldc502::Ldc502(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {
}

std::string Ldc502::getIdentity() {
    return query("*IDN?");
}

void Ldc502::setLaserOutput(bool enable) {
    write(enable ? "LDON 0" : "LDON 1");
}

bool Ldc502::isLaserOutputEnabled() {
    std::string response = query("LDON?");
    return (response.find("1") != std::string::npos || response.find("ON") != std::string::npos);
}

void Ldc502::setCurrentSetpoint(double currentmA) {
    write("SILD " + std::to_string(currentmA));
}

double Ldc502::getCurrentSetpoint() {
    std::string response = query("SILD?");
    return std::stod(response);
}

double Ldc502::getActualCurrent() {
    std::string response = query("RILD?");
    return std::stod(response);
}

std::string Ldc502::getStatus() {
    return query("LASer:CONDition?");
}

std::string Ldc502::getError() {
    return query("ERR?");
}