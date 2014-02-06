#ifndef REINFERIO_SALTFISH_SQL_POOL_HPP
#define REINFERIO_SALTFISH_SQL_POOL_HPP

#include "tasklet.hpp"

#include <cppconn/connection.h>
#include <boost/optional.hpp>
#include <boost/thread/tss.hpp>
#include <zmq.hpp>

#include <memory>
#include <mutex>
#include <string>


namespace sql {
class Driver;
class Connection;
}

namespace reinferio { namespace saltfish { namespace store {

class ConnectionFactory {
 public:
  ConnectionFactory(const std::string& host, const std::string& user,
                    const std::string& pass, const std::string& db);

  std::unique_ptr< ::sql::Connection > new_connection();
 private:
  std::mutex driver_mutex_;
  ::sql::Driver* driver_;
  const std::string host_;
  const std::string user_;
  const std::string pass_;
  const std::string db_;
};

std::unique_ptr<sql::Connection> connect_to_sql(
    const std::string& host, const std::string& user,
    const std::string& pass, const std::string& db);

class SourceMetadataStore {
 public:
  virtual void connect() {}
  virtual void close() {}

  virtual boost::optional<std::string> fetch_schema(
      const std::string& source_id) = 0;

  virtual int create_source(
      const std::string& source_id, int user_id,
      const std::string& schema, const std::string& name) = 0;

  virtual int delete_source(const std::string& source_id) = 0;
};

class SourceMetadataSqlStore : SourceMetadataStore {
 public:
  SourceMetadataSqlStore (const std::string& host, const std::string& user,
                         const std::string& pass, const std::string& db,
                         bool thread_init_end = true);
  virtual void connect();
  virtual void close();

  virtual boost::optional<std::string> fetch_schema(
      const std::string& source_id) override;
  virtual int create_source(
      const std::string& source_id, int user_id,
      const std::string& schema, const std::string& name) override;
  virtual int delete_source(const std::string& source_id) override;
 private:
  const std::string host_;
  const std::string user_;
  const std::string pass_;
  const std::string db_;

  bool thread_init_end_;
  bool connected_;
  sql::Driver* driver_;
  std::unique_ptr<sql::Connection> conn_;
};

class SourceMetadataSqlStoreTasklet {
 public:
  SourceMetadataSqlStoreTasklet(zmq::context_t& context,
      const std::string& host, const std::string& user,
      const std::string& pass, const std::string& db);

  boost::optional<std::string> fetch_schema(const std::string& source_id);
  int create_source(const std::string& source_id, int user_id,
                           const std::string& schema, const std::string& name);
  int delete_source(const std::string& source_id);

  using fetch_schema_type =
      std::function<boost::optional<std::string>(const std::string&)>;
  using create_source_type = std::function<int(
      const std::string&, int, const std::string&, const std::string&)>;
  using delete_source_type = std::function<int(const std::string&)>;
 private:
  template<typename T>
  using ts_ptr = boost::thread_specific_ptr<T>;

  SourceMetadataSqlStore store_;
  lib::Tasklet tasklet_;
  ts_ptr<lib::Connection<fetch_schema_type>> fetch_schema_;
  ts_ptr<lib::Connection<create_source_type>> create_source_;
  ts_ptr<lib::Connection<delete_source_type>> delete_source_;
};

}}}  // namespace reinferio::saltfish::store

#endif  // REINFERIO_SALTFISH_SQL_POOL_HPP
