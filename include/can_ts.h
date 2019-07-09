/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef CAN_TS_H
#define CAN_TS_H

#include <QObject>
#include <QTimer>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>
#include "cantsframe.h"
#include "commdriver.h"

namespace sky
{

//! Provides functions for communication via CAN bus using CAN TS protocol.
class CAN_TS : public QObject {
    Q_OBJECT

public:

    //! Available CAN buses.
    enum class CanBus {
        CAN0, //!< CAN bus nr. 0.
        CAN1  //!< CAN bus nr. 1.
    };

    //! Provides error status of telecommand transfer.
    enum class SendTCError {
        kSendRequestFailed = 0, //!< Failed to send telecommand transfer request frame.
        kMaxRetriesReached = 1 //!< Maximum number of request retries reached.
    };
    Q_ENUM(SendTCError)

    //! Provides error status of telemetry transfer.
    enum class ReceiveTMError {
        kSendRequestFailed = 0, //!< Failed to send telemetry transfer request frame.
        kMaxRetriesReached = 1 //!< Maximum number of request retries reached.
    };
    Q_ENUM(ReceiveTMError)

    //! Provides error status of data block transmission.
    enum class SendBlockError {
        kSendRequestFailed = 0, //!< Failed to send set block transfer request frame.
        kMaxSendRequestRetriesReached = 1, //!< Maximum number of request retries reached.
        kSendDataFailed = 2, //!< Failed to send data frame.
        kSendStatusRequestFailed = 3, //!< Failed to send status request frame.
        kMaxSendStatusRetriesReached = 4, //!< Maximum number of status request retries reached.
        kMaxReportRetriesReached = 5, //!< Maximum number of data retransmissions and status requests reached.
        kSendAbortFailed = 6, //!< Failed to send abort frame.
        kMaxSendAbortRetriesReached = 7, //!< Maximum number of abort retries reached.
        kAbortNACKReceived = 8 //!< NACK received while waiting for abort ACK.
    };
    Q_ENUM(SendBlockError)

    //! Provides error status of data block reception.
    enum class ReceiveBlockError {
        kSendRequestFailed = 0, //!< Failed to send get block transfer request frame.
        kMaxSendRequestRetriesReached = 1, //!< Maximum number of request retries reached.
        kSendStartFailed = 2, //!< Failed to send start frame.
        kMaxSendStartRetriesReached = 3, //!< Maximum number of start retries reached.
        kSendAbortFailed = 4, //!< Failed to send abort frame.
        kMaxSendAbortRetriesReached = 5, //!< Maximum number of abort retries reached.
        kAbortNACKReceived = 6 //!< NACK received while waiting for abort ACK.
    };
    Q_ENUM(ReceiveBlockError)

    //! Abstract base class for lower-level protocol settings.
    struct DriverSettings {
        virtual ~DriverSettings() = 0;
    };

    //! Lower-level protocol settings in case if CANdelaber is used.
    struct CANdelaber : public DriverSettings {
        std::string port_name_can0 = ""; //!< Serial port used for communication via CAN bus 0.
        std::string port_name_can1 = ""; //!< Serial port used for communication via CAN bus 1.
        uint32_t baud = 0; //!< Serial port baud rate.
    };

    //! Lower-level protocol settings in case if IFboard is used.
    struct IFboard : public DriverSettings {
        uint32_t ip = 0; //!< IP address of IFboard.
        uint16_t port = 0; //!< IFboard port number.
    };

    //! Default constructor. Must be defined to prevent compiler error when using Q_DISABLE_COPY.
    CAN_TS() = default;

    //! Starts communication via CAN bus.
    /*!
        \param address CAN address of the source.
        \param timeout Response timeout in milliseconds.
        \param driver Lower-layer protocol settings.
        \retval true CAN communication established.
        \retval false Cannot start the communication.
    */
    bool Start(uint8_t address, uint32_t timeout, const DriverSettings& driver);

    //! Stops communication via CAN bus.
    void Stop();

    //! Starts sending a telecommand.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Data which shall be transmitted.
        \param retry_count Maximum number of request retries after each timeout before transfer fails.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool SendTC(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data, uint8_t retry_count = 0);

    //! Starts receiving telemetry.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param retry_count Maximum number of request retries after each timeout before transfer fails.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool ReceiveTM(uint8_t address, uint8_t channel, uint8_t retry_count = 3);

