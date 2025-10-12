#pragma once

#include <string>
#include <cstdio>

struct WorldTime {
    int day = 1;
    int hour = 8;     // 从上午8点开始
    int minute = 0;

    // 每分钟等于多少游戏tick
    static constexpr int MINUTES_PER_TICK = 5;

    void update(int ticks = 1);

    // 获取格式化的时间字符串
    std::string getTimeString() const;

    // 获取总分钟数
    int getTotalMinutes() const;

    // 判断是否为夜晚时间 (19:00 - 6:00)
    bool isNight() const;

    // 判断是否为傍晚时间 (17:00 - 19:00)
    bool isEvening() const;

    // 判断是否为深夜时间 (22:00 - 5:00)
    bool isLateNight() const;

    // 获取光照等级 (0.0 - 1.0，1.0为最亮)
    float getLightLevel() const;

    // 获取昼夜描述
    std::string getDayPhase() const;
};

// 世界时间组件
struct WorldTimeComponent {
    WorldTime worldTime;
};