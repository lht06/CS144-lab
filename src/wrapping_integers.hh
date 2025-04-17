#pragma once

#include <cstdint>

/*
 * The Wrap32 type represents a 32-bit unsigned integer that:
 *    - starts at an arbitrary "zero point" (initial value), and
 *    - wraps back to zero when it reaches 2^32 - 1.
 */

/*
Wrap32 类型表示的是一个 32 位无符号整数，它有以下两个特点：
任意初始值：它的“零点”不是固定的
0，而是可以任意定义的一个初始值。这意味着计数器可以从任意设定的值开始，而不是从传统意义上的 0 开始。
回绕行为：当该整数的值达到最大值 2^32 - 1
后，不会发生溢出（即转为负数或其他错误），而是自动回绕到最初设定的零点，然后继续递增。这种行为类似于循环计数器，保证在达到上限后能够顺利返回起始状态。
*/

class Wrap32
{
public:
  explicit Wrap32( uint32_t raw_value ) : raw_value_( raw_value ) {}

  /* Construct a Wrap32 given an absolute sequence number n and the zero point. */
  static Wrap32 wrap( uint64_t n, Wrap32 zero_point );

  /*
   * The unwrap method returns an absolute sequence number that wraps to this Wrap32, given the zero point
   * and a "checkpoint": another absolute sequence number near the desired answer.
   *
   * There are many possible absolute sequence numbers that all wrap to the same Wrap32.
   * The unwrap method should return the one that is closest to the checkpoint.
   */
  // unwrap 的主要任务就是找出最接近 checkpoint 的那一个绝对序列号，使得当使用相应的 wrap 方法转换后，能够得到当前的
  // Wrap32 值。
  uint64_t unwrap( Wrap32 zero_point, uint64_t checkpoint ) const;

  // 该运算符重载允许你将一个 32 位无符号整数 n 与一个 Wrap32 对象相加。具体来说，它将当前对象内部存储的原始值
  // raw_value_ 与 n 相加，并构造一个新的 Wrap32 对象返回。这样做的目的是实现 Wrap32 值的递增操作
  Wrap32 operator+( uint32_t n ) const { return Wrap32 { raw_value_ + n }; }

  // 运算符重载用于比较两个 Wrap32 对象是否相等。它通过比较每个对象内部的 raw_value_
  //  是否相同来判断两个对象是否代表相同的 32 位数值。
  bool operator==( const Wrap32& other ) const { return raw_value_ == other.raw_value_; }

protected:
  uint32_t raw_value_ {};
};