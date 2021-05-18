libsphinxql
===========

C++ SphinxQL client to Sphinxsearch.

# API

## Query

To use simple query, `SphinxQL::Query` can be used. If multiple qieries
are supplied, Sphinx could optimize these queries and use multiquery mode.
If that optimization did not happen, queries then runs sequentially.

The `addQuery(query, showMeta)` member function can be used to add single query,
the semicolon termination character is required. The `bool showMeta` argument
can be used to specify whether a `SHOW META` result has to be added to the
current query. It defaults to true.
Queries can be executed by calling `execute()` member function.

```c++
#include "sphinxql.h"

int main() {
    SphinxQL::Query query;
    query.addQuery("SELECT id, some_field FROM index;", false);
    query.addQuery("SELECT id, MAX(some) as some_max FROM index GROUP BY id;");
    query.connect("localhost", 9306);
    SphinxQL::Response result = query.execute();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    if (res) {
        SphinxQL::Row row;
        while (res->getNextRow(row)) {
            // field names can be used to value retrieval
            const int idField = row.getValue<uint32_t>("id");
            const std::string someField = row.getValue<std::string>("some_max");
        }
    }
    // read the second query results
    res = result.next();
    if (res) {
        SphinxQL::Row row;
        while (res->getNextRow(row)) {
            // or stream or the field index in query can be used to value retrieval
            int idField = 0, someMax = 0;
            row >> idField >> someMax;

            // or raw data can be accessed by operator[], to get id field:
            // const char *column = row[0];
        }
    }
}
```

## AsyncQuery

To run multiple queries that won't be optimized to run in mutliquery mode,
class `SphinxQL::AsyncQuery` could be used to execute multiple queries in parallel.

A single query can be added by `add()` member function. Each call to that function
creates a sphinx connection. It accepts `SphinxQL::Query` class instance described
above, so even multiple different multiquery-optimized queries can be run in parallel.
When `add()` is called with string based query, it accepts `bool showMeta` argument
same as the `SphinxQL::Query` does.

```c++
#include "sphinxql.h"

int main() {
    SphinxQL::AsyncQuery query("localhost", 9306);
    query.add("SELECT id, some_field FROM index;");
    query.add("SELECT id, MAX(some) as some_max FROM index GROUP BY id;");
    SphinxQL::Response result = query.launch();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    if (res) {
        SphinxQL::Row row;
        while (res->getNextRow(row)) {
            // field names can be used to value retrieval
            const int idField = row.getValue<uint32_t>("id");
            const std::string someField = row.getValue<std::string>("some_max");
        }
    }
    // and read second query result (same as example above)...
}
```

## Result

`SphinxQL::Response::next()` can be used to get next query result. Results are returned
in same order as they were added to `SphinxQL::Query` and `SphinxQL::AsyncQuery`.
Each query result is contained in `SphinxQL::Result`. It can be used to get `SphinxQL::Row`
containing next result row, or to retrieve `SHOW META` result (if meta was not disabled
for given query). Meta can contain various variables such as query time and so on.

`SphinxQL::Row` is thin wrapper over in-memory stored results. Row data can be retrieved
by several ways:
  - `getValue<T>(fieldName)` to get value using field name.
  - `operator[fieldIndex]` to get raw value using field query index (0 to size()-1).
  - `operator>>()` to fetch next field into variable (not changing its value if field value is NULL).

# Build

C++14 compiler is required. Build can be made using cmake.

Build dependencies: libmysqlclient-dev | libmariadbclient-dev.

```sh
mkdir build
cmake ..
make
```