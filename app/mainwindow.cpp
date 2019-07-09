/* See the file "LICENSE.txt" for the full license governing this code. */

#include <QMessageBox>
#include <QByteArray>
#include <cmath>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "can_ts.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui_(new Ui::MainWindow)
{
    ui_->setupUi(this);

    connect(&cants_, &sky::CAN_TS::SendTCCompleted, this, &MainWindow::cants_SendTCCompleted, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::ReceiveTMCompleted, this, &MainWindow::cants_ReceiveTMCompleted, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::SendBlockCompleted, this, &MainWindow::cants_SendBlockCompleted, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::ReceiveBlockCompleted, this, &MainWindow::cants_ReceiveBlockCompleted, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::SendTCFailed, this, &MainWindow::cants_SendTCFailed, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::ReceiveTMFailed, this, &MainWindow::cants_ReceiveTMFailed, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::SendBlockFailed, this, &MainWindow::cants_SendBlockFailed, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::ReceiveBlockFailed, this, &MainWindow::cants_ReceiveBlockFailed, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::SendUnsolicitedFailed, this, &MainWindow::cants_SendUnsolicitedFailed, Qt::QueuedConnection);
    connect(&cants_, &sky::CAN_TS::SendTimeSyncFailed, this, &MainWindow::cants_SendTimeSyncFailed, Qt::QueuedConnection);
}

MainWindow::~MainWindow()
{
    delete ui_;
}

void MainWindow::cants_SendTCCompleted(uint8_t address, uint8_t channel)
{
    if ((address == nodeid_) && (channel == kLedTcTmCh)) {
        if (!cants_.ReceiveTM(nodeid_, 0x00)) {
            ledTcTmActive_ = false;
            ui_->lblLedStatus->setText("Unknown");
            QMessageBox::critical(this, "Error", "Can't receive CAN-TS telemetry.");
        }
    }
}

void MainWindow::cants_ReceiveTMCompleted(uint8_t address, uint8_t channel, const std::vector<uint8_t> &data)
{
    if ((address == nodeid_) && (channel == kLedTcTmCh)) {
        ledTcTmActive_ = false;

        if (data.size() != 1) {
            ui_->lblLedStatus->setText("Unknown");
            QMessageBox::critical(this, "Error", "Invalid CAN-TS telemetry received. Check your firmware!");
            return;
        }

        if (data[0] & 0x01)
            ui_->lblLedStatus->setText("LED On");
        else
            ui_->lblLedStatus->setText("LED Off");
    }
}

void MainWindow::cants_SendBlockCompleted(uint8_t address)
{
    if (address == nodeid_) {
        QMessageBox::information(this, "Success", "Send block successfully completed.");
    }
}

void MainWindow::cants_ReceiveBlockCompleted(uint8_t address, std::vector<uint8_t> data)
{
    if (address == nodeid_) {

        QByteArray data_temp = QByteArray(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
        ui_->txtBlockData->setPlainText(data_temp.toHex());
    }
}

void MainWindow::cants_SendTCFailed(uint8_t address, uint8_t channel, sky::CAN_TS::SendTCError error)
{
    if ((address == nodeid_) && (channel == kLedTcTmCh)) {
        ledTcTmActive_ = false;
        QMessageBox::critical(this, "Error", QString("Failed sending CAN-TS telecommand (error code: %1).").arg(static_cast<int>(error)));
    }
}

void MainWindow::cants_ReceiveTMFailed(uint8_t address, uint8_t channel, sky::CAN_TS::ReceiveTMError error)
{
    if ((address == nodeid_) && (channel == kLedTcTmCh)) {
        ledTcTmActive_ = false;
        ui_->lblLedStatus->setText("Unknown");
        QMessageBox::critical(this, "Error", QString("Failed receiving CAN-TS telemetry (error code: %1)").arg(static_cast<int>(error)));
    }
}

void MainWindow::cants_SendBlockFailed(uint8_t address, sky::CAN_TS::SendBlockError error)
{
    if (address == nodeid_) {
        QMessageBox::critical(this, "Error", QString("Failed to send CAN-TS data block (error code: %1)").arg(static_cast<int>(error)));
    }
}

void MainWindow::cants_ReceiveBlockFailed(uint8_t address, sky::CAN_TS::ReceiveBlockError error)
{
    if (address == nodeid_) {
        QMessageBox::critical(this, "Error", QString("Failed to receive CAN-TS data block (error code: %1)").arg(static_cast<int>(error)));
    }
}

void MainWindow::cants_SendUnsolicitedFailed(uint8_t address, uint8_t channel)
{
    if ((address == nodeid_) && (channel == kKeepAliveCh)) {
        keepAliveTmr_.stop();
        QMessageBox::critical(this, "Error", "Failed to send CAN-TS keep alive signal.");
    }
}

void MainWindow::cants_SendTimeSyncFailed()
{
    QMessageBox::critical(this, "Error", "Failed to send CAN-TS time sync timestamp.");
}

void MainWindow::keepAliveTmr_timeout()
{
    if (!cants_.SendUnsolicited(static_cast<uint8_t>(sky::CanTsFrame::Address::KEEP_ALIVE),
                                kKeepAliveCh, std::vector<uint8_t>())) {
        keepAliveTmr_.stop();
        QMessageBox::critical(this, "Error", "Can't send CAN-TS keep alive signal.");
    }
}

void MainWindow::on_btnOpenPort_clicked()
{
    if (!portOpened_) {
        bool ok = false;
        uint32_t timeout = static_cast<uint32_t>(ui_->txtCanTsTimeout->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert CAN-TS timeout input to number. Input string must be in decimal format.");
            return;
        }

        sky::CAN_TS::CANdelaber can;
        can.port_name_can0 = ui_->txtNominalBus->text().toStdString();
        can.port_name_can1 = ui_->txtRedundantBus->text().toStdString();
        can.baud = static_cast<uint32_t>(ui_->txtBaudRate->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert serial baudrate input to number. Input string must be in decimal format.");
            return;
        }

        uint8_t localnodeid = static_cast<uint8_t>(ui_->txtCanTsLocalAddress->text().toUInt(&ok, 16));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert local node CAN-TS address. Input string must be in hexadecimal format.");
            return;
        }

        nodeid_ = static_cast<uint8_t>(ui_->txtCanTsRemoteAddress->text().toUInt(&ok, 16));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert remote node CAN-TS address Input string must be in hexadecimal format..");
            return;
        }

        ok = cants_.Start(localnodeid, timeout, can);
        if (!ok) {
            QMessageBox::critical(this, "Error", "Port open failed.");
            return;
        }

        ui_->lblConnStatus->setText("Open");
        ui_->lblActiveBus->setText("Bus 0 (N)");
        portOpened_ = true;
    }
}

