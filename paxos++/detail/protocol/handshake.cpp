#include <boost/ref.hpp>
#include <boost/bind.hpp>

#include <boost/uuid/uuid_io.hpp>

#include "../util/debug.hpp"
#include "../../quorum.hpp"
#include "../connection_pool.hpp"

#include "command.hpp"
#include "protocol.hpp"
#include "handshake.hpp"

namespace paxos { namespace detail { namespace protocol {

handshake::handshake (
   detail::protocol::protocol & protocol)
   : protocol_ (protocol)
{
}

void
handshake::start ()
{
   step1 ();
}

void
handshake::receive_handshake_start (
   tcp_connection &     connection,
   command const &      command)
{
   step3 (connection,
          command);
}

void
handshake::step1 ()
{
   for (auto const & i : protocol_.quorum ().servers ())
   {
      boost::asio::ip::tcp::endpoint const & endpoint = i.first;

      if (endpoint == protocol_.quorum ().self ().endpoint_)
      {
         PAXOS_DEBUG ("skipping self: " << endpoint);
         continue;
      }

      tcp_connection & new_connection = protocol_.connection_pool ().create ();
      
      new_connection.socket ().async_connect (endpoint,
                                              boost::bind (&handshake::step2,
                                                           this,
                                                           boost::ref (endpoint),
                                                           boost::ref (new_connection),
                                                           boost::asio::placeholders::error));
   }
}

void
handshake::step2 (
   boost::asio::ip::tcp::endpoint const &       endpoint,
   tcp_connection &                             connection,
   boost::system::error_code const &            error)
{

   if (error)
   {
      PAXOS_WARN ("An error occured while establishing a connection, marking host as dead: " << error.message ());
      protocol_.quorum ().lookup (endpoint).set_state (remote_server::state_dead);
      return;
   }

   command command;
   command.set_type (command::type_handshake_start);

   PAXOS_DEBUG ("Connection established!");

   /*!
     Send this command to the other side, which will enter in handshake::step3 as
     defined in protocol.cpp's handle_command () function.
    */
   protocol_.write_command (command,
                            connection);

   //! Timeout of 3 seconds
   connection.start_timeout (boost::posix_time::milliseconds (3000));

   /*!
     And now we expect a response from the other side that we indeed are allowed to
     elect a new leader.
    */
   protocol_.read_command (connection,
                           boost::bind (&handshake::step4,
                                        this,
                                        boost::ref (endpoint),
                                        boost::ref (connection),
                                        _1));
}

void
handshake::step3 (
   tcp_connection &     connection,
   command const &      command)
{
   PAXOS_DEBUG ("received handshake request");

   detail::protocol::command ret;
   ret.set_type (command::type_handshake_response);
   ret.set_host_id (protocol_.quorum ().self ().id_);
   ret.set_host_endpoint (protocol_.quorum ().self ().endpoint_);
   ret.set_host_state (protocol_.quorum ().self ().state_);

   protocol_.write_command (ret,
                            connection);
}


void
handshake::step4 (
   boost::asio::ip::tcp::endpoint const &       endpoint,
   tcp_connection &                             connection,
   command const &                              command)
{
   PAXOS_DEBUG ("step4 received command, "
                "host id = " << command.host_id () << ", "
                "endpoint = " << endpoint << ", "
                "state = " << remote_server::to_string (command.host_state ()));

   /*! 
     This validates that the endpoint this host thinks it is, is the same as the endpoint
     we just connected to.
   */
   PAXOS_ASSERT (command.host_endpoint () == endpoint);

   /*!
     Now, update the host id and the state the host thinks it's in.
   */
   protocol_.quorum ().lookup (endpoint).set_state (command.host_state ());
}

}; }; };