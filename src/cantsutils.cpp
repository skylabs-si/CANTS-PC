/* See the file "LICENSE.txt" for the full license governing this code. */

#include "cantsutils.h"
#include <vector>
#include <cstdint>

namespace sky
{

bool CanTsUtils::IsBitmapSet(const std::vector<uint8_t>& bitmap, uint8_t num_blocks)
{
    for (uint8_t i = 0; i < num_blocks / 8; i++) {
        if (bitmap[i] != 0xFF)
            return false;
    }

    if (num_blocks % 8) {
        if (bitmap[num_blocks / 8] != (0x01 << (num_blocks % 8)) - 1)
            return false;
    }

    return true;
}

bool CanTsUtils::IsBitmapCleared(const std::vector<uint8_t>& bitmap, uint8_t num_blocks)
{
    for (uint8_t i = 0; i < (num_blocks + 7) / 8; i++) {
        if (bitmap[i] != 0)
            return false;
    }

    return true;
}

bool CanTsUtils::IsBitmapBitSet(const std::vector<uint8_t>& bitmap, uint8_t bit_indx)
{
    return (bitmap[bit_indx / 8] & (0x01 << (bit_indx % 8)));
}

void CanTsUtils::SetBitmapBit(std::vector<uint8_t>& bitmap, uint8_t bit_indx)
{
    bitmap[bit_indx / 8] |= (0x01 << (bit_indx % 8));
}

void CanTsUtils::ClearBitmapBit(std::vector<uint8_t>& bitmap, uint8_t bit_indx)
{
    bitmap[bit_indx / 8] &= ~(0x01 << (bit_indx % 8));
}

size_t CanTsUtils::GetBitmapNumBytes(uint8_t num_blocks)
{
    return (num_blocks + 7) / 8;
}

bool CanTsUtils::IsBitmapValid(const std::vector<uint8_t>& bitmap, uint8_t num_blocks)
{
    int remainder;

    if (GetBitmapNumBytes(num_blocks) != bitmap.size())
        return false;

    remainder = num_blocks % 8;

    if (!remainder)
        return true;

    return !(bitmap[num_blocks / 8] & (~((0x01 << remainder) - 1)));
}

void CanTsUtils::SetBitmap(std::vector<uint8_t>& bitmap, uint8_t num_blocks)
{
    for (uint8_t i = 0; i < num_blocks / 8; i++) {
        bitmap[i] = 0xFF;
    }

    if (num_blocks % 8) {
        bitmap[num_blocks / 8] = static_cast<uint8_t>((0x01 << (num_blocks % 8)) - 1);
    }
}

QString CanTsUtils::VectorToQString(const std::vector<uint8_t> &data)
{
    QString str;
    for (uint8_t i : data)
        str.append(QString("%1").arg(i, 2, 16, QChar('0')));
    return str;
}

}
