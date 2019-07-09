/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef CANTSPACKET_H
#define CANTSPACKET_H

#include <cstdint>
#include <vector>
#include <QJsonObject>
#include <QMap>

namespace sky
{

//! Provides functions for creating different CAN TS Frames.
class CanTsFrame
{

public:

    //! Special destination addresses.
    enum class Address : uint8_t {
        TIME_SYNC   = 0x00, //!< Time sync address.
        KEEP_ALIVE  = 0x01  //!< Keep alive address.
    };

    //! Transfer Types of the CAN TS frame.
    enum TransferType : uint8_t {
        TIME_SYNC   = 0x00, //!< Time Sync.
        UNSOLICITED = 0x01, //!< Unsolicited Telemetry.
        TELECOMMAND = 0x02, //!< Telecommand.
        TELEMETRY   = 0x03, //!< Telemetry.
        SET_BLOCK   = 0x04, //!< Set Block.
        GET_BLOCK   = 0x05  //!< Get Block.
    };

    //! Telecommand frame types.
    enum class TelecommandFrameType : uint8_t {
        REQUEST = 0x00, //!< Request.
        ACK     = 0x01, //!< Acknowledge.
        NACK    = 0x02  //!< Negative acknowledge.
    };

    //! Telemetry frame types.
    enum class TelemetryFrameType : uint8_t {
        REQUEST = 0x00, //!< Request.
        ACK     = 0x01, //!< Acknowledge.
        NACK    = 0x02  //!< Negative acknowledge.
    };

    //! Set Block frame types.
    enum class SetBlockFrameType : uint8_t {
        REQUEST   = 0x00, //!< Request.
        TRANSFER  = 0x01, //!< Transfer.
        ACK       = 0x02, //!< Acknowledge.
        ABORT     = 0x03, //!< Abort.
        NACK      = 0x04, //!< Negative acknowledge.
        STATUS    = 0x06, //!< Status request.
        REPORT    = 0x07  //!< Report.
    };

    //! Get Block frame types.
    enum class GetBlockFrameType : uint8_t {
        REQUEST  = 0x00, //!< Request.
        ACK      = 0x02, //!< Acknowledege.
        ABORT    = 0x03, //!< Abort.
        NACK     = 0x04, //!< Negative acknowledge.
        START    = 0x06, //!< Start.
        TRANSFER = 0x07  //!< Transfer.
    };

    uint8_t toAddress_ = 0;     //!< to address of the CanTsFrame
    uint8_t type_ = 0;          //!< Transfer type of the CanTsFrame
    uint8_t fromAddress_ = 0;   //!< from address of the CanTsFrame
    uint16_t command_ = 0;      //!< command of the CanTsFrame
    std::vector<uint8_t> data_; //!< data of the CanTsFrame

    //! Creates a RAW CAN-TS frame from given parameters. Can be used to create non-valid CAN-TS packets packets.
    /*!
        \param toAddress     Destination address
        \param transferType  CAN-TS transfer type
        \param fromAddress   source address
        \param command       the 10-bit command of the CAN-TS packet
        \param data          data vector
    */
    static CanTsFrame CreateFrameRaw(uint8_t toAddress, uint8_t transferType, uint8_t fromAddress, uint16_t command, const std::vector<uint8_t>& data);

    //! Creates a RAW CAN-TS frame from given parameters. Can be used to create non-valid CAN-TS packets packets.
    /*!
        \param toAddress     Destination address
        \param transferType  CAN-TS transfer type
        \param fromAddress   source address
        \param command       the 10-bit command of the CAN-TS packet
        \param data          data vector
    */
    static CanTsFrame CreateFrameRaw(uint8_t toAddress, TransferType transferType, uint8_t fromAddress, uint16_t command, const std::vector<uint8_t>& data);

    //! Creates a telecommand from given parameters.
    static CanTsFrame CreateTelecommand(uint8_t toAddress, uint8_t fromAddress, TelecommandFrameType frameType, uint8_t tcChannel, const std::vector<uint8_t>& data);

    //! Creates a telecommand Request from given parameters.
    static CanTsFrame CreateTelecommandRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel, const std::vector<uint8_t>& data);

    //! Creates a telecommand Acknowledge from given parameters.
    static CanTsFrame CreateTelecommandAck(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel);

    //! Creates a telecommand Negative acknowledge from given parameters.
    static CanTsFrame CreateTelecommandNack(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel);

    //! Creates a Telemetry from given parameters.
    static CanTsFrame CreateTelemetry(uint8_t toAddress, uint8_t fromAddress, TelemetryFrameType frameType, uint8_t tmChannel, const std::vector<uint8_t>& data);

