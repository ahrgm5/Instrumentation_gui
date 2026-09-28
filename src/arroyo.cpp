#include "arroyo.hpp"
#include <string>
#include <stdexcept>

ChannelSettings Arroyo5240::defaultSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 2000;
    settings.terminationCharacter = '\n';
    settings.baudRate = 38400;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

Arroyo5240::Arroyo5240(ViSession defaultRM)
    : Instrument(defaultRM) {
}

Arroyo5240::Arroyo5240(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {
}

std::string Arroyo5240::getIdentity() {
    return query("*IDN?");
}

std::string Arroyo5240::beep() {
    return query("BEEP?");
}

void Arroyo5240::setTemperatureSetpoint(double tempC) {
    write("TEC:T " + std::to_string(tempC));
}

double Arroyo5240::getTemperatureSetpoint() {
    std::string response = query("TEC:SET:T?");
    return std::stod(response);
}

double Arroyo5240::getCurrentTemperature() {
    std::string response = query("TEC:T?");
    return std::stod(response);
}

void Arroyo5240::setOutputEnable(bool enable) {
    write(enable ? "TEC:OUT 1" : "TEC:OUT 0");
}

bool Arroyo5240::isOutputEnabled() {
    std::string response = query("TEC:OUT?");
    return std::stoi(response) != 0;
}