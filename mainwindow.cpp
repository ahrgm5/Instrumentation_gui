#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zaber.hpp"
#include "velmex.hpp"
#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <memory>

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

    // --- Connect Remote Target Combo Box ---
    connect(ui->comboRemoteTarget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onControllerTargetChanged);

    // --- Connect Execution Button ---
    connect(ui->btnExecuteSequence, &QPushButton::clicked, this, &MainWindow::onExecuteSequenceClicked);

    // --- Initialize Camera ---
    initCamera();

    // --- Timers ---
    m_cameraCheckTimer = new QTimer(this);
    connect(m_cameraCheckTimer, &QTimer::timeout, this, &MainWindow::checkCameraConnection);
    m_cameraCheckTimer->start(1000);

    m_deviceCheckTimer = new QTimer(this);
    connect(m_deviceCheckTimer, &QTimer::timeout, this, &MainWindow::checkDeviceConnections);
    m_deviceCheckTimer->start(2000);

    // --- Initialize Controller ---
    if (m_remoteController.initialize()) {
        qDebug() << "Connected to controller:" << QString::fromStdString(m_remoteController.getControllerName());
    }

    m_inputThrottleTimer.start();
    m_reconnectElapsedTimer.start();

    m_controllerTimer = new QTimer(this);
    connect(m_controllerTimer, &QTimer::timeout, this, &MainWindow::pollControllerInput);
    m_controllerTimer->start(16);

    // --- Setup Station Selection Dropdown ---
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
    if (m_controllerTimer) m_controllerTimer->stop();
    if (m_cameraCheckTimer) m_cameraCheckTimer->stop();
    if (m_deviceCheckTimer) m_deviceCheckTimer->stop();
    if (m_isStreaming) {
        try {
            m_grabber.streamStop();
        } catch (...) {}
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
        m_remoteController.clearTargetMotor();
        updateControllerTargetCombo();
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
        std::shared_ptr<Instrument> instrument;

        if (devName.contains("Zaber", Qt::CaseInsensitive)) {
            instrument = std::make_shared<Zaber>(m_vrm.handle());
        } else if (devName.contains("Velmex", Qt::CaseInsensitive)) {
            instrument = std::make_shared<VelmexVXM>(m_vrm.handle());
        } else {
            instrument = std::make_shared<Instrument>(m_vrm.handle());
        }

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
        ui->comboSequence->addItem(settings.value(key).toString(), key);
    }
    settings.endGroup();

    ui->treeWidget->expandAll();
    updateControllerTargetCombo();
}

void MainWindow::updateControllerTargetCombo()
{
    ui->comboRemoteTarget->blockSignals(true);
    ui->comboRemoteTarget->clear();
    ui->comboRemoteTarget->addItem("-- Select Target Device --", "");

    for (const auto& [devName, instrument] : m_stationDevices) {
        if (devName.contains("Zaber", Qt::CaseInsensitive)) {
            ui->comboRemoteTarget->addItem("Zaber Stage (" + devName + ")", devName);
        } else if (devName.contains("Velmex", Qt::CaseInsensitive)) {
            ui->comboRemoteTarget->addItem("Velmex Stage (" + devName + ")", devName);
        } else if (devName.contains("Laser", Qt::CaseInsensitive) || devName.contains("Welder", Qt::CaseInsensitive)) {
            ui->comboRemoteTarget->addItem("Laser Welder (" + devName + ")", devName);
        }
    }

    ui->comboRemoteTarget->blockSignals(false);
}

void MainWindow::onControllerTargetChanged(int index)
{
    QString devName = ui->comboRemoteTarget->itemData(index).toString();

    if (devName.isEmpty() || !m_stationDevices.count(devName)) {
        m_remoteController.clearTargetMotor();
        qDebug() << "Cleared remote controller target.";
        return;
    }

    auto instrument = m_stationDevices[devName];

    if (auto zaberInst = std::dynamic_pointer_cast<Zaber>(instrument)) {
        m_remoteController.bindZaber(zaberInst);
        qDebug() << "Remote controller targeting:" << devName << "(Zaber)";
    } else if (auto velmexInst = std::dynamic_pointer_cast<VelmexVXM>(instrument)) {
        m_remoteController.bindVelmex(velmexInst);
        qDebug() << "Remote controller targeting:" << devName << "(Velmex)";
    }
}

