const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

// Chạy đúng mã trang nhúng với giao diện DOM tối thiểu và HTTP giả lập.
const source = fs.readFileSync(path.resolve(__dirname, '../../main/boards/UTA-robot/uta_web_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
class Element {
    constructor() {
        this.value = ''; this.checked = false; this.textContent = ''; this.dataset = {};
        this.children = []; this.classList = {toggle() {}};
    }
    append(...children) { this.children.push(...children); }
    replaceChildren(...children) { this.children = children; }
    add(item) { this.children.push(item); if (!this.value) this.value = item.value; }
    setPointerCapture() {}
}
const elements = new Map();
for (const match of source.matchAll(/\bid="([^"]+)"/g)) elements.set(match[1], new Element());
const movements = ['tien', 'lui', 'trai', 'phai'].map(direction => {
    const button = new Element(); button.dataset.huong = direction; return button;
});
const requests = [];
const timers = new Map();
let nextTimer = 1;
const pins = ['d_mosi','d_clk','d_dc','d_cs','d_blk','d_rst','cam_ung_miso','cam_ung_cs','cam_ung_irq','robot_tx','robot_rx','mic_ws','mic_sck','mic_din','spk_dout','spk_bclk','spk_lrck','dc_trai_1','dc_trai_2','dc_phai_1','dc_phai_2','dc_trai_en','dc_phai_en','servo_dau','servo_co'];
const defaults = [40,39,42,2,1,41,11,12,13,38,48,4,5,6,7,15,16,8,9,10,14,17,18,21,43];
const status = {
    trang_thai_nhac:'Đã dừng', may_chu_nhac:'https://maychu', ten_radio:'Đài đã lưu',
    url_radio:'https://dai/amthanh', cam_ung:false, robot_uart:false, robot_dang_bat:false, can_khoi_dong_lai:false,
    danh_sach_dai:[{ten:'Đài thử',ma:'vov1'}],
    driver_dc:0, toc_do_dc:60, dao_dc_trai:false, dao_dc_phai:false, bat_servo:false,
    uart_dang_bat:false, servo_dang_bat:false,
    goc_min_0:15, goc_max_0:165, goc_nha_0:90, goc_min_1:20, goc_max_1:160, goc_nha_1:85,
    dang_min_0:15, dang_max_0:165, dang_nha_0:90, dang_min_1:20, dang_max_1:160, dang_nha_1:85,
    chan:pins.map((key,i)=>({ma:key,ten:key,gia_tri:defaults[i]})),
};
const inputs = () => elements.get('bangchan').children.flatMap(row => row.children.flatMap(cell => cell.children));
const sandbox = {
    document: {
        getElementById:id=>elements.get(id), createElement:()=>new Element(),
        querySelectorAll:selector=>selector==='[data-huong]' ? movements : inputs(),
        addEventListener() {}, hidden:false,
    },
    window:{addEventListener() {}},
    Option:function(text,value) {return {text,value};}, confirm:()=>true,
    AbortController, setTimeout, clearTimeout,
    setInterval:fn=>{const id=nextTimer++;timers.set(id,fn);return id;},
    clearInterval:id=>timers.delete(id),
    fetch:async (url,options)=>{
        requests.push({url,options});
        return {ok:true,status:200,json:async()=>url==='/api/trang-thai' ? status : {thong_bao:'Đã nhận lệnh'}};
    },
};
vm.createContext(sandbox);
vm.runInContext(script,sandbox);

(async()=>{
    await new Promise(resolve=>setImmediate(resolve));
    assert.equal(inputs().length,25);
    assert.equal(elements.get('maychu').value,'https://maychu');
    assert.equal(elements.get('trangthainhac').textContent,'Đã dừng');
    await elements.get('caidat').onsubmit({preventDefault(){}});
    const saved=requests.find(request=>request.url==='/api/cai-dat');
    assert(saved);
    assert.equal(elements.get('dieukhienrobot').hidden,true);
    const body=JSON.parse(saved.options.body);
    assert.equal(body.robot_uart,false);
    assert.deepEqual(body.chan,Object.fromEntries(pins.map((pin,i)=>[pin,defaults[i]])));
    assert.equal(saved.options.headers['X-UTA-Yeu-Cau'],'1');
    assert.equal(body.goc_nha_1,85);
    assert.equal(body.toc_do_dc,60);
    elements.get('maychu').value='https://may-chu-khac/nhac';
    elements.get('driverdc').value='2';
    elements.get('batservo').checked=true;
    elements.get('daotrai').checked=true;
    await elements.get('caidat').onsubmit({preventDefault(){}});
    const updated=JSON.parse(requests.at(-1).options.body);
    assert.equal(updated.may_chu_nhac,'https://may-chu-khac/nhac');
    assert.equal(updated.driver_dc,2);
    assert.equal(updated.bat_servo,true);
    assert.equal(updated.dao_dc_trai,true);
    elements.get('urlvideo').value='https://may-chu/video.mp4?ma=abc';
    await elements.get('phatvideo').onclick();
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'video',url:'https://may-chu/video.mp4?ma=abc'});
    elements.get('gocdau').value='120';
    await elements.get('gocdau').onchange();
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'servo',truc:0,goc:120});
    await elements.get('cogiua').onclick();
    await new Promise(resolve=>setImmediate(resolve));
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'servo',truc:1,goc:85});
    movements[0].onpointerdown({preventDefault(){},pointerId:1});
    movements[0].onpointerup();
    await new Promise(resolve=>setImmediate(resolve));
    const commands=requests.filter(request=>request.url==='/api/lenh').map(request=>JSON.parse(request.options.body));
    assert(commands.some(command=>command.huong==='tien' && command.thoi_gian===600));
    assert(commands.some(command=>command.huong==='dung'));
    assert.equal(timers.size,1);
    elements.get('baihat').value='Bài hát thử';
    elements.get('casi').value='Ca sĩ thử';
    await elements.get('phatnhac').onclick();
    const last=JSON.parse(requests.at(-1).options.body);
    assert.equal(last.bai_hat,'Bài hát thử');
    assert.equal(last.ca_si,'Ca sĩ thử');
    console.log('Đạt: 25 GPIO, đổi URL máy chủ, chọn driver, lưu giới hạn servo, lệnh video/servo, nhấn/nhả robot và tên bài tiếng Việt.');
})().catch(error=>{console.error(error);process.exitCode=1;});
