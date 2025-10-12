#include "components/ZoneComponent.h"
#include <utility>

ZoneComponent::ZoneComponent(ZoneType t, const std::string& n, int posX, int posY, int w, int h)
    : type(t), name(n), x(posX), y(posY), width(w), height(h) {
    // 根据类型设置属性
    if (t == ZoneType::RESIDENCE) {
        isPrivate = true;
        isSocialZone = false;
    } else if (t == ZoneType::TAVERN) {
        isPrivate = false;
        isSocialZone = true;
    }
}

bool ZoneComponent::contains(int pointX, int pointY) const {
    return pointX >= x && pointX < x + width &&
           pointY >= y && pointY < y + height;
}

std::pair<int, int> ZoneComponent::getCenter() const {
    return {x + width / 2, y + height / 2};
}

std::string ZoneComponent::getTypeName() const {
    switch (type) {
        case ZoneType::RESIDENCE: return "住宅";
        case ZoneType::TAVERN: return "酒馆";
        case ZoneType::MARKET: return "市场";
        case ZoneType::WORKPLACE: return "工作场所";
        case ZoneType::PUBLIC: return "公共区域";
        default: return "未知";
    }
}