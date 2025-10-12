#pragma once

#include <string>
#include <vector>
#include <entt/entt.hpp>

// 一个简单的事实结构
struct Fact {
    std::string content;
    entt::entity source; // 事实来源 (entt::null代表道听途说)
    float confidence;    // 0.0 to 1.0

    Fact() = default;
    Fact(const std::string& factContent, entt::entity sourceEntity = entt::null, float factConfidence = 1.0f)
        : content(factContent), source(sourceEntity), confidence(factConfidence) {}
};

// 知识库组件
struct KnowledgeComponent {
    std::vector<Fact> knownFacts;

    KnowledgeComponent() = default;

    // 添加新事实
    void addFact(const Fact& fact);

    // 获取随机事实
    const Fact* getRandomFact() const;

    // 清空知识库
    void clear();

    // 获取知识库大小
    size_t size() const;
};