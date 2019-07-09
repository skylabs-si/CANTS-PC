/* See the file "LICENSE.txt" for the full license governing this code. */

#include "cantsframe.h"
#include "cantsutils.h"
#include <cstdint>
#include <QJsonArray>
#include <sstream>
#include <iomanip>
#include <QString>
#include <QDebug>

namespace sky
{

CanTsFrame CanTsFrame::CreateFrameRaw(uint8_t toAddress, uint8_t transferType, uint8_t fromAddress, uint16_t command, const std::vector<uint8_t>& data)
{
    CanTsFrame frame;
    frame.toAddress_ = toAddress;
    frame.type_ = transferType;
    frame.fromAddress_ = fromAddress;
    frame.command_ = command;
    frame.data_ = data;
    return frame;
}

CanTsFrame CanTsFrame::CreateFrameRaw(uint8_t toAddress, TransferType transferType, uint8_t fromAddress, uint16_t command, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(toAddress, static_cast<uint8_t>(transferType), fromAddress, command, data);
}

CanTsFrame CanTsFrame::CreateTelecommand(uint8_t toAddress, uint8_t fromAddress, TelecommandFrameType frameType, uint8_t tcChannel, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(toAddress, CanTsFrame::TransferType::TELECOMMAND, fromAddress, static_cast<uint16_t>((static_cast<uint16_t>(frameType) << 8) | static_cast<uint16_t>(tcChannel)), data);
}

CanTsFrame CanTsFrame::CreateTelecommandRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel, const std::vector<uint8_t>& data)
{
    return CreateTelecommand(toAddress, fromAddress, CanTsFrame::TelecommandFrameType::REQUEST, tcChannel, data);
}

CanTsFrame CanTsFrame::CreateTelecommandAck(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel)
{
    return CreateTelecommand(toAddress, fromAddress, CanTsFrame::TelecommandFrameType::ACK, tcChannel, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateTelecommandNack(uint8_t toAddress, uint8_t fromAddress, uint8_t tcChannel)
{
    return CreateTelecommand(toAddress, fromAddress, CanTsFrame::TelecommandFrameType::NACK, tcChannel, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateTelemetry(uint8_t toAddress, uint8_t fromAddress, CanTsFrame::TelemetryFrameType frameType, uint8_t tmChannel, const std::vector<uint8_t>& data)
{
   return CreateFrameRaw(toAddress, CanTsFrame::TransferType::TELEMETRY, fromAddress, static_cast<uint16_t>((static_cast<uint16_t>(frameType) << 8) | static_cast<uint16_t>(tmChannel)), data);
}

CanTsFrame CanTsFrame::CreateTelemetryRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel)
{
   return CreateTelemetry(toAddress, fromAddress, CanTsFrame::TelemetryFrameType::REQUEST, tmChannel, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateTelemetryAck(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel, const std::vector<uint8_t>& data)
{
   return CreateTelemetry(toAddress, fromAddress, CanTsFrame::TelemetryFrameType::ACK, tmChannel, data);
}

CanTsFrame CanTsFrame::CreateTelemetryNack(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel)
{
   return CreateTelemetry(toAddress, fromAddress, CanTsFrame::TelemetryFrameType::NACK, tmChannel, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateSetBlock(uint8_t toAddress, uint8_t fromAddress, SetBlockFrameType frameType, bool isDone, uint8_t frameNumber, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(toAddress, CanTsFrame::TransferType::SET_BLOCK, fromAddress, static_cast<uint16_t>((static_cast<uint16_t>(frameType) << 7) | (static_cast<uint16_t>(isDone ? 1 : 0) << 6) | static_cast<uint16_t>(frameNumber)), data);
}

CanTsFrame CanTsFrame::CreateSetBlockRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::REQUEST, false, frameNumber, address);
}

CanTsFrame CanTsFrame::CreateSetBlockAck(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::ACK, false, frameNumber, address);
}

CanTsFrame CanTsFrame::CreateSetBlockNack(uint8_t toAddress, uint8_t fromAddress)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::NACK, false, 0, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateSetBlockTransfer(uint8_t toAddress, uint8_t fromAddress, uint8_t sequence, const std::vector<uint8_t>& data)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::TRANSFER, false, sequence, data);
}

CanTsFrame CanTsFrame::CreateSetBlockAbort(uint8_t toAddress, uint8_t fromAddress)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::ABORT, false, 0, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateSetBlockStatus(uint8_t toAddress, uint8_t fromAddress)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::STATUS, false, 0, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateSetBlockReport(uint8_t toAddress, uint8_t fromAddress, bool isDone, const std::vector<uint8_t>& bitmapOfReceivedBlocks)
{
    return CreateSetBlock(toAddress, fromAddress, CanTsFrame::SetBlockFrameType::REPORT, isDone, 0, bitmapOfReceivedBlocks);
}

CanTsFrame CanTsFrame::CreateGetBlock(uint8_t toAddress, uint8_t fromAddress, GetBlockFrameType frameType, uint8_t frameNumber, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(toAddress, CanTsFrame::TransferType::GET_BLOCK, fromAddress, static_cast<uint16_t>((static_cast<uint16_t>(frameType) << 7) | static_cast<uint16_t>(frameNumber)), data);
}

CanTsFrame CanTsFrame::CreateGetBlockRequest(uint8_t toAddress, uint8_t fromAddress, uint8_t blockCount, const std::vector<uint8_t>& address)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::REQUEST, blockCount, address);
}

