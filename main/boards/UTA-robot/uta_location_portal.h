#pragma once

namespace uta {

// Chèn trước </body> của trang AP gốc. Form độc lập với form Wi-Fi/OTA.
inline constexpr char kLocationPortal[] = R"UTA(
<style>
#advanced-tab #ota_url_row { display: none !important; }
#advanced-tab #uta-ota-url {
    box-sizing: border-box;
    width: 100%;
    min-width: 0;
    min-height: 42px;
    padding: 10px 12px;
    font: inherit;
}
</style>
<script>
(() => {
    const tab = document.getElementById('advanced-tab');
    if (!tab) return;
    const otaSection = document.createElement('section');
    otaSection.className = 'section';
    otaSection.innerHTML = `<h3>Địa chỉ OTA</h3>
      <form id="uta-ota-form">
        <label for="uta-ota-url">URL OTA tùy chỉnh</label>
        <input id="uta-ota-url" type="text" inputmode="url" maxlength="2048" spellcheck="false"
               autocomplete="off" aria-describedby="uta-ota-hint" disabled>
        <p id="uta-ota-hint">Để trống và bấm Lưu URL OTA để dùng địa chỉ mặc định.</p>
        <p id="uta-ota-status" role="status" aria-live="polite">Đang đọc URL OTA…</p>
        <button id="uta-ota-save" type="submit" disabled>Lưu URL OTA</button>
      </form>`;
    tab.prepend(otaSection);
    const otaUrl = otaSection.querySelector('#uta-ota-url');
    const otaStatus = otaSection.querySelector('#uta-ota-status');
    const otaSave = otaSection.querySelector('#uta-ota-save');
    async function loadOta() {
        try {
            const response = await fetch('/uta/ota', {cache: 'no-store'});
            if (!response.ok) throw new Error('Không đọc được URL OTA từ bộ nhớ.');
            const data = await response.json();
            otaUrl.value = data.ota_url;
            otaUrl.disabled = otaSave.disabled = false;
            otaStatus.textContent = data.ota_url ? 'Đang dùng URL đã lưu.' : 'Đang dùng URL mặc định.';
        } catch (error) {
            otaStatus.textContent = error.message;
        }
    }
    otaSection.querySelector('#uta-ota-form').addEventListener('submit', async event => {
        event.preventDefault();
        const url = otaUrl.value.trim();
        otaUrl.disabled = otaSave.disabled = true;
        try {
            const response = await fetch('/uta/ota', {
                method: 'POST',
                headers: {'Content-Type': 'application/json', 'X-UTA-Yeu-Cau': '1'},
                body: JSON.stringify({ota_url: url})
            });
            if (response.status === 400) throw new Error('Nhập URL HTTP(S) hợp lệ, tối đa 2048 byte.');
            if (!response.ok) throw new Error('Không lưu được URL OTA vào bộ nhớ.');
            otaUrl.value = url;
            otaStatus.textContent = url ? 'Đã lưu URL OTA.' : 'Đã lưu lựa chọn dùng URL mặc định.';
        } catch (error) {
            otaStatus.textContent = error.message;
        } finally {
            otaUrl.disabled = otaSave.disabled = false;
        }
    });
    // The stock form still handles Wi-Fi settings. Keep OTA out of its payload so
    // saving Wi-Fi settings cannot overwrite the URL with its cached value.
    const advancedForm = tab.querySelector('form[action="/advanced/submit"]');
    if (advancedForm) {
        advancedForm.onsubmit = async event => {
            event.preventDefault();
            const button = document.getElementById('advanced_button');
            const status = document.getElementById('advanced_error');
            button.disabled = true;
            status.textContent = '';
            try {
                const response = await fetch('/advanced/submit', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({
                        max_tx_power: parseInt(document.getElementById('max_tx_power').value, 10),
                        remember_bssid: document.getElementById('remember_bssid').checked,
                        sleep_mode: document.getElementById('sleep_mode').checked
                    })
                });
                if (!response.ok || !(await response.json()).success)
                    throw new Error('Không lưu được cài đặt Wi-Fi nâng cao.');
                showToast('Đã lưu cài đặt Wi-Fi.');
            } catch (error) {
                status.textContent = error.message;
            } finally {
                button.disabled = false;
            }
        };
    }
    loadOta();
    const section = document.createElement('section');
    section.className = 'section';
    section.innerHTML = `<h3>Vị trí và thời tiết</h3>
      <form id="uta-location-form">
        <p><label for="uta-province">Tỉnh / thành phố</label>
        <input id="uta-province" type="text" list="uta-province-list" maxlength="80" placeholder="Để trống để định vị qua IP" disabled>
        <datalist id="uta-province-list"></datalist></p>
        <p>Gõ tên tỉnh/thành Việt Nam hoặc chọn từ gợi ý. Để trống để định vị qua IP khi có Internet.
        Vị trí IP chỉ gần đúng; thời tiết sử dụng vị trí này.</p>
        <p>Dữ liệu thời tiết: <a href="https://open-meteo.com/" target="_blank" rel="noopener">Open-Meteo</a>.</p>
        <p id="uta-location-status" role="status" aria-live="polite">Đang đọc cài đặt vị trí…</p>
        <button id="uta-location-save" type="submit" disabled>Lưu vị trí</button>
        <button id="uta-location-reload" type="button">Đọc lại</button>
      </form>`;
    tab.prepend(section);
    const form = section.querySelector('#uta-location-form');
    const province = section.querySelector('#uta-province');
    const suggestions = section.querySelector('#uta-province-list');
    const status = section.querySelector('#uta-location-status');
    const save = section.querySelector('#uta-location-save');
    const reload = section.querySelector('#uta-location-reload');
    let ready = false;
    let provinces = [];
    const normalize = value => value.normalize('NFD').replace(/[\u0300-\u036f]/g,'')
        .toLowerCase().replace(/đ/g,'d').replace(/[.]/g,' ')
        .trim().replace(/\s+/g,' ').replace(/^(?:tp|thanh pho|tinh)\s+/,'');
    async function load() {
        save.disabled = true;
        province.disabled = true;
        reload.disabled = true;
        try {
            const response = await fetch('/uta/location', {cache: 'no-store'});
            if (!response.ok) throw new Error('Không đọc được cài đặt vị trí.');
            const data = await response.json();
            provinces = data.provinces;
            suggestions.replaceChildren(...provinces.map(item => {
                const option = document.createElement('option');
                option.value = item.name;
                return option;
            }));
            province.value = provinces.find(item => item.code === data.province)?.name ?? '';
            if (otaUrl) otaUrl.placeholder = data.ota_default ?? '';
            ready = true;
            status.textContent = data.province
                ? 'Đã lưu: ' + province.value
                : 'Đang dùng định vị IP. Thiết bị cần kết nối Internet để lấy vị trí.';
        } catch (error) {
            ready = false;
            status.textContent = error.message;
        } finally {
            province.disabled = !ready;
            save.disabled = !ready;
            reload.disabled = false;
        }
    }
    form.addEventListener('submit', async event => {
        event.preventDefault();
        if (!ready) return;
        const entered = province.value.trim();
        const match = entered && provinces.find(item => normalize(item.name) === normalize(entered));
        if (entered && !match) {
            status.textContent = 'Chưa nhận ra tên tỉnh/thành. Anh kiểm tra tên hoặc chọn một gợi ý.';
            return;
        }
        const code = match ? match.code : '';
        save.disabled = true;
        province.disabled = true;
        reload.disabled = true;
        try {
            const response = await fetch('/uta/location', {
                method: 'POST',
                headers: {'Content-Type': 'application/json', 'X-UTA-Yeu-Cau': '1'},
                body: JSON.stringify({province: code})
            });
            if (!response.ok) throw new Error('Chưa lưu được vị trí. Anh thử lại nhé.');
            province.value = match ? match.name : '';
            status.textContent = code
                ? 'Đã lưu vị trí. Cài đặt được giữ sau khi mất nguồn.'
                : 'Đã bật tự động qua IP. Vị trí sẽ cập nhật khi thiết bị có Internet.';
        } catch (error) {
            status.textContent = error.message;
        } finally {
            save.disabled = false;
            province.disabled = false;
            reload.disabled = false;
        }
    });
    reload.addEventListener('click', load);
    load();
})();
</script>
)UTA";

}  // namespace uta
