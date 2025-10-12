#pragma once

// 食物类型枚举
enum class FoodType {
    BASIC,     // 基础食物 (橙色)
    FRUIT,     // 水果 (红色)
    VEGETABLE, // 蔬菜 (绿色)
    MEAT,      // 肉类 (深红色)
    BREAD,     // 面包 (黄色)
    DAIRY      // 乳制品 (白色)
};

// 食物组件
struct FoodComponent {
    FoodType type = FoodType::BASIC;
    float nutritionValue = 0.5f;  // 营养价值 (0.0 - 1.0)

    FoodComponent() = default;
    FoodComponent(FoodType foodType, float nutrition)
        : type(foodType), nutritionValue(nutrition) {}

    FoodComponent(float nutrition) : type(FoodType::BASIC), nutritionValue(nutrition) {}

    // 获取食物类型的颜色
    struct Color {
        unsigned char r, g, b, a;
        Color() : r(255), g(255), b(255), a(255) {}
        Color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255)
            : r(red), g(green), b(blue), a(alpha) {}
    };

    Color getColor() const {
        float brightness = 0.7f + (nutritionValue * 0.3f); // 自然亮度范围 0.7-1.0

        switch (type) {
            case FoodType::BASIC:
                return Color(
                    static_cast<unsigned char>(210 * brightness),
                    static_cast<unsigned char>(145 * brightness), // 自然的橙色
                    static_cast<unsigned char>(40 * brightness),
                    255
                );
            case FoodType::FRUIT:
                return Color(
                    static_cast<unsigned char>(220 * brightness),
                    static_cast<unsigned char>(80 * brightness),  // 自然的红色
                    static_cast<unsigned char>(80 * brightness),
                    255
                );
            case FoodType::VEGETABLE:
                return Color(
                    static_cast<unsigned char>(80 * brightness),   // 自然的绿色
                    static_cast<unsigned char>(180 * brightness),
                    static_cast<unsigned char>(80 * brightness),
                    255
                );
            case FoodType::MEAT:
                return Color(
                    static_cast<unsigned char>(160 * brightness), // 自然的红色
                    static_cast<unsigned char>(70 * brightness),
                    static_cast<unsigned char>(70 * brightness),
                    255
                );
            case FoodType::BREAD:
                return Color(
                    static_cast<unsigned char>(230 * brightness),
                    static_cast<unsigned char>(190 * brightness), // 自然的黄色
                    static_cast<unsigned char>(120 * brightness),
                    255
                );
            case FoodType::DAIRY:
                return Color(
                    static_cast<unsigned char>(240 * brightness),
                    static_cast<unsigned char>(235 * brightness), // 自然的白色
                    static_cast<unsigned char>(200 * brightness),
                    255
                );
            default:
                return Color(210, 145, 40, 255); // 自然橙色
        }
    }

    const char* getTypeName() const {
        switch (type) {
            case FoodType::BASIC: return "基础食物";
            case FoodType::FRUIT: return "水果";
            case FoodType::VEGETABLE: return "蔬菜";
            case FoodType::MEAT: return "肉类";
            case FoodType::BREAD: return "面包";
            case FoodType::DAIRY: return "乳制品";
            default: return "未知食物";
        }
    }
};