    //! Starts sending a block of data.
    /*!
        \param address CAN address of the sink.
        \param start Starting address where data shall be saved at the sink.
        \param data Data which shall be transmitted.
        \param retry_count Maximum number of request retries after each timeout before transfer fails.
        \param report_delay_ms Time delay (in msec) between end of data transmission and status report request.
        \param report_retry_count Maximum number of data retransmissions and status requests before transfer fails.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool SendBlock(uint8_t address, uint64_t start, const std::vector<uint8_t>& data, uint8_t retry_count = 3,
                   uint32_t report_delay_ms = 20, uint8_t report_retry_count = 3);

    //! Starts receiving a block of data.
    /*!
        \param to_address CAN address of the sink.
        \param start_address Starting address at the sink from where the data shall be read.
        \param length Number of data blocks to transfer (8 bytes each).
        \param retry_count Maximum number of request retries after each timeout before transfer fails.
        \param start_retry_count Maximum number of start retries before transfer fails.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool ReceiveBlock(uint8_t to_address, uint64_t start_address, uint8_t length,
                      uint8_t retry_count = 3, uint8_t start_retry_count = 3);

    //! Sends a time synchronisation frame.
    /*!
        \param time Time data which shall be transmitted.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool SendTimeSync(uint64_t time);

    //! Sends unsolicited telemetry.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Data which shall be transmitted.
        \retval true Started transfer.
        \retval false Cannot start transfer.
    */
    bool SendUnsolicited(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);

    //! Returns current active CAN bus.
    /*!
        \return Current active CAN bus.
    */
    CanBus GetActiveBus() const;

    //! Causes toggle active bus between nominal and redundat CAN bus.
    void CanBusSwitch();

    //! Converts CAN TS frame structure to CAN frame structure.
    /*!
        \param can_ts_frame CAN TS frame structure.
        \return CAN frame structure.
    */
    static CanFrame ToCanFrame(const CanTsFrame& can_ts_frame);

    //! Converts CAN frame structure to CAN TS frame structure.
    /*!
        \param can_frame CAN frame structure.
        \return CAN TS frame structure.
    */
    static CanTsFrame FromCanFrame(const CanFrame& can_frame);

    //! Return address of the local CAN-TS node.
    uint8_t GetAddress() const;

signals:

    //! Triggered when telecommand successfuly transmitted.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
    */
    void SendTCCompleted(uint8_t address, uint8_t channel);

    //! Triggered when telemetry successfuly received.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Received data.
    */
    void ReceiveTMCompleted(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);

    //! Triggered when data block successfuly transmitted.
    /*!
        \param address CAN address of the sink.
    */
    void SendBlockCompleted(uint8_t address);

    //! Triggered when data block successfuly received.
    /*!
        \param address CAN address of the sink.
        \param data Received data.
    */
    void ReceiveBlockCompleted(uint8_t address, const std::vector<uint8_t>& data);

    //! Triggered when unsolicited telemetry successfuly received.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Received data.
    */
    void UnsolicitedReceived(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);

    //! Triggered when keep alive frame successfuly received via nominal bus.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Received data.
    */
    void KeepAliveReceivedNominal(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);

    //! Triggered when keep alive frame successfuly received via redundant bus.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param data Received data.
    */
    void KeepAliveReceivedRedundant(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data);

    //! Triggered when time synchronisation frame successfuly transmitted.
    void SendTimeSyncCompleted();

    //! Triggered when time synchronisation frame successfuly received.
    /*!
        \param address CAN address of the sink.
        \param time Received time data.
    */
    void TimeSyncReceived(uint8_t address, const std::vector<uint8_t>& time);

    //! Triggered when unsolicited telemetry successfuly transmitted.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
    */
    void SendUnsolicitedCompleted(uint8_t address, uint8_t channel);

    //! Triggered if telecommand transmission failed.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param error Error code.
    */
    void SendTCFailed(uint8_t address, uint8_t channel, sky::CAN_TS::SendTCError error);

    //! Triggered if telemetry reception failed.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
        \param error Error code.
    */
    void ReceiveTMFailed(uint8_t address, uint8_t channel, sky::CAN_TS::ReceiveTMError error);

    //! Triggered if data block transmission failed.
    /*!
        \param address CAN address of the sink.
        \param error Error code.
    */
    void SendBlockFailed(uint8_t address, sky::CAN_TS::SendBlockError error);

