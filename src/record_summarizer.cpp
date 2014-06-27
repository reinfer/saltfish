#include "record_summarizer.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <condition_variable>
#include <memory>
#include <sstream>


namespace reinferio { namespace saltfish {

namespace {
inline std::string str2hex(const std::string& source_id) {
  constexpr char hex[]{"0123456789abcdef"};
  std::stringstream out;
  for (const unsigned char& c : source_id)  out << hex[c >> 4] << hex[c & 0x0F];
  return out.str();
}
}

void SummarizerMap::push_request(RequestType type, const std::string& msg) {
  CHECK_EQ(type, PUT_RECORDS)
      << "SummarizerMap should only be subscribed to put_records()";
  PutRecordsRequest request;
  CHECK(request.ParseFromString(msg));

  auto summarizer_it = summarizers_.find(request.source_id());
  if (summarizer_it == summarizers_.end()) {
    LOG(INFO) << "Schema not found for id="
              << string_to_hex(request.source_id())
              << "; retreiving it from Riak";
    StandardSummarizer record_summarizer;
    load_summarizer(record_summarizer, request.source_id());
    auto map_pair = summarizers_.emplace(
        request.source_id(), std::move(record_summarizer));
    CHECK(map_pair.second);
    summarizer_it = map_pair.first;
  } else {
    LOG(INFO) << "Source schema id=" << string_to_hex(request.source_id())
              << " already in the map.";
  }
  for (const auto& tag_record : request.records()) {
    summarizer_it->second.push_record(tag_record.record());
  }
  save_summarizer(request.source_id(), summarizer_it->second);
  // LOG(INFO) << "State after req=" << summarizer_it->second.to_json();
}

std::string SummarizerMap::to_json(const std::string& source_id) {
  auto insertion = summarizers_.emplace(source_id, StandardSummarizer{});
  bool missing_summarizer{insertion.second};
  auto& summarizer = insertion.first->second;
  if (missing_summarizer &&
      !load_summarizer(summarizer, source_id)) {
    summarizers_.erase(insertion.first);
    return {};
  } else {
    return summarizer.to_json();
  }
}

bool SummarizerMap::fetch_schema(
    core::Schema& schema, const std::string& source_id) {
  std::mutex fetch_mutex;
  std::condition_variable fetch_cv;
  std::unique_lock<std::mutex> lock(fetch_mutex);
  bool riak_replied{false}, status{false};

  riak_client_.fetch(
      schemas_bucket_, source_id,
      [&] (riak::object obj, std::error_code err) {
        CHECK(obj.valid());
        if (!err) {
          if (obj.exists()) {
            schema.ParseFromString(obj.value());
            status = true;
          } else {
            LOG(WARNING) << "(summarizer) schema missing from riak cache bucket="
                         << schemas_bucket_ << " key(source_id)="
                         << str2hex(source_id);
          }
        } else {
          LOG(WARNING) << "(summarizer) riak error - could not retrieve schema";
        }
        riak_replied = true;
        fetch_cv.notify_one();
      });
  fetch_cv.wait(lock, [&] { return riak_replied; });
  return status;
}

bool SummarizerMap::load_summarizer(
    StandardSummarizer& summarizer, const std::string& source_id) {
  std::mutex fetch_mutex;
  std::condition_variable fetch_cv;
  std::unique_lock<std::mutex> lock{fetch_mutex};
  bool got_summarizer{false}, status{false};

  core::Schema schema;
  riak_client_.fetch(
      summarizers_bucket_, source_id,
      [&] (riak::object obj, std::error_code err) {
        LOG(INFO) << "RIAK GOT BACK" << obj.value().size();
        if (!err) {
          if (obj.exists()) {
            std::stringstream stream{obj.value()};
            cereal::BinaryInputArchive in_binary{stream};
            in_binary(summarizer);
            status = true;
          } else {
            core::Schema schema;
            if (fetch_schema(schema, source_id)) {
              summarizer = StandardSummarizer(schema);
              status = true;
            }
          }
        }
        got_summarizer = true;
        fetch_cv.notify_one();
      });
  fetch_cv.wait(lock, [&] { return got_summarizer; });
  return status;
}

bool SummarizerMap::save_summarizer(
    const std::string& source_id, const StandardSummarizer& summarizer) {
  std::mutex store_mutex;
  std::condition_variable fetch_cv;
  std::unique_lock<std::mutex> lock{store_mutex};
  bool riak_replied{false}, status{false};

  std::stringstream stream;
  cereal::BinaryOutputArchive out_binary{stream};
  out_binary(summarizer);
  riak_client_.store(
      summarizers_bucket_, source_id, stream.str(),
      [&] (std::error_code err) {
        if (err) {
          LOG(WARNING) << "(summarizer) riak error - could not save summarizer";
        }
        riak_replied = true;
        fetch_cv.notify_one();
      });
  fetch_cv.wait(lock, [&] { return riak_replied; });
  return status;
}

}}  // namespace reinferio::saltfish
