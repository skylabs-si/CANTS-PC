/* See the file "LICENSE.txt" for the full license governing this code. */

#include "commdriver.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(com, "sky::commdriver")

namespace sky {

CommDriver::CommDriver()
{
    connect(&serial_port_, &QSerialPort::readyRead,
            this, &CommDriver::BytesRead, Qt::QueuedConnection);
    connect(&serial_port_, &QSerialPort::errorOccurred,
            this, &CommDriver::HandleSerialError, Qt::QueuedConnection);
    connect(&serial_port_, &QSerialPort::bytesWritten,
            this, &CommDriver::BytesWritten, Qt::QueuedConnection);
    connect(&slip_, &SkySlip::FrameReceived,
            this, &CommDriver::SlipFrameReceived, Qt::QueuedConnection);

    connect(&tmr, &QTimer::timeout, this, &CommDriver::WriteTimeout, Qt::QueuedConnection);
    tmr.setInterval(kWriteTimeoutMs);
    tmr.setSingleShot(true);
}

bool CommDriver::Open(const std::string& port_name, uint32_t baud)
{
    qCDebug(com) << "Open" << QString::fromStdString(port_name) << baud;

    if (serial_port_.isOpen())
        return false;

#if DEVICE_SPACE_QUERY
    free_space_ = 0;
#endif

    serial_port_.setPortName(QString::fromStdString(port_name));
    serial_port_.setBaudRate(static_cast<int32_t>(baud));
    serial_port_.setStopBits(QSerialPort::OneStop);
    serial_port_.setParity(QSerialPort::NoParity);
    serial_port_.setDataBits(QSerialPort::Data8);
    serial_port_.setFlowControl(QSerialPort::NoFlowControl);

    return serial_port_.open(QSerialPort::ReadWrite);
}

void CommDriver::Close()
{
    qCDebug(com) << "Close";
    tx_buffer.clear();
    serial_port_.close();
}

bool CommDriver::WriteBytes(const std::vector<uint8_t>& data)
{
    tmr.start();
    qint64 ret = serial_port_.write(reinterpret_cast<const char*>(data.data()),
                                    static_cast<qint64>(data.size()));
    serial_port_.flush();
    return (ret == static_cast<qint64>(data.size()));
}

void CommDriver::WriteTimeout()
{
    qCCritical(com) << "Write timeout in state" << static_cast<uint8_t>(state_)
                    << "send_retry" << send_retry_;

    if (state_ == TxState::WaitForWrite) {
        if (send_retry_--) { // Retransmit last packet.
           WriteBytes(last_slip_frame_);
        } else { // Not all bytes sent successfully after multiple retries.
           emit CanFrameError(last_can_frame_, CanSendError::WriteError);
           state_ = TxState::Idle;
        }
    }
#if DEVICE_SPACE_QUERY
    else if (state_ == TxState::WaitForFreeSpace) {

        if (send_retry_--) {
            // Retransmit available space frame.
            WriteBytes(slip_space_);
        } else {
            // Not all bytes sent successfully after multiple retries.
            emit CanFrameError(last_can_frame_, CanSendError::DongleBusy);
            state_ = TxState::Idle;
        }
    }
#endif
}

bool CommDriver::Send(const CanFrame& frame)
{
    if (!serial_port_.isOpen())
        return false;

    if (state_ != TxState::Idle) {
        tx_buffer.push_back(frame);
    } else {
        WritePacket(frame);
    }

    return true;
}

void CommDriver::WritePacket(const CanFrame& frame)
{
    send_retry_ = kSendRetryNum;
    last_can_frame_ = frame;
    last_slip_frame_ = slip_.Encode(sky::SkySlip::Cmd::SendCan0, frame.ToStdVector());

#if DEVICE_SPACE_QUERY
    if (free_space_ < last_slip_frame_.size()) {
        qCDebug(com, "Requesting available space on dongle");
        state_ = TxState::WaitForFreeSpace;
        WriteBytes(slip_space_);
    } else {
        state_ = TxState::WaitForWrite;
        free_space_ -= static_cast<uint16_t>(last_slip_frame_.size());
        WriteBytes(last_slip_frame_);
    }
#else
    state_ = TxState::WaitForWrite;
    WriteBytes(last_slip_frame_);
#endif
}

void CommDriver::BytesRead()
{
    QByteArray b = serial_port_.readAll();
    qCDebug(com) << "Bytes received" << b.size() << b.toHex();
    slip_.Decode(std::vector<uint8_t>(b.begin(), b.end()));
}

void CommDriver::BytesWritten(qint64 bytes)
{
    assert(TxState::Idle != state_);

    // Successfull transmission of last frame.
    if (TxState::WaitForWrite == state_ &&
        bytes == static_cast<qint64>(last_slip_frame_.size())) {
        tmr.stop();
        qCDebug(com) << "Bytes sent" << bytes << QByteArray(reinterpret_cast<const char*>(last_slip_frame_.data()),
                                                              static_cast<int>(last_slip_frame_.size())).toHex();
        emit CanFrameSent(last_can_frame_);
        state_ = TxState::Idle;

        // Send buffered packets.
        if (!tx_buffer.empty()) {
            WritePacket(tx_buffer.front());
            tx_buffer.erase(tx_buffer.begin());
        }
    } else {
        qCDebug(com) << "Bytes sent" << bytes;
    }

    // In case of an transmit error, WriteTimout shall be called
    // and retransmit shall be executed.
}

void CommDriver::HandleSerialError(QSerialPort::SerialPortError serialPortError)
{
    qCDebug(com) << "Serial port error =" << serialPortError;
    emit SerialError(serialPortError);
}

void CommDriver::SlipFrameReceived(SkySlip::Frame slip)
{
    if (slip.cmd == SkySlip::Cmd::SendCan0 ||
        slip.cmd == SkySlip::Cmd::SendCan1) {

        emit RawFrameReceived(slip.payload);

        if (slip.payload.size() > 4) {
            CanFrame frame_rcv_ = CanFrame::FromStdVector(slip.payload);
            emit CanFrameReceived(frame_rcv_);
        }

    }
#if DEVICE_SPACE_QUERY
    else if (slip.cmd == SkySlip::Cmd::DongleReport &&
               slip.payload.size() == 2) {

        // Process available space response from communication dongle (CANdelaber, USB2CAN).

        free_space_ = static_cast<uint16_t>(slip.payload[1]<<8 | slip.payload[0]);
        qCDebug(com, "Dongle free space reported %d", free_space_);

        if (free_space_ > last_slip_frame_.size()) {
            // If enough available space on dongle, retransmit last frame.
            state_ = TxState::WaitForWrite;
            free_space_ -= static_cast<uint16_t>(last_slip_frame_.size());
            WriteBytes(last_slip_frame_);
        } else if (send_retry_--) {
            // If stil not enough space, retry available space query.
            qCDebug(com, "Requesting available space on dongle");
            state_ = TxState::WaitForFreeSpace;
            WriteBytes(slip_space_);
        } else {
            // After multiple retries still no available space on dongle. Signal DongleBusy and stop.
            emit CanFrameError(last_can_frame_, CanSendError::DongleBusy);
            state_ = TxState::Idle;
        }
    }
#endif
}

std::string CommDriver::GetPortName() const
{
    return serial_port_.portName().toStdString();
}

} // namespace sky