    //! Triggered if data block reception failed.
    /*!
        \param address CAN address of the sink.
        \param error Error code.
    */
    void ReceiveBlockFailed(uint8_t address, sky::CAN_TS::ReceiveBlockError error);

    //! Triggered when time synchronisation frame transmission failed.
    void SendTimeSyncFailed();

    //! Triggered if unsolicited telemetry transmission failed.
    /*!
        \param address CAN address of the sink.
        \param channel Channel number.
    */
    void SendUnsolicitedFailed(uint8_t address, uint8_t channel);

private:
    //! Disable copy and assignment constructors.
    Q_DISABLE_COPY(CAN_TS)

    //! Stores common tranmission state.
    struct Transfer {
        uint8_t address = 0; //!< Address of transfer destination.
        std::shared_ptr<QTimer> watchdog = nullptr; //!< Watchdog timer.
        uint8_t retry_count = 0; //!< Number of request retries.
        uint8_t max_retries = 0; //!< Maximum number of request retries before transfer fails.
        uint8_t channel = 0; //!< Transfer channel number.

        //! Transmission state.
        enum class TxState : uint8_t {
            kIdle, //!< Idle state.
            kSendingRequest //!< Sending request frame.
        } txState = TxState::kIdle;

        //! Reception state.
        enum class RxState : uint8_t {
            kIdle, //!< Idle state.
            kWaitingForRequestACK //!< Waiting for ACK response.
        } rxState = RxState::kIdle;
    };

    //! Stores state of a telecommand transfer.
    struct TelecommandTransfer : Transfer {
        std::vector<uint8_t> data; //!< Data to be transferred.
    };

    //! Stores state of a telemetry transfer.
    struct TelemetryTransfer : Transfer {
    };

    //! Stores common block transmission state.
    struct BlockTransfer {
        uint8_t address = 0; //!< Address of transfer destination.
        std::vector<uint8_t> start; //!< Start address at transfer destination where data is stored.
        std::vector<uint8_t> data; //!< Data to be transferred.
        std::vector<uint8_t> bitmap; //!< Bitmap of data blocks.
        uint8_t blocks = 0; //!< Number of data blocks to be transfered.
        std::shared_ptr<QTimer> watchdog = nullptr; //!< Watchdog timer.
        uint8_t retry_count = 0; //!< Number of request retries.
        uint8_t max_retries = 0; //!< Maximum number of request retransmissions before transfer fails.

        //! Reception state.
        enum class RxState : uint8_t {
            kIdle, //!< Idle state.
            kWaitingForRequestACK, //!< Waiting for ACK after transfer request was sent.
            kWaitingForData, //!< Waiting for data.
            kWaitingForAbortACK //!< Wating for ACK after abort was sent.
        } rxState = RxState::kIdle; //!< Current RX state.

        //! Transmission state.
        enum class TxState : uint8_t {
            kIdle, //!< Idle state.
            kSendingRequest, //!< Sending transfer request frame.
            kSendingStart, //!< Sending start frame.
            kSendingData, //!< Sending data frame.
            kWaitingForSendStatusRequest, //!< Generating delay between data transmission and status request.
            kSendingStatusRequest, //!< Sending status request frame.
            kSendingAbort //!< Sending abort frame.
        } txState = TxState::kIdle; //!< Current TX state.
    };

    //! Stores state of a set block transfer.
    struct SetBlockTransfer : BlockTransfer {
        bool done = false; //!< Indicates if transfer is complete.
        std::shared_ptr<QTimer> report_delay_timer = nullptr; //!< Timer for generation of delay between data transmission and status request.
        uint32_t report_delay = 0; //!< Delay between data transmission and status request.
        uint8_t report_retry_count = 0; //!< Number of data retransmissions and status requests.
        uint8_t max_report_retries = 0; //!< Maximum number of data retransmissions and status requests before transfer fails.
    };

    //! Stores state of a get block transfer.
    struct GetBlockTransfer : BlockTransfer {
        uint8_t start_retry_count = 0; //!< Number of start request retransmissions.
        uint8_t max_start_retries = 0; //!< Maximum number of start request retransmissions before transfer fails.
    };

    std::vector<TelecommandTransfer> tc_transfers_; //!< Outbound telecommand transfers.
    std::vector<TelemetryTransfer> tm_transfers_; //!< Outbound telemetry transfers.
    std::vector<SetBlockTransfer> sb_transfers_; //!< Outbound set block transfers.
    std::vector<GetBlockTransfer> gb_transfers_; //!< Outbound get block transfers.

