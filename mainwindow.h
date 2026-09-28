#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include <ic4/ic4.h>
#include "remoteControl.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void toggleStreaming();
    void pollControllerInput();

private:
    void initCamera();

    Ui::MainWindow *ui;
    QWidget *m_videoContainer;
    QPushButton *m_btnToggleStream;

    // IC4 Camera & Display
    ic4::Grabber m_grabber;
    std::shared_ptr<ic4::Display> m_ic4Display;
    bool m_isStreaming;

    // Xbox Controller & Throttling
    RemoteControl m_remoteController;
    QTimer *m_controllerTimer;
    QElapsedTimer m_inputThrottleTimer;
    QElapsedTimer m_reconnectElapsedTimer; // <-- Added to throttle hot-plug reconnection attempts
    QString m_lastLoggedInputs;
};