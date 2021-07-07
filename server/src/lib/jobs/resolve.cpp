#include "resolve.h"
#include "asset/asset-db.h"
#include "lib/storage.h"
#include <fty_common_asset_types.h>
#include <fty_common_db_connection.h>
#include <fty_common_db_dbpath.h>
#include <tntdb.h>

namespace fty::job {

using namespace fmt::literals;

// =====================================================================================================================

static std::string op(const Group::Condition& cond)
{
    switch (cond.op) {
        case Group::ConditionOp::Contains:
            return "like";
        case Group::ConditionOp::DoesNotContain:
            return "not like";
        case Group::ConditionOp::Is:
            return "=";
        case Group::ConditionOp::IsNot:
            return "<>";
        case Group::ConditionOp::Unknown:
            return "unknown";
    }
    return "unknown";
}


static std::string value(const Group::Condition& cond, std::string (*f)(const std::string&) = nullptr)
{
    if (cond.op == Group::ConditionOp::Contains || cond.op == Group::ConditionOp::DoesNotContain) {
        // Like or not like
        // Escape the forbiden char at first
        std::string val = cond.value.value();

        val = std::regex_replace(val, std::regex(R"(\\)"), R"(\\\\)");
        val = std::regex_replace(val, std::regex(R"(%)"), R"(\%)");
        val = std::regex_replace(val, std::regex(R"(_)"), R"(\_)");

        if (f) {
            val = f(val);
        }

        return "%{}%"_format(val);
    } else {
        return cond.value.value();
    }
}

static std::string sqlLogicalOperator(const Group::LogicalOp& op)
{
    switch (op) {
        case Group::LogicalOp::And:
            return "AND";
        case Group::LogicalOp::Or:
            return "OR";
        case Group::LogicalOp::Unknown:
            return "unknown";
    }
    return "unknown";
}

static std::vector<uint64_t> vmLinkTypes()
{
    static std::vector<uint64_t> ids = []() {
        std::string         sql = "select id_asset_link_type from t_bios_asset_link_type where name like '%.hosts.vm'";
        fty::db::Connection conn;
        std::vector<uint64_t> ret;
        for (const auto& it : conn.select(sql)) {
            ret.push_back(it.get<uint64_t>("id_asset_link_type"));
        }
        return ret;
    }();

    return ids;
}

// =====================================================================================================================

static std::string byName(const Group::Condition& cond)
{
    static std::string sql = R"(
        SELECT id_asset_element
        FROM t_bios_asset_ext_attributes
        WHERE keytag='name' AND value {op} '{val}')";

    // clang-format off
    return fmt::format(sql,
        "op"_a  = op(cond),
        "val"_a = value(cond)
    );
    // clang-format on
}

// =====================================================================================================================

static std::string byInternalName(const Group::Condition& cond)
{
    static std::string sql = R"(
        SELECT id_asset_element
        FROM t_bios_asset_element
        WHERE name {} '{}')";

    // clang-format off
    return fmt::format(sql,
        "op"_a  = op(cond),
        "val"_a = value(cond)
    );
    // clang-format on
}

// =====================================================================================================================

static std::string byContact(const Group::Condition& cond)
{
    std::string tmpOp = op(cond);
    std::string sql   = R"(
        SELECT id_asset_element
        FROM t_bios_asset_ext_attributes
        WHERE (keytag='device.contact' OR keytag='contact_email') AND
              value {op} '{val}')";

    if (cond.op == Group::ConditionOp::IsNot || cond.op == Group::ConditionOp::DoesNotContain) {
        sql =
            "SELECT id_asset_element FROM t_bios_asset_element \
             WHERE id_asset_element NOT IN (" +
            sql + ")";
        tmpOp = cond.op != Group::ConditionOp::IsNot ? "like" : "=";
    }

    // clang-format off
    return fmt::format(sql,
        "op"_a  = tmpOp,
        "val"_a = value(cond)
    );
    // clang-format on
}

// =====================================================================================================================

