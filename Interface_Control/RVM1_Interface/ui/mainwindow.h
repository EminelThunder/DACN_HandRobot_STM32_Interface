#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SerialManager;
class RobotRenderer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onBtnConnectClicked();
    void onBtnRefreshClicked();
    void onBtnHomeClicked();
    void onSliderJ1Changed(int val);
    void onSliderJ2Changed(int val);
    void onSliderJ3Changed(int val);
    void onSliderJ4Changed(int val);
    void onSliderJ5Changed(int val);
    void onConnectionChanged(bool connected);
    void onJointAnglesReceived(double q1, double q2, double q3,
                               double q4, double q5);

private:
    Ui::MainWindow *ui;
    SerialManager  *m_serial   = nullptr;
    RobotRenderer  *m_renderer = nullptr;
    double          m_angles[5]{};

    void updateSlidersFromAngles();
    void sendCurrentAngles();
};

#endif // MAINWINDOW_H
