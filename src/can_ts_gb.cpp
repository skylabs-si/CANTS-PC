/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>
#include <cmath>

Q_LOGGING_CATEGORY(cants_gb, "sky::CAN_TS::GetBlock")

namespace sky
{

bool CAN_TS::ReceiveBlock(uint8_t to_address, uint64_t start_address, uint8_t length, uint8_t retry_count, uint8_t start_retry_count)
{
    if (CanTsFrame::IsBroadcastAddress(to_address)) {
        qCCritical(cants_gb) << "Invalid address" << to_address;
        return false;
    }

    if(std::any_of(gb_transfers_.begin(), gb_transfers_.end(),
                   [to_address](const GetBlockTransfer& t) { return (t.address == to_address); })) {
        qCCritical(cants_gb) << "Transfer already active to address" << to_address;
        return false;
    }

    if (length < 1) {
        qCCritical(cants_gb) << "Invalid length" << length;
        return false;
    }

    std::vector<uint8_t> start_addr = CanTsUtils::ToByteVector(start_address, true);
    CanTsFrame frame = CanTsFrame::CreateGetBlockRequest(to_address, address_, length - 1, start_addr);

    if (!SendFrame(frame)) {
        qCCritical(cants_gb) << "Failed sending request frame" << frame;
        emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendRequestFailed);
        return false;
    }

    GetBlockTransfer transfer;
    transfer.address = frame.toAddress_;
    transfer.bitmap.resize((length + 7)/ 8);
    transfer.blocks = length;
    transfer.data.resize(length * 8);
    transfer.start = start_addr;
    transfer.max_retries = retry_count;
    transfer.retry_count = 0;
    transfer.max_start_retries = start_retry_count;
    transfer.start_retry_count = 0;
    transfer.rxState = GetBlockTransfer::RxState::kIdle;
    transfer.txState = GetBlockTransfer::TxState::kSendingRequest;
    transfer.watchdog = std::make_shared<QTimer>();
    transfer.watchdog->setSingleShot(true);
    CanTsUtils::SetBitmap(transfer.bitmap, length);
    gb_transfers_.push_back(transfer);

    auto it = gb_transfers_.end() - 1;
    connect(transfer.watchdog.get(), &QTimer::timeout, this, [this, it] () {
        emit ReceiveBlockFrameSentTimeout(it);
    }, Qt::QueuedConnection);

    qCDebug(cants_gb) << "Starting receive (get) block transfer to destination address =" << to_address
            << "memory address =" << start_address << "retry_count =" << retry_count << "report_delay_ms =";
    return true;
}

void CAN_TS::ReceiveBlockRetryRequest(const std::vector<GetBlockTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_gb) << "Max retries reached";
        emit ReceiveBlockFailed(transfer->address, ReceiveBlockError::kMaxSendRequestRetriesReached);
        gb_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateGetBlockRequest(transfer->address, address_, transfer->blocks - 1, transfer->start);

        if (!SendFrame(frame)) {
            qCCritical(cants_gb) << "Send retry failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendRequestFailed);
            gb_transfers_.erase(transfer);
        } else {
            transfer->txState = GetBlockTransfer::TxState::kSendingRequest;
            qCDebug(cants_gb) << "Retrying block request";
        }
    }
}

void CAN_TS::ReceiveBlockRetryStart(const std::vector<GetBlockTransfer>::iterator& transfer)
{
    if (transfer->start_retry_count > transfer->max_start_retries) {
        qCCritical(cants_gb) << "Max retries reached";

        CanTsFrame frame = CanTsFrame::CreateGetBlockAbort(transfer->address, address_);
        if (!SendFrame(frame)) {
            qCCritical(cants_gb) << "Sending abort frame failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendAbortFailed);
            gb_transfers_.erase(transfer);
        } else {
            transfer->txState = GetBlockTransfer::TxState::kSendingAbort;
            qCDebug(cants_gb) << "Retrying abort frame";
        }
    } else {
        CanTsFrame frame = CanTsFrame::CreateGetBlockStart(transfer->address, address_, transfer->bitmap);

        if (!SendFrame(frame)) {
            qCCritical(cants_gb) << "Sending start frame failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendStartFailed);
            gb_transfers_.erase(transfer);
        } else {
            transfer->txState = GetBlockTransfer::TxState::kSendingStart;
            qCDebug(cants_gb) << "Retrying start frame";
        }
    }
}

