/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef COMMDRIVER_H
#define COMMDRIVER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QTimer>
#include <cstdint>
#include <memory>
#include "skyslip.h"
#include "canframe.h"

namespace sky {

#define DEVICE_SPACE_QUERY (0)

/*! Communication driver for CANdelaber and USB2CAN device.

    Devices act as a bridge between PC (Serial Port) and CAN network.

    Data is transmitted with Send method. If device is currently busy,
    sent packet is buffered inside driver and transmitted at first oportunity.
    If after multiple retries, data still can't be transmitted error signal
    is returned.

    Data is received via CanFrameReceived signal.

    Signal CanBusError is emited if error is detected on CAN bus.
*/
class CommDriver : public QObject
{
    Q_OBJECT

public:

    //! This enum describes errors that occur during frame transmission.
    enum CanSendError {
        WriteError, //!< Enough space on dongle, but PC could not write all bytes.
        DongleBusy  //!< Not enough space on dongle.
    };

    //! This enum describes state of transmission.
    enum class TxState {
        Idle,             //!< Ready to write.
        WaitForFreeSpace, //!< Waiting for dongle to report available space in dongle buffer.
        WaitForWrite      //!< Waiting for write results (operation in progress).
    };

    //! Default constructor connects internal signals and slots.
    CommDriver();

    //! Open serial port \a port_name with baud rate \a baud.
    bool Open(const std::string& port_name, uint32_t baud);

    //! Close active connection.
    void Close();

    /*!
      Method sends \a frame to remote unit.

      If frame was written to serial port, this function returns \c true;
      otherwise returns \c false. After a while, signal CanFrameSent() or
      CanFrameError() is emitted.
    */
    bool Send(const CanFrame& frame);

    //! Returns the driver port name
    std::string GetPortName() const;

signals:
    //! Signal emits when CAN \a frame was received and parsed.
    void CanFrameReceived(sky::CanFrame frame);

    //! Signal emits when CAN \a frame was successfully sent.
    void CanFrameSent(const sky::CanFrame& frame);

    /*!
      Signal emits after \a error occured during \a frame transmission
      and its retransmission also has failed.
    */
    void CanFrameError(const sky::CanFrame& frame, sky::CommDriver::CanSendError error);

    //! Signal emits when \a data (raw) frame was received on CAN bus.
    void RawFrameReceived(const std::vector<uint8_t>& data);

    //! Signal emits when \a error was detected on serial port.
    void SerialError(QSerialPort::SerialPortError error);

private slots:
    //! Slot process \a serialPortError.
    void HandleSerialError(QSerialPort::SerialPortError error);

    //! Slot is called after \a bytes is written to serial port.
    void BytesWritten(qint64 bytes);

    //! Slot is called when new data is available for reading from serial port.
    void BytesRead();

    //! Slot process received \a slip frame.
    void SlipFrameReceived(SkySlip::Frame slip);

    //! Slot process timeout to Write function.
    void WriteTimeout();

private:

    Q_DISABLE_COPY(CommDriver)

    TxState state_ = TxState::Idle; //! State of transmission.
    QSerialPort serial_port_; //! Object for serial port operations.

#if DEVICE_SPACE_QUERY
    uint16_t free_space_ = 0; //! Available space in communication dongle (CANdelaber or USB2CAN) internal receive buffer.
    const std::vector<uint8_t> slip_space_ = {0xC0, 0x02, 0xC0}; //! Dongle free space SLIP frame request.
#endif

    SkySlip slip_; //! Slip encoder/decoder object.

    std::vector<CanFrame> tx_buffer; //! Transmit buffer.
    uint8_t send_retry_ = 0;  //! Number of transmit retries to dongle.

    CanFrame last_can_frame_; //! Saved last transmitted CanFrame.
    std::vector<uint8_t> last_slip_frame_; //! Saved current 'transmitting' SLIP frame. Used also during retransmission.

    static constexpr uint8_t kWriteTimeoutMs = 200; //! Write timeout in ms.
    static constexpr uint8_t kSendRetryNum = 3; //! Number of send retries.

    QTimer tmr; //! Internal timer used for write timeout.

    //! Writes bytes in \a data to opened serial port.
    bool WriteBytes(const std::vector<uint8_t>& data);

    //! Writes \a frame to opened serial port.
    void WritePacket(const CanFrame& frame);
};

} // namespace sky

Q_DECLARE_METATYPE(sky::CanFrame);
Q_DECLARE_METATYPE(sky::CommDriver::CanSendError);

#endif
