#include "resolve-list.h"
#include "resolve.h"
#include "lib/storage.h"
#include "lib/mutex.h"
#include <tntdb.h>

namespace fty::job {

using namespace fmt::literals;

// =====================================================================================================================

void ResolveList::run(const commands::resolve::list::In& in, commands::resolve::list::Out& assetGroupList)
{
    logDebug("resolve list {}", *pack::json::serialize(in));

    std::vector<uint64_t> ids;
    // If some id groups defined
    if (in.ids.hasValue()) {
        ids = in.ids.value();
    } else {
        // else get id of all groups
        fty::storage::Mutex::Read mx;
        std::lock_guard<fty::storage::Mutex::Read> lock(mx);
        for (const auto& id : Storage::ids()) {
            ids.push_back(id);
        }
    }

    // Normal connect in _this_ thread, otherwise tntdb will fail
    tntdb::connect(getenv("DBURL") ? getenv("DBURL") : DBConn::url);
    // Normal connection, continue my sad work with db
    fty::db::Connection conn;

    // For each group defined
    for (auto id : ids) {
        auto& groupLine = assetGroupList.append();
        groupLine.id = id;

        auto group = Storage::byId(id);
        if (!group) {
            throw Error(group.error());
        }

        std::string groupsSql = groupSql(conn, group->rules);

        std::string sql = fmt::format(R"(
            SELECT
                id_asset_element as id,
                name
            FROM t_bios_asset_element
            WHERE id_asset_element IN ({}) AND name <> 'rackcontroller-0'
            ORDER BY id
        )", groupsSql);

        // std::cerr << sql << std::endl;

        try {
            for (const auto& row : conn.select(sql)) {
                auto& line = groupLine.assets.append();
                line.id    = row.get<u_int64_t>("id");
                line.name  = row.get("name");
            }
        } catch (const std::exception& e) {
            throw Error(e.what());
        }
    }
    //auto ret = pack::json::serialize(assetGroupList);
    //logDebug("assetGroupList={}", ret->c_str());
}

} // namespace fty::job
