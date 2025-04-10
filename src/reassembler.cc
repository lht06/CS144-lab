#include "reassembler.hh"
#include "debug.hh"
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    eof_len_ = first_index + data.size(); // 记录eoflen
  }
  auto read_end = output_.writer().bytes_pushed();
  auto write_end = read_end + output_.writer().available_capacity();
  for ( uint64_t i = first_index; i < first_index + data.size(); i++ ) {
    if ( read_end <= i && i < write_end ) {
      while ( buffer_.size() <= i ) {
        buffer_.push_back( '\0' );
        is_inserted_.push_back( false );
      }
      if ( is_inserted_[i] ) {
        continue;
      }
      buffer_[i] = data[i - first_index];
      is_inserted_[i] = true;
      pending_bytes++;
    }
  }
  string put;
  while ( read_end < buffer_.size() ) {
    if ( is_inserted_[read_end] ) {
      put += buffer_[read_end++];
    } else {
      break;
    }
  }
  auto pre_bytes = output_.writer().bytes_pushed();
  output_.writer().push( put );
  auto suf_bytes = output_.writer().bytes_pushed();
  pending_bytes -= suf_bytes - pre_bytes;
  if ( eof_len_ != static_cast<uint64_t>( -1 ) && read_end == eof_len_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return pending_bytes;
  // debug( "unimplemented count_bytes_pending() called" );
}
