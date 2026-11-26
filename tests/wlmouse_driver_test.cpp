// Actual driver with a simulated feature-report transport. No hardware required.
#include "../src/drivers/drivers.h"
#include <cassert>
#include <cstdio>
#include <map>
namespace {
std::map<std::wstring, std::vector<std::vector<BYTE>>> replies;
int writes = 0, reads = 0;
bool writeOk = true;
std::vector<std::wstring> opened;
HidColl Collection(USHORT pid = 0xA882, const wchar_t* path = L"receiver") {
    HidColl c;
    c.vid = 0x36A7; c.pid = pid; c.path = path;
    c.caps.UsagePage = 0xFFFF; c.caps.Usage = 0;
    c.caps.FeatureReportByteLength = 65;
    return c;
}
std::vector<BYTE> Reply(BYTE pct = 64, BYTE charging = 0, BYTE status = 0xA1) {
    return {0, status, 0, 2, 2, 0, 0x83, charging, pct};
}
PollContext Poll(const std::vector<std::vector<BYTE>>& frames) {
    replies = {{L"receiver", frames}};
    PollContext ctx; ctx.colls = {Collection()};
    PollWlmouse(ctx); return ctx;
}
}
bool HidDev::OpenFeatureOnly(const HidColl& c) {
    c_ = c; h_ = reinterpret_cast<HANDLE>(1); opened.push_back(c.path); return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
bool HidDev::SetFeature(const std::vector<BYTE>& req) {
    std::vector<BYTE> expected(65, 0);
    expected[3] = 2; expected[4] = 2; expected[6] = 0x83;
    assert(req == expected); ++writes; return writeOk;
}
bool HidDev::GetFeature(BYTE id, std::vector<BYTE>& out) {
    assert(id == 0); ++reads;
    auto& queue = replies[c_.path];
    if (queue.empty()) return false;
    out = queue.front(); queue.erase(queue.begin()); return true;
}
std::wstring Hex4(USHORT v) {
    wchar_t buf[8]; swprintf(buf, 8, L"%04X", v); return buf;
}
int main() {
    auto ctx = Poll({Reply()});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 64);
    assert(ctx.out[0].kind == DeviceKind::Mouse && !ctx.out[0].charging);
    assert(ctx.out[0].via == L"2.4 GHz receiver" && ctx.out[0].graceSec == 1800);
    assert(ctx.IsClaimed(0x36A7, 0xA882));
    ctx = Poll({Reply(100, 1, 0xA2)});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 100 && ctx.out[0].charging);
    ctx = Poll({Reply(0)});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 0 && !ctx.out[0].asleep);
    ctx = Poll({Reply(0, 0, 0xA0)});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == -1 && ctx.out[0].asleep);
    for (auto bad : {std::vector<BYTE>{}, std::vector<BYTE>{0,0xA1}, Reply(101), Reply(255)}) {
        ctx = Poll({bad}); assert(ctx.out.empty() && ctx.claimed.empty());
    }
    auto wrong = Reply(); wrong[6] = 0x84;
    ctx = Poll({wrong, Reply(37)});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 37);
    auto echo = Reply(0, 0, 0);
    ctx = Poll({echo, Reply(12)});
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 12);
    writes = reads = 0;
    ctx = Poll(std::vector<std::vector<BYTE>>(12, echo));
    assert(ctx.out.empty() && writes == 2 && reads == 12);
    writeOk = false;
    ctx = Poll({Reply()}); assert(ctx.out.empty()); writeOk = true;
    // Ignore unrelated products, non-vendor collections, short reports and claimed devices.
    ctx = {};
    auto unknown = Collection(0xA887); auto standard = Collection(); standard.caps.UsagePage = 1;
    auto shortReport = Collection(); shortReport.caps.FeatureReportByteLength = 9;
    auto other = Collection(); other.vid = 0x1234;
    ctx.colls = {unknown, standard, shortReport, other};
    opened.clear(); PollWlmouse(ctx); assert(opened.empty() && ctx.out.empty());
    ctx.colls = {Collection()}; ctx.Claim(0x36A7, 0xA882);
    PollWlmouse(ctx); assert(opened.empty());
    // Prefer usage 0, even when enumerated last.
    ctx = {};
    auto secondary = Collection(0xA882, L"secondary"); secondary.caps.Usage = 1;
    ctx.colls = {secondary, Collection()}; replies = {{L"receiver", {Reply()}}};
    opened.clear(); PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && opened.size() == 1 && opened[0] == L"receiver");
    // A responding cable claims its receiver to avoid duplicate icons.
    ctx = {}; ctx.colls = {Collection(0xA883), Collection(0xA884, L"cable")};
    replies = {{L"cable", {Reply(87, 1)}}, {L"receiver", {Reply(86)}}};
    PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 87 && ctx.out[0].charging);
    assert(ctx.out[0].via == L"USB cable" && ctx.IsClaimed(0x36A7, 0xA883));
    assert(ctx.IsClaimed(0x36A7, 0xA882));
    // Cache identity must survive plugging in the cable after a wireless poll.
    auto cableKey = ctx.out[0].key;
    auto cableName = ctx.out[0].name;
    auto wireless = Poll({Reply(93)});
    assert(wireless.out[0].key == cableKey);
    assert(wireless.out[0].name == cableName);
    ctx = {}; ctx.colls = {Collection(0xA883)};
    replies = {{L"receiver", {Reply(92)}}}; PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].key == cableKey);
    // With both attached, only the cable is queried, including the 1K variant.
    ctx = {}; ctx.colls = {Collection(), Collection(0xA884, L"cable")};
    replies = {{L"cable", {Reply(87, 1)}}, {L"receiver", {Reply(93)}}};
    opened.clear(); PollWlmouse(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].key == cableKey && ctx.out[0].charging);
    assert(opened.size() == 1 && opened[0] == L"cable");
    puts("WLMouse driver tests passed");
}
