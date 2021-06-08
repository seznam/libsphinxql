/**
 * SphinxQL query builder.
 * The MIT License (MIT)
 * Copyright (c) 2021 Seznam.cz a.s.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <list>


namespace SphinxQL {



/// Helper to build query for Sphinx. API similar to cppsphinxclient library.
class QueryConstructor {
    class PrivateData;
    /// Private data (hidden for ABI compatibility)
    std::unique_ptr<PrivateData> data;
  public:
    QueryConstructor();
    ~QueryConstructor();
    /// Set searched index.
    void setIndex(const std::string &i);
    /// Set searched keyword (phrase).
    void setMatching(const std::string &m);
    /// Set offset and limit. Default LIMIT 0, 20.
    void setPaging(int o, int l);
    /// Set sphinx option max_matches.
    void setMaxMatches(int max);
    /// Set sphinx option max_query_time.
    void setMaxQueryTime(int time);
    /// Set sorting expression (Order by). Empty to disable already existing one.
    void setSorting(const std::string &s);
    /// Get sorting expression.
    std::string getSorting() const;
    /// Set ranking expression.
    void setRanking(const std::string &r);
    /// Set grouping field and expression for group sorting order.
    void setGrouping(const std::string &f, const std::string &expr);
    /// Set field weight. See sphinx option field_weights.
    void setFieldWeight(const std::string &f, int w);
    /// Add filter to field with values stored in a vector.
    void addEnumFilter(const std::string &f, const std::vector<uint32_t> &val);
    /// Add filter to field with values stored in a list.
    void addEnumFilter(const std::string &f, const std::list<uint32_t> &val);
    /// Add filter to field with values supplied by an iterator.
    template <typename Iterator>
    void addEnumFilter(const std::string &f,
            const Iterator begin, const Iterator end);
    /// Add range filter to field between min and max values. Included..
    void addRangeFilter(const std::string &f, uint32_t min, uint32_t max);
    /// Get current selectClause.
    std::string getSelectClause() const;
    /// Set select clause (fields retrieved by query).
    void setSelectClause(const std::string &s);

    /// Get final query string. Semicolon at the end included.
    std::string getQuery() const;
};


}