/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>
#include <cmath>
#include <algorithm>

Q_LOGGING_CATEGORY(cants_sb, "sky::CAN_TS::SetBlock")

namespace sky
{

bool CAN_TS::SendBlock(uint8_t to_address, uint64_t start_address, const std::vector<uint8_t>& data, uint8_t retry_count,
                       uint32_t report_delay_ms, uint8_t report_retry_count)
{
    if (CanTsFrame::IsBroadcastAddress(to_address)) {
        qCCritical(cants_sb) << "Invalid to address =" << to_address;
        return false;
    }

    if(std::any_of(sb_transfers_.begin(), sb_transfers_.end(),
                   [to_address](const SetBlockTransfer& t) { return (t.address == to_address); })) {
        qCCritical(cants_sb) << "Transfer already active";
        return false;
    }

    if (data.empty() || (data.size() > 512)) {
        qCCritical(cants_sb) << "Invalid data length = "<< data.size() << "to address =" << to_address;
        return false;
    }

    auto num_blocks = static_cast<uint8_t>((data.size() + 7) / 8);
    std::vector<uint8_t> start_addr = CanTsUtils::ToByteVector(start_address, true);
    CanTsFrame frame = CanTsFrame::CreateSetBlockRequest(to_address, address_, num_blocks-1, start_addr);

    if (!SendFrame(frame)) {
        qCCritical(cants_sb) << "Failed sending request frame to address =" << frame.toAddress_;
        emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendRequestFailed);
        return false;
    }

    SetBlockTransfer transfer;
    transfer.address = frame.toAddress_;
    transfer.blocks = num_blocks;
    transfer.bitmap.resize((data.size() + 63) / 64);
    std::fill(transfer.bitmap.begin(), transfer.bitmap.end(), 0);
    transfer.done = false;
    transfer.data = data;
    transfer.start = start_addr;
    transfer.max_retries = retry_count;
    transfer.retry_count = 0;
    transfer.max_report_retries = report_retry_count;
    transfer.report_retry_count = 0;
    transfer.report_delay = report_delay_ms;
    transfer.rxState = SetBlockTransfer::RxState::kIdle;
    transfer.txState = SetBlockTransfer::TxState::kSendingRequest;
    transfer.watchdog = std::make_shared<QTimer>();
    transfer.watchdog->setSingleShot(true);
    transfer.report_delay_timer = std::make_shared<QTimer>();
    transfer.report_delay_timer->setSingleShot(true);
    sb_transfers_.push_back(transfer);

    auto it = sb_transfers_.end() - 1;
    connect(transfer.watchdog.get(), &QTimer::timeout, this, [this, it] () {
        emit SendBlockFrameSentTimeout(it);
    }, Qt::QueuedConnection);

    connect(transfer.report_delay_timer.get(), &QTimer::timeout, this, [this, it] () {
        emit SendBlockReportRequestDelayTimeout(it);
    }, Qt::QueuedConnection);

    qCDebug(cants_sb) << "Starting send (set) block transfer to destination address =" << to_address << "memory address =" << start_address
        << "retry_count =" << retry_count << "report_delay_ms =" << report_delay_ms << "report_retry_count =" << report_retry_count << "data =" << data;
    return true;
}

void CAN_TS::SendBlockRetryRequest(const std::vector<SetBlockTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_sb) << "Max retries reached to address =" << transfer->address;
        emit SendBlockFailed(transfer->address, SendBlockError::kMaxSendRequestRetriesReached);
        sb_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateSetBlockRequest(transfer->address, address_, transfer->blocks - 1, transfer->start);
        if (!SendFrame(frame)) {
            sb_transfers_.erase(transfer);
            qCCritical(cants_sb) << "Failed retrying request frame to address =" << frame.toAddress_;
            emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendRequestFailed);
        } else {
            transfer->txState = SetBlockTransfer::TxState::kSendingRequest;
            qCDebug(cants_sb) << "Retrying request frame to address =" << frame.toAddress_;
        }
    }
}

