/*  ====================================================================================================================
    resolve-list.cpp - Implementation of resolve operation on a list of group

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

#include "resolve-list.h"
#include "common/commands.h"
#include "common/message-bus.h"
#include "common/string.h"
#include "group-rest.h"
#include <asset/json.h>
#include <fty/rest/component.h>
#include <fty/string-utils.h>
#include <fty_common_json.h>

#include <fty_log.h>
#include <fty_common_rest_audit_log.h>

namespace fty::agroup {

unsigned ResolveList::run()
{
    rest::User user(m_request);
    if (auto ret = checkPermissions(user.profile(), m_permissions); !ret) {
        throw rest::Error(ret.error());
    }

    fty::groups::MessageBus bus;
    if (auto res = bus.init(AgentName); !res) {
        throw rest::errors::Internal(res.error());
    }

    fty::commands::resolve::list::In in;

    // Read ids paramater (optional)
    // If empty, return all assets for all groups defined
    auto strIdsPrt = m_request.queryArg<std::string>("ids");
    if (strIdsPrt && !strIdsPrt->empty()) {
        std::stringstream ss(*strIdsPrt);
        std::string substr;
        while (std::getline(ss, substr, ',')) {
            if (!fty::groups::isNumeric(substr)) {
                throw rest::errors::Internal("Not a number");
            }
            in.ids.append(fty::convert<uint16_t>(substr));
        }
    }

    // Read detail parameter (optional)
    bool detail{true}; // default
    auto strDetailPtr = m_request.queryArg<std::string>("detail");
    if (strDetailPtr) {
        if (*strDetailPtr == "true") {
            detail = true;
        }
        else if (*strDetailPtr == "false") {
            detail = false;
        }
        else {
            throw rest::errors::Internal("Detail not in ['true', 'false']");
        }
    }

    fty::groups::Message msg = message(fty::commands::resolve::list::Subject);
    msg.setData(*pack::json::serialize(in));

    auto ret = bus.send(fty::Channel, msg);
    if (!ret) {
        throw rest::errors::Internal(ret.error());
    }

    commands::resolve::list::Out data;
    auto info = pack::json::deserialize(ret->userData[0], data);
    if (!info) {
        throw rest::errors::Internal(info.error());
    }

    bool isFirst = true;
    m_reply << "[";
    for (const auto& it : data) {
        if (!isFirst) {
            m_reply << ",";
        }
        else {
            isFirst = false;
        }
        m_reply << "{";
        m_reply << "\"id\": " << std::to_string(it.id) << ",";
        m_reply << "\"assets\": [";
        std::vector<std::string> out;
        for (const auto& asset : it.assets) {
            std::string json;

            if (detail) { // full payload
                json = asset::getJsonAsset(fty::convert<uint32_t>(asset.id.value()));
                if (json.empty()) {
                    throw rest::errors::Internal("Cannot build asset information");
                }
            }
            else { // light payload
                cxxtools::SerializationInfo si;
                si.addMember("id") <<= asset.id.value();
                si.addMember("name") <<= asset.name; //iname
                json = JSON::writeToString(si, false);
            }

            out.push_back(json);
        }
        m_reply << fty::implode(out, ", ");
        m_reply << "]";
        m_reply << "}";
    }
    m_reply << "]";
    return HTTP_OK;
}

} // namespace fty::agroup

registerHandler(fty::agroup::ResolveList)
