#pragma once

#include "byte_stream.hh"
#include <cstdint>
#include <vector>

class DynamicBitset
{
public:
  DynamicBitset( u_int64_t size ) : bitset( ( size + 63 ) / 64, 0 ) {}

  // 调整大小
  void resize( uint64_t n );

  // 设置某个位为 1
  void set( uint64_t index );

  // 设置某个位为 0
  void reset( uint64_t index );

  // 查找下一个为 0 的位
  uint64_t find_next( uint64_t start = 0 ) const;

  uint64_t find_first_zero_from( uint64_t start ) const;

  // 获取指定位置的位值
  bool get( uint64_t index ) const;

private:
  std::vector<uint64_t> bitset;
};

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}
  // 构造函数接受一个 右值引用的 ByteStream 对象（即一个临时的、可移动的流）。
  // 使用 std::move(output) 表示你要把外部的流资源“夺过来”（而不是拷贝一份）。

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_; // 这是 Reassembler 最重要的内部成员：最终要把拼好的数据写到这个流里去。
  uint64_t eof_len_ = -1;
  uint64_t pending_bytes_ {};
  std::vector<char> buffer_ {};
  // std::vector<bool> is_inserted_ {};
  DynamicBitset is_inserted_{0};
};
