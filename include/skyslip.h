/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef SLIP_H
#define SLIP_H

#include <QObject>
#include <cstdint>

namespace sky {

//! Provides functions to SLIP and SKY-SLIP framing implementation.
/*!
    SLIP is variable-length frame encoding and decoding according to RFC 1055.
    SKY-SLIP is SkyLabs' application protocol encapsulated in SLIP framing.
*/
class SkySlip : public QObject
{
    Q_OBJECT

public:

    // NOTE
    // If 'SendCan0' and 'SendCan1' are changed, make sure
    // you update corresponding values in CommDriver.
    //! This enum describes possible commands in SKY-SLIP frame.
    enum Cmd : uint8_t {
        SendCan0     = 0x00, //!< Send frame to CAN0 interface.
        SendCan1     = 0x01, //!< Send frame to CAN1 interface.
        DongleReport = 0x02, //!< Report available space in Tx buffer.
    };
    Q_ENUM(Cmd)

    //! This enum describes state of SKY-SLIP parser.
    enum State : uint8_t {
        RxBegin, //!< Start of frame marker.
        Command, //!< Command byte.
        Payload //!< Payload bytes.
    };

    //! This struct contains SKY-SLIP frame in \a payload.
    //! \sa Cmd
    struct Frame {
        Cmd cmd = Cmd::SendCan0;
        std::vector<uint8_t> payload;
    };

    //! Encodes bytes in \a data into SLIP frame, adds \a cmd and returns SKY-SLIP frame.
    std::vector<uint8_t> Encode(Cmd cmd, const std::vector<uint8_t>& data);

    //! Decodes \a data from SKY-SLIP frame and returns decoded bytes via Qt signal.
    void Decode(const std::vector<uint8_t>& data);

    //! Flushes current SLIP processing information.
    void Flush();

signals:
    void FrameReceived(sky::SkySlip::Frame f);

private:
    static const uint8_t SLIP_END;
    static const uint8_t SLIP_ESC;
    static const uint8_t SLIP_ESC_END;
    static const uint8_t SLIP_ESC_ESC;

    bool  esc_ = false;
    State state_ = State::RxBegin;
    Frame frame_;

    //! Checks if given command is valid.
    bool IsValidCmd(uint8_t);
};

} // namespace sky

Q_DECLARE_METATYPE(sky::SkySlip::Frame);

#endif // SLIP_H
