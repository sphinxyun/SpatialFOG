/*
 * AnppPacket.cpp
 *
 *  Created on: Jun 1, 2016
 *      Author: burghart
 */

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <boost/archive/binary_iarchive.hpp>
#include <logx/Logging.h>
#include "AnppPacket.h"

LOGGING("AnppPacket");

// Latest time of validity received in a System State or Unix Time packet.
uint32_t AnppPacket::_LatestTimeOfValiditySeconds = 0;
uint32_t AnppPacket::_LatestTimeOfValidityMicroseconds = 0;

AnppPacket::BadHeader::~BadHeader() throw () {}
AnppPacket::NeedMoreData::~NeedMoreData() throw () {}

AnppPacket::AnppPacket() :
    _dataPtr(NULL),
    _timeOfValiditySeconds(0),
    _timeOfValidityMicroseconds(0) {
    // Verify that our header struct is packed without added padding bytes
    assert(sizeof(_header) == _HEADER_LEN);
    // Zero-fill the header
    memset(reinterpret_cast<void*>(&_header), 0, sizeof(_header));
}

AnppPacket::AnppPacket(uint8_t packetId, uint8_t packetDataLen) :
  _dataPtr(NULL),
  // Use latest time of validity we've received
  _timeOfValiditySeconds(_LatestTimeOfValiditySeconds),
  _timeOfValidityMicroseconds(_LatestTimeOfValidityMicroseconds) {
  _header._packetId = packetId;
  _header._packetDataLen = packetDataLen;
}

AnppPacket::~AnppPacket() {
}

uint8_t
AnppPacket::_CalculateLRC(const void * data, int length) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t*>(data);
  uint8_t lrc = 0;
  for (int i = 0; i < length; i++) {
    lrc += bytes[i];          // overflow (if any) is truncated
  }
  lrc = (lrc ^ 0xff) + 1;     // again, overflow is truncated
  return(lrc);
}

// There are pedantic arguments about how to correctly implement CRC16-CCITT.
// The implementation below comes from Advanced Navigation's own example
// software, so presumably generates the version which they consider correct.
// Hence, that's what we should use as well.
static const uint16_t CRC16_TABLE[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

uint16_t
AnppPacket::_CalculateCRC(const void *data, int length)
{
  const uint8_t *bytes = (const uint8_t *) data;
  uint16_t crc = 0xFFFF, i;
  for (i = 0; i < length; i++)
  {
    crc = (uint16_t)((crc << 8) ^ CRC16_TABLE[(crc >> 8) ^ bytes[i]]);
  }
  return crc;
}

void
AnppPacket::_initializeFromRaw(const void * rawData, uint32_t rawLength) {
  const uint8_t * uint8Data = reinterpret_cast<const uint8_t*>(rawData);

  // ANPP packets contain little-endian data, and much of the implementation
  // here assumes the machine we're working on is also little-endian.
  //
  // The static variable will be evaluated once on the first call to
  // this method.
  static const bool machineIsLittleEndian = MachineIsLittleEndian();
  assert(machineIsLittleEndian);

  // Verify that our header struct is packed without added padding bytes
  assert(sizeof(_header) == _HEADER_LEN);

  // The ANPP header is 5 bytes long. If we have less than that, throw a
  // NeedMoreData exception.
  std::ostringstream oss;
  if (rawLength < _HEADER_LEN) {
    oss << "Need " << _HEADER_LEN - rawLength << " more bytes for ANPP header";
    throw NeedMoreData(oss.str());
  }

  // Assign the latest time of validity we received as this packet's time
  _setTimeOfValidity();

  // Corner case: An all-zero header will give a valid LRC, but is not really
  // a valid packet header, because all valid packets have a non-zero data
  // length. Throw BadHeader in this case.
  uint headerSum = 0;
  for (int i = 0; i < _HEADER_LEN; i++) {
      headerSum += uint8Data[i];
  }
  if (headerSum == 0) {
      throw BadHeader("All-zero header");
  }

  // Copy the 5-byte header into our header struct
  memcpy(&_header, rawData, _HEADER_LEN);

  // Byte 0 of the header contains the LRC for the remaining four bytes of the
  // header. Read it and confirm that it's correct.
  uint8_t calculatedLRC =
          _CalculateLRC(reinterpret_cast<const uint8_t*>(&_header) + 1, 4);
  if (calculatedLRC != headerLRC()) {
    oss << "Header LRC 0x" << std::hex << headerLRC() <<
          " does not match calculated LRC 0x" << uint(calculatedLRC);
    throw BadHeader(oss.str());
  }

  // With the header in place, we now know the packet data length. If rawData
  // does not have enough bytes to provide the header + the data length, throw
  // NeedMoreData.
  if (rawLength < fullPacketLen()) {
      oss << "Need " << fullPacketLen() - rawLength << " more data bytes";
      throw NeedMoreData(oss.str());
  }

  // Copy the data bytes into the packet's _dataPtr.
  memcpy(_dataPtr, uint8Data + _HEADER_LEN, packetDataLen());
}

void
AnppPacket::_updateHeader() {
  // Do the CRC first, since the CRC itself goes into the calculation of 
  // LRC below.
  _header._packetDataCRC = _dataPtr ? _CalculateCRC(_dataPtr, packetDataLen()) : 0;
  
  // Now calculate the header LRC from the last four bytes of the header.
  _header._headerLRC = 
      _CalculateLRC(reinterpret_cast<const uint8_t*>(&_header) + 1, 4);
}

bool
AnppPacket::crcIsGood() const {
    return(crcFromHeader() == _CalculateCRC(_dataPtr, packetDataLen()));
}

std::vector<uint8_t>
AnppPacket::rawBytes() const {
    std::vector<uint8_t> rawVec;

    // Copy the header into rawVec
    const uint8_t * headerBytes = reinterpret_cast<const uint8_t*>(&_header);
    for (uint i = 0; i < _HEADER_LEN; i++) {
        rawVec.push_back(headerBytes[i]);
    }

    // Copy the data bytes into rawVec
    for (uint i = 0; i < packetDataLen(); i++) {
        rawVec.push_back(_dataPtr[i]);
    }

    return(rawVec);
}