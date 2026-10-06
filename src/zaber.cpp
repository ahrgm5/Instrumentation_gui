#include "zaber.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

ChannelSettings Zaber::defaultZaberSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 5000;
    settings.terminationCharacter = '\n';
    settings.writeTermination = "\n";
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

Zaber::Zaber(ViSession defaultRM)
    : Instrument(defaultRM) {
}

Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {
}

std::string Zaber::sendCommand(int index, const std::string& command) {
    std::string cmd = "/" + std::to_string(index) + " " + command;
    std::string response = query(cmd);

    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber command rejected (" + cmd + "): " + response);
    }

    return response;
}

int32_t Zaber::parseSettingResponse(const std::string& response) {
    std::string trimmed = trim(response);
    std::istringstream iss(trimmed);

    std::string token;
    int32_t parsedValue = 0;
    bool foundNumber = false;

    // Scan through response tokens to isolate numeric data and ignore status flags (e.g. IDLE, FE, OK)
    while (iss >> token) {
        try {
            parsedValue = std::stol(token);
            foundNumber = true;
        }
        catch (...) {
            // Skip warning codes or status labels
        }
    }

    if (!foundNumber) {
        throw std::runtime_error("Failed to parse numeric value from Zaber response: " + response);
    }

    return parsedValue;
}

void Zaber::home(int index) {
    sendCommand(index, "home");
}

void Zaber::moveAbsolute(int index, int32_t microsteps) {
    sendCommand(index, "move abs " + std::to_string(microsteps));
}

void Zaber::moveRelative(int index, int32_t microsteps) {
    sendCommand(index, "move rel " + std::to_string(microsteps));
}

void Zaber::stop(int index) {
    sendCommand(index, "stop");
}

int32_t Zaber::getPosition(int numDevices) {
    std::cout << "--- Device Positions (All Devices) ---\n";
    int32_t firstDevicePos = 0;

    for (int devIndex = 1; devIndex <= numDevices; ++devIndex) {
        try {
            int32_t pos = parseSettingResponse(sendCommand(devIndex, "get pos"));
            std::cout << "Device " << devIndex << " Position: " << pos << " microsteps\n";
            if (devIndex == 1) {
                firstDevicePos = pos;
            }
        }
        catch (const std::exception& ex) {
            std::cout << "Device " << devIndex << " Position: [Error: " << ex.what() << "]\n";
        }
    }
    std::cout << "-------------------------------------\n";
    return firstDevicePos;
}

int32_t Zaber::getPositionForDevice(int index) {
    if (index == 0) {
        return getPosition();
    }
    return parseSettingResponse(sendCommand(index, "get pos"));
}

bool Zaber::isIdle(int index) {
    std::string response = sendCommand(index, "get status");
    return (response.find("idle") != std::string::npos || response.find("IDLE") != std::string::npos);
}

void Zaber::waitUntilIdle(int index, int pollIntervalMs, int timeoutSeconds) {
    auto start = std::chrono::steady_clock::now();

    while (!isIdle(index)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start
        ).count();

        if (elapsed >= timeoutSeconds) {
            throw std::runtime_error("Timeout waiting for Zaber axis " +
                std::to_string(index) + " to become idle.");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }
}

int32_t Zaber::getLimitMax(int index) {
    return parseSettingResponse(sendCommand(index, "get limit.max"));
}

int32_t Zaber::getLimitMin(int index) {
    return parseSettingResponse(sendCommand(index, "get limit.min"));
}

int32_t Zaber::getResolution(int index) {
    return parseSettingResponse(sendCommand(index, "get resolution"));
}

std::vector<ZaberAxisLimits> Zaber::queryLimitsForAllDevices(int numDevices) {
    std::vector<ZaberAxisLimits> allLimits;

    for (int index = 1; index <= numDevices; ++index) {
        try {
            ZaberAxisLimits axis;
            axis.index = index;
            axis.minPosition = getLimitMin(index);
            axis.maxPosition = getLimitMax(index);
            axis.resolution = getResolution(index);

            allLimits.push_back(axis);
        }
        catch (const std::exception& ex) {
            std::cerr << "Warning: Could not query limits for axis index " << index
                << " (" << ex.what() << ")\n";
        }
    }

    return allLimits;
}