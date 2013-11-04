#ifndef REINFERIO_SALTFISH_SERVICE_UTILS_HPP
#define REINFERIO_SALTFISH_SERVICE_UTILS_HPP

#include "source.pb.h"
#include "service.pb.h"
#include "service.rpcz.h"
#include "riak_proxy.hpp"

#include <rpcz/rpcz.hpp>

#include <string>
#include <thread>
#include <mutex>
#include <sstream>
#include <set>
#include <utility>


namespace reinferio {
namespace saltfish {

std::string schema_to_str(const source::Schema& schema);
bool schema_has_duplicates(const source::Schema& schema);
std::pair<bool, string> put_records_check_schema(const source::Schema& schema,
                                                 const PutRecordsRequest& request);

class PutRecordsReplier {
 public:
  PutRecordsReplier(const std::vector<std::string>& record_ids,
                    rpcz::reply<PutRecordsResponse> reply);
  ~PutRecordsReplier();

  void reply(PutRecordsResponse::Status status, const std::string& msg);

 private:
  const std::vector<string> record_ids_;
  const std::size_t n_records_;

  std::uint32_t n_resp_received_;
  std::mutex n_resp_received_mutex_;

  rpcz::reply<PutRecordsResponse> reply_;
  std::mutex reply_mutex_;
  bool already_replied_;
};


}  // namespace saltfish
}  // namespace reinferio

#endif  // REINFERIO_SALTFISH_SERVICE_UTILS_HPP