    //! Creates a Telemetry Request from given parameters.
    static CanTsFrame CreateTelemetryRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel);

    //! Creates a Telemetry Acknownledge from given parameters.
    static CanTsFrame CreateTelemetryAck(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel, const std::vector<uint8_t>& data);

    //! Creates a Telemetry Negative acknowledge from given parameters.
    static CanTsFrame CreateTelemetryNack(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel);

    //! Creates a Set Block from given parameters.
    static CanTsFrame CreateSetBlock(uint8_t toAddress, uint8_t fromAddress, SetBlockFrameType frameType, bool isDone, uint8_t frameNumber, const std::vector<uint8_t>& data);

    //! Creates a Set Block Request from given parameters.
    static CanTsFrame CreateSetBlockRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address);

    //! Creates a Set Block Acknowledge from given parameters.
    static CanTsFrame CreateSetBlockAck(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address);

    //! Creates a Set Block Negative acknowledge from given parameters.
    static CanTsFrame CreateSetBlockNack(uint8_t toAddress, uint8_t fromAddress);

    //! Creates a Set Block Transfer from given parameters.
    static CanTsFrame CreateSetBlockTransfer(uint8_t toAddress, uint8_t fromAddress, uint8_t sequence, const std::vector<uint8_t>& data);

    //! Creates a Set Block Abort from given parameters.
    static CanTsFrame CreateSetBlockAbort(uint8_t toAddress, uint8_t fromAddress);

    //! Creates a Set Block Status request from given parameters.
    static CanTsFrame CreateSetBlockStatus(uint8_t toAddress, uint8_t fromAddress);

    //! Creates a Set Block Status Report from given parameters.
    static CanTsFrame CreateSetBlockReport(uint8_t toAddress, uint8_t fromAddress, bool isDone, const std::vector<uint8_t>& bitmapOfReceivedBlocks);

    //! Creates a Get Block from given parameters.
    static CanTsFrame CreateGetBlock(uint8_t toAddress, uint8_t fromAddress, GetBlockFrameType frameType, uint8_t frameNumber, const std::vector<uint8_t>& data);

    //! Creates a Get Block Request from given parameters.
    static CanTsFrame CreateGetBlockRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t blockCount, const std::vector<uint8_t>& address);

    //! Creates a Get Block Acknowledge from given parameters.
    static CanTsFrame CreateGetBlockAck(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address);

    //! Creates a Get Block Negative acknowledge from given parameters.
    static CanTsFrame CreateGetBlockNack(uint8_t toAddress, uint8_t fromAddress);

    //! Creates a Get Block Start from given parameters.
    static CanTsFrame CreateGetBlockStart(uint8_t toAddress, uint8_t fromAddress, const std::vector<uint8_t>& bitmapOfBlocksToSend);

    //! Creates a Get Block Transfer from given parameters.
    static CanTsFrame CreateGetBlockTransfer(uint8_t toAddress, uint8_t fromAddress, uint8_t sequence, const std::vector<uint8_t>& data);

    //! Creates a Get Block Abort from given parameters.
    static CanTsFrame CreateGetBlockAbort(uint8_t toAddress, uint8_t fromAddress);

    //! Creates any Unsolicited Telemetry frame from given parameters.
    static CanTsFrame CreateUnsolicited(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel, const std::vector<uint8_t>& data);

    //! Creates Time Sync from given parameters.
    static CanTsFrame CreateTimeSync(uint8_t fromAddress, const std::vector<uint8_t>& data);

    //! Check if \a address is valid broadcast address (time sync or keep alive address).
    static bool IsBroadcastAddress(uint8_t address);

    //! Returns telecommand frametype from frame.
    TelecommandFrameType GetFrameType() const;

    //! Returns 'get block' frametype from frame.
    GetBlockFrameType GetGBFrameType() const;

    //! Returns 'set block' frametype from frame.
    SetBlockFrameType GetSBFrameType() const;

    //! Returns sequence of a frame.
    uint8_t GetBlockSequence() const;

    //! Returns 'get block' command bits from frame.
    uint8_t GetBlockCmdBits() const;

    //! Returns telecommand channel.
    uint8_t GetChannel() const;

    //! Returns source (from) address.
    uint8_t GetFromAddress() const;

    //! Returns destination (to) address.
    uint8_t GetToAddress() const;

    //! Returns data bytes.
    std::vector<uint8_t> GetData() const;

    //! Returns frame done bit.
    bool GetDoneBit() const;

    //! Convert frame to QString representation.
    QString ToQString() const;
};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const CanTsFrame &message);
#endif

} // namespace sky

#endif // CANTSFRAME_H
