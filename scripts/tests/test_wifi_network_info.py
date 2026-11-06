"""Exercise the actual station/AP address reader against host network doubles."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def actual_reader():
    source = (ROOT / "main/boards/common/wifi_board.cc").read_text(encoding="utf-8")
    start = source.index("WifiBoard::WifiNetworkInfo WifiBoard::GetWifiNetworkInfo() const")
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class WifiNetworkInfoTests(unittest.TestCase):
    def test_live_station_ap_disconnect_and_dhcp_changes(self):
        compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Cần trình biên dịch C++ trên máy để chạy giả lập kết nối Wi-Fi")
        platform = r'''
            #include <cassert>
            #include <cstdint>
            #include <cstdio>
            #include <cstring>
            #include <string>
            constexpr int ESP_OK=0;
            #define IPSTR "%u.%u.%u.%u"
            #define IP2STR(p) unsigned((p)->addr&255),unsigned(((p)->addr>>8)&255),unsigned(((p)->addr>>16)&255),unsigned(((p)->addr>>24)&255)
            struct Address { uint32_t addr=0; };
            struct esp_netif_ip_info_t { Address ip; };
            struct Netif { bool up=true; uint32_t ip=0; bool fail=false; } station,ap;
            bool drop_on_read=false;
            class WifiManager {
            public:
                bool config=false, connected=true;
                static WifiManager& GetInstance(){static WifiManager instance;return instance;}
                bool IsConfigMode(){return config;}
                bool IsConnected(){return connected;}
            };
            Netif* esp_netif_get_handle_from_ifkey(const char* key){
                return std::strcmp(key,"WIFI_STA_DEF")==0?&station:&ap;
            }
            bool esp_netif_is_netif_up(Netif* netif){return netif->up;}
            int esp_netif_get_ip_info(Netif* netif,esp_netif_ip_info_t* out){
                if(netif->fail)return -1;
                out->ip.addr=netif->ip;
                if(drop_on_read)WifiManager::GetInstance().connected=false;
                return ESP_OK;
            }
            class WifiBoard {
            public:
                struct WifiNetworkInfo {bool connected=false,config_mode=false;std::string ip,ap_ip;};
                WifiNetworkInfo GetWifiNetworkInfo() const;
            };
            constexpr uint32_t ipv4(unsigned a,unsigned b,unsigned c,unsigned d){return a|(b<<8)|(c<<16)|(d<<24);}
        '''
        checks = r'''
            int main(){
                WifiBoard board;
                auto& wifi=WifiManager::GetInstance();
                station.ip=ipv4(192,168,1,42);ap.ip=ipv4(192,168,4,1);
                auto info=board.GetWifiNetworkInfo();
                assert(info.connected && info.ip=="192.168.1.42" && info.ap_ip.empty());
                station.ip=ipv4(10,0,0,77);
                assert(board.GetWifiNetworkInfo().ip=="10.0.0.77");
                wifi.config=true;
                info=board.GetWifiNetworkInfo();
                assert(!info.connected && info.ip.empty() && info.ap_ip=="192.168.4.1");
                wifi.config=false;wifi.connected=false;
                info=board.GetWifiNetworkInfo();
                assert(info.ip.empty() && info.ap_ip.empty());
                wifi.connected=true;station.up=false;
                assert(board.GetWifiNetworkInfo().ip.empty());
                station.up=true;station.fail=true;
                assert(board.GetWifiNetworkInfo().ip.empty());
                station.fail=false;station.ip=0;
                assert(board.GetWifiNetworkInfo().ip.empty());
                station.ip=UINT32_MAX;
                assert(board.GetWifiNetworkInfo().ip.empty());
                station.ip=ipv4(192,168,2,88);drop_on_read=true;
                info=board.GetWifiNetworkInfo();
                assert(!info.connected && info.ip.empty());
            }
        '''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "network.cc"
            executable = Path(directory) / ("network.exe" if os.name == "nt" else "network")
            source.write_text(platform + actual_reader() + checks, encoding="utf-8")
            result = subprocess.run([compiler, "-std=c++23", str(source), "-o", str(executable)],
                                    capture_output=True, text=True, encoding="utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
