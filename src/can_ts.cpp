/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>
#include <memory>

Q_LOGGING_CATEGORY(cants, "sky::CAN_TS")

namespace sky
{

// NOTE
// This implementation of pure virtual destructor of abstract class
// enables polymorphism for low-level driver settings and prevents
// DriverSettings object instantiation. DO NOT REMOVE!
CAN_TS::DriverSettings::~DriverSettings() = default;

bool CAN_TS::Start(uint8_t address, uint32_t timeout, const DriverSettings& driver)
{
    if (CanTsFrame::IsBroadcastAddress(address)) {
        qCCritical(cants) << "Invalid address" << address;
        return false;
    }

    address_ = address;
    timeout_ = timeout;

    auto candelaber = dynamic_cast<const CANdelaber*>(&driver);
    if (candelaber) {
        if (!com0_.Open(candelaber->port_name_can0, candelaber->baud)) {
            qCCritical(cants) << "Port open failed" << candelaber->port_name_can0.data();
            return false;
        }

        if (!com1_.Open(candelaber->port_name_can1, candelaber->baud)) {
            qCCritical(cants) << "Port open failed" << candelaber->port_name_can1.data();
            return false;
        }

        // Set CAN0 as nominal bus.
        active_bus_ = CanBus::CAN0;

        // Initialise nominal bus.
        connect(&com0_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal, Qt::QueuedConnection);
        connect(&com0_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal, Qt::QueuedConnection);
        connect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal, Qt::QueuedConnection);

        // Initialise redundant bus.
        connect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant, Qt::QueuedConnection);

        qCDebug(cants) << "Started CAN-TS stack (using candelaber) with address =" << address << "timeout =" << timeout;
        return true;
    } else {
        return false;
    }
}

void CAN_TS::Stop()
{
    // Uninitialise nominal and redundant bus signals.
    if (active_bus_ == CanBus::CAN0) {
        disconnect(&com0_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant);
    } else {
        disconnect(&com1_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant);
    }

    tc_transfers_.clear();
    tm_transfers_.clear();
    sb_transfers_.clear();
    gb_transfers_.clear();

    com0_.Close();
    com1_.Close();

    qCDebug(cants) << "Stopped CAN-TS stack";
}

CAN_TS::CanBus CAN_TS::GetActiveBus() const
{
    return active_bus_;
}

void CAN_TS::CanBusSwitch()
{
    tc_transfers_.clear();
    tm_transfers_.clear();
    sb_transfers_.clear();
    gb_transfers_.clear();

    // Uninitialise nominal and  bus signals.
    if (active_bus_ == CanBus::CAN0) {
        disconnect(&com0_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant);
    } else {
        disconnect(&com1_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal);
        disconnect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal);
        disconnect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant);
    }

    // Switch buses.
    if (active_bus_ == CanBus::CAN0)
        active_bus_ = CanBus::CAN1;
    else
        active_bus_ = CanBus::CAN0;

    // Initialise nominal and redundant bus signals.
    if (active_bus_ == CanBus::CAN0) {
        connect(&com0_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal, Qt::QueuedConnection);
        connect(&com0_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal, Qt::QueuedConnection);
        connect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal, Qt::QueuedConnection);
        connect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant, Qt::QueuedConnection);
    } else {
        connect(&com1_, &sky::CommDriver::CanFrameSent, this, &sky::CAN_TS::CanFrameSentNominal, Qt::QueuedConnection);
        connect(&com1_, &sky::CommDriver::CanFrameError, this, &sky::CAN_TS::CanFrameSendErrorNominal, Qt::QueuedConnection);
        connect(&com1_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedNominal, Qt::QueuedConnection);
        connect(&com0_, &sky::CommDriver::CanFrameReceived, this, &sky::CAN_TS::CanFrameReceivedRedundant, Qt::QueuedConnection);
    }

    qCDebug(cants) << "Bus switched";
}

CanFrame CAN_TS::ToCanFrame(const CanTsFrame& can_ts_frame)
{
    CanFrame can_frame;

    can_frame.id = static_cast<uint32_t>(can_ts_frame.command_) |
            static_cast<uint32_t>(can_ts_frame.fromAddress_ << 10) |
            static_cast<uint32_t>(can_ts_frame.type_ << 18) |
            static_cast<uint32_t>(can_ts_frame.toAddress_ << 21);

    can_frame.data = can_ts_frame.data_;
    can_frame.extid = true;
    can_frame.rtr = false;
    return can_frame;
}

CanTsFrame CAN_TS::FromCanFrame(const CanFrame& can_frame)
{
    CanTsFrame can_ts_frame;
    can_ts_frame.command_ = static_cast<uint16_t>(can_frame.id & 0x3FF);
    can_ts_frame.fromAddress_ = static_cast<uint8_t>((can_frame.id >> 10) & 0xFF);
    can_ts_frame.type_ = static_cast<uint8_t>((can_frame.id >> 18) & 0x07);
    can_ts_frame.toAddress_ = static_cast<uint8_t>((can_frame.id >> 21) & 0xFF);
    can_ts_frame.data_ = can_frame.data;
    return can_ts_frame;
}

bool CAN_TS::SendFrame(const CanTsFrame& frame)
{
    qCDebug(cants) << "Sending frame" << frame;

    if (active_bus_ == CanBus::CAN0)
        return com0_.Send(ToCanFrame(frame));
    else
        return com1_.Send(ToCanFrame(frame));
}

