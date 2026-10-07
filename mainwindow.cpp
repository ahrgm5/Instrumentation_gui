#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zaber.hpp"
#include "velmex.hpp"

#include <QMessageBox>
#include <QDebug>
#include <QTime>
#include <QTreeWidgetItem>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <QColor>
#include <QTextCursor>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isStreaming(false)
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

    // --- Connect Remote Target Combo Box & Home Button ---
    connect(ui->comboRemoteTarget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onControllerTargetChanged);

    ui->motorHomeBtn->setVisible(false);
    connect(ui->motorHomeBtn, &QPushButton::clicked,
            this, &MainWindow::onHomeTargetClicked);

    connect(ui->comboRemoteTarget, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onControllerTargetChanged);

    // --- Context Menu for Clearing Text Controller Stream ---
    ui->textControllerStream->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textControllerStream, &QTextEdit::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu *menu = ui->textControllerStream->createStandardContextMenu();
        menu->addSeparator();
        QAction *clearAction = menu->addAction("Clear Log Window");
        connect(clearAction, &QAction::triggered, ui->textControllerStream, &QTextEdit::clear);
        menu->exec(ui->textControllerStream->mapToGlobal(pos));
        delete menu;
    });

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

    // Safely close active VISA sessions
    for (auto& [name, inst] : m_stationDevices) {
        if (inst && inst->isOpen()) {
            inst->close();
        }
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
        m_axisTreeItems.clear();
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
    m_axisTreeItems.clear();

    settings.beginGroup("Devices");
    QStringList deviceNames = settings.childKeys();

    for (const QString& devName : deviceNames) {
        QString resourceString = settings.value(devName).toString();
        std::shared_ptr<Instrument> instrument;

        QString devUpper = devName.toUpper();

        if (devUpper.contains("ZABER")) {
            instrument = std::make_shared<Zaber>(m_vrm.handle());
        } else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
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

        // Parent item in treeWidget
        QTreeWidgetItem* item = new QTreeWidgetItem(ui->treeWidget);
        item->setText(0, devName);

        if (isConnected) {
            item->setText(1, "● Connected");
            item->setForeground(1, QBrush(QColor("#2ecc71")));
        } else {
            item->setText(1, "● Disconnected");
            item->setForeground(1, QBrush(QColor("#e74c3c")));
        }

        // VISA Resource string child node
        QTreeWidgetItem* childItem = new QTreeWidgetItem(item);
        childItem->setText(0, "Resource");
        childItem->setText(1, resourceString);

        // Populate live axis coordinate child nodes inside Devices tree tab
        if (devUpper.contains("ZABER")) {
            QStringList labels = {
                "Axis 1 (/1 X-MCA)",
                "Axis 2 (/2 LSQ)",
                "Axis 3 (/3 DMQ-1)",
                "Axis 4 (/4 DMQ-2)"
            };
            for (int i = 1; i <= 4; ++i) {
                QTreeWidgetItem* axisItem = new QTreeWidgetItem(item);
                axisItem->setText(0, labels[i - 1]);
                axisItem->setText(1, "0 steps");
                m_axisTreeItems[devName][i] = axisItem;
            }
        }
        else if (devUpper.contains("VELMEX") || devUpper.contains("VXM")) {
            for (int i = 1; i <= 2; ++i) {
                QTreeWidgetItem* axisItem = new QTreeWidgetItem(item);
                axisItem->setText(0, QString("Axis %1").arg(i));
                axisItem->setText(1, "0 steps");
                m_axisTreeItems[devName][i] = axisItem;
            }
        }
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
        QString devUpper = devName.toUpper();
        if ((devUpper.contains("ZABER") || devUpper.contains("VELMEX") || devUpper.contains("VXM"))
            && instrument && instrument->isOpen())
        {
            ui->comboRemoteTarget->addItem(devName, devName);
        }
    }

    ui->comboRemoteTarget->blockSignals(false);

    if (ui->comboRemoteTarget->count() > 1) {
        ui->comboRemoteTarget->setCurrentIndex(1);
        onControllerTargetChanged(1);
    } else {
        m_remoteController.clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
    }
}



void MainWindow::onControllerTargetChanged(int index)
{
    // Clear log stream and reset text formatting whenever target changes
    ui->textControllerStream->clear();
    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

    QString devName = ui->comboRemoteTarget->itemData(index).toString();

    if (devName.isEmpty() || !m_stationDevices.count(devName) || !m_stationDevices[devName]) {
        m_remoteController.clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        return;
    }

    auto instrument = m_stationDevices[devName];

    if (!instrument->isOpen()) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'>Cannot bind target %2: Serial session closed.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        m_remoteController.clearTargetMotor();
        ui->motorHomeBtn->setVisible(false);
        return;
    }

    bool isMotorTarget = false;

    if (auto zaberInst = std::dynamic_pointer_cast<Zaber>(instrument)) {
        m_remoteController.bindZaber(zaberInst);
        isMotorTarget = true;
    } else if (auto velmexInst = std::dynamic_pointer_cast<VelmexVXM>(instrument)) {
        m_remoteController.bindVelmex(velmexInst);
        isMotorTarget = true;
    } else {
        m_remoteController.clearTargetMotor();
    }

    ui->motorHomeBtn->setVisible(isMotorTarget);
}



