//
// Network. (WWIV network redirector)
//
// example usage:
//
// addresses.binkp:
// @1 localhost:24554 -
// @2 localhost:24554 -
//

#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/ppp_config.h"

#include "sdk/config.h"
#include "sdk/networks.h"

#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;

static void ShowHelp() {
  cout << "Usage: network [flags]" << endl
    << "Flags:" << endl
    << "/N####     Network node number to dial." << endl
    << ".####      Network number (as defined in INIT)" << endl
    << "--network  Network name to use (i.e. wwivnet)" << endl
    << "--bbsdir   (optional) BBS directory if other than current directory " << endl
    << endl;
}

static map<string, string> ParseArgs(int argc, char** argv) {
  map<string, string> args;
  for (int i = 0; i < argc; i++) {
    const string s(argv[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string value = (delims.size() > 1) ? delims[1] : "";
      args.emplace(delims[0].substr(2), value);
    } else if (starts_with(s, "/")) {
      char letter = std::toupper(s[1]);
      const string key(1, letter);
      const string value = s.substr(2);
      args.emplace(key, value);
    } else if (starts_with(s, ".")) {
      const string key = "network_number";
      const string value = s.substr(1);
      args.emplace(key, value);
    }
  }
  return args;
}

static int LaunchOldNetworkingStack(const std::string exe, int argc, char** argv) {
  std::ostringstream os;
  os << exe;
  for (int i = 1; i < argc; i++) {
    const string s(argv[1]);
    if (starts_with(s, "/")) {
      os << " " << argv[i];
    }
  }
  const string command_line = os.str();
  Logger() << "Executing Command: '" << command_line << "'";
  return system(command_line.c_str());
}

int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    map<string, string> args = ParseArgs(argc, argv);

    for (const auto& arg : args) {
      Logger() << "arg: --" << arg.first << "=" << arg.second;
      if (arg.first == "help") {
        ShowHelp();
        return 0;
      }
    }

    string network_name = get_or_default(args, "network", "");
    string network_number = get_or_default(args, "network_number", "");

    if (network_name.empty() && network_number.empty()) {
      Logger() << "--network=[network name] or .[network_number] must be specified.";
      ShowHelp();
      return 1;
    }

    string bbsdir = File::current_directory();
    if (contains(args, "bbsdir")) {
      bbsdir = args["bbsdir"];
    }
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      Logger() << "Unable to load config.dat.";
      ShowHelp();
      return 1;
    }
    Networks networks(config);
    if (!networks.IsInitialized()) {
      Logger() << "Unable to load networks.";
      ShowHelp();
      return 1;
    }

    if (!network_number.empty() && network_name.empty()) {
      // Need to set the network name based on the number.
      network_name = networks[std::stoi(network_number)].name;
    }

    int expected_remote_node = 0;
    if (contains(args, "N")) {
      expected_remote_node = std::stoi(args.at("N"));
    }

    if (expected_remote_node == 32767) {
      // 32767 is the PPP project address for "send everything". Some people use this
      // "magic" node number.
      Logger() << "USE PPP Project to send to: Internet Email (@32767)";
      return LaunchOldNetworkingStack("networkp", argc, argv);
    }

    BinkConfig bink_config(network_name, config, networks);
    const BinkNodeConfig* node_config = bink_config.node_config_for(expected_remote_node);
    if (node_config != nullptr) {
      // We have a node configuration for this one, use networkb.
      Logger() << "USE networkb: " << node_config->host << ":" << node_config->port;
      const string command_line = StringPrintf("networkb --send --network=%s --node=%d",
        network_name.c_str(), expected_remote_node);
      Logger() << "Executing Command: '" << command_line << "'";
      return system(command_line.c_str());
    }

    PPPConfig ppp_config(network_name, config, networks);
    const PPPNodeConfig* ppp_node_config = ppp_config.node_config_for(expected_remote_node);
    if (ppp_node_config != nullptr) {
      Logger() << "USE PPP Project to send to: " << ppp_node_config->email_address;
      return LaunchOldNetworkingStack("networkp", argc, argv);
    }

    // Use legacy networking.
    return LaunchOldNetworkingStack("network0", argc, argv);
  } catch (const std::exception& e) {
    Logger() << e.what();
  }
}