static std::string byType(const Group::Condition& cond)
{
    static std::string sql = R"(
        SELECT e.id_asset_element
        FROM t_bios_asset_element AS e
        LEFT JOIN t_bios_asset_element_type as t ON e.id_type = t.id_asset_element_type
        WHERE t.name {op} '{val}')";

    // clang-format off
    return fmt::format(sql,
        "op"_a  = op(cond),
        "val"_a = value(cond)
    );
    // clang-format on
}

// =====================================================================================================================

static std::string bySubType(const Group::Condition& cond)
{
    static std::string sql = R"(
        SELECT e.id_asset_element
        FROM t_bios_asset_element as e
        LEFT JOIN t_bios_asset_device_type as t ON e.id_subtype = t.id_asset_device_type
        WHERE t.name {op} '{val}')";

    // clang-format off
    return fmt::format(sql,
        "op"_a  = op(cond),
        "val"_a = value(cond)
    );
    // clang-format on
}

// =====================================================================================================================

static std::vector<uint64_t> hypervisorLinkTypes()
{
    static std::vector<uint64_t> ids = []() {
        std::string sql =
            "select id_asset_link_type from t_bios_asset_link_type where name = 'ipminfra.server.hosts.os'";
        fty::db::Connection   conn;
        std::vector<uint64_t> ret;
        for (const auto& it : conn.select(sql)) {
            ret.push_back(it.get<uint64_t>("id_asset_link_type"));
        }
        return ret;
    }();

    return ids;
}

static std::string byLocation(fty::db::Connection& conn, const Group::Condition& cond)
{
    std::string dcSql = R"(
        SELECT id_asset_element
        FROM t_bios_asset_element
        WHERE id_type in ({avail}) AND name {op} '{val}')";

    std::vector<int> avail = {persist::DATACENTER, persist::ROW, persist::RACK, persist::ROOM};
    // clang-format off
    dcSql = fmt::format(dcSql,
        "avail"_a = fty::implode(avail, ", "),
        "op"_a    = cond.op != Group::ConditionOp::IsNot ? op(cond) : "=",
        "val"_a   = value(cond)
    );
    // clang-format on

    try {
        std::vector<int64_t> ids;
        // Select all ids for location

        std::string elQuery = R"(
            SELECT p.id_asset_element
            FROM v_bios_asset_element_super_parent p
            WHERE :containerid in (p.id_parent1, p.id_parent2, p.id_parent3, p.id_parent4,
                p.id_parent5, p.id_parent6, p.id_parent7, p.id_parent8, p.id_parent9, p.id_parent10)
        )";

        for (const auto& row : conn.select(dcSql)) {
            for (const auto& elRow : conn.select(elQuery, "containerid"_p = row.get<int64_t>("id_asset_element"))) {
                ids.push_back(elRow.get<int64_t>("id_asset_element"));
            }
        }

        if (ids.empty()) {
            return R"(
                SELECT id_asset_element
                FROM t_bios_asset_element
                WHERE id_asset_element = 0
            )";
        }

        // get hypervisors
        std::string sqlHypervisor = R"(
            select l.id_asset_device_dest 
            from t_bios_asset_link as l
            LEFT JOIN t_bios_asset_element AS e 
            ON e.id_asset_element = l.id_asset_device_src
            where l.id_asset_device_src in ({id}) and 
                  l.id_asset_link_type IN ({linkTypes})     
        )";

        // hosted by | get VMs
        std::string sqlVM = R"(
        SELECT l.id_asset_device_dest FROM t_bios_asset_link AS l
        LEFT JOIN t_bios_asset_element AS e ON e.id_asset_element = l.id_asset_device_src
        WHERE
            l.id_asset_link_type IN ({linkTypes}) AND
            e.id_type = {type} AND
            e.id_asset_element in ({val})
        )";

        // clang-format off
        sqlHypervisor = fmt::format(sqlHypervisor, 
            "id"_a        = fty::implode(ids, ", "), 
            "linkTypes"_a = fty::implode(hypervisorLinkTypes(), ", ")
        );

        sqlVM = fmt::format(sqlVM,
            "linkTypes"_a = fty::implode(vmLinkTypes(), ", "), 
            "val"_a = sqlHypervisor, 
            "type"_a = persist::HYPERVISOR
        );
        // clang-format on  

        for (const auto& row : conn.select(sqlHypervisor)) {
            ids.push_back(row.get<int64_t>("id_asset_device_dest"));
        }

        for (const auto& row : conn.select(sqlVM)) {
            ids.push_back(row.get<int64_t>("id_asset_device_dest"));
        }

        std::string ret = R"(
            SELECT id_asset_element
            FROM t_bios_asset_element
            WHERE id_asset_element in ({})
        )"_format(fty::implode(ids, ","));

        if (cond.op == Group::ConditionOp::IsNot) {
            ret = "SELECT id_asset_element FROM t_bios_asset_element WHERE id_asset_element NOT IN (" + ret + ")";
        }

        ret += " AND id_type NOT IN ({})"_format(fty::implode(avail, ", "));

        return ret;
    } catch (const std::exception& e) {
        throw Error(e.what());
    }
}

