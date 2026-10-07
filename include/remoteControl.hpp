#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <memory>
#include "imotoraxis.hpp"
#include "instrument.hpp"
#include "zaber.hpp"
#include "velmex.hpp"

// --- Remote Control Class ---
class RemoteControl {
public:
    RemoteControl();
    ~RemoteControl();

    bool initialize();
    void pollEvents();
    bool isConnected() const;
    std::string getControllerName() const;

    bool isButtonPressed(SDL_GamepadButton button) const;
    Sint16 getAxisValue(SDL_GamepadAxis axis) const;

    void setTargetMotor(std::shared_ptr<IMotorAxis> motor);
    void bindZaber(std::shared_ptr<Zaber> zaber);
    void bindVelmex(std::shared_ptr<VelmexVXM> velmex);
    void clearTargetMotor();

    void moveRelative(int axis, int distance);
    void moveAbsolute(int axis, int position);
    void setSpeed(int axis, int speed);
    void indexIncremental(int axis, int steps);
    void stop(int axis = 0);

    void processMotionCommands(int stepDistance = 200);

private:
    SDL_Gamepad* controller;
    bool connected;
    std::shared_ptr<IMotorAxis> m_activeMotor;
};