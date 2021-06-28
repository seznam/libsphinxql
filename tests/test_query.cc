#include <gtest/gtest.h>

#include "sphinxql.h"

TEST(Query, meta) {
    SphinxQL::Query query;
    query.addQuery("SELECT id FROM idx_test;");
    query.addQuery("SELECT id FROM idx_test;", false);
    query.connect("/tmp/test-sphinxql.s");
    SphinxQL::Response result = query.execute();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    ASSERT_TRUE(res);

    // get show meta results
    ASSERT_EQ("", res->getMeta("unknown meta field"));
    ASSERT_EQ("2", res->getMeta("total"));
    ASSERT_EQ("2", res->getMeta("total_found"));
    ASSERT_EQ("0.000", res->getMeta("time"));

    // read second query result (without meta results)
    res = result.next();
    ASSERT_TRUE(res);
    ASSERT_THROW({res->getMeta("time");}, SphinxQL::Exception);
}

TEST(Query, query) {
    SphinxQL::Query query;
    query.addQuery("SELECT * FROM idx_test ORDER BY id ASC LIMIT 10;");
    query.addQuery("SELECT * FROM idx_test WHERE multi_data = 1607798880006;", false);
    query.connect("/tmp/test-sphinxql.s");
    SphinxQL::Response result = query.execute();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    ASSERT_TRUE(res);

    SphinxQL::Row row;
    // get first row
    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(219, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(2, row.getValue<float>("float_data"));
    ASSERT_EQ("Additional string attribute", row.getValue<std::string>("string_data"));
    ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])",
              row.getValue<std::string>("json_data"));
    ASSERT_EQ("1607798880006,7313020011448", row.getValue<std::string>("multi_data"));

    // get second row
    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(0, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(0, row.getValue<float>("float_data"));
    ASSERT_EQ("", row.getValue<std::string>("string_data"));
    ASSERT_EQ("", row.getValue<std::string>("json_data"));
    ASSERT_EQ("", row.getValue<std::string>("multi_data"));

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));

    // read the second query results
    res = result.next();
    ASSERT_TRUE(res);

    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(219, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(2, row.getValue<float>("float_data"));
    ASSERT_EQ("Additional string attribute", row.getValue<std::string>("string_data"));
    ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])",
              row.getValue<std::string>("json_data"));
    ASSERT_EQ("1607798880006,7313020011448", row.getValue<std::string>("multi_data"));

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));
}

TEST(Query, queryStreamGetter) {
    SphinxQL::Query query;
    query.addQuery("SELECT int_data, float_data, string_data, json_data, multi_data "
                   "FROM idx_test ORDER BY id ASC LIMIT 10;");
    query.connect("/tmp/test-sphinxql.s");
    SphinxQL::Response result = query.execute();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    ASSERT_TRUE(res);

    SphinxQL::Row row;
    // get first row
    ASSERT_TRUE(res->getNextRow(row));
    {
        uint32_t intData = 0;
        float floatData = 0.f;
        std::string stringData, jsonData, multiData;

        row >> intData >> floatData >> stringData >> jsonData >> multiData;

        ASSERT_EQ(219, intData);
        ASSERT_EQ(2, floatData);
        ASSERT_EQ("Additional string attribute", stringData);
        ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])", jsonData);
        ASSERT_EQ("1607798880006,7313020011448", multiData);
    }

    // get second row
    ASSERT_TRUE(res->getNextRow(row));
    {
        uint32_t intData = 0;
        float floatData = 0.f;
        std::string stringData, jsonData, multiData;

        row >> intData >> floatData >> stringData >> jsonData >> multiData;

        ASSERT_EQ(0, intData);
        ASSERT_EQ(0, floatData);
        ASSERT_EQ("", stringData);
        ASSERT_EQ("", jsonData);
        ASSERT_EQ("", multiData);
    }

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));
}


TEST(AsyncQuery, query) {
    SphinxQL::AsyncQuery query("/tmp/test-sphinxql.s", 0);
    query.add("SELECT * FROM idx_test ORDER BY id ASC LIMIT 10;");
    query.add("SELECT * FROM idx_test WHERE multi_data = 1607798880006;", false);
    SphinxQL::Response result = query.launch();

    // read first query results
    std::unique_ptr<SphinxQL::Result> res = result.next();
    ASSERT_TRUE(res);

    SphinxQL::Row row;
    // get first row
    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(219, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(2, row.getValue<float>("float_data"));
    ASSERT_EQ("Additional string attribute", row.getValue<std::string>("string_data"));
    ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])",
              row.getValue<std::string>("json_data"));
    ASSERT_EQ("1607798880006,7313020011448", row.getValue<std::string>("multi_data"));

    // get second row
    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(0, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(0, row.getValue<float>("float_data"));
    ASSERT_EQ("", row.getValue<std::string>("string_data"));
    ASSERT_EQ("", row.getValue<std::string>("json_data"));
    ASSERT_EQ("", row.getValue<std::string>("multi_data"));

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));

    // read the second query results
    res = result.next();
    ASSERT_TRUE(res);

    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(219, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(2, row.getValue<float>("float_data"));
    ASSERT_EQ("Additional string attribute", row.getValue<std::string>("string_data"));
    ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])",
              row.getValue<std::string>("json_data"));
    ASSERT_EQ("1607798880006,7313020011448", row.getValue<std::string>("multi_data"));

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));

    // reuse query object with additional query
    query.add("SELECT * FROM idx_test WHERE MATCH('first');", false);
    result = query.launch();

    // read query results
     res = result.next();
    ASSERT_TRUE(res);

    // get first row
    ASSERT_TRUE(res->getNextRow(row));
    ASSERT_EQ(219, row.getValue<uint32_t>("int_data"));
    ASSERT_EQ(2, row.getValue<float>("float_data"));
    ASSERT_EQ("Additional string attribute", row.getValue<std::string>("string_data"));
    ASSERT_EQ(R"([["1607798880006",true,1],["7313020011448",true,189]])",
              row.getValue<std::string>("json_data"));
    ASSERT_EQ("1607798880006,7313020011448", row.getValue<std::string>("multi_data"));

    // no further rows
    ASSERT_FALSE(res->getNextRow(row));
}


int main(int argc, char *argv[]) {
    // NOTE tests are expecting running searchd, this is done from outside by
    // run-tests.sh script.
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
