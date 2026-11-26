// Read-only battery queries. AJAZZ: gha-projects/ak820-mac and Twyx22/ak820-center.
// Lofree: official Control HUB /home/assets/index-BTVblIUr.js (v2, ha, oi).
#include "drivers.h"
#include <algorithm>

namespace {
bool SonixQuery(HidDev& d, BYTE command, std::vector<BYTE>& reply) {
    std::vector<BYTE> request(33, 0);
    request[1] = command;
    request[2] = command == 0x20 ? 1 : 0;
    request[32] = request[1] + request[2];
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!d.Write(request)) return false;
        ULONGLONG end = GetTickCount64() + 350;
        while (GetTickCount64() < end) {
            if (d.Read(reply, 25) < 4) continue;
            if (reply[0] == command && reply[1] == request[2] && reply[2] == 0) return true;
        }
    }
    return false;
}

bool FlowQuery(HidDev& d, BYTE command, std::vector<BYTE>& reply,
               std::initializer_list<BYTE> payload = {}) {
    std::vector<BYTE> request(17, 0);
    request[0] = 8; request[1] = command;
    request[5] = 0x80 | (BYTE)payload.size();
    size_t pos = 6;
    for (BYTE b : payload) request[pos++] = b;
    BYTE sum = 0;
    for (int i = 0; i < 16; ++i) sum += request[i];
    request[16] = (BYTE)(0x55 - sum);
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!d.Write(request)) return false;
        ULONGLONG end = GetTickCount64() + 400;
        while (GetTickCount64() < end) {
            if (d.Read(reply, 25) != 17) continue;
            if (reply[0] != 8 || reply[1] != command || reply[2] != 0 || (reply[5] & 0x80) == 0) continue;
            sum = 0;
            for (BYTE b : reply) sum += b;
            if (sum == 0x55) return true;
        }
    }
    return false;
}

// AK980 Pro newer firmware: vendor GET_DEVICE_INFO, never configuration writes.
// Wire evidence: wushuai0802/myfinder-device-battery issue 966 (8039 / FEFE).
bool AK980Info(HidDev& d,int& percent){
 std::vector<BYTE> q(65,0),r;q[1]=0xAA;q[2]=0x10;q[3]=48;q[7]=1;
 if(!d.Write(q))return false;
 ULONGLONG until=GetTickCount64()+700;
 while(GetTickCount64()<until){
  if(d.Read(r,25)<26)continue;
  if(r[0]!=0x55||r[1]!=0x10||r[2]<18||r[2]>48||r[3]!=0||r[4]!=0)continue;
  if(r[12]!=0x45||r[13]!=0x0C||r[14]!=0x39||r[15]!=0x80)continue;
  if((r[24]!=0&&r[24]!=2)||r[25]>100)continue;
  percent=r[25];return true;
 }
 return false;
}

DeviceStatus Keyboard(const wchar_t* name, const std::wstring& key) {
    DeviceStatus s;
    s.name = name; s.key = key; s.kind = DeviceKind::Keyboard;
    s.via = L"2.4 GHz receiver"; s.graceSec = 1800;
    return s;
}
}

