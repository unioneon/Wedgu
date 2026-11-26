// WLMouse (VID 36A7): Beast X 8K, Beast X Mini / Mini Pro, Beast X Pro, Beast MAX, Sword X 8K, Beast G and
// their receivers. The Sword X 8K ids (A878 receiver, A879 cable) and its FFFF:0000 collection come from an
// owner's --dump (issue #75). The Beast X V2 8K receiver (A856) comes from an owner's --dump (issue #121): it
// exposes the same 65-byte FFFF:0000 collection on interface 2 as the other 8K receivers, so it gets the same
// query (HaloBattery sends it to every 36A7 product id). Its cable id is not known yet.
// Beast G (issue #237): A860 is its 8K receiver and A861 its cable, per the official WL MOUSE HUB device
// list as recorded by github.com/late-autumn-gif/mousedock (docs/device-db/mouse-db.json, same Feature 0 /
// 0x83 battery query), github.com/OpenMouse-Project/mouse-protocol (src/drivers/vendors.ts) and
// github.com/Zelozzzz/dorsal (src/dorsal/models.py). A860 was polled before as an unnamed cable.
// Read-only status query on the vendor collection (usage page FFFF, 65-byte feature report 0):
//   SET_FEATURE [00][00][00][02][02][00][83][00 ...], then GET_FEATURE report 0 until
//   [00][status][00][02][02][00][83][charging][battery%]; status A1 / A2 = answered, A0 = mouse asleep.
//   No checksum. The receiver answers for the mouse; a silent receiver means the mouse is asleep or off.
// From github.com/djnawara/mouse-gauge (verified on a Beast X 8K through its A883 receiver) and
// github.com/Waturus/HaloBattery (providers/wlmouse.py; A880 receiver confirmed on hardware).
// Not handled: A887 / A888 (Beast X 4K / original Beast X). The sources disagree on their protocol
// (an output report 04 query, or a different CRC framing) and one reports they never answer this query.
// Ported from krisgfx9/Wireless-Device-Battery-Monitor, GPL-3.0-or-later.
// Source snapshot: notjiccu/wireless-device-battery-monitor@79705baa15412ad1f96bd7c9d3798b5d981c0d2a.
#include "drivers.h"

#include <algorithm>

namespace {

const USHORT kVid = 0x36A7;

// Cables come before receivers: a mouse on its cable with the receiver also plugged in shows up under both
// ids, so a cable that answers claims its receiver (`receiverPid`) and the mouse is listed once.
struct Model { USHORT pid; bool receiver; const wchar_t* name; USHORT receiverPid; };
const Model kModels[] = {
    { 0xA879, false, L"WLMouse Sword X 8K", 0xA878 },
    { 0xA884, false, L"WLMouse Beast X 8K", 0xA883 },
    { 0xA861, false, L"WLMouse Beast G", 0xA860 },
    { 0xA866, false, L"WLMouse mouse", 0 },
    { 0xA867, false, L"WLMouse mouse", 0 },
    { 0xA871, false, L"WLMouse Beast X Pro", 0xA870 },  // cable; tested on the charging Beast X Pro
    { 0xA856, true, L"WLMouse Beast X V2", 0 },  // 8K receiver; seen with a Beast X V2 Max
    { 0xA860, true, L"WLMouse Beast G", 0 },  // 8K receiver
    { 0xA868, true, L"WLMouse Beast X Mini Pro", 0 },
    { 0xA870, true, L"WLMouse Beast X Pro", 0 },
    { 0xA878, true, L"WLMouse Sword X 8K", 0 },
    { 0xA880, true, L"WLMouse Beast MAX", 0 },
    { 0xA882, true, L"WLMouse Beast X", 0 },  // locally observed receiver
    { 0xA883, true, L"WLMouse Beast X 8K", 0 },
    { 0xA885, true, L"WLMouse Beast X Mini", 0 },
};

enum class Reply { Battery, Asleep, None };

Reply Query(HidDev& d, int& pct, bool& charging) {
    std::vector<BYTE> req(65, 0);  // [0] = report id 0
    req[3] = 0x02;
    req[4] = 0x02;
    req[6] = 0x83;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!d.SetFeature(req)) return Reply::None;
        for (int read = 0; read < 6; ++read) {
            Sleep(60);
            std::vector<BYTE> r;
            if (!d.GetFeature(0x00, r) || r.size() < 9) return Reply::None;
            if (r[3] != 0x02 || r[4] != 0x02 || r[6] != 0x83) continue;  // not the reply (yet)
            if (r[1] == 0xA0) return Reply::Asleep;
            if (r[1] != 0xA1 && r[1] != 0xA2) continue;  // still our own request echoed back
            if (r[8] > 100) return Reply::None;
            pct = r[8];
            charging = r[7] == 0x01;
            return Reply::Battery;
        }
    }
    return Reply::None;
}

}  // namespace

void PollWlmouse(PollContext& ctx) {
    for (const Model& m : kModels) {
        if (ctx.IsClaimed(kVid, m.pid)) continue;
        auto colls = ctx.Find(kVid, m.pid, [](const HidColl& c) { return c.page() == 0xFFFF && c.featLen() >= 65; });
        // Usage 0 is the collection that answers in both sources; try it first.
        std::stable_sort(colls.begin(), colls.end(), [](const HidColl* a, const HidColl* b) {
            return (a->usage() == 0) > (b->usage() == 0);
        });
        for (const HidColl* c : colls) {
            HidDev d;
            if (!d.OpenFeatureOnly(*c)) continue;
            int pct = -1;
            bool charging = false;
            Reply rep = Query(d, pct, charging);
            if (rep == Reply::None) continue;
            DeviceStatus s;
            // Beast X uses A882 (1K receiver), A883 (8K receiver), A884 (cable).
            // Claims only suppress this poll; the UI cache is keyed across polls, so
            // all three transports must also share an identity and display name.
            bool beastX = m.pid == 0xA882 || m.pid == 0xA883 || m.pid == 0xA884;
            s.key = L"wlmouse:" + Hex4(beastX ? 0xA882 : m.pid);
            s.name = beastX ? L"WLMouse Beast X" : m.name;
            s.kind = DeviceKind::Mouse;
            s.via = m.receiver ? L"2.4 GHz receiver" : L"USB cable";
            if (m.receiver) s.graceSec = 1800;  // a sleeping mouse can leave the receiver silent
            if (rep == Reply::Asleep) {
                s.asleep = true;  // shown with the last known value
            } else {
                s.percent = pct;
                s.charging = charging;
            }
            ctx.Add(s, kVid, m.pid);
            if (m.pid == 0xA884) ctx.Claim(kVid, 0xA882);  // alternative Beast X 1K receiver
            if (m.receiverPid) ctx.Claim(kVid, m.receiverPid);  // same mouse, already listed via its cable
            break;
        }
    }
}
