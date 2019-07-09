/* See the file "LICENSE.txt" for the full license governing this code. */

#include "can_ts.h"
#include "cantsutils.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(cants_ts, "sky::CAN_TS::TimeSync")

namespace sky
{

bool CAN_TS::SendTimeSync(uint64_t time)
{
    const std::vector<uint8_t>& data = CanTsUtils::ToByteVector(time, false);
    CanTsFrame frame = CanTsFrame::CreateTimeSync(address_, data);

    if (!SendFrame(frame)) {
        qCCritical(cants_ts) << "Time sync send frame failed";
        emit SendTimeSyncFailed();
        return false;
    } else {
        qCDebug(cants_ts) << "Sending time sync frame with time =" << time;
        return true;
    }
}

void CAN_TS::SendTimeSyncFrameSent()
{
    qCDebug(cants_ts) << "Time sync sent";
    emit SendTimeSyncCompleted();
}

void CAN_TS::SendTimeSyncFrameSendError(CommDriver::CanSendError error)
{
    qCCritical(cants_ts) << "Failed sending time sync error=" << error;
    emit SendTimeSyncFailed();
}

void CAN_TS::SendTimeSyncFrameReceived(const CanTsFrame& frame)
{
    qCDebug(cants_ts) << "Received time sync from address=" << frame.GetFromAddress() << "time =" << frame.GetData();
    emit TimeSyncReceived(frame.GetFromAddress(), frame.GetData());
}

} // namespace sky
