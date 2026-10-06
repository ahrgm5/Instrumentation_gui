#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <memory>
#include "instrument.hpp"
#include "zaber.hpp"
#include "velmex.hpp"

// --- Abstract Motor Interface ---
class IMotorAxis {
public:
    virtual ~IMotorAxis() = default;

    virtual void moveRelative(int axis, int distance) = 0;
    virtual void moveAbsolute(int axis, int position) = 0;
    virtual void stop(int axis) = 0;
    virtual void setSpeed(int axis, int speed) = 0;
    virtual void indexIncremental(int axis, int steps) = 0;
};

// --- Zaber Motor Adapter ---
class ZaberAdapter : public IMotorAxis {
public:
    explicit ZaberAdapter(std::shared_ptr<Zaber> zaber) : m_zaber(zaber) {}

    void moveRelative(int axis, int distance) override {
        if (m_zaber && m_zaber->isOpen()) {
            m_zaber->moveRelative(axis, distance);
        }
    }

    void moveAbsolute(int axis, int position) override {
        if (m_zaber && m_zaber->isOpen()) {
            m_zaber->moveAbsolute(axis, position);
        }
    }

    void stop(int axis) override {
        if (m_zaber && m_zaber->isOpen()) {
            m_zaber->stop(axis);
        }
    }

    void setSpeed(int axis, int speed) override {
        if (m_zaber && m_zaber->isOpen()) {
            m_zaber->query("/" + std::to_string(axis) + " set maxspeed " + std::to_string(speed));
        }
    }

    void indexIncremental(int axis, int steps) override {
        moveRelative(axis, steps);
    }

private:
    std::shared_ptr<Zaber> m_zaber;
};

// --- Velmex Motor Adapter ---
class VelmexAdapter : public IMotorAxis {
public:
    explicit VelmexAdapter(std::shared_ptr<VelmexVXM> velmex) : m_velmex(velmex) {}

    void moveRelative(int axis, int distance) override {
        if (m_velmex && m_velmex->isOpen()) {
            m_velmex->clearMemory();
            m_velmex->indexIncremental(axis, distance);
            m_velmex->run();
        }
    }

    void moveAbsolute(int axis, int position) override {
        if (m_velmex && m_velmex->isOpen()) {
            m_velmex->clearMemory();
            std::string cmd = "IA" + std::to_string(axis) + "M" + std::to_string(position);
            m_velmex->write(cmd);
            m_velmex->run();
        }
    }

    void stop(int /*axis*/) override {
        if (m_velmex && m_velmex->isOpen()) {
            m_velmex->clearMemory();
        }
    }

    void setSpeed(int axis, int speed) override {
        if (m_velmex && m_velmex->isOpen()) {
            m_velmex->setSpeed(axis, speed, false);
        }
    }

    void indexIncremental(int axis, int steps) override {
        if (m_velmex && m_velmex->isOpen()) {
            m_velmex->indexIncremental(axis, steps);
        }
    }

private:
    std::shared_ptr<VelmexVXM> m_velmex;
};

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