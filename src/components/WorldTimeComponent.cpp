#include "components/WorldTimeComponent.h"
#include <cstdio>

void WorldTime::update(int ticks) {
    minute += ticks * MINUTES_PER_TICK;

    // 时间进位
    while (minute >= 60) {
        minute -= 60;
        hour++;
    }

    while (hour >= 24) {
        hour -= 24;
        day++;
    }
}

std::string WorldTime::getTimeString() const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Day %d, %02d:%02d", day, hour, minute);
    return std::string(buffer);
}

int WorldTime::getTotalMinutes() const {
    return (day - 1) * 24 * 60 + hour * 60 + minute;
}

bool WorldTime::isNight() const {
    return hour >= 19 || hour < 6;
}

bool WorldTime::isEvening() const {
    return hour >= 17 && hour < 19;
}

bool WorldTime::isLateNight() const {
    return hour >= 22 || hour < 5;
}

float WorldTime::getLightLevel() const {
    if (hour >= 6 && hour < 8) {
        // 日出 6-8点
        return 0.3f + (hour - 6) * 0.35f;
    } else if (hour >= 8 && hour < 17) {
        // 白天 8-17点
        return 1.0f;
    } else if (hour >= 17 && hour < 19) {
        // 黄昏 17-19点
        return 1.0f - (hour - 17) * 0.35f;
    } else if (hour >= 19 && hour < 22) {
        // 傍晚 19-22点
        return 0.3f - (hour - 19) * 0.1f;
    } else {
        // 深夜 22-6点
        return 0.1f;
    }
}

std::string WorldTime::getDayPhase() const {
    if (hour >= 5 && hour < 8) return "黎明";
    else if (hour >= 8 && hour < 12) return "上午";
    else if (hour >= 12 && hour < 14) return "中午";
    else if (hour >= 14 && hour < 17) return "下午";
    else if (hour >= 17 && hour < 19) return "黄昏";
    else if (hour >= 19 && hour < 22) return "傍晚";
    else return "深夜";
}