void CAN_TS::ReceiveBlockRetryAbort(const std::vector<GetBlockTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_gb) << "Max retries reached";
        emit ReceiveBlockFailed(transfer->address, ReceiveBlockError::kMaxSendAbortRetriesReached);
        gb_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateGetBlockAbort(transfer->address, address_);

        if (!SendFrame(frame)) {
            qCCritical(cants_gb) << "Sending abort frame failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendAbortFailed);
            gb_transfers_.erase(transfer);
        } else {
            transfer->txState = GetBlockTransfer::TxState::kSendingAbort;
            qCDebug(cants_gb) << "Retrying abort frame";
        }
    }
}

void CAN_TS::ReceiveBlockFrameSentTimeout(const std::vector<GetBlockTransfer>::iterator& transfer)
{
    assert(transfer->rxState != GetBlockTransfer::RxState::kIdle);

    transfer->watchdog->stop();
    transfer->rxState = GetBlockTransfer::RxState::kIdle;
    ReceiveBlockRetryRequest(transfer);

    qCCritical(cants_gb) << "Transfer timeout";
}

void CAN_TS::ReceiveBlockFrameSent(const CanTsFrame& frame)
{
    auto to_address = frame.GetToAddress();
    auto frame_type = frame.GetGBFrameType();

    auto it = std::find_if(std::begin(gb_transfers_), std::end(gb_transfers_),
                           [to_address](const GetBlockTransfer& t) -> bool {
        return (t.address == to_address);
    });

    if (it == gb_transfers_.end()) {
        qCDebug(cants_gb) << "Transfer not active";
    } else if (frame_type == CanTsFrame::GetBlockFrameType::REQUEST &&
               it->txState == GetBlockTransfer::TxState::kSendingRequest) {
        it->watchdog->start(static_cast<int>(timeout_));
        it->txState = GetBlockTransfer::TxState::kIdle;
        it->rxState = GetBlockTransfer::RxState::kWaitingForRequestACK;
        it->retry_count++;
        qCDebug(cants_gb) << "Request frame sent";
    } else if (frame_type == CanTsFrame::GetBlockFrameType::ABORT &&
               it->txState == GetBlockTransfer::TxState::kSendingAbort) {
        it->watchdog->start(static_cast<int>(timeout_));
        it->txState = GetBlockTransfer::TxState::kIdle;
        it->rxState = GetBlockTransfer::RxState::kWaitingForAbortACK;
        it->retry_count++;
        qCDebug(cants_gb) << "Abort frame sent";
    } else if (frame_type == CanTsFrame::GetBlockFrameType::START &&
               it->txState == GetBlockTransfer::TxState::kSendingStart) {
        it->watchdog->start(static_cast<int>(timeout_));
        it->txState = GetBlockTransfer::TxState::kIdle;
        it->rxState = GetBlockTransfer::RxState::kWaitingForData;
        it->start_retry_count++;
        qCDebug(cants_gb) << "Start frame sent";
    }
}