// =====================================================================================================================

static std::string byHostName(const Group::Condition& cond)
{
    std::string tmpOp = op(cond);
    std::string sql = R"(
        SELECT e.id_asset_element
        FROM t_bios_asset_element AS e
        LEFT JOIN t_bios_asset_ext_attributes a ON e.id_asset_element = a.id_asset_element
        WHERE
            a.value {op} '{val}' AND
            ((
                a.keytag='hostname.1'
            ) OR (
                a.keytag = 'hostname' 
            ))
    )";


    if (cond.op == Group::ConditionOp::IsNot  || cond.op == Group::ConditionOp::DoesNotContain) {
        sql =
            "SELECT id_asset_element FROM t_bios_asset_element \
             WHERE id_asset_element NOT IN (" +
            sql + ")";
        tmpOp = cond.op != Group::ConditionOp::IsNot ? "like" : "=";
    }

    // clang-format off
    std::string ret = fmt::format(sql,
        "op"_a       = tmpOp,
        "val"_a      = value(cond)
    );
    // clang-format on
    return ret;
}

// =====================================================================================================================

static std::string byIpAddress(const Group::Condition& cond)
{
    std::string tmpOp = op(cond);
    std::string sql   = R"(
        SELECT e.id_asset_element
        FROM t_bios_asset_element AS e
        LEFT JOIN t_bios_asset_ext_attributes a ON e.id_asset_element = a.id_asset_element
        LEFT JOIN t_bios_asset_element_type t ON e.id_type = t.id_asset_element_type
        WHERE
            a.value {op} '{val}' AND
            ((
                a.keytag='ip.1'
            ) OR (
                a.keytag = 'ip' 
            ))
            AND t.name != "virtual-machine"
    )";


    if (cond.op == Group::ConditionOp::IsNot || cond.op == Group::ConditionOp::DoesNotContain) {
        sql =
            "SELECT id_asset_element FROM t_bios_asset_element \
             WHERE id_asset_element NOT IN (" +
            sql + ")";
        tmpOp = cond.op != Group::ConditionOp::IsNot ? "like" : "=";
    }

    auto replaceStarByPercent = [](const std::string& val) {
        return std::regex_replace(val, std::regex(R"(\*)"), R"(%)");
    };
    // clang-format off
    std::string ret = fmt::format(sql,
        "op"_a       = tmpOp,
        "val"_a      = value(cond, replaceStarByPercent)
    );
    // clang-format on
    return ret;
}

// =====================================================================================================================

static std::string byHostedBy(const Group::Condition& cond)
{
    static std::string sql = R"(
        SELECT l.id_asset_device_dest FROM t_bios_asset_link AS l
        LEFT JOIN t_bios_asset_element AS e ON e.id_asset_element = l.id_asset_device_src
        WHERE
            l.id_asset_link_type IN ({linkTypes}) AND
            e.id_type = {type} AND
            e.name {op} '{val}'
    )";

    // clang-format off
    return fmt::format(sql,
        "linkTypes"_a = fty::implode(vmLinkTypes(), ", "),
        "op"_a        = op(cond),
        "val"_a       = value(cond),
        "type"_a      = persist::HYPERVISOR
    );
    // clang-format on
}
static std::string byGroupId(tnt::Connection& conn, const Group::Condition& cond);
// =====================================================================================================================

