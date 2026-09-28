#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_isStreaming(false)
{
    ui->setupUi(this);

    // --- UI Setup for Video Feed & Stream Button ---
    if (ui->labelVideoPlaceholder) {
        delete ui->labelVideoPlaceholder;
    }

    m_videoContainer = new QWidget(this);
    m_videoContainer->setStyleSheet("background-color: black;");
    m_videoContainer->setMinimumSize(320, 240);
    ui->verticalLayout->addWidget(m_videoContainer, 1);

    m_btnToggleStream = new QPushButton("Start Stream", this);
    m_btnToggleStream->setMinimumHeight(40);
    // Green for Start/Go
    m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
    ui->verticalLayout->addWidget(m_btnToggleStream);

    connect(m_btnToggleStream, &QPushButton::clicked, this, &MainWindow::toggleStreaming);

    // --- Initialize Camera (Non-blocking if not plugged in yet) ---
    initCamera();

    // --- Initialize Xbox Controller & Timers ---
    if (m_remoteController.initialize()) {
        qDebug() << "Connected to controller:" << QString::fromStdString(m_remoteController.getControllerName());
    } else {
        qDebug() << "Warning: No Xbox controller detected on startup.";
    }

    m_inputThrottleTimer.start();
    m_reconnectElapsedTimer.start();

    m_controllerTimer = new QTimer(this);
    connect(m_controllerTimer, &QTimer::timeout, this, &MainWindow::pollControllerInput);
    m_controllerTimer->start(16); // Poll roughly every 16ms (~60 FPS)
}

MainWindow::~MainWindow() {
    if (m_controllerTimer) {
        m_controllerTimer->stop();
    }
    if (m_isStreaming) {
        m_grabber.streamStop();
    }
    delete ui;
}

void MainWindow::initCamera() {
    auto devices = ic4::DeviceEnum::enumDevices();
    if (devices.empty()) {
        m_btnToggleStream->setEnabled(true);
        return;
    }

    ic4::Error err;
    m_grabber.deviceOpen(devices[0], err);
}

void MainWindow::toggleStreaming() {
    ic4::Error err;

    if (!m_isStreaming) {
        if (!m_grabber.isDeviceOpen()) {
            auto devices = ic4::DeviceEnum::enumDevices();
            if (devices.empty()) {
                QMessageBox::warning(this, "Camera Error", "No camera detected. Please plug in the camera and try again.");
                return;
            }
            if (!m_grabber.deviceOpen(devices[0], err)) {
                QMessageBox::critical(this, "Camera Error", QString("Failed to open camera: %1").arg(QString::fromStdString(err.message())));
                return;
            }
        }

#if defined(Q_OS_WIN)
        HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());
        m_ic4Display = ic4::Display::create(ic4::DisplayType::Default, hwnd, err);
#else
        m_ic4Display = ic4::Display::create(ic4::DisplayType::Default, nullptr, err);
#endif

        if (!m_ic4Display) {
            QMessageBox::critical(this, "Display Error", QString("Failed to create IC4 Display: %1").arg(QString::fromStdString(err.message())));
            return;
        }

        if (m_grabber.streamSetup(m_ic4Display, static_cast<ic4::StreamSetupOption>(0), err)) {
            m_isStreaming = true;
            m_btnToggleStream->setText("Stop Stream");
            // Red for Stop/Danger
            m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
        } else {
            QMessageBox::critical(this, "Stream Error", QString("Failed to start stream: %1").arg(QString::fromStdString(err.message())));
            m_ic4Display.reset();
        }
    } else {
        m_grabber.streamStop();
        m_isStreaming = false;
        m_btnToggleStream->setText("Start Stream");
        // Green for Start/Go
        m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
        m_ic4Display.reset();
    }
}

void MainWindow::pollControllerInput() {
    m_remoteController.pollEvents();

    if (!m_remoteController.isConnected()) {
        ui->lblRemoteStatus->setText("Remote status: <span style='color: #e74c3c; font-size: 14pt;'>&#9679;</span> Disconnected");

        if (m_reconnectElapsedTimer.hasExpired(1000)) {
            if (m_remoteController.initialize()) {
                qDebug() << "Controller connected dynamically:" << QString::fromStdString(m_remoteController.getControllerName());
            }
            m_reconnectElapsedTimer.restart();
        }
        return;
    }

    ui->lblRemoteStatus->setText("Remote status: <span style='color: #2ecc71; font-size: 14pt;'>&#9679;</span> Connected");

    struct ButtonNamePair {
        SDL_GamepadButton button;
        QString name;
    };

    ButtonNamePair buttonsToTrack[] = {
        { SDL_GAMEPAD_BUTTON_SOUTH, "A" },
        { SDL_GAMEPAD_BUTTON_EAST, "B" },
        { SDL_GAMEPAD_BUTTON_WEST, "X" },
        { SDL_GAMEPAD_BUTTON_NORTH, "Y" },
        { SDL_GAMEPAD_BUTTON_BACK, "View" },
        { SDL_GAMEPAD_BUTTON_GUIDE, "Xbox" },
        { SDL_GAMEPAD_BUTTON_START, "Menu" },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, "L-Stick" },
        { SDL_GAMEPAD_BUTTON_RIGHT_STICK, "R-Stick" },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "LB" },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "RB" },
        { SDL_GAMEPAD_BUTTON_DPAD_UP, "DPad-Up" },
        { SDL_GAMEPAD_BUTTON_DPAD_DOWN, "DPad-Down" },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, "DPad-Left" },
        { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "DPad-Right" }
    };

    QStringList activeInputs;
    for (const auto& pair : buttonsToTrack) {
        if (m_remoteController.isButtonPressed(pair.button)) {
            activeInputs.append(pair.name);
        }
    }

    Sint16 leftTrigger = m_remoteController.getAxisValue(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTrigger = m_remoteController.getAxisValue(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    if (leftTrigger > 8000)  activeInputs.append(QString("LT(%1)").arg(leftTrigger));
    if (rightTrigger > 8000) activeInputs.append(QString("RT(%1)").arg(rightTrigger));

    QString currentInputStr = activeInputs.join(", ");

    if (!activeInputs.isEmpty()) {
        bool timeElapsed = m_inputThrottleTimer.hasExpired(250);
        bool inputChanged = (currentInputStr != m_lastLoggedInputs);

        if (timeElapsed || inputChanged) {
            QString timestampedStream = QString("[%1] Active Inputs: %2")
            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                .arg(currentInputStr);

            ui->textControllerStream->append(timestampedStream);

            QTextCursor cursor = ui->textControllerStream->textCursor();
            cursor.movePosition(QTextCursor::End);
            ui->textControllerStream->setTextCursor(cursor);

            m_lastLoggedInputs = currentInputStr;
            m_inputThrottleTimer.restart();
        }
    } else {
        m_lastLoggedInputs.clear();
    }
}