/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(cants_un, "sky::CAN_TS::Unsolicited")

namespace sky
{

bool CAN_TS::SendUnsolicited(uint8_t address, uint8_t channel, const std::vector<uint8_t>& data)
{
    if (address == static_cast<uint8_t>(sky::CanTsFrame::Address::TIME_SYNC)) {
        qCCritical(cants_un) << "Invalid (reserved) address" << address;
        return false;
    }

    if (data.size() > 8) {
        qCCritical(cants_un) << "Invalid data length" << data.size();
        return false;
    }

    CanTsFrame frame = CanTsFrame::CreateUnsolicited(address, address_, channel, data);
    if (!SendFrame(frame)) {
        qCCritical(cants_un) << "Sending unsolicited frame failed" << frame;
        emit SendUnsolicitedFailed(address, channel);
        return false;
    }

    qCDebug(cants_un) << "Sending usolicited frame to address =" << address
                      << "channel =" << channel << " data =" << data;
    return true;
}

void CAN_TS::SendUnsolicitedFrameSent(const CanTsFrame& frame)
{
    qCDebug(cants_un) << "Unsolicited frame sent to address =" << frame.GetToAddress()
                      << "channel =" << frame.GetChannel();
    emit SendUnsolicitedCompleted(frame.GetToAddress(), frame.GetChannel());
}

void CAN_TS::SendUnsolicitedFrameSendError(const CanTsFrame& frame, CommDriver::CanSendError error)
{
    qCCritical(cants_un) << "Failed sending unsolicited to address =" << frame.GetToAddress()
                         << "channel =" << frame.GetChannel() << "error =" << error;
    emit SendUnsolicitedFailed(frame.GetToAddress(), frame.GetChannel());
}

void CAN_TS::ReceivedUnsolicitedFrame(const CanTsFrame& frame)
{
    auto channel = frame.GetChannel();

    qCDebug(cants_un) << "Received unsolicited frame from address =" << frame.GetFromAddress()
                      << "channel =" << frame.GetChannel() << "data =" << frame.GetData();
    emit UnsolicitedReceived(frame.fromAddress_, channel, frame.data_);
}

void CAN_TS::ReceivedKeepAliveFrame(const CanTsFrame& frame, bool nominal_bus)
{
    qCDebug(cants_un) << "Received keep alive fram from address=" << frame.GetFromAddress()
                      << "channel =" << frame.GetChannel() << "data =" << frame.GetData() << "nominal_bus =" << nominal_bus;

    if (nominal_bus)
        emit KeepAliveReceivedNominal(frame.GetFromAddress(), frame.GetChannel(), frame.GetData());
    else
        emit KeepAliveReceivedRedundant(frame.GetFromAddress(), frame.GetChannel(), frame.GetData());
}

} // namespace sky