void CAN_TS::SendBlockRetryStatus(const std::vector<SetBlockTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_sb) << "Max retries reached to address =" << transfer->address;
        emit SendBlockFailed(transfer->address, SendBlockError::kMaxSendStatusRetriesReached);
        sb_transfers_.erase(transfer);
    } else {
        CanTsFrame frame = CanTsFrame::CreateSetBlockStatus(transfer->address, address_);
        if (!SendFrame(frame)) {
            sb_transfers_.erase(transfer);
            qCCritical(cants_sb) << "Failed retrying status frame to address =" << frame.toAddress_;
            emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendStatusRequestFailed);
        } else {
            transfer->txState = SetBlockTransfer::TxState::kSendingStatusRequest;
            qCDebug(cants_sb) << "Retrying status frame to address =" << frame.toAddress_;
        }
    }
}

void CAN_TS::SendBlockRetryAbort(const std::vector<SetBlockTransfer>::iterator& transfer)
{
    if (transfer->retry_count > transfer->max_retries) {
        qCCritical(cants_sb) << "Max retries reached to address =" << transfer->address;

        if (transfer->done && CanTsUtils::IsBitmapSet(transfer->bitmap, transfer->blocks)) {
            // If abort was sent because transfer completed.
            emit SendBlockFailed(transfer->address, SendBlockError::kMaxSendAbortRetriesReached);
            sb_transfers_.erase(transfer);
        } else {
            // If abort was sent because max report retries reached.
            emit SendBlockFailed(transfer->address, SendBlockError::kMaxReportRetriesReached);
            sb_transfers_.erase(transfer);
        }
    } else {
        CanTsFrame frame = CanTsFrame::CreateSetBlockAbort(transfer->address, address_);

        if (!SendFrame(frame)) {
            qCCritical(cants_sb) << "Failed retrying abort frame to address =" << frame.toAddress_;

            if (transfer->done && CanTsUtils::IsBitmapSet(transfer->bitmap, transfer->blocks)) {
                // If abort was sent because transfer completed.
                sb_transfers_.erase(transfer);
                emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendAbortFailed);
            } else {
                // If abort was sent because max report retries reached.
                sb_transfers_.erase(transfer);
                emit SendBlockFailed(frame.toAddress_, SendBlockError::kMaxReportRetriesReached);
            }
        } else {
            transfer->txState = SetBlockTransfer::TxState::kSendingAbort;
            qCDebug(cants_sb) << "Retrying abort frame to address =" << frame.toAddress_;
        }
    }
}

void CAN_TS::SendBlockFrameSentTimeout(const std::vector<SetBlockTransfer>::iterator& transfer)
{
    assert(transfer->rxState != SetBlockTransfer::RxState::kIdle);

    transfer->watchdog->stop();
    transfer->rxState = SetBlockTransfer::RxState::kIdle;
    SendBlockRetryStatus(transfer);

    qCCritical(cants_sb) << "Frame transfer timeout";
}

void CAN_TS::SendBlockReportRequestDelayTimeout(const std::vector<SetBlockTransfer>::iterator& transfer)
{
    transfer->report_delay_timer->stop();
    CanTsFrame frame = CanTsFrame::CreateSetBlockStatus(transfer->address, address_);

    if (!SendFrame(frame)) {
        qCCritical(cants_sb) << "Failed sending status frame to address =" << frame.toAddress_;
        emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendStatusRequestFailed);
        sb_transfers_.erase(transfer);
    } else {
        transfer->txState = SetBlockTransfer::TxState::kSendingStatusRequest;
        qCDebug(cants_sb) << "Sending status frame to address =" << frame.toAddress_;
    }
}

void CAN_TS::SendBlockWaitForResponse(const std::vector<SetBlockTransfer>::iterator& transfer, SetBlockTransfer::RxState rxstate) const
{
    transfer->watchdog->start(static_cast<int>(timeout_));
    transfer->txState = SetBlockTransfer::TxState::kIdle;
    transfer->rxState = rxstate;
    transfer->retry_count++;
}

