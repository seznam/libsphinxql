/**
 * SphinxQL query builder.
 * The MIT License (MIT)
 * Copyright (c) 2021 Seznam.cz a.s.
 */

#include "querybuilder.h"
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include <string>


namespace {
    /// Join container data with op between each two elements.
    template <typename Container>
    std::string join(const Container &data, const std::string &op) {
        std::ostringstream res;
        typename Container::const_iterator it = data.begin();
        typename Container::const_iterator end = data.end();
        // place first element
        if (it != end) {
            res << *it;
            ++it;
        }
        // place remaining elements
        for ( ; it != end; ++it) {
            res << op << *it;
        }
        return res.str();
    }
}


namespace SphinxQL {

class QueryConstructor::PrivateData {
    int offset;     ///< Results offset.
    int limit;      ///< Results rows limit.
    int matches;    ///< Sphinx max_matches conf.
    int querytime;  ///< Sphinx max_query_time conf.
    std::string query;
    std::string index;
    std::string weights;
    std::string sorting;
    std::string ranking;
    std::string matching;
    std::string groupby;
    std::string groupexpr;
    std::vector<std::string> filters;
  public:
    PrivateData(): offset(0), limit(20), matches(3000), querytime(5000) {}
    PrivateData(int offset, int limit, int maxmatches, int querytime)
        : offset(offset), limit(limit), matches(maxmatches),
          querytime(querytime)
    {}

    friend class QueryConstructor;
};

QueryConstructor::QueryConstructor(): data(std::make_unique<PrivateData>()) {}

QueryConstructor::~QueryConstructor() = default;

void QueryConstructor::setIndex(const std::string &i) {
    data->index = i;
}

void QueryConstructor::setMatching(const std::string &m) {
    data->matching = "MATCH('" + m + "')";
}

void QueryConstructor::setPaging(int o, int l) {
    data->offset = o;
    data->limit = l;
}

void QueryConstructor::setMaxMatches(int max) {
    data->matches = max;
}

void QueryConstructor::setMaxQueryTime(int time) {
    data->querytime = time;
}

void QueryConstructor::setSorting(const std::string &s) {
    if (s.empty()) {
        data->sorting = "";
    } else {
        data->sorting = "ORDER BY " + s;
    }
}

std::string QueryConstructor::getSorting() const {
    return data->sorting;
}

void QueryConstructor::setRanking(const std::string &r) {
    data->ranking = "ranker=expr('" + r + "')";
}

void QueryConstructor::setFieldWeight(const std::string &f, int w) {
    std::ostringstream o;
    if (!(data->weights.empty())) o << data->weights << ",";
    o << f << "=" << w;
    data->weights = o.str();
}

void QueryConstructor::addEnumFilter(
        const std::string &f, const std::vector<uint32_t> &val) {
    if (!val.size()) return;
    addEnumFilter(f, val.begin(), val.end());
}

void QueryConstructor::addEnumFilter(
        const std::string &f, const std::list<uint32_t> &val) {
    if (!val.size()) return;
    addEnumFilter(f, val.begin(), val.end());
}

template <typename Iterator>
void QueryConstructor::addEnumFilter(
        const std::string &f, const Iterator begin, const Iterator end) {
    if (begin == end) return;
    Iterator it = begin;
    std::ostringstream o;
    o << f << " IN (" << *it++;
    for (; it != end; ++it) {
        o << ", " << *it;
    }
    o << ")";

    data->filters.push_back(o.str());
}


void QueryConstructor::addRangeFilter(
        const std::string &f, uint32_t min, uint32_t max) {
    std::ostringstream o;
    o << f << " BETWEEN " << min << " and " << max;
    data->filters.push_back(o.str());
}

std::string QueryConstructor::getSelectClause() const {
    return data->query;
}

void QueryConstructor::setSelectClause(const std::string &s) {
    data->query = s;
}

void QueryConstructor::setGrouping(
        const std::string &f, const std::string &expr) {
    data->groupby = f;
    data->groupexpr = expr;
}

std::string QueryConstructor::getQuery() const {
    std::ostringstream out;
    // basic part
    out << "SELECT " << data->query << " FROM " << data->index;
    // add filters
    const std::string &f = join(data->filters, " AND ");
    if (!f.empty() || !(data->matching.empty())) {
        out << " WHERE " << f;
        if (!f.empty() && !(data->matching.empty())) out << " AND ";
        out << data->matching;
    }
    // group by part
    if (!(data->groupby.empty())) {
        out << " GROUP BY " << data->groupby;
        if (!(data->groupexpr.empty()))
            out << " WITHIN GROUP ORDER BY " << data->groupexpr;
    }
    // order by part
    out << " " << data->sorting << " "
        "LIMIT " << data->offset << ", " << data->limit << " ";
    // options
    std::vector<std::string> opt;
    if (!(data->weights.empty()))
        opt.push_back("field_weights=(" + data->weights + ")");
    if (!(data->ranking.empty())) opt.push_back(data->ranking);
    // resource limits
    if (data->querytime) {
        std::ostringstream qt;
        qt << "max_query_time=" << data->querytime;
        opt.push_back(qt.str());
    }
    // max matches
    if (data->matches) {
        std::ostringstream optval;
        optval << "max_matches=" << data->matches;
        opt.push_back(optval.str());
    }
    const std::string &o = join(opt, ", ");
    if (!o.empty()) out << "OPTION " << o;
    out << ";";
    return out.str();
}

}