const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');

// Chạy đúng mã trang nhúng với giao diện DOM tối thiểu và HTTP giả lập.
const preview = process.argv[2];
const source = fs.readFileSync(preview || path.resolve(__dirname, '../../main/boards/UTA-robot/uta_web_page.h'), 'utf8');
const script = [...source.matchAll(/<script>([\s\S]*?)<\/script>/g)].at(-1)[1];
class Element {
    constructor() {
        this.value = ''; this.checked = false; this.textContent = ''; this.dataset = {};
        this.children = []; this.classList = {toggle() {}};
        this.style = {setProperty() {}};
    }
    append(...children) {
        for (const child of children) {
            if (child.parent) child.parent.children.splice(child.parent.children.indexOf(child), 1);
            child.parent = this;
            this.children.push(child);
        }
    }
    replaceChildren(...children) {
        for (const child of this.children) child.parent = undefined;
        this.children = [];
        this.append(...children);
    }
    add(item) { this.children.push(item); if (!this.value) this.value = item.value; }
    setPointerCapture() {}
    setAttribute() {}
    addEventListener(event, callback) {
        const previous = this['on' + event];
        this['on' + event] = () => { previous?.(); callback(); };
    }
    get options() { return this.children; }
}
const elements = new Map();
for (const match of source.matchAll(/\bid="([^"]+)"/g)) elements.set(match[1], new Element());
const root = new Element();
root.append(...elements.values());
for (const [id, wrapper] of elements) {
    if (id.startsWith('opt-')) wrapper.append(elements.get(id.slice(4)));
}
const movements = ['tien', 'lui', 'trai', 'phai'].map(direction => {
    const button = new Element(); button.dataset.huong = direction; return button;
});
const requests = [];
const timers = new Map();
let nextTimer = 1;
const pins = ['d_mosi','d_clk','d_dc','d_cs','d_blk','d_rst','cam_ung_miso','cam_ung_cs','cam_ung_irq','robot_tx','robot_rx','mic_ws','mic_sck','mic_din','spk_dout','spk_bclk','spk_lrck','dc_trai_1','dc_trai_2','dc_phai_1','dc_phai_2','servo_dau','servo_co'];
const defaults = [40,39,42,2,1,41,11,12,13,38,48,4,5,6,7,15,16,8,9,10,14,21,43];
const status = {
    chan_den_cob:[-1,-1,-1,-1,-1],
    den_cob_dang_dung:[true,true,true],
    chan_cam_bien:[11,12,-1,13,14],
    tof_dang_dung:true, tof_mm:316,
    sieu_am_dang_dung:true, sieu_am_mm:-1,
    servo_mo_rong_dang_dung:[44,47,-1,-1],
    goc_man_hinh:90,
    trang_thai_nhac:'Đã dừng', may_chu_nhac:'https://maychu', ten_radio:'Đài đã lưu',
    url_radio:'https://dai/amthanh', cam_ung:false, robot_uart:false, robot_dang_bat:false, can_khoi_dong_lai:false,
    danh_sach_dai:[{ten:'Đài thử',ma:'vov1'}],
    driver_dc:0, toc_do_dc:60, dao_dc_trai:false, dao_dc_phai:false, bat_servo:false,
    uart_dang_bat:false, servo_dang_bat:false,
    goc_min_0:15, goc_max_0:165, goc_nha_0:90, goc_min_1:20, goc_max_1:160, goc_nha_1:85,
    dang_min_0:15, dang_max_0:165, dang_nha_0:90, dang_min_1:20, dang_max_1:160, dang_nha_1:85,
    chan:pins.map((key,i)=>({ma:key,ten:key,gia_tri:defaults[i]})),
};
const descendants = node => (node.children || []).flatMap(child =>
    [child, ...descendants(child)]);