    uint8_t address_  = 0; //!< Address of the source.
    uint32_t timeout_ = 0; //!< CAN TS transfer response timeout.

    CanBus active_bus_ = CanBus::CAN0; //!< Currently active CAN bus.

    CommDriver com0_; //!< CAN bus 0 comm driver.
    CommDriver com1_; //!< CAN bus 1 comm driver.

    //! Executed when telecommand frame successfuly transmitted by lower-level protocol.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
    */
    void SendTCFrameSent(const CanTsFrame& can_ts_frame);

    //! Executed when telemetry frame successfuly transmitted by lower-level protocol.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
    */
    void ReceiveTMFrameSent(const CanTsFrame& can_ts_frame);

    //! Executed when get block frame successfuly transmitted by lower-level protocol.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
    */
    void ReceiveBlockFrameSent(const CanTsFrame& can_ts_frame);

    //! Executed when set block frame successfuly transmitted by lower-level protocol.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
    */
    void SendBlockFrameSent(const CanTsFrame& can_ts_frame);

    //! Executed when time sync frame successfuly transmitted by lower-level protocol.
    void SendTimeSyncFrameSent();

    //! Executed when unsolicited telemetry frame successfuly transmitted by lower-level protocol.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
    */
    void SendUnsolicitedFrameSent(const CanTsFrame& can_ts_frame);

    //! Executed when an error occured while lower-level protocol was transmitting a telecommand frame.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
        \param error Error code.
    */
    void SendTCFrameSendError(const CanTsFrame& can_ts_frame, CommDriver::CanSendError error);

    //! Executed when an error occured while lower-level protocol was transmitting a telemetry frame.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
        \param error Error code.
    */
    void ReceiveTMFrameSendError(const CanTsFrame& can_ts_frame, CommDriver::CanSendError error);

    //! Executed when an error occured while lower-level protocol was transmitting a set block frame.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
        \param error Error code.
    */
    void SendBlockFrameSendError(const CanTsFrame& can_ts_frame, CommDriver::CanSendError error);

    //! Executed when an error occured while lower-level protocol was transmitting a get block frame.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
        \param error Error code.
    */
    void ReceiveBlockFrameSendError(const CanTsFrame& can_ts_frame, CommDriver::CanSendError error);

    //! Executed when an error occured while lower-level protocol was transmitting a time sync frame.
    /*!
        \param error Error code.
    */
    void SendTimeSyncFrameSendError(CommDriver::CanSendError error);

    //! Executed when an error occured while lower-level protocol was transmitting an unsolicited telemetry frame.
    /*!
        \param can_ts_frame Transmitted CAN TS frame structure.
        \param error Error code.
    */
    void SendUnsolicitedFrameSendError(const CanTsFrame& can_ts_frame, CommDriver::CanSendError error);

    //! Executed when telecommand frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void SendTCFrameReceived(const CanTsFrame& can_ts_frame);

    //! Executed when telemetry frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void ReceiveTMFrameReceived(const CanTsFrame& can_ts_frame);

    //! Executed when set block frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void FrameReceivedSetBlock(const CanTsFrame& can_ts_frame);

    //! Executed when get block frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void ReceiveBlockFrameReceived(const CanTsFrame& can_ts_frame);

    //! Executed when time sync frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void SendTimeSyncFrameReceived(const CanTsFrame& can_ts_frame);

    //! Executed when unsolicited telemetry frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
    */
    void ReceivedUnsolicitedFrame(const CanTsFrame& can_ts_frame);

    //! Executed when keep alive frame successfuly received by lower-level protocol.
    /*!
        \param can_ts_frame Received CAN TS frame structure.
        \param nominal_bus If true, frame received via nominal bus. If false, frame received via redundant bus.
    */
    void ReceivedKeepAliveFrame(const CanTsFrame& can_ts_frame, bool nominal_bus);

    //! Sends a CAN TS frame via nominal CAN bus.
    /*!
        \param frame CAN TS frame.
        \retval true CAN TS frame successfully sent.
        \retval false Cannot send CAN TS frame.
    */
    bool SendFrame(const CanTsFrame& frame);

