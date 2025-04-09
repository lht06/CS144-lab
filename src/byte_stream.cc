#include "byte_stream.hh"
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( is_closed() )
    return;
  if ( haveWritten_ - haveRead_ == capacity_ )
    return;
  size_t len = min( capacity_ - ( haveWritten_ - haveRead_ ), data.size() );
  buffer_ += data.substr( 0, len );
  haveWritten_ += len;
}

void Writer::close()
{
  writeClosed_ = true;
}

bool Writer::is_closed() const
{
  return writeClosed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( haveWritten_ - haveRead_ );
}

uint64_t Writer::bytes_pushed() const
{
  return haveWritten_;
}

string_view Reader::peek() const
{
  return std::string_view( buffer_ );
}

void Reader::pop( uint64_t len )
{
  len = min( len, buffer_.size() );
  if ( len == 0 )
    return;
  buffer_.erase( 0, len );
  haveRead_ += len;
}

bool Reader::is_finished() const
{
  return writeClosed_ && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return haveRead_;
}
