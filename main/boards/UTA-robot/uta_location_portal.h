#pragma once

namespace uta {

// Chèn trước </body> của trang AP gốc. Form độc lập với form Wi-Fi/OTA.
inline constexpr char kLocationPortal[] = R"UTA(
<style>
#advanced-tab #ota_url_row { display: block !important; }
#advanced-tab #ota_url {
    box-sizing: border-box;
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
    // Reuse the native advanced form and its wifi/ota_url storage.
    const otaUrl = document.getElementById('ota_url');
    const otaRow = document.getElementById('ota_url_row');
    if (otaUrl && otaRow) {
        otaUrl.inputMode = 'url';
        otaUrl.maxLength = 255;
        otaUrl.pattern = 'https?://.+';
        otaUrl.setAttribute('aria-describedby', 'uta-ota-hint');
        const hint = document.createElement('small');
        hint.id = 'uta-ota-hint';
        hint.textContent = 'Lưu trong tab Nâng cao để dùng URL này khi firmware kiểm tra cập nhật. Để trống để dùng URL mặc định.';
        otaRow.append(hint);
    }
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
