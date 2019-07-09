/* See the file "LICENSE.txt" for the full license governing this code. */

#include "skyslip.h"
#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(slip, "sky::SkySlip")

namespace sky {

const uint8_t SkySlip::SLIP_END = 0xC0;
const uint8_t SkySlip::SLIP_ESC = 0xDB;
const uint8_t SkySlip::SLIP_ESC_END = 0xDC;
const uint8_t SkySlip::SLIP_ESC_ESC = 0xDD;

std::vector<uint8_t> SkySlip::Encode(Cmd cmd, const std::vector<uint8_t>& data)
{
    std::vector<uint8_t> slip;

    slip.push_back(SLIP_END);
    slip.push_back(cmd);

    for(auto& e : data) {
        if (e == SLIP_END) {
            slip.push_back(SLIP_ESC);
            slip.push_back(SLIP_ESC_END);
        } else if (e == SLIP_ESC) {
            slip.push_back(SLIP_ESC);
            slip.push_back(SLIP_ESC_ESC);
        } else {
            slip.push_back(e);
        }
    }

    slip.push_back(SLIP_END);

    return slip;
}

void SkySlip::Decode(const std::vector<uint8_t>& data)
{
    for (uint8_t ch : data) {

        switch (state_) {
        case RxBegin:
            if (SLIP_END == ch) {
                state_ = Command;
                frame_.payload.clear();
            }
            break;
        case Command:
            if (SLIP_END == ch) {
                state_ = Command;
                frame_.payload.clear();
            } else if (IsValidCmd(ch)) {
                frame_.cmd = static_cast<Cmd>(ch);
                state_ = Payload;
            } else {
                qCDebug(slip) << "Ignoring frame with invalid command value" << ch;
                state_ = RxBegin;
            }

            break;
        case Payload:
            if (SLIP_END == ch) {
                // End of frame received
                emit FrameReceived(frame_);
                state_ = RxBegin;
            } else if (SLIP_ESC == ch) {
                esc_ = true;
            } else {
                if (esc_) {
                    if (SLIP_ESC_END == ch) {
                        ch = SLIP_END;
                    } else if (SLIP_ESC_ESC == ch) {
                        ch = SLIP_ESC;
                    }
                    esc_ = false;
                }
                frame_.payload.push_back(static_cast<uint8_t>(ch));
            }

            break;
        }
    }
}

void SkySlip::Flush()
{
    state_ = RxBegin;
    esc_ = false;
}

bool SkySlip::IsValidCmd(uint8_t cmd)
{
    return (cmd == SendCan0) || (cmd == SendCan1) || (cmd == DongleReport);
}

} // namespace sky
