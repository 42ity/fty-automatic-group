/*  =========================================================================
    main.cpp - Implementation of main application

    Copyright (C) 2014 - 2020 Eaton

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
    =========================================================================
 */

#include "lib/config.h"
#include "lib/daemon.h"
#include "lib/server.h"
#include <fty/command-line.h>
#include <iostream>
#include <fty_log.h>

int main(int argc, char** argv)
{
    bool        daemon = false;
    std::string config = "conf/agroup.conf";
    bool        help   = false;

    // clang-format off
    fty::CommandLine cmd("Automatic group service", {
        {"--config", config, "Configuration file"},
        {"--daemon", daemon, "Daemonize this application"},
        {"--help",   help,   "Show this help"}
    });
    // clang-format on

    if (auto res = cmd.parse(argc, argv); !res) {
        std::cerr << res.error() << std::endl;
        std::cout << std::endl;
        std::cout << cmd.help() << std::endl;
        return EXIT_FAILURE;
    }

    if (help) {
        std::cout << cmd.help() << std::endl;
        return EXIT_SUCCESS;
    }

    if (auto ret = fty::Config::instance().load(config); !ret) {
        logError("{}", ret.error());
        return EXIT_FAILURE;
    }

    ManageFtyLog::setInstanceFtylog(fty::Config::instance().actorName, fty::Config::instance().logger);

    if (daemon) {
        logDebug("Start automatic group agent as daemon");
        fty::Daemon::daemonize();
    }

    fty::Server srv;
    if (auto res = srv.run()) {
        srv.wait();
        srv.shutdown();
    } else {
        logError("{}", res.error());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