const inputs = () => descendants(elements.get('bangchan')).filter(input => input.dataset?.chan);
const sandbox = {
    document: {
        getElementById:id=>{
            const node = elements.get(id);
            let ancestor = node;
            while (ancestor?.parent) ancestor = ancestor.parent;
            return ancestor === root ? node : null;
        }, createElement:()=>new Element(),
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
    assert.equal(inputs().length,23);
    const cards = elements.get('bangchan').children;
    const lights = cards.filter(card=>card.dataset.nhom==='1');
    assert.equal(lights.length,4);
    const edison = lights[1];
    const edisonToggle = edison.children[0].children[0];
    assert.equal(edison.children[1].hidden,true);
    edisonToggle.checked=true;
    edisonToggle.onchange();
    assert.equal(edison.children[1].hidden,false);
    const edisonTest = elements.get('the-dieukhien').children
        .find(card=>card.children[0]?.textContent==='Đèn Edison');
    await edisonTest.children[2].children[0].onclick();
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'den_cob',loai:1,gia_tri:64});
    await edisonTest.children[2].children[1].onclick();
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'den_cob',loai:1,gia_tri:0});
    edisonToggle.checked=false;
    edisonToggle.onchange();
    assert.equal(cards.filter(card=>card.dataset.nhom==='4').length,5);
    assert.equal(cards.filter(card=>card.dataset.nhom==='5').length,3);
    const sensorTests = elements.get('the-dieukhien').children;
    const tof = sensorTests.find(card=>card.children[0]?.textContent==='ToF · VL53L0X');
    const ultrasonic = sensorTests.find(card=>card.children[0]?.textContent==='HC-SR04');
    assert.equal(tof.children[1].textContent,'31.6 cm');
    assert.equal(ultrasonic.children[1].textContent,'Chưa có số đo hợp lệ.');
    const wheel = cards.find(card=>card.dataset.nhom==='4');
    const wheelToggle = wheel.children[0].children[0];
    const wheelFields = wheel.children.at(-1);
    assert.equal(wheelFields.hidden,true);
    wheelToggle.checked=true;
    wheelToggle.onchange();
    assert.equal(wheelFields.hidden,false);
    assert.equal(elements.get('nhomchan').children.at(-1).hidden,false);
    const wheelTest = elements.get('the-dieukhien').children.find(card=>card.children[0]?.textContent==='Hai bánh · servo 360°');
    const forward = wheelTest.children.find(node=>node.className==='acts').children[2];
    await forward.onclick();
    assert.deepEqual(JSON.parse(requests.at(-1).options.body),{lenh:'servo_mo_rong',truc:0,gia_tri:50});
    wheelToggle.checked=false;
    wheelToggle.onchange();
    if (preview) {
        assert.equal(cards.filter(card=>card.dataset.nhom==='1').length,4);
        const secondary = cards.filter(card=>card.dataset.nhom==='2')[1];
        const driver = secondary.children.find(node=>node.className==='type');
        const pinRows = secondary.children.find(node=>node.className==='pin-fields');
        assert.equal(pinRows.hidden,true);
        assert.equal(driver.hidden,true);
        const screenToggle = secondary.children[0].children[0].children[0];
        screenToggle.checked=true;
        screenToggle.onchange();
        assert.equal(driver.hidden,false);
        driver.value='5';
        driver.onchange();
        assert.deepEqual(pinRows.children[0].children[1].options.map(option=>option.value),['32','64']);
        assert.deepEqual(pinRows.children.slice(2).filter(row=>!row.hidden)
            .map(row=>row.children[0].textContent),['SDA','SCL']);
        driver.value='6';
        driver.onchange();
        assert.equal(pinRows.children[0].children[1].value,'64');
        driver.value='4';
        driver.onchange();
        assert.equal(pinRows.children[0].children[1].value,'160');
        assert.equal(pinRows.children.slice(2).filter(row=>!row.hidden).length,6);
        driver.value='2';
        driver.onchange();
        assert.equal(pinRows.hidden,false);
        assert.equal(pinRows.children[0].children[1].value,'320');
        pinRows.children[4].children[1].value='44';
        pinRows.children[5].children[1].value='47';
        const card = cards.find(card=>card.dataset.nhom==='4');
        const toggle = card.children[0].children[0];
        const fields = card.children.at(-1);
        assert.equal(fields.hidden,true);
        toggle.checked=true;
        toggle.onchange();
        assert.equal(fields.hidden,false);
        toggle.checked=false;
        toggle.onchange();
        assert.equal(fields.hidden,true);
    }
    const [first, second] = inputs();
    const optionFor = (select, pin) => select.options.find(option=>option.value===String(pin));
    assert.match(optionFor(first,0).text,/GPIO 0 !.*BOOT/);
    assert.match(optionFor(first,19).text,/GPIO 19 !.*USB/);
    assert.match(optionFor(first,46).text,/GPIO 46 !.*Strapping/);
    assert.equal(optionFor(first,35),undefined);
    assert.match(optionFor(first,second.value).text,/Đã gán/);
    assert.match(optionFor(first,17).text,/Trống/);
    const originalPin = first.value;
    first.value = second.value;
    first.onchange();
    assert.equal(elements.get('xungdot').hidden,false);
    assert.equal(elements.get('luugpio').disabled,true);
    first.value = originalPin;
    first.onchange();
    assert.equal(elements.get('luugpio').disabled,false);
    elements.get('noidungma').value='#define PIN_'+first.dataset.chan.toUpperCase()+' 47';
    elements.get('apdungma').onclick();
    assert.equal(first.value,'47');
    elements.get('docgoc').onclick();
    assert.equal(first.value,originalPin);
    assert.equal(elements.get('maychu').value,'https://maychu');
    assert.equal(elements.get('trangthainhac').textContent,'Đã dừng');
    assert.equal(elements.get('gocmanhinh').value,90);
    assert.equal(elements.get('gocdangdung').textContent,'Góc đang dùng 90°');
    elements.get('gocmanhinh').value='270';
    status.goc_man_hinh=270;
    await elements.get('luugoc').onclick();
    assert.equal(requests.at(-2).url,'/api/lenh');
    assert.deepEqual(JSON.parse(requests.at(-2).options.body),{lenh:'xoay_man_hinh',gia_tri:270});
    assert.equal(elements.get('gocdangdung').textContent,'Góc đang dùng 270°');
    await elements.get('caidat').onsubmit({preventDefault(){}});
    const saved=requests.find(request=>request.url==='/api/cai-dat');
    assert(saved);
    assert.equal(elements.get('dieukhienrobot').hidden,true);
    const body=JSON.parse(saved.options.body);
    assert.deepEqual(body.man_chinh,{driver:1,width:240,height:240});
    assert.deepEqual(body.servo_mo_rong,{banh_trai:-1,banh_phai:-1,tay_trai:-1,tay_phai:-1});
    assert.deepEqual(body.chan_cam_bien,{tof_sda:11,tof_scl:12,tof_xshut:-1,trig:13,echo:14});
    assert.deepEqual(body.chan_den_cob,{r:-1,g:-1,b:-1,edison:-1,din:-1,rgb_anode:false});
    if (preview) {
        assert.deepEqual(body.man_phu,{driver:2,width:240,height:320,
            pins:{data:-1,clock:-1,cs:44,dc:47,rst:-1,bl:-1}});
    }
    assert.equal(elements.has('matkhau'),false);
    assert.equal(Object.hasOwn(body,'mat_khau_moi'),false);
    assert.equal(saved.options.headers.Authorization,undefined);
    assert.equal(body.robot_uart,false);
    assert.deepEqual(body.chan,Object.fromEntries(pins.map((pin,i)=>[pin,defaults[i]])));
    assert.equal(saved.options.headers['X-UTA-Yeu-Cau'],'1');
    assert.equal(body.goc_nha_1,85);
    assert.equal(body.toc_do_dc,60);
    elements.get('maychu').value='https://may-chu-khac/nhac';
    elements.get('driverdc').value='2';
    elements.get('batservo').checked=true;
    elements.get('batservo').onchange();
    assert.equal(elements.get('nhomchan').children.at(-1).hidden,false);
    elements.get('daotrai').checked=true;
    await elements.get('caidat').onsubmit({preventDefault(){}});
    const updated=JSON.parse(requests.at(-1).options.body);
    assert.equal(updated.may_chu_nhac,'https://may-chu-khac/nhac');
    assert.equal(updated.driver_dc,2);
    assert.equal(updated.bat_servo_dau,true);
    assert.equal(updated.bat_servo_co,false);
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
    console.log('Đạt: 23 GPIO, đổi URL máy chủ, chọn driver, lưu giới hạn servo, lệnh video/servo, nhấn/nhả robot và tên bài tiếng Việt.');
})().catch(error=>{console.error(error);process.exitCode=1;});
