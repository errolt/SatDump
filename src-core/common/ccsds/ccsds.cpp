#include "ccsds.h"
#include <cstdint>
#include <cmath>
#include <cstring>

namespace ccsds
{
    CCSDSHeader::CCSDSHeader()
    {
    }

    CCSDSHeader::CCSDSHeader(uint8_t *rawi)
    {
        std::memcpy(raw, rawi, 6);
        version = rawi[0] >> 5;
        type = (rawi[0] >> 4) % 2;
        secondary_header_flag = (rawi[0] >> 3) % 2;
        apid = (rawi[0] % (int)pow(2, 3)) << 8 | rawi[1];
        sequence_flag = rawi[2] >> 6;
        packet_sequence_count = (rawi[2] % (int)pow(2, 6)) << 8 | rawi[3];
        packet_length = rawi[4] << 8 | rawi[5];
    }

    void CCSDSHeader::encodeHDR()
    {
        raw[0] = (version & 0b111) << 5 | (type & 0b1) << 4 | (secondary_header_flag & 0b1) << 3 | (apid >> 8) & 0b111;
        raw[1] = apid & 0xFF;
        raw[2] = (sequence_flag & 0b11) << 6 | (packet_sequence_count >> 8) & 0b111111;
        raw[3] = packet_sequence_count & 0xFF;
        raw[4] = packet_length >> 8;
        raw[5] = packet_length & 0xFF;
    }

    void CCSDSPacket::encodeHDR()
    {
        header.packet_length = payload.size() - 1;
        header.encodeHDR();
    }

    // Parse CCSDS header
    CCSDSHeader parseCCSDSHeader(uint8_t *header)
    {
        return CCSDSHeader(header);
    }
} // namespace proba