#ifndef VELMEX_HPP
#define VELMEX_HPP

#include <string>
#include <visa.h>
#include "instrument.hpp"

class VelmexVXM : public Instrument {
public:
    // Inherit Instrument constructors (including ViSession + ChannelSettings)[cite: 1, 2]
    using Instrument::Instrument;

    void writeCommand(const std::string& cmd);
    std::string readResponse();

    virtual void write(const std::string& command) override;

    void clearMemory();
    void setSpeed(int motor, int speed, bool fullPower = false);
    void indexIncremental(int motor, int steps);
    std::string run();
};

#endif // VELMEX_HPP