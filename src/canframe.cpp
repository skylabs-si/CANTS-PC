/* See the file "LICENSE.txt" for the full license governing this code. */

#include "canframe.h"

namespace sky {

std::vector<uint8_t> CanFrame::ToStdVector() const {
    std::vector<uint8_t> v;

    uint8_t byte = 0;
    byte = data.size() & 0xF;            // Bit 3-0 - data length
    byte = rtr ? (byte | 1<<6) : byte;   // Bit 6   - 1-RTR, 0-Data packet
    byte = extid ? (byte | 1<<7) : byte; // Bit 7   - 1-extended CAN ID (29 bit),
                                         //           0-standard CAN ID (11 bit)

    // Byte 0 - Frame options
    v.push_back(byte);

    // Byte 1-2 or 1-4 (if extended) - CAN ID
    v.push_back(id & 0xFF);
    v.push_back((id >> 8) & 0xFF);

    if (extid) {
        v.push_back((id >> 16) & 0xFF);
        v.push_back((id >> 24) & 0xFF);
    }

    // Byte 3-10 or 5-12 (if extended) - Data bytes (max. 8 bytes)
    for (size_t i=0; i < data.size() && i < 8; i++) {
        v.push_back(data.at(i));
    }

    return v;
}

CanFrame CanFrame::FromStdVector(const std::vector<uint8_t>& data) {

    CanFrame f;

    f.rtr = static_cast<bool>((data.at(0) >> 6) & 1);
    f.extid = static_cast<bool>((data.at(0) >> 7) & 1);

    auto di = data.begin();

    if (f.extid) {
        f.id = static_cast<uint32_t>(data.at(4)<<24U) | static_cast<uint32_t>(data.at(3)<<16U) |
               static_cast<uint32_t>(data.at(2)<<8U) | static_cast<uint32_t>(data.at(1));
        std::advance(di, 5);
    } else {
        f.id = static_cast<uint32_t>(data.at(2)<<8U) | static_cast<uint32_t>(data.at(1));
        std::advance(di, 3);
    }

    std::copy(di, data.end(), std::back_inserter(f.data));

    return f;
}

}
