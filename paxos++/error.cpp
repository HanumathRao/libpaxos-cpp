#include "detail/util/debug.hpp"
#include "error.hpp"

namespace paxos {



std::string
to_string (
   enum error_code error_code)
{
   switch (error_code)
   {
         case error_incorrect_proposal:
            return "Incorrect proposal id";
            break;

         case error_inconsistent_response:
            return "Inconsistent response";
            break;
   };

   PAXOS_UNREACHABLE ();

}


};