    //! Retry sending telecommand request.
    /*!
        \param transfer Selected telecommand transfer.
    */
    void SendTCRetry(const std::vector<TelecommandTransfer>::iterator& transfer);

    //! Retry sending telemetry request.
    /*!
        \param transfer Selected telemetry transfer.
    */
    void ReceiveTMRetry(const std::vector<TelemetryTransfer>::iterator& transfer);

    //! Retry sending set block request.
    /*!
        \param transfer Selected set block transfer.
    */
    void SendBlockRetryRequest(const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Retry sending set block status request.
    /*!
        \param transfer Selected set block transfer.
    */
    void SendBlockRetryStatus(const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Retry sending set block abort.
    /*!
        \param transfer Selected set block transfer.
    */
    void SendBlockRetryAbort(const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Retry sending get block request.
    /*!
        \param transfer Selected get block transfer.
    */
    void ReceiveBlockRetryRequest(const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Retry sending get block start.
    /*!
        \param transfer Selected get block transfer.
    */
    void ReceiveBlockRetryStart(const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Retry sending get block abort.
    /*!
        \param transfer Selected get block transfer.
    */
    void ReceiveBlockRetryAbort(const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Process received ACK frame during get block operation.
    void ReceiveBlockFrameReceivedAck(const CanTsFrame& frame, const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Process received NACK frame during get block operation.
    void ReceiveBlockFrameReceivedNack(const CanTsFrame& frame, const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Process received TRANSFER frame during get block operation.
    void ReceiveBlockFrameReceivedTransfer(const CanTsFrame& frame, const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Process send block state after frame sent.
    void SendBlockWaitForResponse(const std::vector<SetBlockTransfer>::iterator& transfer, SetBlockTransfer::RxState rxstate) const;

    //! Process received ACK.
    void SendBlockFrameReceivedAck(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Process received NACK.
    void SendBlockFrameReceivedNack(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Process received REPORT.
    void SendBlockFrameReceivedReport(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer);

private slots:

    //! Triggered when telecommand transmission timeout occurs.
    /*!
        \param transfer Pointer to telecommand transfer structure.
    */
    void SendTCTimeout(const std::vector<TelecommandTransfer>::iterator& transfer);

    //! Triggered when telemetry transmission timeout occurs.
    /*!
        \param transfer Pointer to telemetry transfer structure.
    */
    void ReceiveTMTimeout(const std::vector<TelemetryTransfer>::iterator& transfer);

    //! Triggered when set block transfer timeout occurs.
    /*!
        \param transfer Pointer to set block transfer structure.
    */
    void SendBlockFrameSentTimeout(const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Triggered when set block transfer status report request should be sent.
    /*!
        \param transfer Pointer to set block transfer structure.
    */
    void SendBlockReportRequestDelayTimeout(const std::vector<SetBlockTransfer>::iterator& transfer);

    //! Triggered when get block transfer timeout occurs.
    /*!
        \param transfer Pointer to get block transfer structure.
    */
    void ReceiveBlockFrameSentTimeout(const std::vector<GetBlockTransfer>::iterator& transfer);

    //! Executed when frame successfuly transmitted by lower-level protocol via nominal CAN bus.
    /*!
        \param frame Transmitted CAN frame structure.
    */
    void CanFrameSentNominal(const sky::CanFrame& frame);

    //! Executed when an error occured while lower-level protocol was transmitting a frame via nominal CAN bus.
    /*!
        \param frame CAN frame structure which should have been sent.
        \param error Error code.
    */
    void CanFrameSendErrorNominal(const sky::CanFrame& frame, CommDriver::CanSendError error);

    //! Executed when frame successfuly received by lower-level protocol via nominal CAN bus.
    /*!
        \param frame Received CAN frame structure.
    */
    void CanFrameReceivedNominal(const sky::CanFrame& frame);

    //! Executed when frame successfuly received by lower-level protocol via redundant CAN bus.
    /*!
        \param frame Received CAN frame structure.
    */
    void CanFrameReceivedRedundant(const sky::CanFrame& frame);
};

} // namespace sky

Q_DECLARE_METATYPE(sky::CAN_TS::SendTCError);
Q_DECLARE_METATYPE(sky::CAN_TS::ReceiveTMError);
Q_DECLARE_METATYPE(sky::CAN_TS::SendBlockError);
Q_DECLARE_METATYPE(sky::CAN_TS::ReceiveBlockError);

#endif // CAN_TS_H