void MainWindow::pollControllerInput()
{
    m_remoteController.pollEvents();

    if (!m_remoteController.isConnected()) {
        ui->lblRemoteStatus->setText("<span style='color: #e74c3c; font-size: 16pt;'>&#9679;</span>");
        ui->comboRemoteTarget->setEnabled(false);

        if (m_reconnectElapsedTimer.hasExpired(1000)) {
            if (m_remoteController.initialize()) {
                qDebug() << "Controller connected dynamically:" << QString::fromStdString(m_remoteController.getControllerName());
            }
            m_reconnectElapsedTimer.restart();
        }
        return;
    }

    ui->lblRemoteStatus->setText("<span style='color: #2ecc71; font-size: 16pt;'>&#9679;</span>");
    ui->comboRemoteTarget->setEnabled(true);

    if (m_inputThrottleTimer.hasExpired(150)) {
        m_remoteController.processMotionCommands(200);
    }

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

void MainWindow::initCamera()
{
    try {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (devList.empty()) {
            return;
        }

        m_grabber.deviceOpen(devList.front());

        HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());
        ic4::Error err;

        m_ic4Display = ic4::Display::create(
            static_cast<ic4::DisplayType>(0),
            reinterpret_cast<ic4::WindowHandle>(hwnd),
            err
            );

        if (m_ic4Display) {
            m_grabber.streamSetup(m_ic4Display);
            m_isStreaming = true;
            m_btnToggleStream->setText("Stop Stream");
            m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
        }
    } catch (const std::exception& ex) {
        qDebug() << "Camera initialization failed:" << ex.what();
    }
}

void MainWindow::toggleStreaming()
{
    if (!m_grabber.isDeviceOpen()) {
        try {
            initCamera();
        } catch (...) {}
        return;
    }

    if (m_isStreaming) {
        try {
            m_grabber.streamStop();
        } catch (...) {}
        m_isStreaming = false;
        m_btnToggleStream->setText("Start Stream");
        m_btnToggleStream->setStyleSheet("QPushButton { background-color: #2ecc71; color: white; font-weight: bold; border-radius: 4px; }");
    } else {
        if (m_ic4Display) {
            try {
                m_grabber.streamSetup(m_ic4Display);
                m_isStreaming = true;
                m_btnToggleStream->setText("Stop Stream");
                m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
            } catch (...) {}
        }
    }
}

void MainWindow::checkCameraConnection()
{
    bool currentlyOpen = m_grabber.isDeviceOpen();

    if (!currentlyOpen) {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (!devList.empty()) {
            try {
                m_grabber.deviceOpen(devList.front());
                HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());
                ic4::Error err;

                m_ic4Display = ic4::Display::create(
                    static_cast<ic4::DisplayType>(0),
                    reinterpret_cast<ic4::WindowHandle>(hwnd),
                    err
                    );

                if (m_ic4Display) {
                    m_grabber.streamSetup(m_ic4Display);
                    m_isStreaming = true;
                    m_btnToggleStream->setText("Stop Stream");
                    m_btnToggleStream->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; font-weight: bold; border-radius: 4px; }");
                }
            } catch (...) {}
        }
    }
}

void MainWindow::checkDeviceConnections()
{
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* topItem = ui->treeWidget->topLevelItem(i);
        QString devName = topItem->text(0);

        if (m_stationDevices.count(devName)) {
            auto dev = m_stationDevices[devName];
            QString resStr = m_deviceResourceStrings[devName];

            bool currentlyConnected = dev->isOpen();

            if (!currentlyConnected) {
                try {
                    currentlyConnected = dev->open(resStr.toStdString(), 1000);
                } catch (...) {
                    currentlyConnected = false;
                }
            }

            if (currentlyConnected) {
                topItem->setText(1, "● Connected");
                topItem->setForeground(1, QBrush(QColor("#2ecc71")));
            } else {
                topItem->setText(1, "● Disconnected");
                topItem->setForeground(1, QBrush(QColor("#e74c3c")));
            }
        }
    }
}

void MainWindow::onExecuteSequenceClicked()
{
    QString seqKey = ui->comboSequence->currentData().toString();
    QString seqText = ui->comboSequence->currentText();

    if (seqKey.isEmpty()) {
        QMessageBox::warning(this, "No Sequence Selected", "Please select a product sequence before executing.");
        return;
    }

    emit executeSequenceRequested(seqKey, seqText);
}