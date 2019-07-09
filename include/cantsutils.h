/* See the file "LICENSE.txt" for the full license governing this code. */

#ifndef CANTSUTILS_H
#define CANTSUTILS_H

#include <vector>
#include <cstdint>
#include <QString>

namespace sky
{

//! Miscelaneous CAN-TS functions.
class CanTsUtils {

public:

    // Prevent instancing of this class.
    CanTsUtils() = delete;
    ~CanTsUtils() = delete;

    //! Checks if block transfer's bitmap is set.
    /*!
        \param bitmap Block transfer's bitmap.
        \param num_blocks Number of blocks.
        \retval true All bits set.
        \retval false Not all bits set.
    */
    static bool IsBitmapSet(const std::vector<uint8_t>& bitmap, uint8_t num_blocks);

    //! Checks if block transfer's bitmap is cleared.
    /*!
        \param bitmap Block transfer's bitmap.
        \param num_blocks Number of blocks.
        \retval true All bits cleared.
        \retval false Not all bits cleared.
    */
    static bool IsBitmapCleared(const std::vector<uint8_t>& bitmap, uint8_t num_blocks);

    //! Checks if bit in block transfer's bitmap is set.
    /*!
        \param bitmap Block transfer's bitmap.
        \param bit_indx Index of bit.
        \retval true All bits set.
        \retval false Not all bits set.
    */
    static bool IsBitmapBitSet(const std::vector<uint8_t>& bitmap, uint8_t bit_indx);

    //! Sets bit in block transfer's bitmap.
    /*!
        \param bitmap Block transfer's bitmap.
        \param bit_indx Index of bit.
    */
    static void SetBitmapBit(std::vector<uint8_t>& bitmap, uint8_t bit_indx);

    //! Clears bit in block transfer's bitmap.
    /*!
        \param bitmap Block transfer's bitmap.
        \param bit_indx Index of bit.
    */
    static void ClearBitmapBit(std::vector<uint8_t>& bitmap, uint8_t bit_indx);

    //! Gets number of bytes in block transfer's bitmap.
    /*!
        \param num_blocks Number of blocks transmitted.
    */
    static size_t GetBitmapNumBytes(uint8_t num_blocks);

    //! Checks if the given block transfer's bitmap is valid.
    /*!
        \param bitmap Block transfer's bitmap.
        \param num_blocks Number of blocks transmitted.
    */
    static bool IsBitmapValid(const std::vector<uint8_t>& bitmap, uint8_t num_blocks);

    //! Sets the given block transfer's bitmap bits.
    /*!
        \param bitmap Block transfer's bitmap.
        \param num_blocks Number of blocks transmitted.
    */
    static void SetBitmap(std::vector<uint8_t>& bitmap, uint8_t num_blocks);

    //! Converts value to byte vector in little endian format.
    /*!
        \param value Integer value to be converted into byte array.
        \param trim  If true, remove leading zeros (MSB) from vector.
    */
    template<typename T>
    static std::vector<uint8_t> ToByteVector(T value, bool trim = false) {
        std::vector<uint8_t> byte_array;

        for (size_t i = 0; i < sizeof(value); i++)
            byte_array.push_back(static_cast<uint8_t>(value >> i * 8));

        while (trim && (byte_array.back() == 0) && (byte_array.size() > 1))
            byte_array.pop_back();

        return byte_array;
    }

    static QString VectorToQString(const std::vector<uint8_t>& data);
};

} // namespace sky

#endif // CANTSUTILS_H
