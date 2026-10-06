#include "velmex.hpp"
#include <stdexcept>

void VelmexVXM::writeCommand(const std::string& cmd) {
    write(cmd);
}

std::string VelmexVXM::readResponse() {
    return read();
}

// Corrected: 'C' clears command memory
void VelmexVXM::clearMemory() {
    writeCommand("C");
}

// Implemented: 'R' executes the buffered command sequence
std::string VelmexVXM::run() {
    writeCommand("R");
    return readResponse();
}

void VelmexVXM::setSpeed(int motor, int speed, bool fullPower) {
    // SA sets speed at 100% power, S sets speed at 70% power
    std::string prefix = fullPower ? "SA" : "S";
    std::string cmd = prefix + std::to_string(motor) + "M" + std::to_string(speed);
    writeCommand(cmd);
}

void VelmexVXM::indexIncremental(int motor, int steps) {
    std::string cmd = "I" + std::to_string(motor) + "M" + std::to_string(steps);
    writeCommand(cmd);
}

void VelmexVXM::write(const std::string& command) {
    if (!isOpen()) throw std::runtime_error("Cannot write: VISA session is closed.");

    std::string cmd = command;
    // Strip trailing \n if present and enforce Velmex \r termination
    if (!cmd.empty() && cmd.back() == '\n') {
        cmd.pop_back();
    }
    if (cmd.empty() || cmd.back() != '\r') {
        cmd += "\r";
    }

    ViUInt32 bytesWritten = 0;
    ViStatus status = viWrite(
        m_session,
        reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
        static_cast<ViUInt32>(cmd.size()),
        &bytesWritten
    );
    checkStatus(status, "Write command failed");
}