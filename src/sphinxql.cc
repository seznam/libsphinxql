/**
 * SphinxQL Library.
 * The MIT License (MIT)
 * Copyright (c) 2021 Seznam.cz a.s.
 */

#include "sphinxql.h"
#include <map>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <functional>
#include <string.h>
#include <errmsg.h>

namespace SphinxQL {


// just for multi-threading support
void initThreadSupport() {
    if (mysql_library_init(0, nullptr, nullptr)) {
        throw Exception("Failed to initialize mysql library.");
    }
}


void unload() {
    mysql_library_end();
}


Row::Row(): origin(nullptr), data(nullptr), fields(0), fieldIter(0) {}
Row::~Row() = default;

uint32_t Row::size() const {
    return fields;
}

template <typename T>
T Row::convertResult(const char *data, const T &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    std::stringstream ss(std::string(data));
    T res = T();
    ss >> res;
    return res;
}

template <>
uint32_t Row::convertResult(const char *data, const uint32_t &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    uint32_t val = strtoul(data, &end, 10);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
uint64_t Row::convertResult(const char *data, const uint64_t &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    uint64_t val = strtoull(data, &end, 10);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
int32_t Row::convertResult(const char *data, const int32_t &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    int32_t val = strtol(data, &end, 10);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
int64_t Row::convertResult(const char *data, const int64_t &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    int64_t val = strtoll(data, &end, 10);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
float Row::convertResult(const char *data, const float &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    float val = strtof(data, &end);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
double Row::convertResult(const char *data, const double &defaultValue) {
    if (!data) {
        return defaultValue;
    }

    char *end = 0x0;
    double val = strtod(data, &end);
    if (end != data)
        return val;
    return defaultValue;
}

template <>
std::string Row::convertResult(const char *data, const std::string &defaultValue) {
    if (!data) {
        return defaultValue;
    }
    return std::string(data);
}

void Row::checkBounds(uint32_t index) const {
    if (index >= fields) {
        throw std::out_of_range{"Row column out of range: " + std::to_string(index)};
    }
}

const char *Row::operator[](uint32_t index) const {
    checkBounds(index);
    return data[index];
}


/// Result holder for SHOW META answer.
class MetaReader {
    std::map<std::string, std::string> values;
  public:
    explicit MetaReader(Result &result);

    /**
     * Get value from SHOW META result for variable supplied.
     * @retval Empty string if variable not found.
     */
    std::string getValue(const char *variable) const;
};


MetaReader::MetaReader(Result &result) {
    Row row;
    while (result.getNextRow(row)) {
        std::string key, value;
        row >> key >> value;
        if (!key.empty()) {
            values.emplace(std::move(key), std::move(value));
        }
    }
}


std::string MetaReader::getValue(const char *variable) const {
    std::map<std::string, std::string>::const_iterator it = values.find(variable);
    if (it != values.end()) {
        return it->second;
    }
    return "";
}


/// Build and hold result fields indexes.
class ResultIndex {
    std::map<std::string, unsigned int> index;
  public:
    ResultIndex() = default;
    void initialize(const MYSQL_FIELD *fields, unsigned int fieldCount);

    bool empty() const {
        return index.empty();
    }

    /**
     * Return index for data retrieval of specified field.
     * This index can be used to access data in a Row instance via operator[].
     * @return Index in range <0, row.size()) on success.
     */
    uint32_t getFieldIndex(const std::string &field) const;
};


void ResultIndex::initialize(const MYSQL_FIELD *fields, unsigned int fieldCount) {
    if (fields == nullptr) {
        throw Exception("Cannot initialize result index from null field result");
    }
    for (unsigned int i = 0; i < fieldCount; ++i) {
        index[fields[i].name] = i;
    }
}

uint32_t ResultIndex::getFieldIndex(const std::string &field) const {
    std::map<std::string, unsigned int>::const_iterator pos = index.find(field);
    if (pos == index.end()) {
        throw std::out_of_range{"No such field in result set: " + field};
    }
    return pos->second;
}


class Result::PrivateData {
    /// Result index from fieldName -> query column position.
    ResultIndex columnIndex;
    /// Optional SHOW META results.
    std::unique_ptr<SphinxQL::MetaReader> metaReader;
    /// Result pointer (non owning, currently managed by Result class).
    MYSQL_RES *result;
    /// Result field metadata (non owning pointer).
    MYSQL_FIELD *fieldInfo;
    /// Number of fields (columns) in the result.
    unsigned int fields;
  public:
    PrivateData(): result(nullptr), fieldInfo(nullptr), fields(0) {}
    ~PrivateData() = default;
    friend class Result;
};


Result::Result(MYSQL *con): data(std::make_unique<PrivateData>()) {
    data->result = mysql_store_result(con);
    if (data->result == nullptr) {
        throw Exception(
                std::string("mysql_store_result error: ") + mysql_error(con));
    }
    data->fields = mysql_num_fields(data->result);
    data->fieldInfo = mysql_fetch_fields(data->result);
}

Result::Result(Result &&other): data(std::make_unique<PrivateData>()) {
    data.swap(other.data);
}

Result::~Result() {
    mysql_free_result(data->result);
}

Result &Result::operator=(Result &&other) noexcept {
    if (this != &other) {
        data.swap(other.data);
    }
    return *this;
}

bool Result::getNextRow(Row &row) {
    row.data = mysql_fetch_row(data->result);
    if (row.data) {
        row.origin = this;
        row.fields = data->fields;
    } else {
        row.origin = nullptr;
        row.fields = 0;
    }
    row.fieldIter = 0;
    return row.size() > 0;
}

uint32_t Result::size() const {
    return mysql_num_rows(data->result);
}

uint32_t Result::getColumnIndex(const std::string &fieldName) const {
    if (data->columnIndex.empty()) {
        data->columnIndex.initialize(data->fieldInfo, data->fields);
    }
    return data->columnIndex.getFieldIndex(fieldName);
}

void Result::addMeta(std::unique_ptr<Result> &&meta) {
    if (meta) {
        // TODO get rid of unique_ptr and construct the map from the result directly...
        data->metaReader = std::make_unique<MetaReader>(*meta);
    }
}

std::string Result::getMeta(const char *variable) const {
    if (!data->metaReader) throw Exception("No SHOW META result.");
    return data->metaReader->getValue(variable);
}


namespace {

/// RAII action executor on end of the scope.
class OnScopeEnd {
    /// Function to call (should not throw exceptions).
    std::function<void()> action;
  public:
    explicit OnScopeEnd(std::function<void()> action): action(action) {}
    ~OnScopeEnd() noexcept {
        action();
    }
};

}


/// Private data holder
class Query::PrivateData {
    /// Mysql connection pointer.
    MYSQL *con;
    /// Flag to note whether a connection was already established.
    bool connected;
    /// Flag whether first query (of possible multiple queries) need to be fetched.
    bool firstToRetrieve;
    /// Queries to be scheduled (query, meta flag).
    std::vector<std::pair<std::string, bool>> queries;

    /**
     * Get concatenated query string of all scheduled queries
     * and its optional SHOW META results.
     */
    std::string getQueryString();
  public:
    PrivateData(): con(mysql_init(nullptr)), connected(false), firstToRetrieve(true) {
        if (con == nullptr) {
            throw Exception(mysql_error(con));
        }
    }
    ~PrivateData() {
        mysql_close(con);
    }

    void setConfigOptions(const Query::Config &cfg);

    /// Connect to the host:port or treat host as unix socket if port is zero.
    void connect(const char *host, int port = 0);

    bool isConnected() const {
        return connected;
    }

    void clear() {
        firstToRetrieve = true;
        queries.clear();
    }

    bool empty() const {
        return queries.empty();
    }

    void addQuery(std::string &&query, bool addMeta) {
        queries.emplace_back(std::move(query), addMeta);
    }

    const std::vector<std::pair<std::string, bool>> &getQueries() const {
        return queries;
    }

    /// Run queries in synchronous mode waiting for them to finish.
    void execute();

    /// Send queries in async mode.
    void asyncExecute();
    /// Wait for async executed queries to finish.
    void waitForAsyncResult();

    /**
     * Return results in order queries were registered.
     * Response::fill() function will join result and its SHOW META result together.
     */
    std::unique_ptr<Result> nextResult();
};

void Query::PrivateData::setConfigOptions(const Query::Config &cfg) {
    if (cfg.optProtocol) {
        mysql_options(con, MYSQL_OPT_PROTOCOL, &(cfg.optProtocol));
    }
    unsigned int value = cfg.connectTimeout.count();
    mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &value);
    value = cfg.writeTimeout.count();
    mysql_options(con, MYSQL_OPT_WRITE_TIMEOUT, &value);
    value = cfg.readTimeout.count();
    mysql_options(con, MYSQL_OPT_READ_TIMEOUT, &value);
}

void Query::PrivateData::connect(const char *host, int port) {
    MYSQL *status;
    if (port > 0) {
        status = mysql_real_connect(
                con, host, NULL, NULL, NULL, port, "/var/run/sphinx.s", 0);
    } else {
        // host must contains unixsocket spec
        status = mysql_real_connect(con, NULL, NULL, NULL, NULL, 0, host, 0);
    }
    if (status == nullptr) {
        throw Exception(mysql_error(con));
    }
    connected = true;
}

std::string Query::PrivateData::getQueryString() {
    std::ostringstream query;
    for (const std::pair<std::string, bool> &queryInfo: queries) {
        query << queryInfo.first;
        // append meta query if requested
        if (queryInfo.second) {
            query << "SHOW META; ";
        }
    }
    return query.str();
}

void Query::PrivateData::execute() {
    const std::string query = getQueryString();
    const int status = mysql_query(con, query.c_str());
    if (status != 0) {
        if (status == CR_SERVER_GONE_ERROR || status == CR_SERVER_LOST) {
            throw TimeoutException(mysql_error(con));
        }
        throw Exception(mysql_error(con));
    }
}

void Query::PrivateData::asyncExecute() {
    firstToRetrieve = true;
    const std::string query = getQueryString();
    if (mysql_send_query(con, query.c_str(), query.size())) {
        throw Exception("mysql_send_query failed: " + std::string(mysql_error(con)));
    }
}

void Query::PrivateData::waitForAsyncResult() {
    if (mysql_read_query_result(con) != 0) {
        throw Exception("mysql_read_query_result failed: " + std::string(mysql_error(con)));
    }
}

std::unique_ptr<Result> Query::PrivateData::nextResult() {
    if (firstToRetrieve) {
        firstToRetrieve = false;
        return std::make_unique<Result>(con);
    }
    if (mysql_more_results(con) && mysql_next_result(con) == 0) {
        return std::make_unique<Result>(con);
    }
    throw Exception{"No result returned"};
}


Query::Config::Config(std::chrono::seconds ct, std::chrono::seconds wt, std::chrono::seconds rt)
    : connectTimeout(ct), writeTimeout(wt), readTimeout(rt),
      optProtocol(0)
{}


Query::Query(): data(std::make_unique<PrivateData>()) {}


Query::Query(const Query::Config &cfg): data(std::make_unique<PrivateData>()) {
    data->setConfigOptions(cfg);
}


Query::~Query() = default;


Query::Query(Query &&other): data(std::make_unique<PrivateData>()) {
    data.swap(other.data);
}

Query &Query::operator=(Query &&other) noexcept {
    if (this != &other) {
        data.swap(other.data);
    }
    return *this;
}


void Query::connect(const char *host, int port) {
    data->connect(host, port);
}

void Query::connect(const char *unixsocket) {
    data->connect(unixsocket, 0);
}

bool Query::isConnected() const {
    return data->isConnected();
}

SphinxQL::Response Query::execute() {
    if (!data->isConnected()) {
        throw Exception("No connection established!");
    }

    OnScopeEnd finalClear([&]{
        data->clear();
    });

    data->execute();
    return SphinxQL::Response(*this);
}

void Query::asyncExecute() {
    data->asyncExecute();
}

void Query::waitForAsyncResult() {
    data->waitForAsyncResult();
}


void Query::addQuery(const char *query, bool meta) {
    data->addQuery(query, meta);
}


void Query::clear() {
    data->clear();
}

bool Query::empty() const {
    return data->empty();
}


/// Private data for AsyncQuery object.
class AsyncQuery::PrivateData {
    /// Host/unix socket to connect to.
    std::string host;
    /// Port number (zero if host has to be treated as unix socket).
    int port;
    /// Additional cfg (optional), since c++17 could be std::optional...
    std::unique_ptr<Query::Config> cfg;
  public:
    PrivateData(const char *h, int p): host(h), port(p) {}
    PrivateData(const char *h, int p, const Query::Config &cfg)
        : host(h), port(p), cfg(std::make_unique<Query::Config>(cfg))
    {}
    PrivateData(const PrivateData &other)
        : host(other.host), port(other.port)
    {
        if (other.cfg) {
            cfg = std::make_unique<Query::Config>(*other.cfg);
        }
    }

    friend class AsyncQuery;
};


AsyncQuery::AsyncQuery(const char *host, int port)
    : data(std::make_unique<PrivateData>(host, port))
{}


AsyncQuery::AsyncQuery(const char *host, int port, const Query::Config &cfg)
    : data(std::make_unique<PrivateData>(host, port, cfg))
{}


AsyncQuery::~AsyncQuery() = default;


AsyncQuery::AsyncQuery(AsyncQuery &&other)
  : connections(), queries(std::move(other.queries)),
    data(std::make_unique<PrivateData>(*other.data))
{
    connections.swap(other.connections);
}


AsyncQuery &AsyncQuery::operator=(AsyncQuery &&other) {
    if (this != &other) {
        queries = std::move(other.queries);
        connections.swap(other.connections);
        data.swap(other.data);
    }
    return *this;
}


void AsyncQuery::clear() {
    OnScopeEnd finalClear([&]{
        queries.clear();
    });
    for (QueryPtr &query: queries) {
        query->clear();
        connections.push(std::move(query));
    }
}


void AsyncQuery::add(const char *query, bool meta) {
    QueryPtr worker;
    if (!connections.empty()) {
        worker = std::move(connections.front());
        connections.pop();
    } else {
        if (data->cfg) {
            worker = std::make_unique<Query>(*(data->cfg));
        } else {
            worker = std::make_unique<Query>();
        }

        if (data->port) {
            worker->connect(data->host.c_str(), data->port);
        } else {
            worker->connect(data->host.c_str());
        }
    }
    worker->addQuery(query, meta);
    queries.push_back(std::move(worker));
}


void AsyncQuery::add(Query &&query) {
    if (!query.isConnected()) {
        if (data->port) {
            query.connect(data->host.c_str(), data->port);
        } else {
            query.connect(data->host.c_str());
        }
    }
    if (query.empty()) {
        connections.push(std::make_unique<Query>(std::move(query)));
    } else {
        queries.push_back(std::make_unique<Query>(std::move(query)));
    }
}


SphinxQL::Response AsyncQuery::launch() {
    // discard any unnecessary connections
    while (!connections.empty()) {
        connections.pop();
    }

    try {
        // send query requests
        for (const QueryPtr &query: queries) {
            query->asyncExecute();
        }

        // wait for them
        for (const QueryPtr &query: queries) {
            query->waitForAsyncResult();
        }
    } catch (const std::exception &) {
        queries.clear();
        throw;
    }

    // run after response has been read
    OnScopeEnd finalCleanup([&] {
        try {
            // move queries to connections queue
            for (QueryPtr &query: queries) {
                query->clear();
                connections.push(std::move(query));
            }
            queries.clear();
        } catch (const std::exception &) {
            // just discard connections if exc is thrown
            queries.clear();
        }
    });
    return SphinxQL::Response(*this);
}


Response::Response(Query &query) {
    fill(query);
}

Response::Response(AsyncQuery &queries) {
    for (const AsyncQuery::QueryPtr &query: queries.queries) {
        fill(*query);
    }
}

void Response::fill(Query &query) {
    for (auto &queryInfo: query.data->getQueries()) {
        std::unique_ptr<Result> res = query.data->nextResult();
        if (queryInfo.second) {
            res->addMeta(query.data->nextResult());
        }
        results.push(std::move(res));
    }
}

std::unique_ptr<Result> Response::next() {
    std::unique_ptr<Result> res;
    if (!results.empty()) {
        res = std::move(results.front());
        results.pop();
    }
    return res;
}


} //namespace SphinxQL