void CAN_TS::SendBlockFrameSent(const CanTsFrame& frame)
{
    auto to_address = frame.GetToAddress();
    auto frame_type = frame.GetSBFrameType();

    auto transfer = std::find_if(std::begin(sb_transfers_), std::end(sb_transfers_),
                           [to_address](const SetBlockTransfer& t) -> bool {
        return (t.address == to_address);
    });

    if (transfer == sb_transfers_.end()) {
        qCDebug(cants_sb) << "Transfer not active";
    } else if (frame_type == CanTsFrame::SetBlockFrameType::REQUEST &&
             transfer->txState == SetBlockTransfer::TxState::kSendingRequest) {
        qCDebug(cants_sb) << "Request frame sent to address =" << frame.toAddress_;
        SendBlockWaitForResponse(transfer, SetBlockTransfer::RxState::kWaitingForRequestACK);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::STATUS &&
               transfer->txState == SetBlockTransfer::TxState::kSendingStatusRequest) {
        qCDebug(cants_sb) << "Status frame sent to address =" << frame.toAddress_;
        SendBlockWaitForResponse(transfer, SetBlockTransfer::RxState::kWaitingForData);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::ABORT &&
               transfer->txState == SetBlockTransfer::TxState::kSendingAbort) {
        qCDebug(cants_sb) << "Abort frame sent to address =" << frame.toAddress_;
        SendBlockWaitForResponse(transfer, SetBlockTransfer::RxState::kWaitingForAbortACK);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::TRANSFER &&
               transfer->txState == SetBlockTransfer::TxState::kSendingData) {
        qCDebug(cants_sb) << "Transfer frame sent to address =" << frame.toAddress_;

        // Mark transmitted frame.
        auto tx_sequence = frame.GetBlockSequence();
        CanTsUtils::SetBitmapBit(transfer->bitmap, tx_sequence);

        // Find first block that was not yet transferred.
        bool framesent = false;
        for (uint8_t sequence = tx_sequence + 1; sequence < transfer->blocks; sequence++) {

            if (!CanTsUtils::IsBitmapBitSet(transfer->bitmap, sequence)) {
                std::vector<uint8_t> data_to_send;

                if (static_cast<size_t>(8 * sequence + 8) < transfer->data.size())
                    data_to_send = std::vector<uint8_t>(transfer->data.begin() + 8 * sequence, transfer->data.begin() + 8 * sequence + 8);
                else
                    data_to_send = std::vector<uint8_t>(transfer->data.begin() + 8 * sequence, transfer->data.end());

                CanTsFrame frame = CanTsFrame::CreateSetBlockTransfer(transfer->address, address_, sequence, data_to_send);
                if (!SendFrame(frame)) {
                    sb_transfers_.erase(transfer);
                    qCCritical(cants_sb) << "Failed sending transfer frame to address =" << frame.toAddress_ << "sequence =" << sequence;
                    emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendDataFailed);
                } else {
                    qCDebug(cants_sb) << "Sending transfer frame to address =" << frame.toAddress_ << "sequence =" << sequence << "data =" << data_to_send;
                    framesent = true;
                    break;
                }
            }
        }

        if (!framesent) {
            // If all frames are transferred, generate some delay and then request status report.
            transfer->report_delay_timer->start(static_cast<int>(transfer->report_delay));
            transfer->txState = SetBlockTransfer::TxState::kWaitingForSendStatusRequest;
        }
    }
}

void CAN_TS::SendBlockFrameSendError(const CanTsFrame& frame, CommDriver::CanSendError error)
{
    auto to_address = frame.GetToAddress();
    auto frame_type = frame.GetSBFrameType();

    auto transfer = std::find_if(std::begin(sb_transfers_), std::end(sb_transfers_),
                           [to_address](const SetBlockTransfer& t) -> bool {
        return (t.address == to_address);
    });

    if (frame_type == CanTsFrame::SetBlockFrameType::REQUEST) {
        qCCritical(cants_sb) << "Failed sending request frame to address =" << frame.toAddress_ << "error =" << error;
        emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendRequestFailed);
        sb_transfers_.erase(transfer);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::STATUS) {
        qCCritical(cants_sb) << "Failed sending status frame to address =" << frame.toAddress_ << "error =" << error;
        emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendStatusRequestFailed);
        sb_transfers_.erase(transfer);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::ABORT) {
        qCCritical(cants_sb) << "Failed sending abort frame to address =" << frame.toAddress_ << "error =" << error;
        if (transfer->done && CanTsUtils::IsBitmapSet(transfer->bitmap, transfer->blocks)) {
            // If abort was sent because transfer completed.
            sb_transfers_.erase(transfer);
            emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendAbortFailed);
        } else {
            // If abort was sent because max report retries reached.
            sb_transfers_.erase(transfer);
            emit SendBlockFailed(frame.toAddress_, SendBlockError::kMaxReportRetriesReached);
        }
    } else if (frame_type == CanTsFrame::SetBlockFrameType::TRANSFER) {
        qCCritical(cants_sb) << "Failed sending transfer frame to address =" << frame.toAddress_ << "error =" << error;
        sb_transfers_.erase(transfer);
        emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendDataFailed);
    }
}

