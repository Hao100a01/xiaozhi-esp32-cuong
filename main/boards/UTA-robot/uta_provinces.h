#pragma once

#include <array>
#include <string_view>

namespace uta {

struct Province {
    std::string_view code;
    std::string_view name;
    double latitude;
    double longitude;
};

// Mã tỉnh theo Quyết định 19/2025/QĐ-TTg; tọa độ đại diện khu vực trung tâm,
// không phải vị trí GPS của thiết bị hoặc tâm địa giới tỉnh sau sáp nhập.
inline constexpr std::array<Province, 34> kProvinces{{
    {"01", "Hà Nội", 21.0285, 105.8542},      {"04", "Cao Bằng", 22.6666, 106.2639},
    {"08", "Tuyên Quang", 21.8236, 105.2181}, {"11", "Điện Biên", 21.3860, 103.0230},
    {"12", "Lai Châu", 22.3862, 103.4703},    {"14", "Sơn La", 21.3256, 103.9188},
    {"15", "Lào Cai", 22.4809, 103.9755},     {"19", "Thái Nguyên", 21.5942, 105.8482},
    {"20", "Lạng Sơn", 21.8537, 106.7615},    {"22", "Quảng Ninh", 20.9500, 107.0800},
    {"24", "Bắc Ninh", 21.1861, 106.0763},    {"25", "Phú Thọ", 21.3227, 105.4019},
    {"31", "Hải Phòng", 20.8449, 106.6881},   {"33", "Hưng Yên", 20.6464, 106.0511},
    {"37", "Ninh Bình", 20.2506, 105.9745},   {"38", "Thanh Hóa", 19.8067, 105.7852},
    {"40", "Nghệ An", 18.6796, 105.6813},     {"42", "Hà Tĩnh", 18.3559, 105.8877},
    {"44", "Quảng Trị", 16.8163, 107.1003},   {"46", "Huế", 16.4637, 107.5909},
    {"48", "Đà Nẵng", 16.0544, 108.2022},     {"51", "Quảng Ngãi", 15.1205, 108.7923},
    {"52", "Gia Lai", 13.9833, 108.0000},     {"56", "Khánh Hòa", 12.2388, 109.1967},
    {"66", "Đắk Lắk", 12.6667, 108.0500},     {"68", "Lâm Đồng", 11.9404, 108.4583},
    {"75", "Đồng Nai", 10.9574, 106.8427},    {"79", "TP. Hồ Chí Minh", 10.7769, 106.7009},
    {"80", "Tây Ninh", 11.3352, 106.1099},    {"82", "Đồng Tháp", 10.4549, 105.6319},
    {"86", "Vĩnh Long", 10.2537, 105.9722},   {"91", "An Giang", 10.3864, 105.4352},
    {"92", "Cần Thơ", 10.0452, 105.7469},     {"96", "Cà Mau", 9.1769, 105.1524},
}};

constexpr const Province* FindProvince(std::string_view code) {
    for (const auto& province : kProvinces) {
        if (province.code == code)
            return &province;
    }
    return nullptr;
}

constexpr bool ValidProvinceCode(std::string_view code) {
    return code.empty() || FindProvince(code) != nullptr;
}

}  // namespace uta
