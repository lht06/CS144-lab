#pragma once

#include "wrapping_integers.hh"

#include <string>

/*
 * The TCPSenderMessage structure contains the information sent from a TCP sender to its receiver.
 *
 * It contains five fields:
 *
 * 1) The sequence number (seqno) of the beginning of the segment. If the SYN flag is set, this is the
 *    sequence number of the SYN flag. Otherwise, it's the sequence number of the beginning of the payload.
 *
 * 2) The SYN flag. If set, this segment is the beginning of the byte stream, and the seqno field
 *    contains the Initial Sequence Number (ISN) -- the zero point.
 *
 * 3) The payload: a substring (possibly empty) of the byte stream.
 *
 * 4) The FIN flag. If set, the payload represents the ending of the byte stream.
 *
 * 5) The RST (reset) flag. If set, the stream has suffered an error and the connection should be aborted.
 */

struct TCPSenderMessage
{
  Wrap32 seqno { 0 }; // 表示该 TCP 段的初始序列号

  bool SYN {}; // 代表 TCP 报文段中的 SYN 标志。SYN（synchronize）用于初始化一个连接（例如在三次握手的第一步中）。
  std::string
    payload {}; // 表示 TCP 段中实际传输的数据内容（负载）。在实际传输过程中，payload 可能包含用户数据或者应用数据。
  bool FIN {}; // 表示 TCP 报文段中的 FIN 标志。FIN（finish）用于表明发送方已经没有数据要发送了，并且希望关闭连接。

  bool RST {}; // 表示 TCP 报文段中的 RST
               // 标志。RST（reset）用于异常中断一个连接，比如在出现错误或非法请求时，可以立即中断连接。

  // How many sequence numbers does this segment use?
  size_t sequence_length() const { return SYN + payload.size() + FIN; }
};
