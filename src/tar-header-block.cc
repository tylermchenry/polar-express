#include "tar-header-block.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace polar_express {
namespace {

// All data in the header is stored in ASCII, so endianness is not an issue.
//
// Checksum calculated as: Sum of _all_ bytes in the header block
// (INCLUDING bytes that are not part of the header fields - this accounts
// for extended headers), and considering all checksum bytes to be ASCII
// space (0x20). Stored as a 6-digit octal number with leading zeros,
// terminated by the sequence 0x00 0x20 (null then space)
struct TarHeaderFields {
  char filename[100];          // Null-terminated unless all characters used.
  char mode[8];                // Octal with leading zeros, nul-terminated.
  char owner_uid[8];           // Octal with leading zeros, nul-terminated.
  char owner_gid[8];           // Octal with leading zeros, nul-terminated.
  char size[12];               // Octal with leading zeros, nul-terminated.
  char modification_time[12];  // Octal with leading zeros, nul-terminated.
  char checksum[8];            // Octal, see notes above.
  char link_indicator;         // '0' for normal file.
  char linked_filename[100];   // Not used.
} __attribute__((packed));

const size_t kTarHeaderPaddingLength =
    TarHeaderBlock::kTarHeaderBlockLength - sizeof(TarHeaderFields);

const char kTarLinkIndicatorNormalFile = '0';

// Template to write a number into a fixed-length char buffer,
// formatted as octal with leading zeros and a nul terminator, with
// some extra characters after the nul terminator.
template <typename IntT, int N>
void WriteIntegerAsOctalWithLeadingZeros(
    IntT value, char (&c_str)[N], const string& post_terminator_str = "") {
  ostringstream os;
  os << setfill('0') << setw(N - 1 - post_terminator_str.size()) << oct
     << value;

  const string& octal_str = os.str();
  assert(octal_str.length() == N - 1 - post_terminator_str.size());
  strcpy(c_str, octal_str.c_str());
  strncpy(c_str + octal_str.size() + 1, post_terminator_str.c_str(),
          post_terminator_str.size());
}

// Template to read a number out of a fixed-length char buffer. The
// number is expected to be formatted as octal with a nul-terminator.
template <typename IntT, int N>
IntT ReadOctalWithLeadingZerosAsInteger(const char (&c_str)[N]) {
  IntT value;
  istringstream is(string(c_str, N));
  is >> oct >> value;
  return value;
}

}  // namespace

// Must be outside of the anonymous namespace because of forward
// declaration in .h file.
struct TarFileHeader {
  struct TarHeaderFields fields;
  char padding[kTarHeaderPaddingLength];
} __attribute__((packed));

TarHeaderBlock::TarHeaderBlock(ContainerOffsetPtr<char> data)
    : data_(data) {
  fill(data_.get(), data_.get() + kTarHeaderBlockLength, '\0');
  header()->fields.link_indicator = kTarLinkIndicatorNormalFile;
}

string TarHeaderBlock::filename() const {
  return string(header()->fields.filename, sizeof(header()->fields.filename));
}

void TarHeaderBlock::set_filename(const string& filename) {
  strncpy(header()->fields.filename, filename.c_str(),
          sizeof(header()->fields.filename));
}

int TarHeaderBlock::mode() const {
  return ReadOctalWithLeadingZerosAsInteger<int>(header()->fields.mode);
}

void TarHeaderBlock::set_mode(int mode) {
  WriteIntegerAsOctalWithLeadingZeros(mode, header()->fields.mode);
}

uid_t TarHeaderBlock::owner_uid() const {
  return ReadOctalWithLeadingZerosAsInteger<uid_t>(header()->fields.owner_uid);
}

void TarHeaderBlock::set_owner_uid(uid_t owner_uid) {
  WriteIntegerAsOctalWithLeadingZeros(owner_uid, header()->fields.owner_uid);
}

gid_t TarHeaderBlock::owner_gid() const {
  return ReadOctalWithLeadingZerosAsInteger<gid_t>(header()->fields.owner_gid);
}

void TarHeaderBlock::set_owner_gid(gid_t owner_gid) {
  WriteIntegerAsOctalWithLeadingZeros(owner_gid, header()->fields.owner_gid);
}

size_t TarHeaderBlock::size() const {
  return ReadOctalWithLeadingZerosAsInteger<size_t>(header()->fields.size);
}

void TarHeaderBlock::set_size(size_t size) {
  WriteIntegerAsOctalWithLeadingZeros(size, header()->fields.size);
}

time_t TarHeaderBlock::modification_time() const {
  return ReadOctalWithLeadingZerosAsInteger<time_t>(
      header()->fields.modification_time);
}

void TarHeaderBlock::set_modification_time(time_t time) {
  WriteIntegerAsOctalWithLeadingZeros(time, header()->fields.modification_time);
}

int TarHeaderBlock::checksum() const {
  return ReadOctalWithLeadingZerosAsInteger<int>(header()->fields.checksum);
}

void TarHeaderBlock::set_checksum(int checksum) {
  WriteIntegerAsOctalWithLeadingZeros(checksum, header()->fields.checksum, " ");
}

void TarHeaderBlock::ComputeAndSetChecksum() {
  fill(header()->fields.checksum,
       header()->fields.checksum + sizeof(header()->fields.checksum), ' ');

  int checksum = 0;
  const uint8_t* header_bytes_start = reinterpret_cast<uint8_t*>(header());
  for (int i = 0; i < kTarHeaderBlockLength; ++i) {
    checksum += *(header_bytes_start + i);
  }

  set_checksum(checksum);
}

TarFileHeader* TarHeaderBlock::header() {
  return reinterpret_cast<TarFileHeader*>(data_.get());
}

const TarFileHeader* TarHeaderBlock::header() const {
  return reinterpret_cast<const TarFileHeader*>(data_.get());
}

}  // namespace polar_express
