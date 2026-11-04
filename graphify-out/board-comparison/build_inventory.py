"""Read-only source inventory; outputs are reports, not build configuration."""
from pathlib import Path
from collections import Counter
import contextlib
import html
import importlib.util
import io
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
BOARDS = ROOT / 'main/boards'

def read(p):
    return p.read_text(encoding='utf-8', errors='replace')

def unique(xs):
    return list(dict.fromkeys(xs))

def clean(s):
    return re.sub(r'//[^\n]*|/\*.*?\*/', '', s, flags=re.S)

def refs(files, pattern, cap=60):
    found = []
    rx = re.compile(pattern, re.I)
    for p, text in files:
        for i, line in enumerate(text.splitlines(), 1):
            if rx.search(line):
                found.append({'file': p.relative_to(ROOT).as_posix(), 'line': i,
                              'text': line.strip()[:260]})
    return found[:cap]

spec = importlib.util.spec_from_file_location('xiaozhi_build', ROOT / 'scripts/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
listing_log = io.StringIO()
with contextlib.redirect_stdout(listing_log), contextlib.redirect_stderr(listing_log):
    release61 = build._collect_variants(idf_version=(6, 1, 0))
    release602 = build._collect_variants(idf_version=(6, 0, 2))

display_overrides = {
    'bread-compact-wifi': 'OLED SSD1306 128×32 / 128×64 theo release; có nhánh SH1106',
    'otto-robot': 'LCD ST7789 240×240',
    'electron-bot': 'LCD GC9A01 240×240',
    'espressif/esp-vocat': 'LCD ST77916 360×360',
    'espressif/esp-hi': 'LCD ILI9341, vùng hiển thị 160×80 theo config.h',
    'espressif/esp-sparkbot': 'LCD ST7789 240×240',
    'nulllab-ai-vox-v3': 'LCD ST7789 240×240',
    'm5stack/cardputer-adv': 'LCD ST7789 240×135',
    'm5stack/atoms3r-echo-base': 'LCD 128×128; mã có nhánh ST7789 / GC9A01',
    'm5stack/tab5': 'LCD MIPI 720×1280; ILI9881C / ST7121 / ST7123 theo phần cứng',
    'lilygo/t-display-s3-pro-mvsrlora': 'LCD ST7796 222×480; cảm ứng CST226SE',
    'waveshare/esp32-s3-touch-amoled-1.8': 'AMOLED 368×448',
    'waveshare/esp32-c6-epaper-1.54': 'E-paper 200×200',
}
action_overrides = {
    'otto-robot': '6 servo: đi, quay, nhảy, moonwalk, ngồi, vẫy tay, giơ/hạ tay, chào, múa; góc/tốc độ/trim qua MCP',
    'electron-bot': '6 servo: cử động tay, thân, đầu; chuỗi động tác, dừng, về home, chỉnh trim qua MCP',
    'espressif/esp-hi': 'Robot chó: tiến/lùi, quay, dừng, nằm, lắc, thu chân, bắt tay, duỗi chân sau, nhảy tới; RGB',
    'espressif/esp-sparkbot': 'Xe robot: tiến/lùi, trái/phải, nhảy múa, đổi đèn, lật ảnh camera qua MCP',
    'espressif/esp-vocat': 'BMI270: lắc → confused 1,8 giây; BOOT/chat; phản ứng pin/sạc. Callback pad chạm/vuốt → happy nằm trong nhánh IDF < 6, bị tắt ở IDF 6.x',
    'nulllab-ai-vox-v3': 'BOOT: chat; giữ lúc khởi động/cấu hình → đổi mạng; +/- âm lượng; giữ +/- → max/mute; nhấn đôi AEC nếu bật CONFIG_USE_DEVICE_AEC',
    'lilygo/t-display-s3-pro-mvsrlora': 'Chạm lần đầu → ToggleChatState; nhả chỉ xóa cờ chạm; nút BOOT. Không phải giữ để nói trong callback này',
    'm5stack/cardputer-adv': 'Bàn phím: lên/xuống chỉnh âm lượng, trái/phải chỉnh sáng, Enter bật/tắt chat; W/S mở cấu hình Wi-Fi/SSID đã lưu trong chế độ cấu hình; BOOT',
    'espressif/esp-spot': 'Nút chat; BMI270 có cử chỉ cổ tay đánh thức deep sleep nếu có IMU_INT_GPIO và IMU khởi tạo thành công; LED',
    'lceda-course-examples/eda-robot-pro': 'Robot chó: đi, quay, ngồi, đứng, duỗi, lắc, nâng từng chân; dừng; chỉnh trim và trạng thái qua MCP',
    'lceda-course-examples/eda-super-bear': 'Robot gấu: đi, quay, nhảy, lắc, moonwalk, cúi, lắc chân, nhún; tay nâng/hạ/vẫy theo cấu hình; dừng và trim qua MCP',
}

rows = []
for cfg in sorted(BOARDS.rglob('config.json')):
    folder = cfg.parent
    name = folder.relative_to(BOARDS).as_posix()
    config = json.loads(read(cfg))
    files = [(p, read(p)) for p in sorted(folder.rglob('*'))
             if p.is_file() and p.suffix.lower() in {'.cc', '.cpp', '.c', '.h', '.hpp'}]
    sources = [(p, t) for p, t in files if p.suffix in {'.cc', '.cpp'}]
    text = '\n'.join(clean(t) for _, t in files)
    source = '\n'.join(clean(t) for _, t in sources)
    macro = {}
    for key, value in re.findall(r'^\s*#define\s+(\w+)\s+([^\n]+)', text, re.M):
        macro.setdefault(key, []).append(value.strip())

    def resolve(key, depth=0):
        if depth > 5:
            return []
        values = []
        for value in macro.get(key, []):
            v = value.strip('() ')
            if re.fullmatch(r'\d+', v):
                values.append(v)
            elif re.fullmatch(r'\w+', v):
                values.extend(resolve(v, depth+1))
        return unique(values)

    widths, heights = [], []
    for a, b in [('DISPLAY_WIDTH', 'DISPLAY_HEIGHT'), ('LCD_WIDTH', 'LCD_HEIGHT'),
                 ('EXAMPLE_LCD_WIDTH', 'EXAMPLE_LCD_HEIGHT'), ('LCD_H_RES', 'LCD_V_RES'),
                 ('BSP_LCD_H_RES', 'BSP_LCD_V_RES'), ('QSPI_LCD_H_RES', 'QSPI_LCD_V_RES')]:
        if resolve(a) and resolve(b):
            widths, heights = resolve(a), resolve(b)
            break
    drivers = unique(re.findall(r'esp_lcd_new_panel_(?!io_)(\w+)\s*\(', source))
    display_types = unique(re.findall(r'new\s+(\w*Display)\s*\(', source))
    screen = ', '.join(drivers or display_types) or 'Chưa xác định từ bộ quét'
    if widths and heights:
        screen += ' ' + '/'.join(widths) + ' × ' + '/'.join(heights)
        if len(widths) > 1 or len(heights) > 1:
            screen += ' (nhiều nhánh, chưa ghép theo variant)'
    screen = display_overrides.get(name, screen)

    inheritance = unique(re.findall(r'class\s+\w+\s*:\s*public\s+(\w*Board)\b', source))
    network = ', '.join(inheritance) or 'Chưa xác định'
    calls = unique(re.findall(r'\.(OnClick|OnDoubleClick|OnLongPress|OnPressDown|OnPressUp|OnMultipleClick)\s*\(', source))
    tools = unique(re.findall(r'"(self\.[a-zA-Z0-9_.]+)"', source))
    action = ', '.join(calls) or 'Chưa thấy callback nút theo mẫu quét'
    if re.search(r'esp_lcd_touch_new_|GetTouchPoint|touchpad_daemon|TouchEvent|touch_cap_poll', source):
        action += '; có xử lý cảm ứng (cần xem callback)'
    if re.search(r'SetKeyEventCallback|SetKeyCallback', source):
        action += '; có callback bàn phím'
    if 'BMI2_WRIST_GESTURE' in source:
        action += '; có nhánh cử chỉ cổ tay BMI270'
    if tools:
        action += '; MCP: ' + ', '.join(tools[:8]) + (' …' if len(tools)>8 else '')
    action = action_overrides.get(name, action)
    radio = 'Chưa thấy triển khai FM/AM hoặc radio Internet'
    if re.search(r'\bSX1262\b|\bSX127\d\b', text, re.I):
        radio += '; có khai báo chân LoRa, chưa chứng minh driver truyền nhận'
    music = 'Âm thanh hội thoại/Opus; chưa thấy player MP3/playlist riêng trong board'
    if name == 'nulllab-ai-vox-v3':
        music = 'README quảng bá MP3/radio Internet; mã board chưa xác nhận player tương ứng'
    if name == 'lilygo/t-cameraplus-s3':
        music = 'README mô tả nghe nhạc ở firmware Lvgl_UI của hãng; chưa xác nhận trong firmware XiaoZhi này'
    gpio = 'Chân phần cứng trong firmware; chưa tìm thấy giao diện web đổi pin'
    if name in {'otto-robot', 'electron-bot'}:
        gpio += '; có WebSocket /ws điều khiển hành động/MCP, không phải remap GPIO'
    row = {
        'board': name, 'chip': config.get('target', '?'),
        'configured_variants': [b['name'] for b in config.get('builds', [])],
        'release_idf61': [v['name'] for v in release61 if v['board'] == name],
        'release_idf602': [v['name'] for v in release602 if v['board'] == name],
        'screen': screen, 'screen_driver_candidates': drivers,
        'network_class': network, 'radio': radio, 'music': music,
        'actions': action, 'mcp_tools_in_board_sources': tools, 'gpio_web': gpio,
        'evidence': {
            'config': [{'file': cfg.relative_to(ROOT).as_posix(), 'line': 1, 'text': 'Board target và danh sách builds'}],
            'display': refs(files, r'DISPLAY_(WIDTH|HEIGHT)|LCD_(WIDTH|HEIGHT|H_RES|V_RES)|esp_lcd_new_panel_(?!io_)|new\s+\w*Display'),
            'actions': refs(sources, r'OnClick|OnLongPress|OnDoubleClick|OnPressDown|OnPressUp|"self\.|LEFT_SWIPE|RIGHT_SWIPE|ShowTemporaryEmotion|ShowHappyTouchFeedback|SetKeyEventCallback|SetKeyCallback|BMI2_WRIST_GESTURE|ToggleChatState'),
            'pins': refs(files, r'#define\s+\w*(?:GPIO|_PIN)\w*\s+|\.uri\s*=|Settings\s+\w+\(', 18),
        },
    }
    rows.append(row)

configured = {p.parent for p in BOARDS.rglob('config.json')}
source_only = sorted({p.parent.relative_to(BOARDS).as_posix()
                     for p in BOARDS.rglob('*.cc') if 'DECLARE_BOARD(' in clean(read(p)) and p.parent not in configured})
stats = {
    'commit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
    'config_directories': len(rows), 'configured_variants': sum(len(r['configured_variants']) for r in rows),
    'idf61': {'boards': len({v['board'] for v in release61}), 'variants': len(release61)},
    'idf602': {'boards': len({v['board'] for v in release602}), 'variants': len(release602)},
    'chip_counts_by_config_directory': dict(Counter(r['chip'] for r in rows)),
    'factory_directories_without_config': source_only,
    'listing_warnings': listing_log.getvalue().splitlines(),
}
data = {'method': 'Graphify queries followed by static source inventory; no preprocessing/build/hardware testing. Branch candidates are not guaranteed enabled.',
        'stats': stats, 'boards': rows}
OUT.mkdir(parents=True, exist_ok=True)
(OUT/'boards.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

intro = f'''# So sánh các board XiaoZhi theo source đang có

Phạm vi: commit `{stats['commit'][:8]}`, quét {len(rows)} thư mục có config.json và {stats['configured_variants']} khai báo variant.
Danh sách chuẩn từ scripts/build.py: ESP-IDF 6.1 có {stats['idf61']['boards']} board / {stats['idf61']['variants']} variant;
ESP-IDF 6.0.2 có {stats['idf602']['boards']} board / {stats['idf602']['variants']} variant.
README có thể dùng số lượng cũ. Đây là kiểm kê source, không phải kết quả build.

## Cách đọc

- Chip lấy từ config.json, đối chiếu danh sách release bằng hàm _collect_variants của scripts/build.py.
- Màn hình/driver/callback lấy từ source trong thư mục. Những dòng có nhiều nhánh là các khả năng trong source, chưa tiền xử lý theo từng variant; không mặc định tất cả cùng hoạt động.
- OnClick = nhấn; OnDoubleClick = nhấn đôi; OnLongPress = giữ; OnPressDown/Up = nhấn xuống/nhả. Có cảm biến không có nghĩa đã lập trình mọi cử chỉ.
- Cột mạng giữ tên lớp: WifiBoard = Wi-Fi; Ml307Board/Nt26Board = modem; DualNetworkBoard = chọn mạng; EthernetBoard = Ethernet; RndisBoard = USB RNDIS. P4 có thể dùng chip mạng phụ; tên target không mô tả toàn bộ module trên PCB.
- Chưa thấy = chưa tìm được triển khai trong source kiểm tra, không khẳng định phần cứng tuyệt đối không hỗ trợ.
- MCP tool trong bảng là tool riêng tìm thấy trong thư mục board, không bao gồm mọi tool dùng chung. Không phải kiểm kê API hoàn chỉnh.
- Âm thanh chung: TTS/hội thoại Opus; NotifyPlayer còn tải HTTP(S) và giải gói Ogg/Opus. Chưa thấy ứng dụng MP3/AAC/HLS, playlist, dò FM/AM hay danh sách đài dùng chung.
- Nghe nhạc do máy chủ cung cấp/transcode thành Opus có thể tận dụng đường phát âm thanh, nhưng cần triển khai/kiểm chứng dịch vụ; không coi là player có sẵn.
- Dữ liệu JSON kèm bằng chứng file, dòng và đoạn mã. Bảng HTML cho phép lọc và xem bằng chứng.

## Radio, nhạc và GPIO

AI-VOX3 README nói MP3/radio Internet; lớp board hiện tại có chat, chọn Wi-Fi/modem, volume và AEC tùy cấu hình, chưa thấy player tương ứng.
T-CameraPlus-S3 README nói nghe nhạc trong firmware Lvgl_UI của hãng; điều đó không chứng minh firmware XiaoZhi này đã có player.
RadioCalisthenics ở Otto là tên một bài động tác. T-Display-P4 có khai báo chân SX1262 nhưng chưa có config.json release tại thư mục đó, chưa xác nhận truyền nhận LoRa.
Tên t-display-s3-pro-mvsrlora cũng không chứng minh firmware đang bật LoRa. Radio LoRa là truyền dữ liệu, khác thu FM/AM và radio Internet.

GPIO cho mic, loa, LCD, nút và servo chủ yếu cố định trong config.h/pin_config.h hoặc mã khởi tạo; một số chân chọn qua Kconfig lúc build.
Otto có các sơ đồ HardwareConfig tự chọn theo phần cứng; đó là các cấu hình đã biên dịch sẵn, không phải tùy ý đổi GPIO trên web.
Otto/Electron Bot có WebSocket /ws và MCP điều khiển hành động, góc/tốc độ/trim. Trim lưu NVS không phải đổi chân servo.
Trang cấu hình Wi-Fi, sửa tài nguyên và đổi pin là ba chức năng khác nhau. Chưa tìm thấy web remap GPIO tổng quát trong source này.
GpioManager của jiuchuan-s3 là hàm C++ dùng nội bộ, không phải trang cấu hình chân.
Muốn hỗ trợ phần cứng khác cần board/variant mới với nhận dạng OTA riêng; muốn GPIO runtime/web cần thêm API, giới hạn chân hợp lệ và lưu cấu hình, không sửa pin của board đang tồn tại.

## Phân bố chip (theo thư mục config, không phải số lượng thiết bị)

''' + '\n'.join(f'- {k}: {v}' for k,v in stats['chip_counts_by_config_directory'].items()) + '''

## Bảng đầy đủ

| Board | Chip | Màn hình | Mạng/lớp | Tương tác/hành động | Radio | Nhạc | GPIO/web | Variant IDF 6.1 |
|---|---|---|---|---|---|---|---|---|
'''
def cell(x):
    return str(x).replace('|','/').replace('\n',' ')
table = []
for r in rows:
    table.append('| '+' | '.join(cell(x) for x in [r['board'],r['chip'],r['screen'],r['network_class'],r['actions'],r['radio'],r['music'],r['gpio_web'],', '.join(r['release_idf61']) or 'Không có trong danh sách 6.1'])+' |')
tail = '\n\n## Thư mục có factory nhưng chưa có config.json tại cùng thư mục\n\n' + '\n'.join('- '+x for x in source_only)
tail += '\n\n## Giới hạn\n\nGraphify: 10 tệp báo lỗi parse, 178 tệp không có ký hiệu; truy vấn rộng bị cắt nên đã đối chiếu source. Bộ quét không xác định mọi nhánh điều kiện, không đánh giá chất lượng âm thanh hoặc tính tương thích thực tế. Chưa build, nạp firmware hay kiểm thử phần cứng. Không thay đổi mã firmware.\n'
(OUT/'BOARD_COMPARISON_VI.md').write_text(intro+'\n'.join(table)+tail,encoding='utf-8')

payload = json.dumps(data,ensure_ascii=False).replace('</','<\\/')
page = r'''<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>So sánh board XiaoZhi</title>
<style>body{font:15px system-ui;margin:24px;color:#182735;background:#f4f7fb}h1{margin-bottom:8px}header,p{max-width:1100px;line-height:1.6}.controls{display:flex;gap:12px;flex-wrap:wrap;position:sticky;top:0;background:#f4f7fb;padding:12px 0;z-index:2}input,select{padding:11px;border:1px solid #bdcada;border-radius:6px}input{min-width:360px}table{border-collapse:collapse;background:white;width:100%;font-size:13px}td,th{padding:11px;border:1px solid #d9e2ed;text-align:left;vertical-align:top;min-width:140px}th{background:#173e60;color:white}td:first-child{min-width:210px}tr:nth-child(even){background:#f6faff}.wrap{overflow:auto}details{margin-top:9px;max-width:390px}pre{white-space:pre-wrap;font:12px ui-monospace;background:#edf2f7;padding:8px}small{color:#526576}.badge{background:#e4edf8;padding:4px 8px;border-radius:8px}a{color:#125ca0}</style>
<h1>So sánh các board XiaoZhi</h1><header id="summary"></header>
<p><b>Phân biệt:</b> phát hội thoại/Opus ≠ player MP3; README giới thiệu phần cứng ≠ chức năng firmware. WebSocket điều khiển robot ≠ đổi chân GPIO. Các driver và callback tự quét có thể thuộc nhánh cấu hình khác nhau. <a href="BOARD_COMPARISON_VI.md">Báo cáo đầy đủ</a> · <a href="boards.json">Dữ liệu và bằng chứng</a></p>
<div class="controls"><input id="q" placeholder="Tìm board, chip, màn hình, servo, MCP…" aria-label="Tìm kiếm"><select id="chip" aria-label="Chip"><option value="">Tất cả chip</option></select><select id="feature" aria-label="Nhóm"><option value="">Tất cả nhóm</option><option value="robot">Robot / servo</option><option value="touch">Có dấu vết cảm ứng</option><option value="web">WebSocket điều khiển</option><option value="release">Có variant IDF 6.1</option><option value="notrelease">Chưa có variant IDF 6.1</option></select><span id="count" class="badge"></span></div>
<div class="wrap"><table><thead><tr><th>Board / variant</th><th>Chip / mạng</th><th>Màn hình</th><th>Nút, cử chỉ, hành động</th><th>Radio</th><th>Nghe nhạc</th><th>GPIO qua web</th></tr></thead><tbody id="rows"></tbody></table></div>
<p><b>Giới hạn:</b> kiểm kê source, chưa build hoặc thử phần cứng. “Chưa thấy” không có nghĩa “không thể làm”. NotifyPlayer dùng chung có thể phát HTTP(S) Ogg/Opus; chưa xác nhận một ứng dụng MP3/radio độc lập. P4 có thể dùng chip mạng phụ. Trích xuất Graphify có 10 tệp lỗi parse, 178 tệp không sinh ký hiệu.</p>
<script>const data=PAYLOAD;
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const norm=s=>s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').replace(/đ/g,'d').toLowerCase();
summary.innerHTML=`<b>${data.stats.config_directories} thư mục config · ${data.stats.configured_variants} khai báo variant</b><br>IDF 6.1: ${data.stats.idf61.boards} board / ${data.stats.idf61.variants} variant. IDF 6.0.2: ${data.stats.idf602.boards} board / ${data.stats.idf602.variants} variant. Commit <code>${data.stats.commit.slice(0,8)}</code>.`;
Object.keys(data.stats.chip_counts_by_config_directory).sort().forEach(c=>chip.add(new Option(c,c)));
function evidence(r,cat){let es=r.evidence[cat]||[];return es.length?`<details><summary>Bằng chứng (${es.length})</summary>${es.map(e=>`<pre>${esc(e.file)}:${e.line}\n${esc(e.text)}</pre>`).join('')}</details>`:''}
function render(){let query=norm(q.value);let rs=data.boards.filter(r=>(!query||norm(JSON.stringify(r)).includes(query))&&(!chip.value||r.chip===chip.value)&&(!feature.value||({robot:/servo|robot|self\.dog|self\.chassis/i.test(r.actions),touch:/touch|cảm ứng|chạm|vuốt/i.test(r.actions),web:r.gpio_web.includes('WebSocket'),release:r.release_idf61.length>0,notrelease:r.release_idf61.length===0}[feature.value])));count.textContent=rs.length+' / '+data.boards.length+' board';document.getElementById('rows').innerHTML=rs.map(r=>`<tr><td><b>${esc(r.board)}</b><details><summary>${r.release_idf61.length} variant IDF 6.1</summary>${esc(r.release_idf61.join(', ')||'Không có')}<br><small>Khai báo: ${esc(r.configured_variants.join(', ')||'rỗng')}</small></details>${evidence(r,'config')}</td><td>${esc(r.chip)}<br><small>${esc(r.network_class)}</small></td><td>${esc(r.screen)}${evidence(r,'display')}</td><td>${esc(r.actions)}<details><summary>Tool MCP trong thư mục</summary>${esc(r.mcp_tools_in_board_sources.join(', ')||'Chưa thấy; có thể vẫn dùng tool chung')}</details>${evidence(r,'actions')}</td><td>${esc(r.radio)}</td><td>${esc(r.music)}</td><td>${esc(r.gpio_web)}${evidence(r,'pins')}</td></tr>`).join('')}
q.oninput=chip.onchange=feature.onchange=render;render();</script></html>'''
(OUT/'boards.html').write_text(page.replace('PAYLOAD',payload),encoding='utf-8')
print(json.dumps(stats,ensure_ascii=False,indent=2))
print('Wrote boards.json, boards.html and BOARD_COMPARISON_VI.md')