void CAN_TS::ReceiveBlockFrameSendError(const CanTsFrame& frame, CommDriver::CanSendError error)
{
    auto to_address = frame.GetToAddress();
    auto frame_type = frame.GetGBFrameType();

    auto it = std::find_if(std::begin(gb_transfers_), std::end(gb_transfers_),
                           [to_address](const GetBlockTransfer& t) -> bool {
        return (t.address == to_address);
    });

    if  (it == gb_transfers_.end()) {
        qCCritical(cants_gb) << "Transfer not active";
    } else {
        gb_transfers_.erase(it);

        qCCritical(cants_gb) << "Frame send failed to_address =" << to_address << "error =" << error;

        if (frame_type == CanTsFrame::GetBlockFrameType::REQUEST) {
            emit ReceiveBlockFailed(to_address, ReceiveBlockError::kSendRequestFailed);
        } else if (frame_type == CanTsFrame::GetBlockFrameType::ABORT) {
            emit ReceiveBlockFailed(to_address, ReceiveBlockError::kSendAbortFailed);
        } else if (frame_type == CanTsFrame::GetBlockFrameType::START) {
            emit ReceiveBlockFailed(to_address, ReceiveBlockError::kSendStartFailed);
        }
    }
}

void CAN_TS::ReceiveBlockFrameReceivedAck(const CanTsFrame& frame, const std::vector<GetBlockTransfer>::iterator& transfer)
{
    if (transfer->rxState == GetBlockTransfer::RxState::kWaitingForRequestACK) {

        // ACK received. Check if response is valid.
        if (((frame.GetBlockCmdBits()+1) != transfer->blocks) || (frame.data_ != transfer->start)) { //  FIXME: compare vector content!!!
            qCCritical(cants_gb) << "Invalid GB request response";
            return;
        }

        transfer->watchdog->stop();
        transfer->retry_count = 0;

        CanTsFrame frame = CanTsFrame::CreateGetBlockStart(transfer->address, address_, transfer->bitmap);
        if (!SendFrame(frame)) {
            qCCritical(cants_gb) << "Start frame send failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendStartFailed);
            gb_transfers_.erase(transfer);
        } else {
            qCDebug(cants_gb) << "Sending start frame to_address =" << frame.toAddress_;
            transfer->txState = GetBlockTransfer::TxState::kSendingStart;
            transfer->rxState = GetBlockTransfer::RxState::kIdle;
        }

    } else if (transfer->rxState == GetBlockTransfer::RxState::kWaitingForAbortACK) {

        // ACK received. Check if response is valid.
        if ((frame.GetBlockCmdBits() != 0) || (!frame.data_.empty())) {
            qCCritical(cants_gb) << "Invalid abort response";
        } else {
            transfer->watchdog->stop();
            qCDebug(cants_gb) << "ACK received";

            if (transfer->start_retry_count > transfer->max_start_retries) {
                emit ReceiveBlockFailed(frame.fromAddress_, ReceiveBlockError::kMaxSendStartRetriesReached);
                gb_transfers_.erase(transfer);
            } else {
                emit ReceiveBlockCompleted(frame.fromAddress_, transfer->data);
                gb_transfers_.erase(transfer);
            }
        }
    } else {
        qCCritical(cants_gb) << "Unexpected ACK";
    }
}

void CAN_TS::ReceiveBlockFrameReceivedNack(const CanTsFrame& frame, const std::vector<GetBlockTransfer>::iterator& transfer)
{
    if (transfer->rxState == GetBlockTransfer::RxState::kWaitingForRequestACK) {
        if ((frame.GetBlockCmdBits() != 0) || (!frame.data_.empty())) {
            qCDebug(cants_gb) << "Invalid NACK received from_address =" << frame.GetFromAddress();
        } else {
            transfer->watchdog->stop();
            transfer->rxState = GetBlockTransfer::RxState::kIdle;
            qCCritical(cants_gb) << "NACK received from_address =" << frame.GetFromAddress();
            ReceiveBlockRetryRequest(transfer);
        }
    } else if (transfer->rxState == GetBlockTransfer::RxState::kWaitingForData) {
        if ((frame.GetBlockCmdBits() != 0) || (!frame.data_.empty())) {
            qCDebug(cants_gb) << "Invalid NACK received from_address =" << frame.GetFromAddress();
        } else {
            transfer->watchdog->stop();
            transfer->rxState = GetBlockTransfer::RxState::kIdle;
            qCCritical(cants_gb) << "NACK received from_address =" << frame.GetFromAddress();
            ReceiveBlockRetryStart(transfer);
        }
    } else if (transfer->rxState == GetBlockTransfer::RxState::kWaitingForAbortACK) {
        if ((frame.GetBlockCmdBits() != 0) || (!frame.data_.empty())) {
            qCDebug(cants_gb) << "Invalid NACK received from_address=" << frame.GetFromAddress();
        } else {
            transfer->watchdog->stop();
            gb_transfers_.erase(transfer);
            qCCritical(cants_gb) << "NACK received from_address =" << frame.GetFromAddress();
            emit ReceiveBlockFailed(frame.fromAddress_, ReceiveBlockError::kAbortNACKReceived);
        }
    } else {
        qCCritical(cants_gb) << "Unexpected NACK";
    }
}