void CAN_TS::CanFrameSentNominal(const CanFrame& frame)
{
    CanTsFrame can_ts_frame = FromCanFrame(frame);

    // Sanity checks.
    assert(frame.extid && (!frame.rtr));
    assert(can_ts_frame.fromAddress_ == address_);

    qCDebug(cants) << "Sent frame" << can_ts_frame;

    switch (can_ts_frame.type_) {
    case CanTsFrame::TransferType::TELECOMMAND:
        SendTCFrameSent(can_ts_frame);
        break;

    case CanTsFrame::TransferType::TELEMETRY:
        ReceiveTMFrameSent(can_ts_frame);
        break;

    case CanTsFrame::TransferType::SET_BLOCK:
        SendBlockFrameSent(can_ts_frame);
        break;

    case CanTsFrame::TransferType::GET_BLOCK:
        ReceiveBlockFrameSent(can_ts_frame);
        break;

    case CanTsFrame::TransferType::TIME_SYNC:
        SendTimeSyncFrameSent();
        break;

    case CanTsFrame::TransferType::UNSOLICITED:
        SendUnsolicitedFrameSent(can_ts_frame);
        break;
    }
}

void CAN_TS::CanFrameSendErrorNominal(const CanFrame& frame, CommDriver::CanSendError error)
{
    CanTsFrame can_ts_frame = FromCanFrame(frame);

    // Sanity checks.
    assert(frame.extid && (!frame.rtr));
    assert(can_ts_frame.fromAddress_ == address_);

    qCDebug(cants) << "Failed sending frame" << can_ts_frame;

    switch (can_ts_frame.type_) {
    case CanTsFrame::TransferType::TELECOMMAND:
        SendTCFrameSendError(can_ts_frame, error);
        break;

    case CanTsFrame::TransferType::TELEMETRY:
        ReceiveTMFrameSendError(can_ts_frame, error);
        break;

    case CanTsFrame::TransferType::SET_BLOCK:
        SendBlockFrameSendError(can_ts_frame, error);
        break;

    case CanTsFrame::TransferType::GET_BLOCK:
        ReceiveBlockFrameSendError(can_ts_frame, error);
        break;

    case CanTsFrame::TransferType::TIME_SYNC:
        SendTimeSyncFrameSendError(error);
        break;

    case CanTsFrame::TransferType::UNSOLICITED:
        SendUnsolicitedFrameSendError(can_ts_frame, error);
        break;
    }
}

void CAN_TS::CanFrameReceivedNominal(const CanFrame& frame)
{
    // Basic 11-bit idendifier is not supported.
    if (!frame.extid || frame.rtr) {
        qCCritical(cants) << "Error: 11-bit ID and RTR not supported";
        return;
    }

    CanTsFrame can_ts_frame = FromCanFrame(frame);
    qCDebug(cants) << "Received frame" << can_ts_frame;

    if (can_ts_frame.toAddress_ == address_) {
        // If we are the recepient.
        switch (can_ts_frame.type_) {
        case CanTsFrame::TransferType::TELECOMMAND:
            SendTCFrameReceived(can_ts_frame);
            break;

        case CanTsFrame::TransferType::TELEMETRY:
            ReceiveTMFrameReceived(can_ts_frame);
            break;

        case CanTsFrame::TransferType::SET_BLOCK:
            FrameReceivedSetBlock(can_ts_frame);
            break;

        case CanTsFrame::TransferType::GET_BLOCK:
            ReceiveBlockFrameReceived(can_ts_frame);
            break;

        case CanTsFrame::TransferType::UNSOLICITED:
            ReceivedUnsolicitedFrame(can_ts_frame);
            break;

        default:
            qCCritical(cants) << "Invalid transfer type" << static_cast<int>(can_ts_frame.type_);
            break;
        }
    } else if (can_ts_frame.toAddress_ == static_cast<uint8_t>(sky::CanTsFrame::Address::KEEP_ALIVE) &&
              (can_ts_frame.type_ == CanTsFrame::TransferType::UNSOLICITED)) {
        // If keep alive transfer.
        ReceivedKeepAliveFrame(can_ts_frame, true);
    } else if (can_ts_frame.toAddress_ == static_cast<uint8_t>(sky::CanTsFrame::Address::TIME_SYNC) &&
               can_ts_frame.type_ == CanTsFrame::TransferType::TIME_SYNC) {
        // If time sync transfer.
        SendTimeSyncFrameReceived(can_ts_frame);
    }
}

void CAN_TS::CanFrameReceivedRedundant(const CanFrame& frame)
{
    // Basic 11-bit idendifier is not supported.
    if (!frame.extid || frame.rtr) {
        qCCritical(cants) << "Error: 11-bit ID and RTR not supported";
        return;
    }

    CanTsFrame can_ts_frame = FromCanFrame(frame);
    qCDebug(cants) << "Received frame" << can_ts_frame;

    // On redundat bus we are interested only in keep alive transfers.
    if (can_ts_frame.toAddress_ == static_cast<uint8_t>(sky::CanTsFrame::Address::KEEP_ALIVE) &&
       (can_ts_frame.type_ == CanTsFrame::TransferType::UNSOLICITED)) {
        ReceivedKeepAliveFrame(can_ts_frame, false);
    }
}

uint8_t CAN_TS::GetAddress() const
{
    return address_;
}

} // namespace sky
