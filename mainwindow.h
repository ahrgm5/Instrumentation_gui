#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QWidget>
#include <memory>
#include <ic4/ic4.h>

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

private:
    void initCamera();

    Ui::MainWindow *ui;
    QWidget *m_videoContainer;
    QPushButton *m_btnToggleStream;

    // IC4 Camera & Display
    ic4::Grabber m_grabber;
    std::shared_ptr<ic4::Display> m_ic4Display;
    bool m_isStreaming;
};