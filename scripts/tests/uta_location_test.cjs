const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert/strict');
const root = path.resolve(__dirname, '../..');
const source = fs.readFileSync(path.join(root, 'main/boards/UTA-robot/uta_location_portal.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];

class Element {
    constructor() { this.value = ''; this.disabled = false; this.options = []; this.events = {}; }
    addEventListener(name, callback) { this.events[name] = callback; }
    setAttribute(name, value) { this[name] = value; }
    append(child) { this.child = child; }
    add(option) { this.options.push(option); }
    replaceChildren(...options) { this.options = options; }
    get selectedIndex() { return this.options.findIndex(option => option.value === this.value); }
}

async function scenario({code = '', loadOk = true, postOk = true, networkError = false} = {}) {
    const ids = ['form', 'province', 'status', 'save', 'reload'];
    const nodes = Object.fromEntries(ids.map(id => ['uta-location-' + id, new Element()]));
    nodes['uta-province'] = nodes['uta-location-province'];
    nodes['uta-province-list'] = new Element();
    const requests = [];
    const tab = {prepend(section) { this.section = section; }};
    const section = {querySelector(selector) { return nodes[selector.slice(1)]; }};
    const otaUrl = new Element();
    const otaRow = new Element();
    const sandbox = {
        document: {
            getElementById: id => ({'advanced-tab':tab, ota_url:otaUrl, ota_url_row:otaRow})[id],
            createElement: tag => tag === 'section' ? section : new Element(),
        },
        Option: function(text, value) { this.textContent = text; this.value = value; },
        async fetch(url, options) {
            requests.push({url, options});
            if (networkError) throw new Error('Mất kết nối');
            return {ok: options.method === 'POST' ? postOk : loadOk, async json() {
                return {province: code, ota_default: 'https://firmware.example/ota/',
                    provinces: [{code: '01', name: 'Hà Nội'}, {code: '79', name: 'TP. Hồ Chí Minh'}]};
            }};
        }
    };
    vm.runInNewContext(script, sandbox);
    await new Promise(resolve => setImmediate(resolve));
    return {nodes, requests, tab, otaUrl, otaRow};
}

(async () => {
    let result = await scenario({code: '79'});
    assert(result.tab.section, 'Vị trí phải nằm trong tab nâng cao');
    assert.equal(result.otaUrl.type, 'url');
    assert.equal(result.otaUrl.maxLength, 255);
    assert.match(result.otaRow.child.textContent, /URL mặc định/);
    assert.equal(result.otaUrl.placeholder, 'https://firmware.example/ota/');
    assert.equal(result.nodes['uta-province'].value, 'TP. Hồ Chí Minh');
    assert.match(result.nodes['uta-location-status'].textContent, /Hồ Chí Minh/);
    result.nodes['uta-province'].value = '  ha noi  ';
    await result.nodes['uta-location-form'].events.submit({preventDefault() {}});
    assert.equal(result.requests.at(-1).url, '/uta/location');
    assert.deepEqual(JSON.parse(result.requests.at(-1).options.body), {province: '01'});
    assert.equal(result.requests.at(-1).options.headers['X-UTA-Yeu-Cau'], '1');
    assert.match(result.nodes['uta-location-status'].textContent, /Đã lưu/);
    result.nodes['uta-province'].value = 'Tên chưa có';
    const before = result.requests.length;
    await result.nodes['uta-location-form'].events.submit({preventDefault() {}});
    assert.equal(result.requests.length, before);
    assert.match(result.nodes['uta-location-status'].textContent, /Chưa nhận ra/);
    result.nodes['uta-province'].value = '';
    await result.nodes['uta-location-form'].events.submit({preventDefault() {}});
    assert.deepEqual(JSON.parse(result.requests.at(-1).options.body), {province: ''});
    assert.match(result.nodes['uta-location-status'].textContent, /tự động qua IP/);

    result = await scenario({loadOk: false});
    assert.equal(result.nodes['uta-location-save'].disabled, true);
    await result.nodes['uta-location-form'].events.submit({preventDefault() {}});
    assert.equal(result.requests.length, 1, 'Không lưu đè cấu hình khi đọc thất bại');
    await result.nodes['uta-location-reload'].events.click();
    assert.equal(result.requests.length, 2, 'Có thể thử đọc lại sau lỗi');

    result = await scenario({postOk: false});
    await result.nodes['uta-location-form'].events.submit({preventDefault() {}});
    assert.match(result.nodes['uta-location-status'].textContent, /Chưa lưu/);
    assert.equal(result.nodes['uta-location-save'].disabled, false);
    result = await scenario({networkError: true});
    assert.equal(result.nodes['uta-location-save'].disabled, true);
    assert.match(result.nodes['uta-location-status'].textContent, /Mất kết nối/);

    // Contract với component được ghim: không sửa vendor nhưng phát hiện đổi template/route.
    const portalPath = path.join(root, 'managed_components/78__esp-wifi-connect/assets/wifi_configuration.html');
    if (fs.existsSync(portalPath)) {
        const portal = fs.readFileSync(portalPath, 'utf8');
        assert(portal.includes('id="advanced-tab"'));
        assert(portal.includes('</body>'));
    }
    console.log('Kiểm tra vị trí: đọc/lưu tỉnh, tự động IP, lỗi đọc/lưu/mạng đều đạt.');
})().catch(error => {console.error(error); process.exitCode = 1;});
