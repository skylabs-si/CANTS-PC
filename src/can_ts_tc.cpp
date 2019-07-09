/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(cants_tc, "sky::CAN_TS::TC")

namespace sky
{

bool CAN_TS::SendTC(uint8_t address, uint8_t channel, const std::vector<uint8_t> &data, uint8_t retry_count)
{
    if (CanTsFrame::IsBroadcastAddress(address)) {
        qCCritical(cants_tc) << "Invalid address =" << address << "channel =" << channel;
        return false;
    }

    if(std::any_of(std::begin(tc_transfers_), std::end(tc_transfers_),
                   [address, channel](const TelecommandTransfer& t) { return (t.address == address) && (t.channel == channel); })) {
        qCCritical(cants_tc) << "Transfer already active to address =" << address << "channel =" << channel;
        return false;
    }

    if (data.size() > 8) {
        qCCritical(cants_tc) << "Invalid data length to address =" << address
                             << "channel =" << channel << "data =" << data;
        return false;
    }

    CanTsFrame frame = CanTsFrame::CreateTelecommandRequest(address, address_, channel, data);

    if (!SendFrame(frame)) {
        qCCritical(cants_tc) << "Sending frame failed to address =" << frame.toAddress_ << "channel =" << channel;
        emit SendTCFailed(frame.toAddress_, channel, SendTCError::kSendRequestFailed);
        return false;
    }

    TelecommandTransfer transfer;
    transfer.address = frame.toAddress_;
    transfer.channel = channel;
    transfer.data = data;
    transfer.txState = Transfer::TxState::kSendingRequest;
    transfer.rxState = Transfer::RxState::kIdle;
    transfer.watchdog = std::make_shared<QTimer>();
    transfer.watchdog->setSingleShot(true);
    transfer.retry_count = 0;
    transfer.max_retries = retry_count;
    tc_transfers_.push_back(transfer);

    auto it = tc_transfers_.end() - 1;
    connect(transfer.watchdog.get(), &QTimer::timeout, this, [this, it] () {
        emit SendTCTimeout(it);
    }, Qt::QueuedConnection);

    qCDebug(cants_tc) << "Starting TC transfer to address =" << address << "channel =" << channel << "data =" << data << "retry_count =" << retry_count;
    return true;
}

void CAN_TS::SendTCRetry(const std::vector<TelecommandTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_tc) << "Max retries reached to address =" << transfer->address << "channel =" << transfer->channel;
        emit SendTCFailed(transfer->address, transfer->channel, SendTCError::kMaxRetriesReached);
        tc_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateTelecommandRequest(transfer->address, address_, transfer->channel, transfer->data);

        if (!SendFrame(frame)) {
            transfer->watchdog->stop();
            qCCritical(cants_tc) << "Failed sending TC retry to address =" << transfer->address << "channel =" << transfer->channel;
            emit SendTCFailed(frame.toAddress_, transfer->channel, SendTCError::kSendRequestFailed);
            tc_transfers_.erase(transfer);
        } else {
            transfer->txState = Transfer::TxState::kSendingRequest;
            qCDebug(cants_tc) << "Sending TC retry to address =" << transfer->address << "channel =" << transfer->channel;
        }
    }
}

void CAN_TS::SendTCTimeout(const std::vector<TelecommandTransfer>::iterator& transfer)
{
    transfer->rxState = Transfer::RxState::kIdle;
    qCCritical(cants_tc) << "TC ACK timeout address =" << transfer->address << "channel =" << transfer->channel;
    SendTCRetry(transfer);
}

void CAN_TS::SendTCFrameSent(const CanTsFrame& frame)
{
    auto channel = frame.GetChannel();
    auto to_address = frame.GetToAddress();

    auto it = std::find_if(std::begin(tc_transfers_), std::end(tc_transfers_),
                           [channel, to_address](const TelecommandTransfer& t) -> bool {
        return (t.address == to_address) && (t.channel == channel) &&
               (t.txState == Transfer::TxState::kSendingRequest);
    });

    if (it != std::end(tc_transfers_)) {
        it->watchdog->start(static_cast<int>(timeout_));
        it->rxState = Transfer::RxState::kWaitingForRequestACK;
        it->txState = Transfer::TxState::kIdle;
        it->retry_count++;
        qCDebug(cants_tc) << "Sent TC frame to address =" << to_address << "channel =" << channel;
    }
}

void CAN_TS::SendTCFrameSendError(const CanTsFrame& frame, CommDriver::CanSendError error)
{
    auto channel = frame.GetChannel();
    auto to_address = frame.GetToAddress();

    auto it = std::find_if(tc_transfers_.begin(), tc_transfers_.end(),
                           [channel, to_address](const TelecommandTransfer& t) -> bool {
        return (t.address == to_address) && (t.channel == channel) &&
               (t.txState == Transfer::TxState::kSendingRequest);
    });

    if (it != std::end(tc_transfers_)) {
        qCCritical(cants_tc) << "Failed sending to address =" << frame.toAddress_
                             << "channel =" << channel << "error =" << error;
        emit SendTCFailed(frame.toAddress_, channel, SendTCError::kSendRequestFailed);
    }
}

void CAN_TS::SendTCFrameReceived(const CanTsFrame& frame)
{
    auto frame_type = frame.GetFrameType();
    auto channel = frame.GetChannel();
    auto from_address = frame.GetFromAddress();

    auto it = std::find_if(std::begin(tc_transfers_), std::end(tc_transfers_),
                           [from_address, channel](const TelecommandTransfer& t) -> bool {
        return (t.address == from_address) && (t.channel == channel) &&
               (t.rxState == TelemetryTransfer::RxState::kWaitingForRequestACK);
    });

    if (it == std::end(tc_transfers_)) {
        qCCritical(cants_tc) << "Received invalid frame (non active transfer) from address =" << from_address << "channel =" << channel;
    } else if (frame_type == CanTsFrame::TelecommandFrameType::ACK) {
        tc_transfers_.erase(it);
        emit SendTCCompleted(from_address, channel);
        qCDebug(cants_tc) << "Received TC ACK from address =" << from_address << "channel =" << channel;
    } else if (frame_type == CanTsFrame::TelecommandFrameType::NACK) {
        it->watchdog->stop();
        it->rxState = Transfer::RxState::kIdle;
        SendTCRetry(it);
        qCCritical(cants_tc) << "Received TC NACK from address =" << from_address << "channel =" << channel;
    }
}

} // namespace sky