void CAN_TS::SendBlockFrameReceivedAck(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer)
{
    auto blocks_bits = frame.GetBlockCmdBits();

    if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForRequestACK) {
        // If invalid ACK response.
        if ((blocks_bits + 1 != transfer->blocks) || (frame.data_ != transfer->start)) {
            qCCritical(cants_sb) << "Invalid request ACK response from address =" << frame.fromAddress_
                                  << "blocks = " << blocks_bits << "start address =" << frame.data_;
            return;
        }

        transfer->watchdog->stop();
        transfer->retry_count = 0;

        qCDebug(cants_sb) << "Received request frame ACK from address =" << frame.fromAddress_;

        std::vector<uint8_t> data_to_send;
        if (transfer->data.size() > 8)
            data_to_send = std::vector<uint8_t>(transfer->data.begin(), transfer->data.begin() + 8);
        else
            data_to_send = std::vector<uint8_t>(transfer->data.begin(), transfer->data.end());

        CanTsFrame frame = CanTsFrame::CreateSetBlockTransfer(transfer->address, address_, 0, data_to_send);
        if (!SendFrame(frame)) {
            sb_transfers_.erase(transfer);
            qCCritical(cants_sb) << "Failed sending transfer frame to address =" << frame.toAddress_;
            emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendDataFailed);
        } else {
            transfer->txState = SetBlockTransfer::TxState::kSendingData;
            transfer->rxState = SetBlockTransfer::RxState::kIdle;
            qCDebug(cants_sb) << "Sending transfer frame to address =" << frame.toAddress_ << "data =" << data_to_send;
        }
    } else if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForAbortACK) {
        // If invalid ACK response.
        if ((blocks_bits != 0) || (!frame.data_.empty())) {
            qCDebug(cants_sb) << "Invalid abort frame ACK response from address =" << frame.fromAddress_
                              << "sequence =" << blocks_bits << "data =" << frame.data_;
            return;
        }

        transfer->watchdog->stop();
        qCDebug(cants_sb) << "Received abort frame ACK from address =" << frame.fromAddress_;

        if (transfer->done && CanTsUtils::IsBitmapSet(transfer->bitmap, transfer->blocks)) {
            // If abort was sent because transfer completed.
            sb_transfers_.erase(transfer);
            emit SendBlockCompleted(frame.fromAddress_);
        } else {
            // If abort was sent because max report retries reached.
            sb_transfers_.erase(transfer);
            emit SendBlockFailed(frame.fromAddress_, SendBlockError::kMaxReportRetriesReached);
        }
    } else {
        qCCritical(cants_sb) << "Unexpected ACK from address =" << frame.fromAddress_;
    }
}

