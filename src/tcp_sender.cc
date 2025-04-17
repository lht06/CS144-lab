#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( input_.has_error() ) {
    is_rst_ = true;
  }
  uint16_t effictive_window_size = current_window_size_ > 0 ? current_window_size_ : 1;

  if ( !is_syn_ ) {
    TCPSenderMessage msg {};
    msg.RST = is_rst_;
    msg.seqno = msg.seqno.wrap( next_seq_, isn_ );
    msg.SYN = true;
    is_syn_ = true;
    uint64_t unacknowledged_message = next_seq_ - last_ackno_;
    if ( effictive_window_size - unacknowledged_message > 1 ) {
      uint64_t transmit_size = min<uint64_t>( { TCPConfig::MAX_PAYLOAD_SIZE,
                                                effictive_window_size - unacknowledged_message - 1,
                                                input_.reader().bytes_buffered() } );
      if ( transmit_size ) {
        string data = string( input_.reader().peek().substr( 0, transmit_size ) );
        input_.reader().pop( transmit_size );
        msg.payload = move( data );
        next_seq_ += transmit_size;
      }
    }
    sequence_numbers_in_flight_ += msg.payload.size();
    next_seq_++;
    sequence_numbers_in_flight_++;
    if ( input_.reader().is_finished() ) {
      msg.FIN = true;
      is_fin_ = true;
      next_seq_++;
      sequence_numbers_in_flight_++;
    }
    not_ackownledge_.push( { msg, current_time_ } );
    transmit( msg );
  } else if ( input_.reader().is_finished() && effictive_window_size > ( next_seq_ - last_ackno_ ) && !is_fin_ ) {
    TCPSenderMessage msg {};
    msg.RST = is_rst_;
    msg.seqno = msg.seqno.wrap( next_seq_, isn_ );
    is_fin_ = true;
    msg.FIN = true;
    transmit( msg );
    next_seq_++;
    not_ackownledge_.push( { msg, current_time_ } );
    sequence_numbers_in_flight_++;
  }

  while ( true ) {
    if ( input_.reader().bytes_buffered() == 0 ) {
      return;
    }
    TCPSenderMessage msg {};
    uint64_t unacknowledged_message = next_seq_ - last_ackno_;
    uint64_t transmit_size = min<uint64_t>( { TCPConfig::MAX_PAYLOAD_SIZE,
                                              effictive_window_size - unacknowledged_message,
                                              input_.reader().bytes_buffered() } );
    if ( transmit_size ) {
      string data = string( input_.reader().peek().substr( 0, transmit_size ) );
      input_.reader().pop( transmit_size );
      msg.payload = move( data );
    } else {
      return;
    }
    bool is_break {};
    if ( is_rst_ ) {
      msg.RST = true;
      is_break = true;
    }
    msg.seqno = msg.seqno.wrap( next_seq_, isn_ );
    next_seq_ += transmit_size;
    if ( effictive_window_size - unacknowledged_message > transmit_size && input_.reader().is_finished() ) {
      msg.FIN = true;
      is_fin_ = true;
      next_seq_++;
      sequence_numbers_in_flight_++;
    }

    not_ackownledge_.push( { msg, current_time_ } );
    sequence_numbers_in_flight_ += msg.payload.size();
    transmit( move( msg ) );
    if ( is_break ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg {};
  msg.seqno = msg.seqno.wrap( next_seq_, isn_ );
  if ( is_rst_ || input_.has_error() ) {
    msg.RST = true;
  }
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( input_.has_error() ) {
    is_rst_ = true;
  }
  is_rst_ |= msg.RST;
  if ( is_rst_ ) {
    input_.set_error();
  }
  if ( msg.ackno.has_value() && msg.ackno->unwrap( isn_, last_ackno_ ) > next_seq_ ) {
    return;
  }
  current_window_size_ = msg.window_size;
  if ( msg.ackno.has_value() ) {
    last_ackno_ = msg.ackno->unwrap( isn_, last_ackno_ );
    bool acked_new = false;

    while ( not_ackownledge_.size()
            && last_ackno_ >= not_ackownledge_.front().first.seqno.unwrap( isn_, last_ackno_ )
                                + not_ackownledge_.front().first.sequence_length() ) {
      sequence_numbers_in_flight_ -= not_ackownledge_.front().first.sequence_length();
      not_ackownledge_.pop();
      acked_new = true;
    }

    // 如果 ack 了新数据
    if ( acked_new ) {
      current_ROT_ms_ = initial_RTO_ms_;
      current_time_ = 0;
      consecutive_retransmissions_ = 0;

      // 重新设置第一个未 ack 的 segment 的发送时间为 0
      if ( !not_ackownledge_.empty() ) {
        not_ackownledge_.front().second = 0;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( current_ROT_ms_ == 0 ) {
    current_ROT_ms_ = initial_RTO_ms_;
  }
  current_time_ += ms_since_last_tick;
  if ( not_ackownledge_.size() ) {
    auto& [data, transmit_time] = not_ackownledge_.front();
    if ( current_time_ - transmit_time >= current_ROT_ms_ ) {
      transmit_time = current_time_;
      transmit( move( data ) );
      consecutive_retransmissions_++;
      if ( current_window_size_ > 0 ) {
        current_ROT_ms_ *= 2;
      }
    }
  }
}