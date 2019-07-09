/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef CANFRAME_H
#define CANFRAME_H

#include <cstdint>
#include <vector>

namespace sky {

//! The CanFrame class includes CAN frame.
class CanFrame {
public:
    uint32_t id = 0; //!< CAN frame ID (29 or 11 bits, set remaining to 0).
    bool extid = false; //!< Check if frame uses extended ID (29-bit) or normal ID (11-bit).
    bool rtr = false; //!< Check for Retransmission bit.
    std::vector<uint8_t> data; //!< CanFrame payload (max 8 bytes).

    //! Creates byte vector from CommDriver::CanFrame object.
    std::vector<uint8_t> ToStdVector() const;

    //! Converts input byte array \a data to CommDriver::CanFrame object.
    static CanFrame FromStdVector(const std::vector<uint8_t>& data);
};

}

#endif // CANFRAME_H