void CAN_TS::SendBlockFrameReceivedNack(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer)
{
    auto blocks_bits = frame.GetBlockCmdBits();

    if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForRequestACK) {
        // If invalid NACK response.
        if ((blocks_bits != 0) || (!frame.data_.empty())) {
            qCDebug(cants_sb) << "Invalid request frame NACK from address =" << frame.fromAddress_
                              << "sequence =" << blocks_bits << "data =" << frame.data_;
            return;
        }

        transfer->watchdog->stop();
        transfer->rxState = SetBlockTransfer::RxState::kIdle;
        qCCritical(cants_sb) << "Received request frame NACK from address =" << frame.fromAddress_;
        SendBlockRetryRequest(transfer);
    } else if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForData) {
        // If invalid NACK response.
        if ((blocks_bits != 0) || (!frame.data_.empty())) {
            qCDebug(cants_sb) << "Invalid status frame NACK from address =" << frame.fromAddress_
                              << "sequence =" << blocks_bits << "data =" << frame.data_;
            return;
        }

        transfer->watchdog->stop();
        transfer->rxState = SetBlockTransfer::RxState::kIdle;
        qCCritical(cants_sb) << "Received status frame NACK from address =" << frame.fromAddress_;
        SendBlockRetryStatus(transfer);
    } else if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForAbortACK) {
        // If invalid NACK response.
        if ((blocks_bits != 0) || (!frame.data_.empty())) {
            qCDebug(cants_sb) << "Invalid abort frame NACK from address =" << frame.fromAddress_
                              << "sequence =" << blocks_bits << "data =" << frame.data_;
            return;
        }

        transfer->watchdog->stop();
        qCCritical(cants_sb) << "Received abort frame NACK from address =" << frame.fromAddress_;

        if (transfer->done && CanTsUtils::IsBitmapSet(transfer->bitmap, transfer->blocks)) {
            // If abort was sent because transfer completed.
            sb_transfers_.erase(transfer);
            emit SendBlockFailed(frame.fromAddress_, SendBlockError::kAbortNACKReceived);
        } else {
            // If abort was sent because max report retries reached.
            sb_transfers_.erase(transfer);
            emit SendBlockFailed(frame.fromAddress_, SendBlockError::kMaxReportRetriesReached);
        }
    } else {
        qCCritical(cants_sb) << "Unexpectd NACK from address =" << frame.fromAddress_;
    }
}