void MainWindow::on_btnClosePort_clicked()
{
    portOpened_ = false;
    ledTcTmActive_ = false;

    keepAliveTmr_.stop();
    cants_.Stop();

    ui_->lblActiveBus->setText("N/A");
    ui_->lblConnStatus->setText("Closed");
}

void MainWindow::on_btnLedOn_clicked()
{
    if (!ledTcTmActive_ && portOpened_) {

        if (!cants_.SendTC(nodeid_, kLedTcTmCh, std::vector<uint8_t>({0x01}))) {
            QMessageBox::critical(this, "Error", "Can't send CAN-TS telecommand.");
            return;
        }

        ledTcTmActive_ = true;
    }
}

void MainWindow::on_btnLedOff_clicked()
{
    if (!ledTcTmActive_ && portOpened_) {

        if (!cants_.SendTC(nodeid_, kLedTcTmCh, std::vector<uint8_t>({0x00}))) {
            QMessageBox::critical(this, "Error", "Can't send CAN-TS telecommand.");
            return;
        }

        ledTcTmActive_ = true;
    }
}

void MainWindow::on_btnBlockTransferSend_clicked()
{
    if (portOpened_) {

        bool ok = false;
        uint16_t blockStartAddr = static_cast<uint16_t>(ui_->txtBlockAddress->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert remote node memory location. Input string must be in decimal format.");
            return;
        }

        uint8_t numFrames = static_cast<uint8_t>(ui_->txtNumFrames->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert number of blocks input. Input string must be in decimal format.");
            return;
        } else if (0 == numFrames) {
            QMessageBox::critical(this, "Error", "At least 1 frame must be transmitted");
            return;
        }

        QByteArray data_temp = QByteArray::fromHex(ui_->txtBlockData->toPlainText().toUtf8());
        std::vector<uint8_t> dataBlock = std::vector<uint8_t>(data_temp.begin(), data_temp.end());

        if(std::ceil(dataBlock.size()/8.0) != numFrames) {
            QMessageBox::critical(this, "Error", "Number of frames should match with data size to transmit.");
            return;
        }

        if (!cants_.SendBlock(nodeid_, blockStartAddr, dataBlock)) {
            QMessageBox::critical(this, "Error", "Can't send CAN-TS data block.");
            return;
        }
    }
}

void MainWindow::on_btnBlockTransferReceive_clicked()
{
    if (portOpened_) {

        bool ok = false;
        uint16_t blockStartAddr = static_cast<uint16_t>(ui_->txtBlockAddress->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert remote node memory location. Input string must be in decimal format.");
            return;
        }

        uint8_t numFrames = static_cast<uint8_t>(ui_->txtNumFrames->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert number of blocks input. Input string must be in decimal format.");
            return;
        } else if (0 == numFrames) {
            QMessageBox::critical(this, "Error", "At least 1 frame must be received");
            return;
        }

        if (!cants_.ReceiveBlock(nodeid_, blockStartAddr, numFrames)) {
            QMessageBox::critical(this, "Error", "Can't receive CAN-TS data block.");
        }
    }
}

void MainWindow::on_btnTimeSyncSend_clicked()
{
    if (portOpened_) {
        bool ok = false;
        uint64_t time = static_cast<uint64_t>(ui_->txtTimeSyncTimestamp->text().toULongLong(&ok, 16));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert time sync timestamp input to integer. Input string must be in hexadecimal format.");
            return;
        }

        if (!cants_.SendTimeSync(time)) {
            QMessageBox::critical(this, "Error", "Can't send CAN-TS time sync timestamp.");
            return;
        }
    }
}

void MainWindow::on_btnKeepAliveStart_clicked()
{
    if (portOpened_) {
        bool ok = false;
        uint16_t keepAlivePeriod = static_cast<uint16_t>(ui_->txtKeepAlivePeriod->text().toUInt(&ok, 10));
        if (!ok) {
            QMessageBox::critical(this, "Error", "Can't convert keep alive period input to integer. Input string must be in decimal format.");
            return;
        }

        connect(&keepAliveTmr_, &QTimer::timeout, this, &MainWindow::keepAliveTmr_timeout, Qt::QueuedConnection);
        keepAliveTmr_.start(keepAlivePeriod);
    }
}

void MainWindow::on_btnKeepAliveStop_clicked()
{
    keepAliveTmr_.stop();
}

void MainWindow::on_btnSwitchBus_clicked()
{
    cants_.CanBusSwitch();

    if (cants_.GetActiveBus() == sky::CAN_TS::CanBus::CAN0) {
        ui_->lblActiveBus->setText("Bus 0 (N)");
    } else {
        ui_->lblActiveBus->setText("Bus 1 (R)");
    }
}
