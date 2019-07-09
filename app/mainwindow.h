/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <cstdint>
#include <vector>
#include "commdriver.h"
#include "can_ts.h"

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:

    void cants_SendTCCompleted(uint8_t address, uint8_t channel);
    void cants_ReceiveTMCompleted(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);
    void cants_SendBlockCompleted(uint8_t address);
    void cants_ReceiveBlockCompleted(uint8_t address, std::vector<uint8_t> data);

    void cants_SendTCFailed(uint8_t address, uint8_t channel, sky::CAN_TS::SendTCError error);
    void cants_ReceiveTMFailed(uint8_t address, uint8_t channel, sky::CAN_TS::ReceiveTMError error);
    void cants_SendBlockFailed(uint8_t address, sky::CAN_TS::SendBlockError error);
    void cants_ReceiveBlockFailed(uint8_t address, sky::CAN_TS::ReceiveBlockError error);
    void cants_SendUnsolicitedFailed(uint8_t address, uint8_t channel);
    void cants_SendTimeSyncFailed();

    void keepAliveTmr_timeout();

    void on_btnOpenPort_clicked();
    void on_btnClosePort_clicked();
    void on_btnLedOn_clicked();
    void on_btnLedOff_clicked();
    void on_btnBlockTransferSend_clicked();
    void on_btnBlockTransferReceive_clicked();
    void on_btnTimeSyncSend_clicked();
    void on_btnKeepAliveStart_clicked();
    void on_btnKeepAliveStop_clicked();
    void on_btnSwitchBus_clicked();

private:

    static constexpr uint8_t kLedTcTmCh   = 0;
    static constexpr uint8_t kKeepAliveCh = 0;

    Ui::MainWindow *ui_;
    sky::CAN_TS cants_;
    uint8_t nodeid_ = 0x00;
    QTimer keepAliveTmr_;
    bool portOpened_ = false;
    bool ledTcTmActive_ = false;
};

#endif // MAINWINDOW_H
