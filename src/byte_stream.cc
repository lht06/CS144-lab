#include "byte_stream.hh"
#include <iostream>
#include <string_view>
using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( is_closed() )
    return; // 如果关闭了 就不push 进来了

  size_t writable = capacity_ - ( haveWritten_ - haveRead_ ); // 容量-现在 buffer 里有的
  if ( writable == 0 )
    return; // 如果不能写 也不能push进来

  if ( data.size() <= writable ) {
    haveWritten_ += data.size();
    buffer_ += std::move( data ); // 如果可以读进来 直接 move 夺取
  } else {
    buffer_ += data.substr( 0, writable ); // 能读多少就读多少
    haveWritten_ += writable;
  }
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
  return std::string_view( buffer_ ).substr( offset_ ); // string_view 优化获取子串
}

void Reader::pop( uint64_t len )
{
  len = min( len, buffer_.size() - offset_ ); // 实际能 pop 多少
  if ( len == 0 )
    return;
  offset_ += len;
  haveRead_ += len;
  if ( offset_ > 8192 ) {
    buffer_.erase( 0, offset_ );
    offset_ = 0; // offset优化erase次数
  }
}

bool Reader::is_finished() const
{
  return writeClosed_ && buffer_.size() == offset_;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size() - offset_;
}

uint64_t Reader::bytes_popped() const
{
  return haveRead_;
}