void MainWindow::onHomeTargetClicked()
{
    QString devName = ui->comboRemoteTarget->currentData().toString();

    if (devName.isEmpty() || !m_stationDevices.count(devName) || !m_stationDevices[devName]) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> No motor target selected.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        return;
    }

    auto instrument = m_stationDevices[devName];

    if (!instrument->isOpen()) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Failed:</b> Session for %2 is closed.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
        return;
    }

    ui->textControllerStream->append(
        QString("[%1] Initiating home command for %2...")
            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
        );

    try {
        if (auto zaber = std::dynamic_pointer_cast<Zaber>(instrument)) {
            for (int axis = 1; axis <= 4; ++axis) {
                zaber->home(axis, 1);
            }
        } else if (auto velmex = std::dynamic_pointer_cast<VelmexVXM>(instrument)) {
            velmex->clearMemory();
            velmex->writeCommand("C");
        }

        ui->textControllerStream->append(
            QString("[%1] <span style='color: #2ecc71;'><b>Home Command Sent:</b> %2 is homing.</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName)
            );
    }
    catch (const std::exception& ex) {
        ui->textControllerStream->append(
            QString("[%1] <span style='color: #e74c3c;'><b>Homing Error (%2):</b> %3</span>")
                .arg(QTime::currentTime().toString("hh:mm:ss.zzz"), devName, QString::fromUtf8(ex.what()))
            );
    }

    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
    QTextCursor cursor = ui->textControllerStream->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textControllerStream->setTextCursor(cursor);
}

void MainWindow::updateDeviceTreePositions()
{
    for (auto& [devName, axisMap] : m_axisTreeItems) {
        if (!m_stationDevices.count(devName) || !m_stationDevices[devName]) continue;

        auto inst = m_stationDevices[devName];
        if (!inst->isOpen()) continue;

        if (auto zaber = std::dynamic_pointer_cast<Zaber>(inst)) {
            for (auto& [axisIndex, treeItem] : axisMap) {
                try {
                    int32_t pos = zaber->getPositionForAxis(axisIndex, 1);
                    treeItem->setText(1, QString("%1 steps").arg(pos));
                } catch (...) {
                    treeItem->setText(1, "Error");
                }
            }
        }
        else if (auto velmex = std::dynamic_pointer_cast<VelmexVXM>(inst)) {
            for (auto& [axisIndex, treeItem] : axisMap) {
                try {
                    int32_t pos = velmex->getPosition(axisIndex);
                    treeItem->setText(1, QString("%1 steps").arg(pos));
                } catch (...) {
                    treeItem->setText(1, "Error");
                }
            }
        }
    }
}

void MainWindow::pollControllerInput()
{
    try {
        m_remoteController.pollEvents();

        if (!m_remoteController.isConnected()) {
            ui->lblRemoteStatus->setText("<span style='color: #e74c3c; font-size: 16pt;'>&#9679;</span>");
            ui->comboRemoteTarget->setEnabled(false);

            if (m_reconnectElapsedTimer.hasExpired(1000)) {
                if (m_remoteController.initialize()) {
                    QString logMsg = QString("[%1] Controller connected: %2")
                    .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
                        .arg(QString::fromStdString(m_remoteController.getControllerName()));
                    ui->textControllerStream->append(logMsg);
                    ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
                }
                m_reconnectElapsedTimer.restart();
            }
            return;
        }

        ui->lblRemoteStatus->setText("<span style='color: #2ecc71; font-size: 16pt;'>&#9679;</span>");
        ui->comboRemoteTarget->setEnabled(true);

        if (m_inputThrottleTimer.hasExpired(150)) {
            m_remoteController.processMotionCommands(5000);
            updateDeviceTreePositions();
            m_inputThrottleTimer.restart();
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

                ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());
                ui->textControllerStream->append(timestampedStream);

                QTextCursor cursor = ui->textControllerStream->textCursor();
                cursor.movePosition(QTextCursor::End);
                ui->textControllerStream->setTextCursor(cursor);

                m_lastLoggedInputs = currentInputStr;
            }
        } else {
            m_lastLoggedInputs.clear();
        }
    }
    catch (const std::exception& ex) {
        QString errorMsg = QString("[%1] <span style='color: #e74c3c;'><b>VISA/Motion Error:</b> %2</span>")
        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"))
            .arg(QString::fromUtf8(ex.what()));

        ui->textControllerStream->append(errorMsg);
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

        QTextCursor cursor = ui->textControllerStream->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textControllerStream->setTextCursor(cursor);
    }
    catch (...) {
        QString errorMsg = QString("[%1] <span style='color: #e74c3c;'><b>Hardware Error:</b> Unknown runtime exception occurred.</span>")
        .arg(QTime::currentTime().toString("hh:mm:ss.zzz"));

        ui->textControllerStream->append(errorMsg);
        ui->textControllerStream->setCurrentCharFormat(QTextCharFormat());

        QTextCursor cursor = ui->textControllerStream->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textControllerStream->setTextCursor(cursor);
    }
}

void MainWindow::initCamera()
{
    try {
        auto devList = ic4::DeviceEnum::enumDevices();
        if (devList.empty()) return;

        m_grabber.deviceOpen(devList.front());

#ifdef Q_OS_WIN
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
#endif
    } catch (...) {}
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
#ifdef Q_OS_WIN
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
#endif
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