void CAN_TS::SendBlockFrameReceivedReport(const CanTsFrame& frame, const std::vector<SetBlockTransfer>::iterator& transfer)
{
    auto done_bit = frame.GetDoneBit();

    if (transfer->rxState == SetBlockTransfer::RxState::kWaitingForData) {

        if (!CanTsUtils::IsBitmapValid(frame.data_, transfer->blocks) ||
            (done_bit && (!CanTsUtils::IsBitmapSet(frame.data_, transfer->blocks)))) {
            qCCritical(cants_sb) << "Received report frame with invalid bitmap from address =" << frame.fromAddress_
                                 << "done =" << done_bit << "bitmap =" << frame.data_;
        } else if (done_bit && CanTsUtils::IsBitmapSet(frame.data_, transfer->blocks)) {
            qCDebug(cants_sb) << "Received report frame from address =" << frame.fromAddress_ << "done = true"
                              << "bitmap =" << frame.data_;

            transfer->watchdog->stop();
            transfer->retry_count = 0;
            transfer->bitmap = frame.data_;
            transfer->done = true;

            CanTsFrame frame = CanTsFrame::CreateSetBlockAbort(transfer->address, address_);
            if (!SendFrame(frame)) {
                sb_transfers_.erase(transfer);
                qCCritical(cants_sb) << "Failed sending abort frame to address =" << frame.toAddress_;
                emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendAbortFailed);
            } else {
                transfer->txState = SetBlockTransfer::TxState::kSendingAbort;
                transfer->rxState = SetBlockTransfer::RxState::kIdle;
                qCDebug(cants_sb) << "Sending abort frame to address =" << frame.toAddress_;
            }
        } else if (!done_bit && CanTsUtils::IsBitmapSet(frame.data_, transfer->blocks)) {
            qCDebug(cants_sb) << "Received report frame from address =" << frame.fromAddress_ << "done = false"
                              << "bitmap =" << frame.data_;

            transfer->watchdog->stop();
            transfer->retry_count = 0;
            transfer->bitmap = frame.data_;
            transfer->done = false;

            if (transfer->report_retry_count > transfer->max_report_retries) {
                CanTsFrame frame = CanTsFrame::CreateSetBlockAbort(transfer->address, address_);

                if (!SendFrame(frame)) {
                    sb_transfers_.erase(transfer);
                    emit SendBlockFailed(frame.toAddress_, SendBlockError::kMaxReportRetriesReached);
                    qCCritical(cants_sb) << "Failed sending abort frame to address =" << frame.toAddress_;
                } else {
                    transfer->txState = SetBlockTransfer::TxState::kSendingAbort;
                    transfer->rxState = SetBlockTransfer::RxState::kIdle;
                    qCDebug(cants_sb) << "Sendin abort frame to address =" << frame.toAddress_;
                }
            } else {
                transfer->report_retry_count++;
                transfer->report_delay_timer->start(static_cast<int>(transfer->report_delay));
                transfer->txState = SetBlockTransfer::TxState::kWaitingForSendStatusRequest;
                transfer->rxState = SetBlockTransfer::RxState::kIdle;
            }
        } else {
            qCDebug(cants_sb) << "Received report from address =" << frame.fromAddress_ << "done = false"
                              << "bitmap =" << frame.data_;

            transfer->watchdog->stop();
            transfer->retry_count = 0;
            transfer->bitmap = frame.data_;
            transfer->done = false;

            if (transfer->report_retry_count > transfer->max_report_retries) {
                CanTsFrame frame = CanTsFrame::CreateSetBlockAbort(transfer->address, address_);

                if (!SendFrame(frame)) {
                    sb_transfers_.erase(transfer);
                    emit SendBlockFailed(frame.toAddress_, SendBlockError::kMaxReportRetriesReached);
                    qCCritical(cants_sb) << "Failed sending abort frame to address =" << frame.toAddress_;
                } else {
                    transfer->txState = SetBlockTransfer::TxState::kSendingAbort;
                    transfer->rxState = SetBlockTransfer::RxState::kIdle;
                    qCDebug(cants_sb) << "Sending abort frame to address =" << frame.toAddress_;
                }
            } else {
                for (uint8_t sequence = 0; sequence < transfer->blocks; sequence++) {
                    if (!CanTsUtils::IsBitmapBitSet(transfer->bitmap, sequence)) {
                        std::vector<uint8_t> data_to_send;

                        if (static_cast<size_t>(8 * sequence + 8) < transfer->data.size())
                            data_to_send = std::vector<uint8_t>(transfer->data.begin() + 8 * sequence, transfer->data.begin() + 8 * sequence + 8);
                        else
                            data_to_send = std::vector<uint8_t>(transfer->data.begin() + 8 * sequence, transfer->data.end());

                        CanTsFrame frame = CanTsFrame::CreateSetBlockTransfer(transfer->address, address_, sequence, data_to_send);
                        if (!SendFrame(frame)) {
                            sb_transfers_.erase(transfer);
                            qCCritical(cants_sb) << "Failed sending transfer frame to address =" << frame.toAddress_ << "sequence =" << sequence;
                            emit SendBlockFailed(frame.toAddress_, SendBlockError::kSendDataFailed);
                        } else {
                            transfer->report_retry_count++;
                            transfer->txState = SetBlockTransfer::TxState::kSendingData;
                            transfer->rxState = SetBlockTransfer::RxState::kIdle;
                            qCDebug(cants_sb) << "Sending transfer frame to address =" << frame.toAddress_ << "sequence =" << sequence << "data =" << data_to_send;
                        }
                    }
                }
            }
        }
    } else {
        qCCritical(cants_sb) << "Unexpected report frame from address =" << frame.fromAddress_;
    }
}

void CAN_TS::FrameReceivedSetBlock(const CanTsFrame& frame)
{
    auto frame_type = frame.GetSBFrameType();
    auto from_address = frame.GetFromAddress();

    auto transfer = std::find_if(std::begin(sb_transfers_), std::end(sb_transfers_),
        [from_address](const SetBlockTransfer& t) -> bool { return (t.address == from_address); });

    if (transfer == sb_transfers_.end()) {
        qCCritical(cants_sb) << "Transfer not active";
    } else if (frame_type == CanTsFrame::SetBlockFrameType::ACK) {
        SendBlockFrameReceivedAck(frame, transfer);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::NACK) {
        SendBlockFrameReceivedNack(frame, transfer);
    } else if (frame_type == CanTsFrame::SetBlockFrameType::REPORT) {
        SendBlockFrameReceivedReport(frame, transfer);
    } else {
        qCCritical(cants_sb) << "Recived invalid frame type from address =" << frame.fromAddress_ << "type =" << static_cast<int>(frame_type);
    }
}

} // namespace sky
