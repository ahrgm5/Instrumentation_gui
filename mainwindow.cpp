#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <memory>
#include <unordered_map>

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
    m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
    ui->verticalLayout->addWidget(m_btnToggleStream);

    connect(m_btnToggleStream, &QPushButton::clicked, this, &MainWindow::toggleStreaming);

    // --- Connect Execution Button ---
    connect(ui->btnExecuteSequence, &QPushButton::clicked, this, &MainWindow::onExecuteSequenceClicked);

    // --- Initialize Camera (Non-blocking if not plugged in yet) ---
    initCamera();

    // --- Initialize Camera Hot-Plug Monitor Timer ---
    m_cameraCheckTimer = new QTimer(this);
    connect(m_cameraCheckTimer, &QTimer::timeout, this, &MainWindow::checkCameraConnection);
    m_cameraCheckTimer->start(1000);

    // --- Initialize Device Connection & Reconnect Timer ---
    m_deviceCheckTimer = new QTimer(this);
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &MainWindow::checkDeviceConnections);
    m_deviceCheckTimer->start(2000);

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
    m_controllerTimer->start(16);

    // --- Setup Station Selection Dropdown (Scan Directory for .ini Files) ---
    connect(ui->comboStation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onStationSelected);

    ui->comboStation->blockSignals(true);
    ui->comboStation->clear();
    ui->comboStation->addItem("-- Select Station --", "");

    QString configDirectoryPath = "C:/Users/aron.rezene";
    QDir configDir(configDirectoryPath);

    QStringList filters;
    filters << "*.ini";
    QFileInfoList iniFiles = configDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : iniFiles) {
        QString filePath = fileInfo.absoluteFilePath();
        QSettings settings(filePath, QSettings::IniFormat);
        QString stationName = settings.value("General/StationName", fileInfo.completeBaseName()).toString();
        ui->comboStation->addItem(stationName, filePath);
    }

    ui->comboStation->setCurrentIndex(0);
    ui->comboStation->blockSignals(false);
}

MainWindow::~MainWindow()
{
    if (m_controllerTimer) {
        m_controllerTimer->stop();
    }
    if (m_cameraCheckTimer) {
        m_cameraCheckTimer->stop();
    }
    if (m_deviceCheckTimer) {
        m_deviceCheckTimer->stop();
    }
    if (m_isStreaming) {
        m_grabber.streamStop();
    }
    delete ui;
}

void MainWindow::onStationSelected(int index)
{
    QString iniPath = ui->comboStation->itemData(index).toString();

    if (!iniPath.isEmpty()) {
        loadStationConfig(iniPath);
    } else {
        ui->treeWidget->clear();
        ui->comboSequence->clear();
        m_stationDevices.clear();
        m_deviceResourceStrings.clear();
    }
}

void MainWindow::loadStationConfig(const QString& iniFilePath)
{
    QSettings settings(iniFilePath, QSettings::IniFormat);

    ui->treeWidget->clear();
    ui->comboSequence->clear();
    m_stationDevices.clear();
    m_deviceResourceStrings.clear();

    settings.beginGroup("Devices");
    QStringList deviceNames = settings.childKeys();

    for (const QString& devName : deviceNames) {
        QString resourceString = settings.value(devName).toString();
        auto instrument = std::make_shared<Instrument>(m_vrm.handle());

        m_deviceResourceStrings[devName] = resourceString;

        bool isConnected = false;
        try {
            isConnected = instrument->open(resourceString.toStdString(), 2000);
        } catch (...) {
            isConnected = false;
        }

        m_stationDevices[devName] = instrument;

        QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, devName);

        if (isConnected) {
            item->setText(1, "● Connected");
            item->setForeground(1, QBrush(QColor("#2ecc71")));
        } else {
            item->setText(1, "● Disconnected");
            item->setForeground(1, QBrush(QColor("#e74c3c")));
        }

        QTreeWidgetItem* childItem = new QTreeWidgetItem(item);
        childItem->setText(0, "Resource");
        childItem->setText(1, resourceString);
    }
    settings.endGroup();

    settings.beginGroup("Products");
    QStringList generalKeys = settings.childKeys();

    for (const QString& key : generalKeys) {
        QString sequenceDisplayName = settings.value(key).toString();
        ui->comboSequence->addItem(sequenceDisplayName, key);
    }
    settings.endGroup();

    ui->treeWidget->expandAll();
}

