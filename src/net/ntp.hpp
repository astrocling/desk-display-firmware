#pragma once

#include <cstdint>

namespace desk_net {

void ntpSetup();
void ntpLoop();
bool ntpIsSynced();
std::int64_t ntpUnixUtc();

}  // namespace desk_net