static std::string groupSql(fty::db::Connection& conn, const Group::Rules& group)
{
    struct SubQuery
    {
        std::string query;
        std::string op;
        SubQuery(const std::string& q, const std::string& o = "IN")
            : query(q)
            , op(o)
        {
        }
    };

    std::vector<SubQuery> subQueries;
    for (const auto& it : group.conditions) {
        if (it.is<Group::Condition>()) {
            const auto& cond = it.get<Group::Condition>();
            switch (cond.field) {
                case Group::Fields::Contact:
                    subQueries.emplace_back(byContact(cond));
                    break;
                case Group::Fields::HostName:
                    subQueries.emplace_back(byHostName(cond));
                    break;
                case Group::Fields::IPAddress:
                    subQueries.emplace_back(byIpAddress(cond));
                    break;
                case Group::Fields::Location:
                    subQueries.emplace_back(byLocation(conn, cond));
                    break;
                case Group::Fields::Name:
                    subQueries.emplace_back(byName(cond));
                    break;
                case Group::Fields::Type:
                    subQueries.emplace_back(byType(cond));
                    break;
                case Group::Fields::SubType:
                    subQueries.emplace_back(bySubType(cond));
                    break;
                case Group::Fields::InternalName:
                    subQueries.emplace_back(byInternalName(cond));
                    break;
                case Group::Fields::HostedBy:
                    subQueries.emplace_back(byHostedBy(cond));
                    break;
                case Group::Fields::Group:
                    if (cond.op == fty::Group::ConditionOp::IsNot) {
                        subQueries.emplace_back(byGroupId(conn, cond), "NOT IN");
                    } else {
                        subQueries.emplace_back(byGroupId(conn, cond));
                    }
                    break;
                case Group::Fields::Unknown:
                default:
                    throw Error("Unsupported field '{}' in condition", cond.field.asString());
            }
        } else {
            subQueries.emplace_back(groupSql(conn, it.get<Group::Rules>()));
        }
    }

    if (subQueries.empty()) {
        throw Error("Request is empty");
    }

    auto lambdaImplode = [&]() {
        std::stringstream ss;
        bool              first = true;
        for (const auto& query : subQueries) {
            if (!first) {
                ss << ") " + sqlLogicalOperator(group.groupOp) + " id_asset_element " + query.op + " (";
            }
            first = false;
            ss << query.query;
        }
        return ss.str();
    };

    return R"(
       SELECT
           id_asset_element as id
       FROM t_bios_asset_element
       WHERE id_asset_element IN ({})
   )"_format(lambdaImplode());
}

static std::string byGroupId(tnt::Connection& conn, const Group::Condition& cond)
{
    auto val   = fty::convert<uint64_t, std::string>(cond.value);
    auto group = Storage::byId(val);

    if (!group) {
        throw Error(group.error());
    }

    return groupSql(conn, group->rules);
}

void Resolve::run(const commands::resolve::In& in, commands::resolve::Out& assetList)
{
    logDebug("resolve {}", *pack::json::serialize(in));

    fty::Group group;
    if (in.id.hasValue()) {
        auto groupTmp = Storage::byId(in.id);

        if (!groupTmp) {
            throw Error(groupTmp.error());
        }
        group = groupTmp.value();
    } else {
        group.rules = in.rules;
    }

    // Normal connect in _this_ thread, otherwise tntdb will fail
    tntdb::connect(getenv("DBURL") ? getenv("DBURL") : DBConn::url);
    // Normal connection, continue my sad work with db
    fty::db::Connection conn;

    std::string groups = groupSql(conn, group.rules);

    std::string sql = R"(
        SELECT
            id_asset_element as id,
            name
        FROM t_bios_asset_element
        WHERE id_asset_element IN ({}) AND name <> 'rackcontroller-0'
        ORDER BY id
    )"_format(groups);

    // std::cerr << sql << std::endl;

    try {
        for (const auto& row : conn.select(sql)) {
            auto& line = assetList.append();
            line.id    = row.get<u_int64_t>("id");
            line.name  = row.get("name");
        }
    } catch (const std::exception& e) {
        throw Error(e.what());
    }
}

} // namespace fty::job