CanTsFrame CanTsFrame::CreateGetBlockAck(uint8_t toAddress, uint8_t fromAddress, uint8_t frameNumber, const std::vector<uint8_t>& address)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::ACK, frameNumber, address);
}

CanTsFrame CanTsFrame::CreateGetBlockNack(uint8_t toAddress, uint8_t fromAddress)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::NACK, 0, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateGetBlockStart(uint8_t toAddress, uint8_t fromAddress, const std::vector<uint8_t>& bitmapOfBlocksToSend)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::START, 0, bitmapOfBlocksToSend);
}

CanTsFrame CanTsFrame::CreateGetBlockTransfer(uint8_t toAddress, uint8_t fromAddress, uint8_t sequence, const std::vector<uint8_t>& data)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::TRANSFER, sequence, data);
}

CanTsFrame CanTsFrame::CreateGetBlockAbort(uint8_t toAddress, uint8_t fromAddress)
{
    return CreateGetBlock(toAddress, fromAddress, CanTsFrame::GetBlockFrameType::ABORT, 0, std::vector<uint8_t>());
}

CanTsFrame CanTsFrame::CreateUnsolicited(uint8_t toAddress, uint8_t fromAddress, uint8_t tmChannel, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(toAddress, CanTsFrame::TransferType::UNSOLICITED, fromAddress, tmChannel, data);
}

CanTsFrame CanTsFrame::CreateTimeSync(uint8_t fromAddress, const std::vector<uint8_t>& data)
{
    return CreateFrameRaw(static_cast<uint8_t>(CanTsFrame::Address::TIME_SYNC), CanTsFrame::TransferType::TIME_SYNC, fromAddress, 0, data);
}

bool CanTsFrame::IsBroadcastAddress(uint8_t address)
{
    return ((address == static_cast<uint8_t>(CanTsFrame::Address::TIME_SYNC)) ||
            (address == static_cast<uint8_t>(CanTsFrame::Address::KEEP_ALIVE)));
}

QString CanTsFrame::ToQString() const
{    
    return QString("%1 %2 %3 %4 %5")
            .arg(this->toAddress_, 2, 16, QChar('0'))
            .arg(this->type_, 2, 16, QChar('0'))
            .arg(this->fromAddress_, 2, 16, QChar('0'))
            .arg(this->command_, 3, 16, QChar('0'))
            .arg(CanTsUtils::VectorToQString(this->data_))
            .toUpper();
}

CanTsFrame::TelecommandFrameType CanTsFrame::GetFrameType() const
{
    return static_cast<CanTsFrame::TelecommandFrameType>((command_ >> 8) & 3);
}

CanTsFrame::GetBlockFrameType CanTsFrame::GetGBFrameType() const
{
    return static_cast<CanTsFrame::GetBlockFrameType>((command_ >> 7) & 7);
}

CanTsFrame::SetBlockFrameType CanTsFrame::GetSBFrameType() const
{
    return static_cast<CanTsFrame::SetBlockFrameType>((command_ >> 7) & 7);
}

uint8_t CanTsFrame::GetBlockCmdBits() const
{
    return static_cast<uint8_t>(command_ & 0x3F);
}

uint8_t CanTsFrame::GetChannel() const
{
    return static_cast<uint8_t>(command_);
}

uint8_t CanTsFrame::GetFromAddress() const
{
    return fromAddress_;
}

uint8_t CanTsFrame::GetToAddress() const
{
    return toAddress_;
}

std::vector<uint8_t> CanTsFrame::GetData() const
{
    return data_;
}

uint8_t CanTsFrame::GetBlockSequence() const
{
    return static_cast<uint8_t>(command_ & 0x3F);
}

bool CanTsFrame::GetDoneBit() const
{
    return static_cast<bool>((command_ & 0x40) >> 6);
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const CanTsFrame &frame)
{
    dbg.nospace().noquote() << QString("CanTsFrame(toAddress=%1 type=%2 fromAddress=%3 command=%4 data=%5)")
        .arg(frame.toAddress_, 2, 16, QChar('0'))
        .arg(frame.type_, 2, 16, QChar('0'))
        .arg(frame.fromAddress_, 2, 16, QChar('0'))
        .arg(frame.command_, 3, 16, QChar('0'))
        .arg(CanTsUtils::VectorToQString(frame.data_));
    return dbg.maybeSpace();
}
#endif

} // namespace sky
