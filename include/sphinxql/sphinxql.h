/**
 * SphinxQL connection library.
 * The MIT License (MIT)
 * Copyright (c) 2021 Seznam.cz a.s.
 */

#pragma once

#include <iterator>
#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <stdexcept>
#include <inttypes.h>
#include <mysql.h>

namespace SphinxQL {

/// Exception thrown by SphinxQL classes.
class Exception: public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};


class TimeoutException: public Exception {
  public:
    using Exception::Exception;
};


/**
 * Init threading support. Call before thread creation.
 * @throw Exception on error.
 * @note Not necessary in single-thread application.
 */
void initThreadSupport();


/**
 * Clear resources used by mysql library.
 * Useful for memory checking (valgrind)..
 */
void unload();


class Row;
class Response;
class Query;
class AsyncQuery;


/// Result set of one sphinxQL query.
class Result {
    class PrivateData;
    /// Private data (hidden for ABI compatibility)
    std::unique_ptr<PrivateData> data;
  public:
    explicit Result(MYSQL *con);
    ~Result();

    Result(const Result &) = delete;
    Result(Result &&);
    Result &operator=(const Result &) = delete;
    Result &operator=(Result &&) noexcept;

    /// Return number of rows in result set.
    uint32_t size() const;

    /// Return index for the given field name.
    uint32_t getColumnIndex(const std::string &fieldName) const;

    /// Fill in next row data. Return true if row is valid to read.
    bool getNextRow(Row &row);

    /**
     * Get value from SHOW META result for variable supplied.
     * @retval Empty string if variable was not found.
     * @throw Exception if show meta result is not present.
     */
    std::string getMeta(const char *variable) const;
  private:
    /// Add SHOW META result to query result.
    void addMeta(std::unique_ptr<Result> &&metaResult);

    friend class SphinxQL::Response;
};


/**
 * Row of SphinxQL result. Does not own any data, valid until origin Result is alive.
 *
 * (Note there is no private data pointer so no allocations happen; possible future
 * changes will break ABI).
 */
class Row {
    /// Pointer to result class (to access column index).
    const Result *origin;
    /// Pointer to result data (not owned, validity driven by Result lifetime).
    const char *const *data;
    /// Number of fields in given result row.
    uint32_t fields;
    /// Current field index to read (for operator>>() fillers).
    uint32_t fieldIter;
  public:
    Row();
    ~Row();

    Row(const Row&) = delete;
    Row(Row&&) = default;
    Row &operator=(const Row&) = delete;
    Row &operator=(Row&&) = default;

    /// Return number of columns. Zero if not initialized.
    uint32_t size() const;

    /**
     * Convert result to requested data type T.
     * @param data  Data to be converted.
     * @param defaultValue  Value returned either when data is null or no conversion happen.
     */
    template <typename T>
    static T convertResult(const char *data, const T &defaultValue = T{});

    /**
     * Access raw row data at index <0, size()).
     * If index is greater than row size the std::out_of_range exception is thrown.
     * @retval nullptr if query result for given index is NULL value.
     */
    const char *operator[](unsigned int index) const;

    /**
     * Fill value and increment column index to fill in next call.
     * It keeps original value's value if current column result is NULL.
     */
    template <typename T>
    Row &operator>>(T &value) {
        checkBounds(fieldIter);
        value = Row::convertResult(data[fieldIter++], value);
        return *this;
    }

    /**
     * Return value of column with field name.
     * It creates field index in the origin Result if index is not yet created.
     * @param field  Name of column.
     * @throw std::out_of_range if no such field exists in result set.
     */
    template <typename T>
    inline T getValue(const std::string &field) const {
        if (!origin) {
            throw Exception("Row is probably not initialized!");
        }
        const uint32_t idx = origin->getColumnIndex(field);
        return Row::convertResult<T>((*this)[idx]);
    }
  private:
    /// Throws std::out_of_range if index is out of row size.
    void checkBounds(uint32_t index) const;

    // allow Result to call fill()
    friend class SphinxQL::Result;
};


/**
 * SphinxQL query class.
 * Multiple queries could be optimised by Sphinx to run in multiquery mode,
 * but if optimization fails, queries are evaluated sequentially.
 */
class Query {
    class PrivateData;
    /// D-pointer for ABI compatibility
    std::unique_ptr<PrivateData> data;
  private:
    /// Run queries by AsyncQuery class in asynchronous mode.
    void asyncExecute();
    void waitForAsyncResult();

