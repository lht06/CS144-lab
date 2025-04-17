#include "router.hh"
#include "address.hh"
#include "debug.hh"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  router_table_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );

  // cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
  //      << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
  //      << " on interface " << interface_num << "\n";

  // debug( "unimplemented add_route() called" );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
    for ( const auto& interface : interfaces_ ) {
        auto& queue = interface->datagrams_received();
        while ( !queue.empty() ) {
            auto dgram = std::move( queue.front() );
            queue.pop();

            const uint32_t dst_ip = dgram.header.dst;
            auto best_match = router_table_.end();

            for ( auto it = router_table_.begin(); it != router_table_.end(); ++it ) {
                const auto& entry = *it;
                if ( entry.prefix_length == 0 ) {
                    // 默认路由，仅在无其他匹配时使用
                    if ( best_match == router_table_.end() ) {
                        best_match = it;
                    }
                } else {
                    // 计算掩码（避免UB）
                    const uint32_t mask = static_cast<uint32_t>( 0xFFFFFFFF << ( 32 - entry.prefix_length ) );
                    if ( ( entry.route_prefix & mask ) == ( dst_ip & mask ) ) {
                        if ( best_match == router_table_.end() || entry.prefix_length > best_match->prefix_length ) {
                            best_match = it;
                        }
                    }
                }
            }

            if ( best_match == router_table_.end() || dgram.header.ttl <= 1 ) {
                continue; // 无路由或TTL过期
            }

            dgram.header.ttl--;
            dgram.header.compute_checksum();

            // 发送到下一跳或目标地址
            const auto& target = best_match->next_hop.has_value()
                                    ? best_match->next_hop.value()
                                    : Address::from_ipv4_numeric( dst_ip );
            interfaces_[best_match->interface_num]->send_datagram( dgram, target );
        }
    }
}