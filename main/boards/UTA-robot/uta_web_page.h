#pragma once

namespace uta {

inline constexpr char kWebPage[] = R"UTA(<!doctype html>
<html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>UTA-robot — Bảng điều khiển</title><style>
:root{--bg:#e8edf0;--sf:#fff;--tx:#10212b;--mu:#66767f;--ln:#d3dbe0;--ac:#10212b;--at:#fff;--bad:#d62839;--c1:#3b82f6;--c2:#f59e0b;--c3:#8b5cf6;--c4:#10b981;--c5:#f43f5e;--c6:#14b8a6;--c7:#64748b;box-sizing:border-box;padding-top:env(safe-area-inset-top,0px);padding-bottom:env(safe-area-inset-bottom,0px)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#0d161b;--sf:#15222a;--tx:#e6eef2;--mu:#8a9ba5;--ln:#26373f;--ac:#e6eef2;--at:#0d161b}}
:root[data-theme="dark"]{--bg:#0d161b;--sf:#15222a;--tx:#e6eef2;--mu:#8a9ba5;--ln:#26373f;--ac:#e6eef2;--at:#0d161b}
html{scroll-padding-top:env(safe-area-inset-top,0px)}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:14px/1.45 'Bricolage Grotesque',system-ui,sans-serif}
.wrap{max-width:1160px;margin:auto;padding:20px 16px 40px}
header{display:flex;flex-wrap:wrap;gap:12px;align-items:flex-end;justify-content:space-between;margin-bottom:16px}
h1{margin:0;font-size:28px;line-height:1.1;letter-spacing:-.02em}
h1 small{display:block;font:400 13px 'JetBrains Mono',monospace;color:var(--mu);margin-top:6px;letter-spacing:0}
.stats{display:flex;gap:8px;flex-wrap:wrap}
.st{background:var(--sf);border:1px solid var(--ln);border-radius:10px;padding:6px 12px}
.st b{font-size:18px;margin-right:6px}.st.bad{border-color:var(--bad);color:var(--bad)}
.grid{display:grid;grid-template-columns:minmax(0,1fr) 380px;gap:16px;align-items:start}
@media(max-width:920px){.grid{grid-template-columns:1fr}}
.tabs{display:flex;flex-wrap:wrap;gap:6px;padding-bottom:4px}
.tab{flex:none;display:flex;align-items:center;gap:7px;font:inherit;font-weight:500;color:var(--tx);background:var(--sf);border:1px solid var(--ln);border-radius:999px;padding:7px 14px;cursor:pointer}
.tab i{width:9px;height:9px;border-radius:50%;background:var(--g)}
.tab b{background:var(--g);color:#fff;border-radius:999px;font-size:11px;padding:0 6px}
.tab[hidden]{display:none}.tab.on{background:var(--ac);color:var(--at);border-color:var(--ac)}
.cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(250px,1fr));gap:10px;margin-top:12px}
.card{background:var(--sf);border:1px solid var(--ln);border-left:4px solid var(--g);border-radius:12px;padding:12px}
.hd{position:relative;display:flex;align-items:center;gap:10px;cursor:pointer;font-weight:700}
.hd input{position:absolute;opacity:0}
.sw{width:34px;height:20px;border-radius:99px;background:var(--ln);position:relative;flex:none;transition:.15s}
.sw:after{content:"";position:absolute;top:2px;left:2px;width:16px;height:16px;border-radius:50%;background:#fff;transition:.15s}
.hd input:checked+.sw{background:var(--g)}.hd input:checked+.sw:after{left:16px}
.hd input:focus-visible+.sw{outline:2px solid var(--tx);outline-offset:2px}
.nt{margin:6px 0 2px;color:var(--mu);font-size:12px}
.row{display:grid;grid-template-columns:92px 1fr;gap:8px;align-items:center;margin-top:6px}
.row span{font-size:12.5px}.row em{color:var(--mu);font-style:normal;font-size:11px}
select{width:100%;font:12.5px 'JetBrains Mono',monospace;padding:6px 8px;border-radius:8px;border:1px solid var(--ln);background:var(--bg);color:var(--tx);min-width:0}
select.type{margin-top:8px;font-family:inherit;font-weight:500}
select.bad{border-color:var(--bad);box-shadow:0 0 0 2px color-mix(in srgb,var(--bad) 25%,transparent)}
.side{position:sticky;top:12px;display:flex;flex-direction:column;gap:12px}
.pn{background:var(--sf);border:1px solid var(--ln);border-radius:14px;padding:12px}
.pn h2{margin:0 0 8px;font-size:15px;display:flex;justify-content:space-between;align-items:center;gap:8px}
.map{display:grid;grid-template-columns:1fr 1fr;gap:4px 12px;position:relative}
.map:before{content:"ESP32-S3";position:absolute;left:50%;top:50%;transform:translate(-50%,-50%) rotate(-90deg);font:600 22px 'JetBrains Mono',monospace;color:var(--ln);white-space:nowrap;pointer-events:none}
.pin{display:flex;align-items:center;gap:5px;font:11px 'JetBrains Mono',monospace;padding:2px 6px;border-radius:6px;border:1px solid var(--ln);min-height:24px;min-width:0;background:var(--sf);position:relative}
.pin i{width:8px;height:8px;border-radius:50%;background:var(--ln);flex:none}
.pin b{font-weight:600}.pin span{color:var(--mu);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pin.on{border-color:var(--g)}.pin.on i{background:var(--g)}.pin.on span{color:var(--tx)}
.pin.bad{border-color:var(--bad);background:color-mix(in srgb,var(--bad) 12%,var(--sf))}
.pin.pw,.pin.rs{opacity:.5}.pin.rs{background:repeating-linear-gradient(135deg,transparent 0 4px,var(--ln) 4px 5px)}
.note{font-size:11.5px;color:var(--mu);margin:8px 0 0}
#cf{color:var(--bad);font-size:12px;margin-top:6px}
.bt{font:inherit;font-weight:500;font-size:12.5px;border:1px solid var(--ln);background:var(--sf);color:var(--tx);border-radius:8px;padding:5px 10px;cursor:pointer}
.bt.p{background:var(--ac);color:var(--at);border-color:var(--ac)}
.acts{display:flex;gap:6px;flex-wrap:wrap}
pre{margin:0;font:11.5px/1.5 'JetBrains Mono',monospace;max-height:220px;overflow:auto;background:var(--bg);padding:10px;border-radius:10px}
.imp{display:none;margin-bottom:8px}.imp.on{display:block}.imp .bt{margin-top:6px}
textarea{width:100%;height:110px;resize:vertical;font:11.5px 'JetBrains Mono',monospace;background:var(--bg);color:var(--tx);border:1px solid var(--ln);border-radius:10px;padding:8px}
#ms{font-size:12px;margin:0 0 8px;color:var(--mu)}#ms:empty{display:none}
.ct{font-weight:700}.wide{grid-column:1/-1}input[type=range]{width:100%;accent-color:var(--g)}
.dpad{display:grid;grid-template-columns:repeat(3,48px);gap:6px;margin:10px 0}
.dpad button{height:48px;font-size:18px;border-radius:12px;border:1px solid var(--ln);background:var(--bg);color:var(--tx);cursor:pointer;touch-action:none;user-select:none}
.dpad button:active{background:var(--g);color:#fff;border-color:var(--g)}
.dial{display:flex;align-items:center;gap:10px;margin:4px 0}.dial svg{width:90px}.dial b{font:600 18px 'JetBrains Mono',monospace}
.card .acts{margin-top:8px}
:focus-visible{outline:2px solid var(--c1);outline-offset:2px}
body{color-scheme:light dark}
.grid>*,.cards>*{min-width:0}.map>div{display:grid;gap:4px;align-content:start}
.cards[hidden],.card[hidden],.pin-fields[hidden],.row[hidden]{display:none}.pin-fields{margin-top:8px}
.dial .arc{fill:none;stroke:var(--ln);stroke-width:4}.dial .needle{stroke:var(--g);stroke-width:4;stroke-linecap:round}
section{min-width:0}.tools{margin-top:18px}.tools>section,.settings{margin:10px 0}
.tools>.cards{margin-top:0}.tools .cards section{margin:0}
label:not(.row):not(.hd){display:block;margin:8px 0 4px}input:not([type=checkbox]):not([type=range]){width:100%;padding:7px;border:1px solid var(--ln);border-radius:8px;background:var(--bg);color:var(--tx)}
button:not(.tab):not(.bt){font:inherit;background:var(--sf);color:var(--tx);border:1px solid var(--ln);border-radius:8px;padding:6px 10px;cursor:pointer}
button:disabled{opacity:.55;cursor:default}.hint{color:var(--mu);font-size:12px}
h2{font-size:16px;margin:0 0 8px}h3{font-size:14px;margin:0 0 4px}
#thongbao{margin:8px 0;color:var(--mu);white-space:pre-wrap}#thongbao.error{color:var(--bad)}
fieldset{margin:0;padding:0;border:0;min-width:0}.section-label{color:var(--mu);font:600 11px 'JetBrains Mono',monospace;margin:0 0 8px}
.config-actions{margin-top:14px}.config-actions button{min-height:34px}
.card[hidden],section[hidden]{display:none}
@media(max-width:920px){.side{position:static}.grid{grid-template-columns:minmax(0,1fr)}}
</style></head><body><div class="wrap">
<header><h1>Cấu hình GPIO<small>ESP32-S3-N16R8 · UTA-robot</small></h1>
<div class="stats"><div class="st"><b id="dagan">0</b>đã gán</div><div class="st"><b id="controng">0</b>trống</div><div class="st bad" id="xungdot" hidden><b id="soxungdot">0</b>xung đột</div></div></header>
<p id="thongbao" role="status" aria-live="polite">Đang đọc trạng thái thiết bị…</p>
<form id="caidat"><div class="grid"><main>
<nav class="tabs" aria-label="Nhóm thiết bị" id="nhomchan"></nav>
<div hidden><div id="xoay-chinh"><label for="gocmanhinh">Xoay màn hình</label><select id="gocmanhinh"><option value="0">0°</option><option value="90">90°</option><option value="180">180°</option><option value="270">270°</option></select><button type="button" id="luugoc" disabled>Lưu góc xoay</button><p class="hint" id="gocdangdung"></p></div></div><div class="cards" id="bangchan"></div><div class="cards" id="the-dieukhien" hidden><p class="nt wide" id="dieukhien-tat" hidden>Bật driver động cơ, UART hoặc servo trong tab cấu hình tương ứng, lưu rồi khởi động lại để điều khiển.</p><section class="card" id="dieukhienrobot" style="--g:var(--c4)" hidden><h2>Di chuyển</h2><p class="hint">Giữ nút để chạy, thả để dừng. Robot tự dừng khi mất lệnh điều khiển.</p>
<div class="dpad" style="--g:var(--c4)"><span></span><button type="button" data-huong="tien" aria-label="Tiến">▲</button><span></span><button type="button" data-huong="trai" aria-label="Trái">◀</button><button type="button" id="dungrobot" class="danger" aria-label="Dừng">■</button><button type="button" data-huong="phai" aria-label="Phải">▶</button><span></span><button type="button" data-huong="lui" aria-label="Lùi">▼</button><span></span></div>
</section><section class="card" id="dieukhienservo" style="--g:var(--c5)" hidden><h2>Đầu và cổ</h2><div id="testdau"><label for="gocdau">Góc đầu</label><div class="dial" aria-hidden="true"><svg viewBox="0 0 90 48"><path class="arc" d="M5 44a40 40 0 0 1 80 0"/><path class="needle" id="kim-gocdau" d="M45 44h-33"/></svg><b id="so-gocdau">90°</b></div><input id="gocdau" type="range" min="15" max="165" value="90"><button type="button" id="daugiua">Đầu về giữa</button></div><div id="testco"><label for="gocco">Góc cổ</label><div class="dial" aria-hidden="true"><svg viewBox="0 0 90 48"><path class="arc" d="M5 44a40 40 0 0 1 80 0"/><path class="needle" id="kim-gocco" d="M45 44h-33"/></svg><b id="so-gocco">90°</b></div><input id="gocco" type="range" min="15" max="165" value="90"><button type="button" id="cogiua">Cổ về giữa</button></div></section><section class="card" id="dieukhiencob" style="--g:var(--c2)" hidden><h2>Hiệu ứng WS2812B · 8 LED</h2><label for="cobhieung">Hiệu ứng</label><select id="cobhieung"><option value="rainbow">Cầu vồng từng LED</option><option value="cycle">Đổi màu liên tục</option><option value="chase">Điểm sáng chạy</option><option value="strobe">Chớp đổi màu</option><option value="pulse">Thở màu</option><option value="off">Tắt hiệu ứng</option></select><button type="button" id="apdungcob">Áp dụng hiệu ứng</button><p class="hint">Khi quẩy, hiệu ứng đèn chạy đồng thời với chuyển động đầu và cổ.</p></section></div><div hidden><div id="phukienuart"><button type="button" id="nhay">Nhảy múa</button><label for="den">Hiệu ứng đèn</label><select id="den"><option value="1">Sáng thường</option><option value="2">Nhấp nháy</option><option value="3">Sáng dịu chậm</option><option value="4">Sáng dịu nhanh</option><option value="5">Chuyển màu</option><option value="6">Trình diễn</option></select><button type="button" id="doiden">Đổi hiệu ứng</button></div><div id="opt-camung"><label class="hd"><input id="camung" type="checkbox" style="width:auto"><span class="sw" aria-hidden="true"></span>Bật cảm ứng XPT2046 đã đấu dây</label></div><div id="opt-robotuart"><label class="hd"><input id="robotuart" type="checkbox" style="width:auto"><span class="sw" aria-hidden="true"></span>Bật bộ điều khiển robot qua UART tương thích lệnh x/y, d, w</label></div><div id="opt-driverdc"><label for="driverdc">Driver cho hai động cơ N20</label><select id="driverdc"><option value="0">Tắt động cơ trực tiếp</option><option value="1">L298N mini</option><option value="2">DRV8833</option></select></div><div id="opt-tocdodc"><label for="tocdodc">Tốc độ động cơ (%)</label><input id="tocdodc" type="number" min="1" max="100" value="60"></div><div id="opt-daotrai"><label class="hd"><input id="daotrai" type="checkbox" style="width:auto"><span class="sw" aria-hidden="true"></span>Đảo chiều động cơ trái</label></div><div id="opt-daophai"><label class="hd"><input id="daophai" type="checkbox" style="width:auto"><span class="sw" aria-hidden="true"></span>Đảo chiều động cơ phải</label></div><div id="opt-batservo"><label class="hd"><input id="batservo" type="checkbox" style="width:auto"><span class="sw" aria-hidden="true"></span>Bật servo đầu</label></div>

<div id="opt-min0"><label>Đầu · góc nhỏ nhất</label><input id="min0" type="number" min="0" max="180" value="15"></div><div id="opt-max0"><label for="max0">Đầu · góc lớn nhất</label><input id="max0" type="number" min="0" max="180" value="165"></div><div id="opt-nha0"><label for="nha0">Đầu · góc giữa</label><input id="nha0" type="number" min="0" max="180" value="90"></div><div id="opt-batcose"><input id="batcose" type="checkbox"></div><div id="opt-min1"><label>Cổ · góc nhỏ nhất</label><input id="min1" type="number" min="0" max="180" value="15"></div><div id="opt-max1"><label for="max1">Cổ · góc lớn nhất</label><input id="max1" type="number" min="0" max="180" value="165"></div><div id="opt-nha1"><label for="nha1">Cổ · góc giữa</label><input id="nha1" type="number" min="0" max="180" value="90"></div>
</div>
<div class="acts config-actions"><button class="bt p" id="luugpio" disabled type="submit">Lưu cài đặt</button><button class="bt" id="khoidonglai" type="button">Khởi động lại</button></div>
<p class="hint">Đấu dây theo cấu hình mới trước khi khởi động lại. Chân bộ nhớ bị khóa. Dấu ! báo chân có chức năng đặc biệt.</p>
</main><aside class="side"><section class="pn"><h2>Sơ đồ chân <span class="acts"><button class="bt" id="docgoc" type="button">Đọc cấu hình đã lưu</button></span></h2>
<div class="map" id="sodochan"></div><p class="note">Chân sọc bị khóa. Dấu ! báo chân đặc biệt. Gán GPIO 0 sẽ dành chân này cho thiết bị đã chọn thay cho nút BOOT trong firmware.</p>
<p id="cf" aria-live="polite" hidden>Chọn chân riêng cho mỗi chức năng.</p></section>
<section class="pn"><h2>Mã xuất ra <span class="acts"><button class="bt" id="chonfile" type="button">Nhập file</button><button class="bt" id="danma" type="button">Dán mã</button><button class="bt" id="saochan" type="button">Sao chép</button></span></h2>
<input type="file" id="filegpio" accept=".h,.hpp,.ino,.txt,text/plain" hidden>
<div class="imp" id="nhapma"><textarea id="noidungma" aria-label="Mã cần nhập" placeholder="Dán mã GPIO đã xuất từ UTA-robot"></textarea><button class="bt p" id="apdungma" type="button">Áp dụng</button></div>
<p id="ms" role="status"></p><pre id="machan" tabindex="0"></pre></section>
</aside></div>
<details class="pn settings"><summary>Cài đặt âm thanh và truy cập</summary><label for="maychu">URL máy chủ nhạc có thể thay đổi</label><input id="maychu" type="url" maxlength="512" placeholder="https://may-chu-cua-ban"><p class="hint">Máy chủ cần hỗ trợ tìm bài theo giao thức của repo IoT Việt Nam. Có thể thay máy chủ khác cùng giao thức, địa chỉ được lưu sau khi tắt nguồn. Không cần nạp lại firmware.</p>
<label for="khoanhac">Khóa máy chủ nhạc</label><input id="khoanhac" type="password" maxlength="128" autocomplete="off" placeholder="Để trống để giữ khóa hiện tại"><label for="tendai">Tên đài tùy chỉnh</label><input id="tendai" maxlength="120"><label for="urldai">Địa chỉ luồng đài tùy chỉnh</label><input id="urldai" type="url" maxlength="512">
<button class="bt p" type="submit">Lưu cài đặt</button></details></form>
<details class="pn settings"><summary>Giải trí và thiết bị</summary><div class="cards">
<section class="pn"><h2>Nhạc trực tuyến</h2><label for="baihat">Tên bài hát</label><input id="baihat" maxlength="160" placeholder="Nhập tên bài hát"><label for="casi">Ca sĩ</label><input id="casi" maxlength="160" placeholder="Có thể để trống"><button id="phatnhac">Tìm và phát</button>
<p id="trangthainhac">Đã dừng</p><button id="tamdung">Tạm dừng</button><button id="tieptuc">Tiếp tục</button><button id="dungnhac">Dừng nhạc</button><label for="amluong">Âm lượng</label><input id="amluong" type="range" min="0" max="100" value="60"><label for="dosang">Độ sáng màn hình</label><input id="dosang" type="range" min="5" max="100" value="80">
</section>

<section class="pn"><h2>Phát video MP4</h2><label for="urlvideo">Địa chỉ tệp MP4 trực tiếp</label><input id="urlvideo" type="url" maxlength="2048" placeholder="https://may-chu/video.mp4"><p class="hint">Phát hình và tiếng trên thiết bị. Dùng H.264 Baseline hoặc MJPEG, tối đa 240×240, 15 hình/giây; âm thanh AAC-LC hoặc MP3. Máy chủ cần hỗ trợ tải từng phần. Không dùng liên kết trang xem video.</p><button id="phatvideo">Phát video trên thiết bị</button><p class="hint">Dùng các nút tạm dừng, tiếp tục và dừng ở phần nhạc. Phổ nhạc hiển thị trên màn hình khi nghe nhạc hoặc radio.</p></section>
<section class="pn"><h2>Đài phát thanh Internet</h2><p class="hint">Các đài cần kết nối mạng. Đây không phải bộ thu sóng FM bằng anten.</p><label for="dai">Chọn đài</label><select id="dai"></select><button id="phatdai">Phát đài đã chọn</button><button id="phattuychinh">Phát đài tùy chỉnh đã lưu</button></section>
<section class="pn"><h2>Thiết bị</h2><button id="cauhinhwifi">Cấu hình lại Wi-Fi</button><p class="hint">Mở trực tiếp trang cấu hình, không cần đăng nhập. Nhấn BOOT ba lần để hiện lại địa chỉ trên màn hình.</p></section>
</div></details></div><script>
let duLieu=null,giuNut=null,dangGui=false;const el=id=>document.getElementById(id);
const nhom=[
    {ten:'Audio',mau:1,the:[['Micro','mic_','INMP441'],['Khuếch đại âm thanh','spk_','MAX98357A']]},
    {ten:'Đèn COB',mau:2,the:[['Hiệu ứng đèn qua UART','light','Bộ điều khiển đế']]},
    {ten:'Màn hình',mau:3,the:[['Màn hình chính','d_','ST7789'],['Màn hình phụ','secondary']]},
    {ten:'Động cơ',mau:4,the:[['Driver động cơ','dc_'],['Bộ điều khiển UART','robot_','UART']]},
    {ten:'Servo',mau:5,the:[['Đầu · 180°','servo_dau','PWM 50 Hz'],['Cổ · 180°','servo_co','PWM 50 Hz']]},
    {ten:'Cảm biến',mau:6,the:[['Cảm ứng','cam_ung_','XPT2046']]},
    {ten:'Điều khiển',mau:7,the:[]}
];
const tuyChonThe={
    dc_:['driverdc','tocdodc','daotrai','daophai'],
    servo_dau:['batservo','min0','max0','nha0'],
    servo_co:['batcose','min1','max1','nha1'],
    cam_ung_:['camung'],robot_:['robotuart']
};
const kyHieu={
    d_mosi:'SDA',d_clk:'SCL',d_dc:'DC',d_cs:'CS',d_blk:'BL',d_rst:'RST',
    cam_ung_miso:'T_DO',cam_ung_cs:'T_CS',cam_ung_irq:'T_IRQ',
    robot_tx:'TX',robot_rx:'RX',mic_ws:'WS',mic_sck:'SCK',mic_din:'SD',
    spk_dout:'DIN',spk_bclk:'BCLK',spk_lrck:'LRC',
    dc_trai_1:'IN1',dc_trai_2:'IN2',dc_phai_1:'IN3',dc_phai_2:'IN4',
    servo_dau:'PWM',servo_co:'PWM'
};
const drv8833={dc_trai_1:'AIN1',dc_trai_2:'AIN2',dc_phai_1:'BIN1',dc_phai_2:'BIN2'};
const trai=['3V3','3V3','RST',4,5,6,7,15,16,17,18,8,3,46,9,10,11,12,13,14,'5V','GND'];
const phai=['GND',43,44,1,2,42,41,40,39,38,37,36,35,0,45,48,47,21,20,19,'GND','GND'];
const cacChan=Array.from({length:49},(_,i)=>i).filter(i=>i<=21||i>=26);
const hopLe=i=>(i>=0&&i<=21)||(i>=38&&i<=48);
const canhBaoChan={0:'Strapping / nút BOOT',3:'Strapping',45:'Strapping',46:'Strapping',
    19:'USB D−',20:'USB D+',43:'UART0 TX',44:'UART0 RX',48:'LED RGB trên board'};
const oChonChan=[];
function themChan(select,ten,dangDung=()=>true){
    oChonChan.push({select,ten,dangDung});
    cacChan.filter(hopLe).forEach(pin=>select.add(new Option('GPIO '+pin,String(pin))));
    select.onchange=veSoDo;
}
const nut=(tag,lop,chu)=>{const node=document.createElement(tag);node.className=lop;if(chu)node.textContent=chu;return node};
const lcdForms={};
const lcdDrivers=[
    ['ST7789','SPI',240,[240,320]],['ILI9341','SPI',240,[320]],
    ['GC9A01','SPI',240,[240]],['ST7735','SPI',128,[160]],
    ['SSD1306','I²C',128,[32,64]],['SH1106','I²C',128,[64]]
];
const lcdPinKeys=['data','clock','cs','dc','rst','bl'];
function lcdForm(card,secondary,primaryPins,title,refreshBadge){
    const key=secondary?'man_phu':'man_chinh';
    const saved=duLieu[key]||{driver:secondary?0:1,width:240,height:240,pins:Array(6).fill(-1)};
    const driver=nut('select','type');driver.setAttribute('aria-label','Driver '+(secondary?'màn phụ':'màn chính'));
    const heading=nut('label','hd'),toggle=nut('input','');toggle.type='checkbox';
    toggle.checked=saved.driver!==0;
    heading.append(toggle,nut('span','sw'),nut('span','',title.textContent));
    title.replaceChildren(heading);
    lcdDrivers.forEach(([name,bus],i)=>driver.add(new Option(name+' · '+bus,String(i+1))));
    driver.value=String(saved.driver||1);
    const fields=nut('div','pin-fields');
    const size=nut('select','');
    const sizeRow=nut('label','row');
    sizeRow.append(nut('span','','Kích thước'),size);fields.append(sizeRow);
    const pins={},pinRows=[];
    const rotation=secondary?null:el('xoay-chinh');
    if(rotation)fields.append(rotation);
    const note=nut('p','nt');fields.append(note);
    if(secondary){
        for(const [i,name] of ['SDA / MOSI','SCL / SCK','CS','DC','RST','BL'].entries()){
            const row=nut('label','row'),select=nut('select','');
            select.add(new Option(i<2?'Dùng chung':i<4?'Chọn GPIO':'Không nối','-1'));
            themChan(select,'Màn phụ · '+name,()=>{
                const selected=Number(driver.value);
                return toggle.checked&&(i<2||lcdDrivers[selected-1]?.[1]!=='I²C');
            });
            select.value=String(saved.pins[i]);pins[lcdPinKeys[i]]=select;
            row.append(nut('span','',name),select);pinRows.push(row);fields.append(row);
        }
    }
    const update=()=>{
        const selected=Number(driver.value);
        const [,bus,width,heights]=lcdDrivers[selected-1]||lcdDrivers[0];
        const oled=bus==='I²C';
        const height=Number(size.value)||saved.height;
        note.textContent=oled?'OLED I²C · địa chỉ 0x3C':secondary
            ?'Dùng chung SDA/SCL chỉ khi màn chính cũng chạy SPI. CS và DC phải riêng.'
            :'SPI · SDA = MOSI, SCL = SCK';
        if(secondary){
            pinRows.forEach((row,i)=>{
                row.hidden=oled&&i>=2;
                if(i<2){
                    row.children[0].textContent=oled?['SDA','SCL'][i]:['SDA / MOSI','SCL / SCK'][i];
                    row.children[1].options[0].text=oled?'Chọn GPIO':'Dùng chung';
                }
            });
        }else{
            [...primaryPins.children].forEach(row=>{
                const pin=row.children[1].dataset.chan;
                row.hidden=!toggle.checked||(oled&&!['d_mosi','d_clk'].includes(pin));
            });
        }
        size.replaceChildren();
        heights.forEach(h=>size.add(new Option(width+' × '+h+' px',String(h))));
        size.value=String(heights.includes(height)?height:heights[0]);
        fields.hidden=!toggle.checked;driver.hidden=!toggle.checked;
        if(rotation)rotation.hidden=selected<1||selected>3;
        card.dataset.active=String(toggle.checked);
    };
    driver.onchange=toggle.onchange=()=>{update();refreshBadge();veSoDo()};update();card.append(driver,fields);
    lcdForms[key]=()=>({driver:toggle.checked?Number(driver.value):0,width:lcdDrivers[Number(driver.value)-1]?.[2]||240,height:Number(size.value),
        ...(secondary?{pins:Object.fromEntries(Object.entries(pins).map(([k,v])=>[k,Number(v.value)]))}:{})});
}
const extraServoKeys=['banh_trai','banh_phai','tay_trai','tay_phai'];
const extraServoForms=[];
function extraServoCards(index,updateControls){
    const saved=duLieu.servo_mo_rong||[-1,-1,-1,-1];
    const active=duLieu.servo_mo_rong_dang_dung||[-1,-1,-1,-1];
    const cards=[];
    for(const [title,axes] of [['Hai bánh · servo 360°',[0,1]],['Tay trái · 180°',[2]],['Tay phải · 180°',[3]]]){
        const card=nut('div','card');card.dataset.nhom=String(index);card.style.setProperty('--g','var(--c5)');
        const heading=nut('label','hd'),toggle=nut('input','');toggle.type='checkbox';
        toggle.checked=axes.every(axis=>saved[axis]>=0);
        heading.append(toggle,nut('span','sw'),nut('span','',title));
        const fields=nut('div','pin-fields');
        const test=nut('section','card');test.style.setProperty('--g','var(--c5)');
        test.append(nut('h3','',title));
        const ready=axes.every(axis=>active[axis]>=0);
        if(!ready)test.append(nut('p','nt','Lưu cấu hình và khởi động lại để thử servo.'));
        for(const axis of axes){
            const name=axis===0?'PWM trái':axis===1?'PWM phải':'PWM';
            const row=nut('label','row'),select=nut('select','');
            select.add(new Option('Chọn GPIO','-1'));
            themChan(select,title+' · '+name,()=>toggle.checked);
            select.value=String(saved[axis]);
            row.append(nut('span','',name),select);fields.append(row);
            extraServoForms[axis]={toggle,select};
            if(!ready)continue;
            const send=value=>lenh('servo_mo_rong',{truc:axis,gia_tri:value});
            if(axis<2){
                const buttons=nut('div','acts');
                for(const [label,value] of [['Lùi',-50],['Dừng',0],['Tiến',50]]){
                    const button=nut('button','bt',label);button.type='button';button.onclick=()=>send(value);
                    buttons.append(button);
                }
                test.append(nut('p','nt',axis===0?'Bánh trái':'Bánh phải'),buttons);
            }else{
                const control=nut('label',''),value=nut('span','','90°'),slider=nut('input','');
                slider.type='range';slider.min=0;slider.max=180;slider.value=90;
                slider.oninput=()=>value.textContent=slider.value+'°';
                slider.onchange=()=>send(Number(slider.value));
                control.append(nut('span','','Góc tay · '),value,slider);test.append(control);
            }
        }
        if(ready&&axes.length===2)test.append(nut('p','nt','Mỗi lần bấm quay tối đa 0,4 giây.'));
        const sync=()=>{
            fields.hidden=!toggle.checked;
            test.hidden=!toggle.checked;
            card.dataset.active=String(toggle.checked);
        };
        toggle.onchange=()=>{sync();updateControls();veSoDo()};
        card.append(heading,fields);el('the-dieukhien').append(test);sync();cards.push(card);
    }
    return cards;
}
function extraServoConfig(){
    return Object.fromEntries(extraServoKeys.map((key,axis)=>{
        const {toggle,select}=extraServoForms[axis];
        if(toggle.checked&&Number(select.value)<0)throw Error('Chọn đủ GPIO cho các servo đã bật.');
        return [key,toggle.checked?Number(select.value):-1];
    }));
}
const rangeKeys=['tof_sda','tof_scl','tof_xshut','trig','echo'];
const rangeForms=[];
const rangeReadouts=[];
function rangeCards(index,updateControls){
    const saved=duLieu.chan_cam_bien||Array(rangeKeys.length).fill(-1);
    return [
        ['ToF · VL53L0X',['SDA','SCL','XSHUT'],0,'Có thể dùng chung cặp SDA/SCL với OLED. XSHUT tùy chọn.','tof'],
        ['HC-SR04',['TRIG','ECHO'],3,'ECHO 5 V cần chia áp hoặc chuyển mức xuống 3,3 V.','sieu_am']
    ].map(([title,labels,start,tip,key])=>{
        const card=nut('div','card');card.dataset.nhom=String(index);card.style.setProperty('--g','var(--c6)');
        const heading=nut('label','hd'),toggle=nut('input','');toggle.type='checkbox';
        toggle.checked=saved[start]>=0;
        heading.append(toggle,nut('span','sw'),nut('span','',title));
        const fields=nut('div','pin-fields');
        fields.append(nut('p','nt',tip));
        const readout=nut('section','card');readout.style.setProperty('--g','var(--c6)');
        const distance=nut('p','nt');
        readout.append(nut('h3','',title),distance);
        rangeReadouts.push({key,distance});
        for(const [offset,label] of labels.entries()){
            const axis=start+offset,optional=axis===2;
            const row=nut('label','row'),select=nut('select','');
            select.add(new Option(optional?'Không nối':'Chọn GPIO','-1'));
            themChan(select,title+' · '+label,()=>toggle.checked);
            select.value=String(saved[axis]);
            row.append(nut('span','',label),select);fields.append(row);
            rangeForms[axis]={toggle,select,optional};
        }
        const sync=()=>{
            fields.hidden=!toggle.checked;readout.hidden=!toggle.checked;
            card.dataset.active=String(toggle.checked);
        };
        toggle.onchange=()=>{sync();updateControls();veSoDo()};
        card.append(heading,fields);el('the-dieukhien').append(readout);sync();
        return card;
    });
}
function rangeConfig(){
    return Object.fromEntries(rangeKeys.map((key,index)=>{
        const {toggle,select,optional}=rangeForms[index];
        if(toggle.checked&&!optional&&Number(select.value)<0)throw Error('Chọn đủ chân cho cảm biến đã bật.');
        return [key,toggle.checked?Number(select.value):-1];
    }));
}
function updateDistances(data){
    for(const {key,distance} of rangeReadouts){
        const mm=data[key+'_mm'];
        distance.textContent=!data[key+'_dang_dung']?'Lưu và khởi động lại để xem khoảng cách.'
            :Number.isFinite(mm)&&mm>=0?(mm/10).toFixed(1)+' cm':'Chưa có số đo hợp lệ.';
    }
}
const cobKeys=['r','g','b','edison','din'];
const cobForms=[];
let cobAnode;
function cobCards(index,updateControls){
    const saved=duLieu.chan_den_cob||Array(cobKeys.length).fill(-1);
    const ready=duLieu.den_cob_dang_dung||[];
    return [
        ['Đèn RGB',['R','G','B'],0],
        ['Đèn Edison',['PWM'],3],
        ['Đèn WS2812B · 8 LED',['DIN'],4]
    ].map(([title,labels,start],light)=>{
        const card=nut('div','card');card.dataset.nhom=String(index);card.style.setProperty('--g','var(--c2)');
        const heading=nut('label','hd'),toggle=nut('input','');toggle.type='checkbox';
        toggle.checked=saved[start]>=0;
        heading.append(toggle,nut('span','sw'),nut('span','',title));
        const fields=nut('div','pin-fields');
        if(light===0){
            const row=nut('label','row');cobAnode=nut('select','');
            cobAnode.add(new Option('Cathode chung','0'));cobAnode.add(new Option('Anode chung','1'));
            cobAnode.value=duLieu.rgb_anode?'1':'0';
            row.append(nut('span','','Loại RGB'),cobAnode);fields.append(row);
        }
        for(const [offset,label] of labels.entries()){
            const axis=start+offset,row=nut('label','row'),select=nut('select','');
            select.add(new Option('Chọn GPIO','-1'));
            themChan(select,title+' · '+label,()=>toggle.checked);
            select.value=String(saved[axis]);cobForms[axis]={toggle,select};
            row.append(nut('span','',label),select);fields.append(row);
        }
        const test=nut('section','card');test.style.setProperty('--g','var(--c2)');
        test.append(nut('h3','',title));
        if(ready[light]){
            const row=nut('label',''),input=nut('input',''),buttons=nut('div','acts');
            const edison=light===1;
            input.type=edison?'range':'color';
            if(edison){input.min=0;input.max=100;input.step=1}
            input.value=edison?'25':'#202020';
            row.append(nut('span','',edison?'Độ sáng (%)':'Màu đèn'),input);
            const apply=nut('button','bt','Thử đèn'),off=nut('button','bt','Tắt đèn');
            apply.type=off.type='button';
            apply.onclick=()=>lenh('den_cob',{loai:light,gia_tri:edison
                ?Math.round(Number(input.value)*255/100):parseInt(input.value.slice(1),16)});
            off.onclick=()=>lenh('den_cob',{loai:light,gia_tri:0});
            buttons.append(apply,off);test.append(row,buttons);
        }else test.append(nut('p','nt','Lưu cấu hình và khởi động lại để thử đèn.'));
        const sync=()=>{
            fields.hidden=!toggle.checked;test.hidden=!toggle.checked;
            card.dataset.active=String(toggle.checked);
        };
        toggle.onchange=()=>{sync();updateControls();veSoDo()};
        card.append(heading,fields);el('the-dieukhien').append(test);sync();
        return card;
    });
}
function cobConfig(){
    const pins=Object.fromEntries(cobKeys.map((key,index)=>{
        const {toggle,select}=cobForms[index];
        if(toggle.checked&&Number(select.value)<0)throw Error('Chọn đủ GPIO cho đèn COB đã bật.');
        return [key,toggle.checked?Number(select.value):-1];
    }));
    return {...pins,rgb_anode:cobAnode.value==='1'};
}
function dungGpio(chan){
    el('nhomchan').replaceChildren();
    const cacThe=[],cacTab=[];
    oChonChan.length=0;
    const capNhatDieuKhien=()=>{
        const tab=cacTab.at(-1);
        const extraEnabled=extraServoForms.some(form=>form.toggle.checked);
        const sensorEnabled=rangeForms.some(form=>form.toggle.checked);
        const cobEnabled=cobForms.some(form=>form.toggle.checked);
        tab.hidden=Number(el('driverdc').value)===0&&!el('robotuart').checked&&!el('batservo').checked&&!el('batcose').checked&&!extraEnabled&&!sensorEnabled&&!cobEnabled;
        el('dieukhien-tat').hidden=duLieu.robot_dang_bat||duLieu.servo_dang_bat||extraEnabled||sensorEnabled||cobEnabled;
        if(tab.hidden&&el('bangchan').hidden)cacTab[0].onclick();
    };
    for(const [index,g] of nhom.entries()){
        const control=g.ten==='Điều khiển';
        const tab=nut('button','tab');tab.type='button';tab.style.setProperty('--g','var(--c'+g.mau+')');
        tab.append(nut('i',''),nut('span','',g.ten));cacTab.push(tab);el('nhomchan').append(tab);
        tab.onclick=()=>{cacTab.forEach((item,i)=>{item.className='tab'+(i===index?' on':'');item.setAttribute('aria-pressed',String(i===index))});cacThe.forEach(card=>card.hidden=card.dataset.nhom!==String(index));el('the-dieukhien').hidden=!control;el('bangchan').hidden=control};
        const badge=nut('b','');if(!control)tab.append(badge);
        const syncBadge=()=>{
            badge.textContent=cacThe.filter(t=>t.dataset.nhom===String(index)&&t.dataset.active==='true').length;
            badge.hidden=badge.textContent==='0';
        };
        if(g.mau===2){
            for(const card of cobCards(index,()=>{capNhatDieuKhien();syncBadge()})){
                cacThe.push(card);el('bangchan').append(card);
            }
        }
        if(g.mau===6){
            for(const card of rangeCards(index,()=>{capNhatDieuKhien();syncBadge()})){
                cacThe.push(card);el('bangchan').append(card);
            }
        }
        if(g.mau===5){
            for(const card of extraServoCards(index,()=>{capNhatDieuKhien();syncBadge()})){
                cacThe.push(card);el('bangchan').append(card);
            }
        }
        for(const [ten,tienTo,loai] of g.the){
            const card=nut('div','card');card.dataset.nhom=String(index);card.style.setProperty('--g','var(--c'+g.mau+')');
            const title=nut('h3','ct',ten);card.append(title);
            if(loai)card.append(nut('p','nt',loai));
            if(tienTo==='dc_')card.append(nut('p','nt','L298N mini và DRV8833 đều dùng bốn chân tín hiệu, điều tốc bằng PWM trên chân IN.'));
            if(tienTo==='light'){
                card.append(el('phukienuart'));
                if(!duLieu.uart_dang_bat)card.append(nut('p','nt','Bật bộ điều khiển UART và khởi động lại để dùng hiệu ứng đèn.'));
            }
            el('bangchan').append(card);
            for(const id of tuyChonThe[tienTo]||[]){
                const setting=el('opt-'+id);
                if(!setting)continue;
                if(['batservo','batcose','camung','robotuart'].includes(id)){
                    const heading=nut('label','hd'),toggle=el(id);
                    heading.append(toggle,nut('span','sw'),nut('span','',ten));
                    title.replaceChildren(heading);
                }else card.append(setting);
            }
            const fields=nut('div','pin-fields');
            const toggleId={servo_dau:'batservo',servo_co:'batcose',cam_ung_:'camung',robot_:'robotuart',dc_:'driverdc'}[tienTo];
            let motorSwitch;
            if(tienTo==='dc_'){
                const driver=el('driverdc'),heading=nut('label','hd');
                motorSwitch=nut('input','');motorSwitch.type='checkbox';
                heading.append(motorSwitch,nut('span','sw'),nut('span','',ten));
                title.replaceChildren(heading);
                let lastDriver=Number(driver.value)||2;
                motorSwitch.onchange=()=>{
                    if(Number(driver.value))lastDriver=Number(driver.value);
                    driver.value=String(motorSwitch.checked?lastDriver:0);
                    sync();capNhatDieuKhien();veSoDo();
                };
            }
            const labels=[];
            const sync=()=>{
                const toggle=toggleId&&el(toggleId);
                const active=!toggle||(toggleId==='driverdc'?Number(toggle.value)!==0:toggle.checked);
                fields.hidden=!active;
                if(motorSwitch){
                    motorSwitch.checked=active;
                    for(const id of tuyChonThe.dc_)el('opt-'+id).hidden=!active;
                }
                if(toggleId==='batservo'||toggleId==='batcose'){
                    for(const id of tuyChonThe[tienTo].filter(id=>id!==toggleId))
                        el('opt-'+id).hidden=!active;
                }
                for(const [label,input] of labels){
                    const key=input.dataset.chan;
                    const symbol=Number(el('driverdc').value)===2&&drv8833[key]||kyHieu[key];
                    label.textContent=symbol;
                    input.dataset.ten=ten+' · '+symbol;
                }
                card.dataset.active=tienTo==='light'?String(duLieu.uart_dang_bat):String(active);
                syncBadge();
            };
            if(toggleId)el(toggleId).addEventListener('change',()=>{sync();veSoDo()});
            for(const c of chan.filter(pin=>pin.ma.startsWith(tienTo))){
                const row=nut('label','row'),input=nut('select','');
                input.dataset.chan=c.ma;input.dataset.ten=c.ten;input.dataset.mau=g.mau;
                if(['d_rst','cam_ung_irq'].includes(c.ma))input.add(new Option('Không nối','-1'));
                themChan(input,()=>input.dataset.ten,()=>!fields.hidden&&!row.hidden);
                input.value=String(c.gia_tri);input.onchange=veSoDo;
                const label=nut('span','',kyHieu[c.ma]);label.title=c.ten;labels.push([label,input]);
                row.append(label,input);fields.append(row);
            }
            if(tienTo==='d_'||tienTo==='secondary')lcdForm(card,tienTo==='secondary',fields,title,syncBadge);
            card.append(fields);cacThe.push(card);
            if(tienTo==='d_'||tienTo==='secondary'){
                for(const [label,input] of labels)input.dataset.ten=ten+' · '+label.textContent;
                syncBadge();
            }else sync();
        }
    }
    el('driverdc').addEventListener('change',capNhatDieuKhien);
    el('robotuart').addEventListener('change',capNhatDieuKhien);
    el('batservo').addEventListener('change',capNhatDieuKhien);
    el('batcose').addEventListener('change',capNhatDieuKhien);
    cacTab[0].onclick();capNhatDieuKhien();veSoDo();updateDistances(duLieu);
}
function veSoDo(){
    const pins=[...document.querySelectorAll('[data-chan]')],gan=new Map();
    for(const input of pins){
        const pin=Number(input.value);
        if(pin>=0){if(!gan.has(pin))gan.set(pin,[]);gan.get(pin).push(input);}
    }
    const chuSoHuu=new Map();
    for(const {select,ten,dangDung} of oChonChan){
        const pin=Number(select.value);
        if(select.value===''||pin<0||!dangDung())continue;
        if(!chuSoHuu.has(pin))chuSoHuu.set(pin,[]);
        chuSoHuu.get(pin).push({select,ten:typeof ten==='function'?ten():ten});
    }
    for(const {select} of oChonChan){
        for(const option of select.options){
            const pin=Number(option.value);
            if(option.value===''||pin<0)continue;
            const owners=chuSoHuu.get(pin)||[];
            const others=owners.filter(owner=>owner.select!==select);
            const state=others.length?'Đã gán: '+others.map(owner=>owner.ten).join(' / ')
                :owners.length?'Đang chọn':'Trống';
            const warning=canhBaoChan[pin];
            option.text='GPIO '+pin+(warning?' !':'')+' · '+state+(warning?' · '+warning:'');
        }
    }
    const xungDot=[...gan.values()].filter(items=>items.length>1).length;
    for(const input of pins){
        const trung=(gan.get(Number(input.value))||[]).length>1;
        input.className=trung?'bad':'';
        input.setAttribute('aria-invalid',String(trung));
    }
    const cols=[nut('div',''),nut('div','')];
    const danhSach=[trai,phai];
    for(const [i,col] of danhSach.entries()){
        for(const pin of col){
            const owners=chuSoHuu.get(pin)||[],isGpio=typeof pin==='number';
            const assigned=owners.map(owner=>owner.select).filter(select=>select.dataset.chan);
            const chip=nut('div','pin'+(!isGpio?' pw':!hopLe(pin)?' rs':assigned.length>1?' bad':owners.length?' on':''));
            if(owners.length)chip.style.setProperty('--g','var(--c'+(assigned[0]?.dataset.mau||7)+')');
            const warning=canhBaoChan[pin];
            const detail=owners.map(owner=>owner.ten).join(' / ');
            const name=[detail,warning].filter(Boolean).join(' · ')||
                (isGpio&&!hopLe(pin)?(pin>=35&&pin<=37?'PSRAM · Đã khóa':'Bộ nhớ · Đã khóa'):'');
            chip.title=name;if(isGpio)chip.append(nut('i',''));chip.append(nut('b','',String(pin)+(warning?' !':'')),nut('span','',name));cols[i].append(chip);
        }
    }
    el('sodochan').replaceChildren(...cols);
    el('dagan').textContent=chuSoHuu.size;el('controng').textContent=cacChan.filter(pin=>hopLe(pin)&&!chuSoHuu.has(pin)).length;
    el('soxungdot').textContent=xungDot;el('xungdot').hidden=!xungDot;el('cf').hidden=!xungDot;el('luugpio').disabled=!!xungDot;
    el('machan').textContent=pins.map(p=>'#define PIN_'+p.dataset.chan.toUpperCase()+' '+p.value).join('\n');
}
function nhapGpio(text){
    const inputs=[...document.querySelectorAll('[data-chan]')];
    const fields=new Map(inputs.map(input=>[input.dataset.chan.toUpperCase(),input]));
    const changes=new Map();
    for(const line of text.split(/\r?\n/)){
        const match=line.match(/^\s*#define\s+PIN_(\w+)\s+(-?\d+)\s*(?:\/\/.*)?$/);
        if(!match||!fields.has(match[1]))continue;
        const input=fields.get(match[1]);
        if(changes.has(input)||![...input.options].some(option=>option.value===match[2])){
            el('ms').textContent='Chân không hợp lệ hoặc khai báo lặp ở '+match[1]+'.';return;
        }
        changes.set(input,match[2]);
    }
    if(!changes.size){el('ms').textContent='Không tìm thấy chân GPIO của UTA-robot.';return}
    for(const [input,value] of changes)input.value=value;
    veSoDo();el('ms').textContent='Đã nhập '+changes.size+' chân. Kiểm tra sơ đồ trước khi lưu.';
}
el('docgoc').onclick=()=>{
    if(!duLieu)return;
    const saved=new Map(duLieu.chan.map(pin=>[pin.ma,pin.gia_tri]));
    document.querySelectorAll('[data-chan]').forEach(input=>input.value=String(saved.get(input.dataset.chan)));
    veSoDo();
};
el('chonfile').onclick=()=>el('filegpio').click();
el('filegpio').onchange=async()=>{
    const file=el('filegpio').files[0];
    if(!file)return;
    try{if(file.size>65536)throw Error();nhapGpio(await file.text())}
    catch{el('ms').textContent='Không đọc được file GPIO (tối đa 64 KB).'}
    finally{el('filegpio').value=''}
};
el('danma').onclick=()=>el('nhapma').classList.toggle('on');
el('apdungma').onclick=()=>nhapGpio(el('noidungma').value);
el('saochan').onclick=async()=>{
    const button=el('saochan'),code=el('machan');
    try{
        if(navigator.clipboard&&window.isSecureContext)await navigator.clipboard.writeText(code.textContent);
        else{
            const selection=window.getSelection(),range=document.createRange();range.selectNodeContents(code);
            selection.removeAllRanges();selection.addRange(range);
            if(!document.execCommand('copy'))throw Error();
            selection.removeAllRanges();
        }
        button.textContent='Đã sao chép';setTimeout(()=>button.textContent='Sao chép',1400);
    }catch{thongBao('Hãy chọn và sao chép khối mã bằng tay.',true)}
};
function thongBao(noiDung,loi=false){el('thongbao').textContent=noiDung;el('thongbao').classList.toggle('error',loi)}
async function yeuCau(duongDan,noiDung){const tuyChon=noiDung?{method:'POST',headers:{'Content-Type':'application/json','X-UTA-Yeu-Cau':'1'},body:JSON.stringify(noiDung)}:{};const boDieuKhien=new AbortController(),henGio=setTimeout(()=>boDieuKhien.abort(),6000);let phanHoi;try{phanHoi=await fetch(duongDan,{...tuyChon,cache:'no-store',signal:boDieuKhien.signal})}catch{throw Error('Không kết nối được với thiết bị. Hãy kiểm tra Wi-Fi và thử lại.')}finally{clearTimeout(henGio)}let data;try{data=await phanHoi.json()}catch{throw Error('Thiết bị trả về dữ liệu không hợp lệ.')}if(!phanHoi.ok)throw Error(data.thong_bao||'Không thực hiện được yêu cầu.');return data}
async function lenh(ten,thamSo={}){try{const data=await yeuCau('/api/lenh',{lenh:ten,...thamSo});thongBao(data.thong_bao);return true}catch(e){thongBao(e.message,true);return false}}
async function capNhat(dau=false){try{const data=await yeuCau('/api/trang-thai');updateDistances(data);el('trangthainhac').textContent=data.trang_thai_nhac;el('gocdangdung').textContent='Góc đang dùng '+data.goc_man_hinh+'°';if(dau){el('gocmanhinh').value=data.goc_man_hinh;el('luugoc').disabled=false;duLieu=data;el('maychu').value=data.may_chu_nhac;el('tendai').value=data.ten_radio;el('urldai').value=data.url_radio;el('camung').checked=data.cam_ung;el('robotuart').checked=data.robot_uart;el('dieukhienrobot').hidden=!data.robot_dang_bat;el('phukienuart').hidden=!data.uart_dang_bat;el('dieukhienservo').hidden=!data.servo_dang_bat;el('testdau').hidden=!(data.dang_servo_dau??data.servo_dang_bat);el('testco').hidden=!(data.dang_servo_co??data.servo_dang_bat);el('dieukhiencob').hidden=!(data.den_cob_dang_dung?.[2]);el('dieukhien-tat').hidden=data.robot_dang_bat||data.servo_dang_bat;el('driverdc').value=data.driver_dc;el('tocdodc').value=data.toc_do_dc;el('daotrai').checked=data.dao_dc_trai;el('daophai').checked=data.dao_dc_phai;el('batservo').checked=data.bat_servo_dau??data.bat_servo;el('batcose').checked=data.bat_servo_co??data.bat_servo;for(let i=0;i<2;i++){el('min'+i).value=data['goc_min_'+i];el('max'+i).value=data['goc_max_'+i];el('nha'+i).value=data['goc_nha_'+i];const o=el(i?'gocco':'gocdau');o.min=data['dang_min_'+i];o.max=data['dang_max_'+i];o.value=data['dang_nha_'+i];o.oninput()}el('dai').replaceChildren();data.danh_sach_dai.forEach(d=>el('dai').add(new Option(d.ten,d.ma)));dungGpio(data.chan);thongBao(data.can_khoi_dong_lai?'Cài đặt chân đang chờ khởi động lại.':'Đã kết nối với UTA-robot.')}}catch(e){thongBao(e.message,true)}}
function dung(){if(giuNut){clearInterval(giuNut);giuNut=null;lenh('robot',{huong:'dung',thoi_gian:100})}}
document.querySelectorAll('[data-huong]').forEach(nut=>{nut.onpointerdown=e=>{e.preventDefault();dung();nut.setPointerCapture(e.pointerId);const gui=async()=>{if(dangGui)return;dangGui=true;try{await lenh('robot',{huong:nut.dataset.huong,thoi_gian:600})}finally{dangGui=false}};gui();giuNut=setInterval(gui,250)};nut.onpointerup=nut.onpointercancel=nut.onlostpointercapture=dung});window.addEventListener('blur',dung);document.addEventListener('visibilitychange',()=>{if(document.hidden)dung()});el('dungrobot').onclick=()=>{dung();lenh('robot',{huong:'dung',thoi_gian:100})};
el('nhay').onclick=()=>lenh('robot',{huong:'nhay',thoi_gian:2000});el('apdungcob').onclick=()=>lenh('den_cob_hieu_ung',{hieu_ung:el('cobhieung').value});el('doiden').onclick=()=>lenh('robot',{huong:'den',thoi_gian:Number(el('den').value)});el('phatnhac').onclick=()=>lenh('nhac',{bai_hat:el('baihat').value,ca_si:el('casi').value});el('phatdai').onclick=()=>lenh('radio',{ma_dai:el('dai').value});el('phattuychinh').onclick=()=>lenh('radio',{ma_dai:'tuy_chinh'});el('tamdung').onclick=()=>lenh('tam_dung');el('tieptuc').onclick=()=>lenh('tiep_tuc');el('dungnhac').onclick=()=>lenh('dung_nhac');el('amluong').onchange=()=>lenh('am_luong',{gia_tri:Number(el('amluong').value)});el('dosang').onchange=()=>lenh('do_sang',{gia_tri:Number(el('dosang').value)});
for(const key of ['gocdau','gocco']){
    const slider=el(key);
    slider.oninput=()=>{
        el('kim-'+key).setAttribute('transform','rotate('+Number(slider.value)+' 45 44)');
        el('so-'+key).textContent=slider.value+'°';
    };
}
el('phatvideo').onclick=()=>lenh('video',{url:el('urlvideo').value});el('gocdau').onchange=()=>{el('gocdau').oninput();return lenh('servo',{truc:0,goc:Number(el('gocdau').value)})};el('gocco').onchange=()=>{el('gocco').oninput();return lenh('servo',{truc:1,goc:Number(el('gocco').value)})};el('daugiua').onclick=()=>{el('gocdau').value=duLieu.dang_nha_0;el('gocdau').onchange()};el('cogiua').onclick=()=>{el('gocco').value=duLieu.dang_nha_1;el('gocco').onchange()};
el('caidat').onsubmit=async e=>{e.preventDefault();if(!duLieu||el('luugpio').disabled){thongBao('Kiểm tra các chân GPIO trước khi lưu.',true);return}const chan={};document.querySelectorAll('[data-chan]').forEach(x=>chan[x.dataset.chan]=Number(x.value));const dongCo={driver_dc:Number(el('driverdc').value),toc_do_dc:Number(el('tocdodc').value),dao_dc_trai:el('daotrai').checked,dao_dc_phai:el('daophai').checked,bat_servo:el('batservo').checked&&el('batcose').checked,bat_servo_dau:el('batservo').checked,bat_servo_co:el('batcose').checked};for(let i=0;i<2;i++){dongCo['goc_min_'+i]=Number(el('min'+i).value);dongCo['goc_max_'+i]=Number(el('max'+i).value);dongCo['goc_nha_'+i]=Number(el('nha'+i).value)}try{const data=await yeuCau('/api/cai-dat',{may_chu_nhac:el('maychu').value,khoa_nhac:el('khoanhac').value,ten_radio:el('tendai').value,url_radio:el('urldai').value,man_chinh:lcdForms.man_chinh(),man_phu:lcdForms.man_phu(),servo_mo_rong:extraServoConfig(),chan_cam_bien:rangeConfig(),chan_den_cob:cobConfig(),cam_ung:el('camung').checked,robot_uart:el('robotuart').checked,chan,...dongCo});el('khoanhac').value='';duLieu.chan.forEach(pin=>pin.gia_tri=chan[pin.ma]);thongBao(data.thong_bao)}catch(err){thongBao(err.message,true)}};
el('luugoc').onclick=async()=>{const nut=el('luugoc');nut.disabled=true;try{if(await lenh('xoay_man_hinh',{gia_tri:Number(el('gocmanhinh').value)}))await capNhat()}finally{nut.disabled=false}};
el('khoidonglai').onclick=()=>{if(confirm('Khởi động lại UTA-robot và áp dụng sơ đồ chân đã lưu?'))lenh('khoi_dong_lai')};el('cauhinhwifi').onclick=()=>{if(confirm('Dừng phát nhạc và mở chế độ cấu hình Wi-Fi?'))lenh('cau_hinh_wifi')};capNhat(true);setInterval(()=>capNhat(false),3000);
</script></body></html>)UTA";

}  // namespace uta
