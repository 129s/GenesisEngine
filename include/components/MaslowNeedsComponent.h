#pragma once

#include <string>

// 马斯洛需求层次组件
struct MaslowNeedsComponent {
    // 生理需求 (0.0 - 1.0, 1.0为最迫切)
    float physiological = 0.8f;  // 饥饿度等基本生理需求 - 设置为高值以便测试

    // 疲劳度 (0.0 - 1.0, 1.0为最疲劳)
    float fatigue = 0.0f;        // 疲劳度，活动时消耗，休息时恢复

    // 安全需求 (暂时保留，后续扩展)
    float safety = 0.0f;

    // 社交需求 (暂时保留，后续扩展)
    float loveBelonging = 0.0f;

    // 尊重需求 (暂时保留，后续扩展)
    float esteem = 0.0f;

    // 自我实现需求 (暂时保留，后续扩展)
    float selfActualization = 0.0f;

    // 需求阈值，超过此值会产生行动动机
    // 注意：这些阈值现在由配置系统管理，但这里保留默认值作为备用
    static constexpr float HUNGER_THRESHOLD = 0.7f;
    static constexpr float CRITICAL_HUNGER_THRESHOLD = 0.9f;
    static constexpr float SOCIAL_THRESHOLD = 0.6f;  // 社交需求阈值
    static constexpr float FATIGUE_THRESHOLD = 0.7f; // 疲劳阈值
    static constexpr float CRITICAL_FATIGUE_THRESHOLD = 0.9f; // 极度疲劳阈值

    // 获取最主要的需求类型
    std::string getMostUrgentNeed() const;

    // 是否需要寻找食物
    bool needsFood() const;

    // 是否需要休息
    bool needsRest() const;

    // 是否需要社交
    bool needsSocialization() const;
};