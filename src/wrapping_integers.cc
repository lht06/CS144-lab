#include "wrapping_integers.hh"
#include "debug.hh"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // zero_point+ans-k*2^32=raw_value_
  // ans=raw_value-zero_point+k*2^32
  // 注意到模2^32意义下的值保持不变
  constexpr uint64_t mod = static_cast<uint64_t>( 1 ) << 32;
  uint64_t temp = static_cast<uint64_t>( raw_value_ ) - static_cast<uint64_t>( zero_point.raw_value_ );
  uint64_t rest1 = temp % mod;
  uint64_t rest2 = checkpoint % mod;
  uint64_t ans1 = checkpoint - ( rest2 - rest1 );
  uint64_t ans2 = ans1 + mod;
  uint64_t ans3 = ans1 - mod;
  auto distance = [&]( uint64_t a, uint64_t b ) {
    if ( a < b ) {
      return b - a;
    } else {
      return a - b;
    }
  };
  vector<array<uint64_t, 2>> candidate { { distance( ans1, checkpoint ), ans1 },
                                         { distance( ans2, checkpoint ), ans2 },
                                         { distance( ans3, checkpoint ), ans3 } };
  ranges::sort( candidate );
  return candidate[0][1];
}