void CAN_TS::ReceiveBlockFrameReceivedTransfer(const CanTsFrame &frame, const std::vector<GetBlockTransfer>::iterator& transfer)
{
    // Check if received frame is valid.
    if ((frame.data_.size() != 8) || (frame.GetBlockCmdBits() >= transfer->blocks)) {
        qCCritical(cants_gb) << "Invalid frame";
        return;
    }

    // If frame was ready received.
    if (!CanTsUtils::IsBitmapBitSet(transfer->bitmap, frame.GetBlockCmdBits())) {
        qCCritical(cants_gb) << "Frame already received";
        return;
    }

    transfer->watchdog->stop();
    transfer->retry_count = 0;
    CanTsUtils::ClearBitmapBit(transfer->bitmap, frame.GetBlockCmdBits());
    qCDebug(cants_gb) << "Received transfer frame from_address =" << frame.fromAddress_
        << "sequence =" << frame.GetBlockCmdBits() << "data =" << frame.data_;

    for (uint8_t i = 0; i < 8; i++) {
        transfer->data[i + frame.GetBlockCmdBits() * 8] = frame.data_[i];
    }

    // If received all frames.
    if (CanTsUtils::IsBitmapCleared(transfer->bitmap, transfer->blocks)) {
        CanTsFrame frame = CanTsFrame::CreateGetBlockAbort(transfer->address, address_);

        if (!SendFrame(frame)) {
            gb_transfers_.erase(transfer);
            qCCritical(cants_gb) << "Sending abort failed";
            emit ReceiveBlockFailed(frame.toAddress_, ReceiveBlockError::kSendAbortFailed);
        } else {
            transfer->txState = GetBlockTransfer::TxState::kSendingAbort;
            transfer->rxState = GetBlockTransfer::RxState::kIdle;
            qCDebug(cants_gb) << "Sending abort frame";
        }
    }
}

void CAN_TS::ReceiveBlockFrameReceived(const CanTsFrame& frame)
{
    auto frame_type = frame.GetGBFrameType();
    auto from_address = frame.GetFromAddress();

    auto transfer = std::find_if(std::begin(gb_transfers_), std::end(gb_transfers_),
                           [from_address](const GetBlockTransfer& t) -> bool {
        return (t.address == from_address);
    });

    if (transfer == gb_transfers_.end()) {
        qCCritical(cants_gb) << "Transfer not active";
    } else if (frame_type == CanTsFrame::GetBlockFrameType::ACK) {
        ReceiveBlockFrameReceivedAck(frame, transfer);
    } else if (frame_type == CanTsFrame::GetBlockFrameType::NACK) {
        ReceiveBlockFrameReceivedNack(frame, transfer);
    } else if (frame_type == CanTsFrame::GetBlockFrameType::TRANSFER) {
        ReceiveBlockFrameReceivedTransfer(frame, transfer);
    } else {
        qCCritical(cants_gb) << "Unexpected frame type" << frame.type_;
    }
}

} // namespace sky
