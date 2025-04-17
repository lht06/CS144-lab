#include <cstdint>
#include <iostream>
#include <type_traits>

#include "address.hh"
#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const uint32_t ip = next_hop.ipv4_numeric();
  if ( auto iter = ip_to_address_.find( ip ); iter == ip_to_address_.end() ) {
    waiting_datagrams_[ip].push_back( { current_time_, dgram } );
    if ( recently_query_.contains( ip ) ) {
      return;
    }
    ARPMessage arp = { .opcode = ARPMessage::OPCODE_REQUEST,
                       .sender_ethernet_address = this->ethernet_address_,
                       .sender_ip_address = this->ip_address_.ipv4_numeric(),
                       .target_ethernet_address {},
                       .target_ip_address = ip };
    Serializer S;
    arp.serialize( S );
    EthernetHeader fream_header
      = { .dst = ETHERNET_BROADCAST, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP };
    EthernetFrame frame = { .header = fream_header, .payload = S.finish() };
    transmit( frame );
    recently_query_.insert( ip );
    recently_query_queue_.push( { current_time_, ip } );
  } else {
    Serializer S;
    dgram.serialize( S );
    auto address = iter->second;
    EthernetHeader frame_header
      = { .dst = address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
    EthernetFrame frame = { .header = frame_header, .payload = S.finish() };
    transmit( frame );
  }

  // debug( "unimplemented send_datagram called" );
  // (void)dgram;
  // (void)next_hop;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != this->ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    Parser P( frame.payload );
    InternetDatagram dgram;
    dgram.parse( P );
    if ( !P.has_error() ) {
      datagrams_received_.push( dgram );
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser P( frame.payload );
    ARPMessage arp;
    arp.parse( P );
    if ( P.has_error() ) {
      return;
    }

    ip_to_address_[arp.sender_ip_address] = arp.sender_ethernet_address;
    ip_to_address_query_.push( { current_time_, arp.sender_ip_address } );
    ip_to_address_cnt[arp.sender_ip_address]++;

    if ( arp.opcode == ARPMessage::OPCODE_REPLY ) {
      if ( waiting_datagrams_.count( arp.sender_ip_address ) ) {
        for ( auto [add_time, dgram] : waiting_datagrams_[arp.sender_ip_address] ) {
          if ( current_time_ - add_time > 5000 )
            continue;
          Serializer S;
          dgram.serialize( S );
          EthernetHeader frame_header = {
            .dst = arp.sender_ethernet_address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
          EthernetFrame send_frame = { .header = frame_header, .payload = S.finish() };
          transmit( send_frame );
        }
        waiting_datagrams_.erase( arp.sender_ip_address );
      }
    } else if ( arp.opcode == ARPMessage::OPCODE_REQUEST ) {
      if ( arp.target_ip_address != this->ip_address_.ipv4_numeric() ) {
        return;
      }

      ARPMessage reply = { .opcode = ARPMessage::OPCODE_REPLY,
                           .sender_ethernet_address = this->ethernet_address_,
                           .sender_ip_address = this->ip_address_.ipv4_numeric(),
                           .target_ethernet_address = arp.sender_ethernet_address,
                           .target_ip_address = arp.sender_ip_address };

      Serializer S;
      reply.serialize( S );
      EthernetHeader frame_header
        = { .dst = arp.sender_ethernet_address, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP };
      EthernetFrame reply_frame = { .header = frame_header, .payload = S.finish() };
      transmit( reply_frame );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  current_time_ += ms_since_last_tick;
  while ( ip_to_address_query_.size() ) {
    auto [set_time, ip32] = ip_to_address_query_.front();
    if ( current_time_ - set_time > 30000 ) {
      ip_to_address_query_.pop();
      ip_to_address_cnt[ip32]--;
      if ( ip_to_address_cnt[ip32] == 0 ) {
        ip_to_address_.erase( ip_to_address_.find( ip32 ) );
      }
    } else {
      break;
    }
  }

  while ( recently_query_queue_.size() ) {
    auto [set_time, ip32] = recently_query_queue_.front();
    if ( current_time_ - set_time > 5000 ) {
      recently_query_.erase( recently_query_.find( ip32 ) );
      recently_query_queue_.pop();
    } else {
      break;
    }
  }
  // debug( "unimplemented tick({}) called", ms_since_last_tick );
}
