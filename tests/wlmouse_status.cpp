// Read-only battery diagnostic for connected WLMouse devices.
#include "../src/drivers/drivers.h"
#include <cstdio>
int main() {
    PollContext ctx;
    for (const auto& c : EnumHid()) {
        if (c.vid != 0x36A7) continue;
        ctx.colls.push_back(c);
        printf("%04X:%04X IF=%d usage=%04X:%04X in=%u out=%u feature=%u product=%ls\n",
               c.vid, c.pid, c.iface, c.page(), c.usage(), c.inLen(), c.outLen(), c.featLen(), HidProduct(c).c_str());
    }
    PollWlmouse(ctx);
    for (const auto& s : ctx.out)
        printf("%ls [%ls] percent=%d charging=%d asleep=%d key=%ls\n", s.name.c_str(), s.via.c_str(),
               s.percent, s.charging, s.asleep, s.key.c_str());
    return ctx.out.empty() ? 1 : 0;
}
