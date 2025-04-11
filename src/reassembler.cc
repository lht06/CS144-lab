#include "reassembler.hh"
#include "debug.hh"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sys/types.h>

using namespace std;

// 调整大小
void DynamicBitset::resize( u_int64_t n )
{
  u_int64_t new_size = ( n + 63 ) / 64; // 每个 uint64_t 存储 64 位
  bitset.resize( new_size, 0 );
}

// 设置某个位为 1
void DynamicBitset::set( u_int64_t index )
{
  bitset[index / 64] |= ( 1ULL << ( index % 64 ) );
}

// 设置某个位为 0
void DynamicBitset::reset( u_int64_t index )
{
  bitset[index / 64] &= ~( 1ULL << ( index % 64 ) );
}

uint64_t DynamicBitset::find_first_zero_from( uint64_t start ) const
{
  uint64_t start_block = start / 64; // 计算起始块的位置
  uint64_t start_bit = start % 64;   // 计算起始位置在块内的偏移量

  // 遍历从 start_block 开始的所有块
  for ( uint64_t block = start_block; block < bitset.size(); ++block ) {
    uint64_t block_bits = bitset[block]; // 获取当前块的 64 位

    // 如果是起始块，将起始位置之前的位都置为 1
    if ( block == start_block ) {
      block_bits |= ( ( 1ULL << start_bit ) - 1 );
    }

    // 查找当前块中第一个为 0 的位
    uint64_t first_zero = ~block_bits; // 反转所有的位

    // 如果反转后 first_zero 中包含 0 位
    if ( first_zero != 0 ) {
      // 使用 __builtin_ffsll 找到最低的 0 位（即第一个为 0 的位）
      uint64_t i = __builtin_ffsll( first_zero ) - 1; // ffs返回的是1-based索引，需要减去1
      uint64_t pos = block * 64 + i;                  // 计算该位置的全局索引
      return pos;                                     // 返回该位置
    }
  }

  // 如果没有找到任何 0 位，则返回 bitset 的大小，表示没有更多的 0 位
  return bitset.size() * 64;
}

// 查找下一个为 0 的位，返回严格大于 start 的位置
uint64_t DynamicBitset::find_next( uint64_t start ) const
{
  return find_first_zero_from( start + 1 ); // 从 start + 1 开始查找
}

bool DynamicBitset::get( u_int64_t index ) const
{
  return ( bitset[index / 64] & ( 1ULL << ( index % 64 ) ) ) != 0;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    eof_len_ = first_index + data.size(); // 记录eoflen
  }
  auto read_end = output_.writer().bytes_pushed();//已经推了多少进去
  auto write_end = read_end + output_.writer().available_capacity();//最多能读多少
  auto loop_end = min( first_index + data.size(), write_end );//和字符串的最后位置取最小值
  if ( buffer_.size() < loop_end ) {
    buffer_.resize( loop_end * 2, '\0' );//扩容 利用x2 导致最多扩容log次 
    is_inserted_.resize( loop_end * 2 );
  }
  for ( uint64_t i = is_inserted_.find_first_zero_from( max( first_index, read_end ) ); i < loop_end;
        i = is_inserted_.find_next( i ) ) {//利用动态bitset快速找到第一个没填的位置
    buffer_[i] = data[i - first_index];
    is_inserted_.set( i );
    pending_bytes_++;
  }
  auto pre_read = read_end;
  read_end=is_inserted_.find_first_zero_from(read_end);//利用动态bitset找到连续的1
  string put( read_end - pre_read, '\0' );
  for ( uint64_t i = pre_read; i < read_end; i++ )
    put[i - pre_read] = buffer_[i];
  auto pre_bytes = output_.writer().bytes_pushed();
  output_.writer().push( move( put ) );//move加速
  auto suf_bytes = output_.writer().bytes_pushed();
  pending_bytes_ -= suf_bytes - pre_bytes;//读了多少进去
  if ( eof_len_ != static_cast<uint64_t>( -1 ) && read_end == eof_len_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return pending_bytes_;
  // debug( "unimplemented count_bytes_pending() called" );
}