  public:
    struct Config {
        unsigned int connectTimeout;    ///< connection timeout (sec)
        unsigned int writeTimeout;      ///< write timeout (sec)
        unsigned int readTimeout;       ///< read timeout (sec)
        unsigned int optProtocol;       ///< mysql_protocol_type from mysql.h

        Config();
        Config(unsigned ct, unsigned wt = 3, unsigned rt = 3);
    };

    /**
     * Initialize Query object with default configuration options.
     */
    Query();
    /**
     * Initialize Query object with supplied configuration.
     */
    Query(const Config &cfg);
    ~Query();

    Query(const Query &) = delete;
    Query(Query &&);
    Query &operator=(const Query &) = delete;
    Query &operator=(Query &&) noexcept;

    /**
     * Connect to Sphinx at host:port.
     * @return True if connecting succeeded.
     */
    void connect(const char *host, int port);
    /**
     * Connect to Sphinx using unix domain socket.
     */
    void connect(const char *unixsocket);

    /**
     * Return true if query object is already connected.
     */
    bool isConnected() const;

    /**
     * Add query to process.
     * Multiple queries could be optimized by Sphinx to run as multi-query.
     * @param query  Query to sphinx. Semicolon termination required.
     * @param meta   Flag if query meta information is requested for query.
     *               If so the SHOW META query result is added to the result
     *               and each returned variable can be read by Result::getMeta().
     */
    void addQuery(const char *query, bool meta = true);

    /// Execute query and get Response. Blocking function call.
    SphinxQL::Response execute();

    /**
     * Clear previously added queries to allow reusing of the connection.
     * Instance is automatically cleared on execute() call on already connected object.
     */
    void clear();

    /**
     * Return true if query object has no queries scheduled.
     */
    bool empty() const;

    /// Allow calls of private methods..
    friend class SphinxQL::AsyncQuery;
    friend class SphinxQL::Response;
};


/**
 * Run queries supplied in parallel by non blocking function calls.
 */
class AsyncQuery {
    using QueryPtr = std::unique_ptr<Query>;

    /// Prepared connections to be reused.
    std::queue<QueryPtr> connections;
    /// Queries to run.
    std::vector<QueryPtr> queries;

    class PrivateData;
    /// D-pointer for ABI compatibility
    std::unique_ptr<PrivateData> data;
  public:
    AsyncQuery(const char *host, int port);
    AsyncQuery(const char *host, int port, const Query::Config &cfg);
    ~AsyncQuery();

    AsyncQuery(const AsyncQuery &) = delete;
    AsyncQuery(AsyncQuery &&);
    AsyncQuery &operator=(const AsyncQuery &) = delete;
    AsyncQuery &operator=(AsyncQuery &&);

    /**
     * Add a query prepared to async launch.
     * @param query  Query to sphinx. Semicolon termination required.
     * @param meta   Flag if query meta information is requested for query.
     */
    void add(const char *query, bool meta = true);
    inline void add(const std::string &query, bool meta = true) { add(query.c_str(), meta); }

    /**
     * Add a query object. It could contain scheduled queries inside, or it is just added
     * into available connections otherwise.
     * While it could be already connected to some sphinx search daemon, beware mixing
     * different sphinx connections inside single AsyncQuery instance. This can easily
     * lead to serious bugs.
     */
    void add(Query &&);

    /// Launch queries asynchronously and get Response.
    SphinxQL::Response launch();

    friend class SphinxQL::Response;
};


/**
 * Response received either from Query::execute() or from AsyncQuery::launch().
 * Individual responses are returned in order of query scheduling.
 */
class Response {
    /// Results from executed queries.
    std::queue<std::unique_ptr<Result>> results;

    // Private constructors from friend classes Query and AsyncQuery.
    Response(Query &query);
    Response(AsyncQuery &queries);

    /// Fill query results into results queue.
    void fill(Query &query);
  public:
    ~Response() = default;

    /**
     * Get next available result.
     * Return {nullptr} if no further results are present in queue.
     */
    std::unique_ptr<Result> next();

    Response(Response &&) = default;
    Response &operator=(Response &&) = default;

    friend class SphinxQL::Query;
    friend class SphinxQL::AsyncQuery;
};

}
