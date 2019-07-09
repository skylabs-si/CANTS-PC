/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(cants_tm, "sky::CAN_TS::TM")

namespace sky
{

bool CAN_TS::ReceiveTM(uint8_t address, uint8_t channel, uint8_t retry_count)
{
    if (CanTsFrame::IsBroadcastAddress(address)) {
        qCCritical(cants_tm) << "Invalid address =" << address;
        return false;
    }

    if (std::any_of(std::begin(tm_transfers_), std::end(tm_transfers_),
                    [address, channel](const TelemetryTransfer& t) { return (t.address == address) && (t.channel == channel); })) {
        qCCritical(cants_tm) << "Transfer already active to address =" << address << "and channel =" << channel;
        return false;
    }

    CanTsFrame frame = CanTsFrame::CreateTelemetryRequest(address, address_, channel);

    if (!SendFrame(frame)) {
        qCCritical(cants_tm) << "Sending frame failed to address =" << address << "channel =" << channel;
        emit ReceiveTMFailed(frame.toAddress_, channel, ReceiveTMError::kSendRequestFailed);
        return false;
    }

    TelemetryTransfer transfer;
    transfer.address = frame.toAddress_;
    transfer.channel = channel;
    transfer.rxState = Transfer::RxState::kIdle;
    transfer.txState = Transfer::TxState::kSendingRequest;
    transfer.watchdog = std::make_shared<QTimer>();
    transfer.watchdog->setSingleShot(true);
    transfer.retry_count = 0;
    transfer.max_retries = retry_count;
    tm_transfers_.push_back(transfer);

    auto it = tm_transfers_.end() - 1;
    connect(transfer.watchdog.get(), &QTimer::timeout, this, [this, it] () {
        emit ReceiveTMTimeout(it);
    }, Qt::QueuedConnection);

    qCDebug(cants_tm) << "Starting TM transfer to address =" << address << "channel =" << channel << "retry_count =" << retry_count;
    return true;
}

void CAN_TS::ReceiveTMRetry(const std::vector<TelemetryTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_tm) << "Max retries reached address=" << transfer->address << "channel =" << transfer->channel;
        emit ReceiveTMFailed(transfer->address, transfer->channel, ReceiveTMError::kMaxRetriesReached);
        tm_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateTelemetryRequest(transfer->address, address_, transfer->channel);

        if (!SendFrame(frame)) {
            qCCritical(cants_tm) << "Failed sending retry to address =" << transfer->address << "channel =" << transfer->channel;
            emit ReceiveTMFailed(transfer->address, transfer->channel, ReceiveTMError::kSendRequestFailed);
            tm_transfers_.erase(transfer);
        } else {
            transfer->txState = Transfer::TxState::kSendingRequest;
            qCDebug(cants_tm) << "Sending TM retry to address =" << transfer->address << "channel =" << transfer->channel;
        }
    }
}

void CAN_TS::ReceiveTMTimeout(const std::vector<TelemetryTransfer>::iterator& transfer)
{
    transfer->watchdog->stop();
    transfer->rxState = Transfer::RxState::kIdle;
    qCCritical(cants_tm) << "TM ACK timeout address =" << transfer->address << "channel =" << transfer->channel;
    ReceiveTMRetry(transfer);
}

void CAN_TS::ReceiveTMFrameSent(const CanTsFrame& frame)
{
    auto channel = frame.GetChannel();
    auto to_address = frame.GetToAddress();

    auto it = std::find_if(std::begin(tm_transfers_), std::end(tm_transfers_),
                           [channel, to_address](const TelemetryTransfer& t) -> bool {
        return (t.address == to_address) && (t.channel == channel) &&
               (t.txState == Transfer::TxState::kSendingRequest);
    });

    if (it != std::end(tm_transfers_)) {
        it->watchdog->start(static_cast<int>(timeout_));
        it->rxState = TelemetryTransfer::RxState::kWaitingForRequestACK;
        it->txState = TelemetryTransfer::TxState::kIdle;
        it->retry_count++;
        qCDebug(cants_tm) << "Sent TM frame to address =" << to_address << "channel =" << channel;
    }
}

void CAN_TS::ReceiveTMFrameSendError(const CanTsFrame& frame, CommDriver::CanSendError error)
{
    auto channel = frame.GetChannel();
    auto to_address = frame.GetToAddress();

    auto it = std::find_if(tm_transfers_.begin(), tm_transfers_.end(),
                           [channel, to_address](const TelemetryTransfer& t) -> bool {
        return (t.address == to_address) && (t.channel == channel) &&
               (t.txState == Transfer::TxState::kSendingRequest);
    });

    if (it != std::end(tm_transfers_)) {
        qCCritical(cants_tm) << "Failed sending to address =" << frame.GetToAddress()
                             << "channel =" << channel << "error =" << error;
        emit ReceiveTMFailed(frame.toAddress_, channel, ReceiveTMError::kSendRequestFailed);
    }
}

void CAN_TS::ReceiveTMFrameReceived(const CanTsFrame& frame)
{
    auto frame_type = frame.GetFrameType();
    auto channel = frame.GetChannel();
    auto from_address = frame.GetFromAddress();

    auto it = std::find_if(std::begin(tm_transfers_), std::end(tm_transfers_),
                           [from_address, channel](const TelemetryTransfer& t) -> bool {
        return (t.address == from_address) && (t.channel == channel) &&
               (t.rxState == TelemetryTransfer::RxState::kWaitingForRequestACK);
    });

    if (it == std::end(tm_transfers_)) {
        qCCritical(cants_tm) << "Received invalid frame (non activa transfer) from address =" << from_address << "channel =" << channel;
    } else if (frame_type == CanTsFrame::TelecommandFrameType::ACK) {
        tm_transfers_.erase(it);
        emit ReceiveTMCompleted(from_address, channel, frame.data_);
        qCDebug(cants_tm) << "Received TM ACK from address =" << from_address << "channel =" << channel;
    } else if (frame_type == CanTsFrame::TelecommandFrameType::NACK) {
        it->watchdog->stop();
        it->rxState = Transfer::RxState::kIdle;
        ReceiveTMRetry(it);
        qCCritical(cants_tm) << "Received TM NACK from address =" << from_address << "channel =" << channel;
    }
}

} // namespace sky