void MainWindow::checkDeviceConnections()
{
    if (m_stationDevices.empty()) return;

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        if (!item) continue;

        QString devName = item->text(0);

        if (m_stationDevices.count(devName)) {
            auto instrument = m_stationDevices[devName];

            bool isConnected = false;

            if (instrument && instrument->isOpen()) {
                try {
                    std::string idnResponse = instrument->query("*IDN?", 500);
                    isConnected = !idnResponse.empty();
                } catch (...) {
                    isConnected = false;
                }
            }

            if (!isConnected && instrument) {
                QString resourceString = m_deviceResourceStrings[devName];
                if (!resourceString.isEmpty()) {
                    try {
                        isConnected = instrument->open(resourceString.toStdString(), 500);
                    } catch (...) {
                        isConnected = false;
                    }
                }
            }

            QString statusText = isConnected ? "● Connected" : "● Disconnected";
            QColor statusColor = isConnected ? QColor("#2ecc71") : QColor("#e74c3c");

            if (item->text(1) != statusText) {
                item->setText(1, statusText);
                item->setForeground(1, QBrush(statusColor));

                if (isConnected) {
                    qDebug() << "Device connected dynamically:" << devName;
                } else {
                    qDebug() << "Device disconnected dynamically:" << devName;
                }
            }
        }
    }
}

void MainWindow::onExecuteSequenceClicked()
{
    int index = ui->comboSequence->currentIndex();
    if (index < 0) return;

    QString sequenceKey = ui->comboSequence->itemData(index).toString();
    QString sequenceDisplayName = ui->comboSequence->currentText();

    if (sequenceKey.isEmpty()) {
        QMessageBox::warning(this, "Select Sequence", "Please select a valid sequence.");
        return;
    }

    qDebug() << "Sequence execution requested:" << sequenceDisplayName << "(" << sequenceKey << ")";

    emit executeSequenceRequested(sequenceKey, sequenceDisplayName);
}

void MainWindow::initCamera()
{
    auto devices = ic4::DeviceEnum::enumDevices();
    if (devices.empty()) {
        m_btnToggleStream->setEnabled(true);
        return;
    }

    ic4::Error err;
    m_grabber.deviceOpen(devices[0], err);
}

void MainWindow::checkCameraConnection()
{
    auto devices = ic4::DeviceEnum::enumDevices();

    if (m_grabber.isDeviceOpen()) {
        bool deviceStillPresent = false;
        auto currentSerial = m_grabber.deviceInfo().serial();
        for (const auto& dev : devices) {
            if (dev.serial() == currentSerial) {
                deviceStillPresent = true;
                break;
            }
        }

        if (!deviceStillPresent) {
            qDebug() << "Camera disconnected dynamically.";
            if (m_isStreaming) {
                m_grabber.streamStop();
                m_isStreaming = false;
                m_ic4Display.reset();
                m_btnToggleStream->setText("Start Stream");
                m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
            }
            m_grabber.deviceClose();
        }
    } else {
        if (!devices.empty()) {
            ic4::Error err;
            if (m_grabber.deviceOpen(devices[0], err)) {
                qDebug() << "Camera reconnected and recognized automatically:" << QString::fromStdString(devices[0].modelName());
            }
        }
    }
}

void MainWindow::toggleStreaming()
{
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
            m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
        } else {
            QMessageBox::critical(this, "Stream Error", QString("Failed to start stream: %1").arg(QString::fromStdString(err.message())));
            m_ic4Display.reset();
        }
    } else {
        m_grabber.streamStop();
        m_isStreaming = false;
        m_btnToggleStream->setText("Start Stream");
        m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
        m_ic4Display.reset();
    }
}

void MainWindow::pollControllerInput()
{
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