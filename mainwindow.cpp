#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_isStreaming(false)
{
    // Load the full UI from mainwindow.ui
    ui->setupUi(this);

    // Remove the static "Video Feed" placeholder label from the UI layout
    if (ui->labelVideoPlaceholder) {
        delete ui->labelVideoPlaceholder;
    }

    // Create the video container widget that will host the IC4 HWND display
    m_videoContainer = new QWidget(this);
    m_videoContainer->setStyleSheet("background-color: black;");
    m_videoContainer->setMinimumSize(320, 240);
    ui->verticalLayout->addWidget(m_videoContainer, 1);

    // Create the Stream Control Button and add it below the video feed
    m_btnToggleStream = new QPushButton("Start Stream", this);
    m_btnToggleStream->setMinimumHeight(40);
    ui->verticalLayout->addWidget(m_btnToggleStream);

    // Connect Button Signal
    connect(m_btnToggleStream, &QPushButton::clicked, this, &MainWindow::toggleStreaming);

    // Initialize Camera on Startup
    initCamera();
}

MainWindow::~MainWindow() {
    if (m_isStreaming) {
        m_grabber.streamStop();
    }
    delete ui;
}

void MainWindow::initCamera() {
    // Enumerate available devices and open the first one
    auto devices = ic4::DeviceEnum::enumDevices();
    if (devices.empty()) {
        m_btnToggleStream->setEnabled(false);
        return;
    }

    // Open first available camera
    ic4::Error err;
    if (!m_grabber.deviceOpen(devices[0], err)) {
        m_btnToggleStream->setEnabled(false);
    }
}

void MainWindow::toggleStreaming() {
    ic4::Error err;

    if (!m_isStreaming) {
#if defined(Q_OS_WIN)
        // Retrieve native window handle (HWND) from the video container inside your UI
        HWND hwnd = reinterpret_cast<HWND>(m_videoContainer->winId());

        // Create IC4 Display bound to the window handle[cite: 2]
        m_ic4Display = ic4::Display::create(ic4::DisplayType::Default, hwnd, err);
#else
        m_ic4Display = ic4::Display::create(ic4::DisplayType::Default, nullptr, err);
#endif

        if (!m_ic4Display) {
            QMessageBox::critical(this, "Display Error", QString("Failed to create IC4 Display: %1").arg(QString::fromStdString(err.message())));
            return;
        }

        // Setup stream with the display sink
        if (m_grabber.streamSetup(m_ic4Display, static_cast<ic4::StreamSetupOption>(0), err)) {
            m_isStreaming = true;
            m_btnToggleStream->setText("Stop Stream");
        } else {
            QMessageBox::critical(this, "Stream Error", QString("Failed to start stream: %1").arg(QString::fromStdString(err.message())));
            m_ic4Display.reset();
        }
    } else {
        m_grabber.streamStop();
        m_isStreaming = false;
        m_btnToggleStream->setText("Start Stream");
        m_ic4Display.reset();
    }
}