void PollKeyboards(PollContext& ctx) {
    // AK980 receivers have two incompatible revisions. Require the matching
    // collection/report size and a valid reply before adding a device.
    for(const auto& c:ctx.colls){
      if(c.vid!=0x0C45||ctx.IsClaimed(c.vid,c.pid))continue;
      bool radio=c.pid==0xFEFE&&c.iface==3&&c.page()==0xFF60&&c.usage()==0x61;
      bool modern=(radio|| (c.pid==0x8039&&c.iface==2&&c.page()==0xFF68&&c.usage()==0x61))&&c.inLen()==65&&c.outLen()==65;
      bool sonix=radio&&c.inLen()==33&&c.outLen()==33;
      if(!modern&&!sonix)continue;
      HidDev d;if(!d.Open(c))continue;d.Flush();int pct=-1;
      if(modern){if(!AK980Info(d,pct))continue;}
      else{std::vector<BYTE> reply;if(!SonixQuery(d,0x20,reply))continue;if(reply[3]>=1&&reply[3]<=100)pct=reply[3];}
      DeviceStatus s;s.name=L"AJAZZ AK980";s.key=L"keyboard:0C45:FEFE";s.kind=DeviceKind::Keyboard;s.graceSec=1800;s.via=radio?L"2.4 GHz receiver":L"USB cable";s.percent=pct;
      auto previous=std::find_if(ctx.out.begin(),ctx.out.end(),[&](const DeviceStatus& item){return item.key==s.key;});
      if(previous==ctx.out.end())ctx.Add(s,c.vid,c.pid);
      else{if(previous->percent<0||radio)*previous=s;ctx.Claim(c.vid,c.pid);}
    }
    bool wiredAjazz = false;
    // A successful wired read takes precedence over the paired receiver.
    for (const auto& c : ctx.colls) {
        if (c.vid != 0x0C45 || (c.pid != 0x8009 && c.pid != 0x800A) ||
            c.page() != 0xFF13 || c.featLen() != 65 || ctx.IsClaimed(c.vid, c.pid)) continue;
        auto name=HidProduct(c);for(auto& ch:name)ch=towlower(ch);
        if(name.find(L"ak980")!=name.npos){
          // On the older AK980 firmware USB supplies no measured percentage.
          if(std::none_of(ctx.out.begin(),ctx.out.end(),[](const DeviceStatus&s){return s.key==L"keyboard:0C45:FEFE";})){
            auto s=Keyboard(L"AJAZZ AK980",L"keyboard:0C45:FEFE");s.via=L"USB cable";s.levelText=L"USB";ctx.Add(s,c.vid,c.pid);
          }else ctx.Claim(c.vid,c.pid);
          continue;
        }
        HidDev d;
        if (!d.OpenFeatureOnly(c)) continue;
        std::vector<BYTE> request(65, 0), reply;
        request[1] = 0x20; request[2] = 1;
        if (!d.SetFeature(request)) continue;
        Sleep(50);
        if (!d.GetFeature(0, reply) || reply.size() < 5 || reply[1] != 0x20 || reply[2] != 1) continue;
        DeviceStatus s = Keyboard(L"AJAZZ AK820 Pro", L"keyboard:0C45:FDFD");
        s.via = L"USB cable";
        if (reply[4] >= 1 && reply[4] <= 100) s.percent = reply[4];
        else if (reply[4] == 0xFF) s.levelText = L"USB"; // Special wired marker, not a measured 100%.
        ctx.Add(s, c.vid, c.pid);
        wiredAjazz = true;
        break;
    }
    for (const auto& c : ctx.colls) {
        if (ctx.IsClaimed(c.vid, c.pid)) continue;
        const bool ajazz = c.vid == 0x0C45 && c.pid == 0xFDFD && c.iface == 3 &&
                           c.page() == 0xFF60 && c.usage() == 0x61 && c.outLen() == 33 && c.inLen() == 33;
        const bool flow = c.vid == 0x05AC && c.pid == 0x024F &&
                          c.page() == 0xFF02 && c.usage() == 2 && c.outLen() == 17 && c.inLen() == 17;
        if (!ajazz && !flow) continue;
        if (ajazz && wiredAjazz) { ctx.Claim(c.vid, c.pid); continue; }
        HidDev d;
        if (!d.Open(c)) continue;
        d.Flush();
        std::vector<BYTE> r;
        DeviceStatus s;
        if (ajazz) {
            if (!SonixQuery(d, 2, r)) continue;
            s = Keyboard(L"AJAZZ AK820 Pro", L"keyboard:0C45:FDFD");
            if (SonixQuery(d, 0x20, r) && r[3] >= 1 && r[3] <= 100) s.percent = r[3];
            // A zero reply supplies no battery reading; never display it as 0%.
        } else {
            // Identify before online/battery, exactly as the official HUB does.
            if (!FlowQuery(d, 1, r, {0x12,0x34,0x56,0x78,0,0,0,0}) ||
                (r[5] & 0x7F) < 8 || r[10] != 1 || r[11] != 1) continue;
            if (!FlowQuery(d, 3, r) || (r[5] & 0x7F) < 1 || r[6] > 1) continue;
            s = Keyboard(L"Lofree Flow Lite84", L"keyboard:05AC:024F");
            s.connected = r[6] == 1;
            // An idle keyboard may leave the radio link. Omit the fresh reading
            // so the UI keeps the last known value for its existing grace period.
            if (!s.connected) { ctx.Claim(c.vid, c.pid); continue; }
            if (s.connected && FlowQuery(d, 4, r) && (r[5] & 0x7F) >= 4 && r[6] <= 100 && r[7] <= 1) {
                s.percent = r[6]; s.charging = r[7] == 1;
                s.millivolts = (r[8] << 8) | r[9];
            }
        }
        ctx.Add(s, c.vid, c.pid);
    }
}
