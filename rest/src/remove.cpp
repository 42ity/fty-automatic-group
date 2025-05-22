/*  ====================================================================================================================
    remove.cpp - Implementation of remove operation on any group

    Copyright (C) 2024 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ====================================================================================================================
*/

#include "remove.h"
#include "common/commands.h"
#include "common/message-bus.h"
#include "common/string.h"
#include "group-rest.h"
#include <fty/rest/component.h>
#include <fty/string-utils.h>

namespace fty::agroup {

unsigned Remove::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    auto strIdPrt = m_request.queryArg<std::string>("id");
    if (!strIdPrt) {
        throw rest::errors::RequestParamRequired("id");
    }

    // get vector of ids (numbers, comma separator)
    std::vector<std::string> ids = fty::split(*strIdPrt, ",");
    for (const auto& id : ids) {
        if (!fty::groups::isNumeric(id)) {
            throw rest::errors::Internal("Not a number");
        }
    }

    fty::groups::MessageBus bus;
    if (auto res = bus.init(AgentName); !res) {
        throw rest::errors::Internal(res.error());
    }

    auto msg = message(commands::remove::Subject);

    fty::commands::remove::In in;
    for (const auto& id : ids) {
        in.append(fty::convert<uint64_t>(id));
    }

    msg.setData(*pack::json::serialize(in));

    auto ret = bus.send(fty::Channel, msg);
    if (!ret) {
        throw rest::errors::Internal(ret.error());
    }

    commands::remove::Out out;
    auto info = pack::json::deserialize(ret->userData[0], out);
    if (!info) {
        throw rest::errors::Internal(info.error());
    }

    return HTTP_NO_CONTENT;
}

} // namespace fty::agroup

registerHandler(fty::agroup::Remove)
