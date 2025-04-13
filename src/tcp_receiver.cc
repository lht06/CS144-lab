#include "tcp_receiver.hh"
#include "debug.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>
#include <sys/types.h>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  /*
  当接收到带有 RST（复位）标志的 segment 时，TCP 协议要求直接中止连接，并将连接置为错误状态。
  RST 一般表示对端遇到严重错误，或者明确告知自己放弃当前连接。

  实现目的：
  如果收到 RST，那么不再继续其他数据处理，调用底层 ByteStream（通过 reassembler_.reader()）的 set_error()
  方法，设置错误状态。这样， 后续 send() 会依据错误状态来设置 msg.RST，同时底层流的状态也会反映出出错情况。
  */

  /*
  TCP 连接的建立始于三次握手。第一个 segment 带有 SYN 标志，用于初始化连接并传递初始序列号（ISN）。
  在接收端，如果还没建立连接（即 isn_ 未设置），只接受带 SYN 的 segment。

  实现目的：
  如果当前 TCPReceiver 尚未建立连接（isn_ 没有值），只有收到带 SYN 的 segment 才会设置 isn_ 并将 checkpoint 置为 0。
  如果收到其它非 SYN 报文，则被忽略。这样保证只有合法的建立连接报文能激活接收逻辑。
  */
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( !isn_.has_value() ) {
    if ( !message.SYN ) { // 如果没有 SYN，就忽略该 segment
      return;
    }
    isn_ = message.seqno;
    checkpoint_ = 0;
  }

  /*
  TCP 的序列号是 32 位环绕（Wrap32）。

  当接收到第一个带 SYN 的 segment 时，SYN 消耗一个序号，但此时流中的有效数据起始位置应为 0。
  如果直接用 abs_seq 作为数据位置，则前面有1个字节（SYN占用的序号）并不对应于用户数据。

  对于非 SYN 的 segment，由于第一个字节（SYN）已经被消耗，
  所以收到的绝对序号（abs_seq）应减去 1 才对应于用户数据在流中的起始位置。
  */
  uint64_t abs_seq = message.seqno.unwrap( isn_.value(), checkpoint_ );

  // 若不是 SYN，数据在流中的起始索引应当为 abs_seq - 1
  uint64_t stream_idx = message.SYN ? abs_seq : abs_seq - 1;

  /*
  TCP 可能会乱序到达，需要“重组”这些数据，保证应用层接收到的是连续有序的数据。
  重组器的职责是将不同 segment 的数据按照正确的位置插入，处理乱序与重传问题，并识别 FIN 的到达（结束标志）。
  */
  reassembler_.insert( stream_idx, message.payload, message.FIN );

  /*
  在 TCP 中，接收端需要知道从流起始处到目前为止的连续正确接收的数据长度，从而确定下一个期望的字节序号。
  */
  checkpoint_ = reassembler_.reader().bytes_popped() + reassembler_.reader().bytes_buffered();
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg {};

  if ( isn_.has_value() ) {
    // 连续组装好的数据数 = bytes_popped() + bytes_buffered()
    uint64_t contiguous = reassembler_.reader().bytes_popped() + reassembler_.reader().bytes_buffered();

    // 加上 SYN 消耗的1个序号。
    uint64_t next_expected_byte = contiguous + 1;

    // 若 FIN 到达，再额外加 1
    if ( reassembler_.writer().is_closed() ) {
      next_expected_byte++;
    }

    // 将绝对序号转换为 Wrap32 类型的 ackno（基于初始序号 isn_）
    msg.ackno = Wrap32::wrap( next_expected_byte, isn_.value() );
  }

  // 限制窗口大小不超过 65535
  uint64_t avail = reassembler_.writer().available_capacity();
  msg.window_size = avail > 65535 ? 65535 : static_cast<uint16_t>( avail );

  if ( reassembler_.reader().has_error() ) {
    msg.RST = true;
  } else {
    msg.RST = false;
  }

  return msg;
}
