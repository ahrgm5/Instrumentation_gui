#pragma once

#include <cstdint>

class IMotorAxis {
public:
    virtual ~IMotorAxis() = default;






    virtual void stop(int axis) = 0;
    virtual void setSpeed(int axis, int speed) = 0;
    virtual void moveRelative(int axis, int distance) = 0;
    virtual void moveAbsolute(int axis, int position) = 0;
    virtual void indexIncremental(int axis, int steps) = 0;


    // Added position query for interface polymorphism
    virtual int32_t getPosition(int axis) = 0;


};