/**
 * Readme examples buildability check.
 */

#include <iostream>
#include "sphinxql.h"


void simpleQueryExample() {
    SphinxQL::Query query;
    query.addQuery("SELECT id, some_field FROM index;");
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
            std::cout << "Query1 row: " << idField << ", " << someField << "\n";
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
            std::cout << "Query2 row: " << idField << ", " << someMax << "\n";
        }
    }
}


void asyncQueryExample() {
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
            std::cout << "Query1 row: " << idField << ", " << someField << "\n";
        }
    }
    // and read second query results
    res = result.next();
    if (res) {
        SphinxQL::Row row;
        while (res->getNextRow(row)) {
            // or raw data can be accessed by operator[], to get id field:
            const char *idField = row[0];
            const char *someMax = row[1];
            std::cout << "Query2 row: " << idField << ", " << someMax << "\n";
        }
    }
}


int main() {
    simpleQueryExample();
    asyncQueryExample();
}
