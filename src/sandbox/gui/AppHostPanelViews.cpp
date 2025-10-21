#include "sandbox/gui/AppHost.hpp"
#include "ImGuiLogSink.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Genesis::Sandbox::Gui
{
using json = nlohmann::json;

void AppHost::drawSceneTabContent()
{
    auto drawModeButton = [&](const char* label, SceneViewMode mode) {
        const bool active = (scene_view_mode_ == mode);
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.80f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.50f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.72f, 1.0f));
        }
        if (ImGui::Button(label))
        {
            scene_view_mode_ = mode;
        }
        if (active)
        {
            ImGui::PopStyleColor(3);
        }
    };

    drawModeButton("世界概览", SceneViewMode::Map);
    ImGui::SameLine();
    drawModeButton("节点细节", SceneViewMode::Node);

    ImGui::Separator();

    if (scene_view_mode_ == SceneViewMode::Map)
    {
        drawSceneWorldMapContent();
    }
    else
    {
        drawSceneNodeContent();
    }
}

void AppHost::drawInspectorPanel()
{
    if (!show_inspector_)
    {
        return;
    }

    if (!ImGui::Begin("Inspector", &show_inspector_))
    {
        ImGui::End();
        return;
    }

    if (!latest_snapshot_)
    {
        ImGui::TextUnformatted("Waiting for snapshot...");
        ImGui::End();
        return;
    }

    const auto& snapshot = *latest_snapshot_;
        const auto &tick = snapshot.telemetry;
        const RuntimeBridge::WorldAtlas *atlasPtr = runtime_bridge_ ? &runtime_bridge_->atlas() : nullptr;

        ImGui::InputTextWithHint("##InspectorSearch", "Search name/id/type", inspector_search_buffer_.data(), inspector_search_buffer_.size());
        std::string filterRaw(inspector_search_buffer_.data());
        std::string filterLower = filterRaw;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        const bool filterEmpty = filterLower.empty();

        auto toLowerString = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        };

        auto matchesFilter = [&](const std::string &text, std::uint32_t id, const std::string &extra) -> bool {
            if (filterEmpty)
            {
                return true;
            }
            if (!text.empty())
            {
                auto lowered = toLowerString(text);
                if (lowered.find(filterLower) != std::string::npos)
                {
                    return true;
                }
            }
            if (!extra.empty())
            {
                auto lowered = toLowerString(extra);
                if (lowered.find(filterLower) != std::string::npos)
                {
                    return true;
                }
            }
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%u", id);
            auto lowered = toLowerString(std::string{buffer});
            return lowered.find(filterLower) != std::string::npos;
        };

        const auto resourceTypeName = [](genesis::world::ResourceType type) -> const char * {
            switch (type)
            {
            case genesis::world::ResourceType::Food:
                return "Food";
            case genesis::world::ResourceType::Drink:
                return "Drink";
            case genesis::world::ResourceType::Social:
                return "Social";
            default:
                return "Unknown";
            }
        };

        ImGui::Separator();

        const float listWidth = 260.0f;
        const ImVec2 listSize{listWidth, ImGui::GetContentRegionAvail().y};
        ImGui::BeginChild("InspectorList", listSize, true);

        if (ImGui::CollapsingHeader("Agents", ImGuiTreeNodeFlags_DefaultOpen))
        {
            std::vector<std::size_t> indices(tick.agents.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(), [&](std::size_t lhs, std::size_t rhs) {
                const auto &a = tick.agents[lhs];
                const auto &b = tick.agents[rhs];
                if (a.name == b.name)
                {
                    return a.entityId < b.entityId;
                }
                if (a.name.empty())
                {
                    return false;
                }
                if (b.name.empty())
                {
                    return true;
                }
                return a.name < b.name;
            });

            for (std::size_t idx : indices)
            {
                const auto &agent = tick.agents[idx];
                std::string nodeName;
                if (atlasPtr)
                {
                    for (const auto &node : atlasPtr->nodes)
                    {
                        if (node.id.value == agent.location.value)
                        {
                            nodeName = node.name;
                            break;
                        }
                    }
                }

                if (!matchesFilter(agent.name, agent.entityId, nodeName))
                {
                    continue;
                }

                char label[128];
                if (!agent.name.empty())
                {
                    std::snprintf(label, sizeof(label), "%s [#%u]", agent.name.c_str(), agent.entityId);
                }
                else
                {
                    std::snprintf(label, sizeof(label), "Agent [#%u]", agent.entityId);
                }

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Agent && inspector_selected_primary_ == agent.entityId;
                ImGui::PushID(static_cast<int>(agent.entityId));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Agent;
                    inspector_selected_primary_ = agent.entityId;
                    inspector_selected_secondary_ = static_cast<std::uint32_t>(idx);
                    inspector_highlight_node_ = agent.location.value;
                    map_selected_node_ = agent.location.value;
                    if (inspector_follow_selection_)
                    {
                        scene_selected_node_ = agent.location.value;
                    }
                }
                ImGui::PopID();
                if (!nodeName.empty() && ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Node: %s", nodeName.c_str());
                }
            }
        }

        if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (std::size_t i = 0; i < tick.resources.size(); ++i)
            {
                const auto &resource = tick.resources[i];
                std::string nodeName;
                if (atlasPtr)
                {
                    for (const auto &node : atlasPtr->nodes)
                    {
                        if (node.id.value == resource.location.value)
                        {
                            nodeName = node.name;
                            break;
                        }
                    }
                }

                const std::string extra = std::string(resourceTypeName(resource.type)) + " " + nodeName;
                if (!matchesFilter(resource.name, resource.location.value, extra))
                {
                    continue;
                }

            char label[160];
            std::snprintf(label, sizeof(label), "%s (%s) [Node #%u]", resource.name.c_str(), resourceTypeName(resource.type), resource.location.value);

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Resource && inspector_selected_primary_ == static_cast<std::uint32_t>(i);
                ImGui::PushID(static_cast<int>(resource.location.value * 4096 + static_cast<std::uint32_t>(i)));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Resource;
                    inspector_selected_primary_ = static_cast<std::uint32_t>(i);
                    inspector_selected_secondary_ = resource.location.value;
                    inspector_highlight_node_ = resource.location.value;
                    map_selected_node_ = resource.location.value;
                }
                ImGui::PopID();
            }
        }

        if (atlasPtr && ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto &node : atlasPtr->nodes)
            {
                if (!matchesFilter(node.name, node.id.value, ""))
                {
                    continue;
                }

                char label[128];
                std::snprintf(label, sizeof(label), "%s [#%u]", node.name.c_str(), node.id.value);

                const bool selected = inspector_selection_type_ == InspectorSelectionType::Node && inspector_selected_primary_ == node.id.value;
                ImGui::PushID(static_cast<int>(node.id.value));
                if (ImGui::Selectable(label, selected))
                {
                    inspector_selection_type_ = InspectorSelectionType::Node;
                    inspector_selected_primary_ = node.id.value;
                    inspector_selected_secondary_ = 0;
                    inspector_highlight_node_ = node.id.value;
                    map_selected_node_ = node.id.value;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("InspectorDetails", ImVec2(0.0f, listSize.y), false);

        switch (inspector_selection_type_)
        {
        case InspectorSelectionType::None:
            ImGui::TextUnformatted("Select an entry on the left to view details.");
            break;
        case InspectorSelectionType::Agent:
        {
            const genesis::telemetry::AgentSnapshot *agent = nullptr;
            for (const auto &candidate : tick.agents)
            {
                if (candidate.entityId == inspector_selected_primary_)
                {
                    agent = &candidate;
                    break;
                }
            }

            if (!agent)
            {
                ImGui::Text("Agent #%u is not present in the current snapshot.", inspector_selected_primary_);
                break;
            }

            std::string nodeName = "(Unknown)";
            const RuntimeBridge::WorldAtlas::Node *nodeInfo = nullptr;
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.id.value == agent->location.value)
                    {
                        nodeName = node.name;
                        nodeInfo = &node;
                        break;
                    }
                }
            }

            if (!agent->name.empty())
            {
                ImGui::Text("%s", agent->name.c_str());
            }
            else
            {
                ImGui::Text("Agent #%u", agent->entityId);
            }
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("定位地图##agentFocus"))
            {
                main_view_active_tab_ = MainViewTab::Scene;
                browser_active_section_ = BrowserSection::Scene;
                scene_view_mode_ = SceneViewMode::Map;
                inspector_highlight_node_ = agent->location.value;
                map_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("打开节点视图##agentScene"))
            {
                main_view_active_tab_ = MainViewTab::Scene;
                browser_active_section_ = BrowserSection::Scene;
                scene_view_mode_ = SceneViewMode::Node;
                scene_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            bool followChanged = ImGui::Checkbox("Follow##agentFollow", &inspector_follow_selection_);
            if (followChanged && inspector_follow_selection_)
            {
                scene_selected_node_ = agent->location.value;
                map_selected_node_ = agent->location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("Copy JSON##agentCopy"))
            {
                json agentJson;
                agentJson["type"] = "agent";
                agentJson["step"] = tick.step;
                agentJson["snapshotVersion"] = snapshot.version;

                auto &agentNode = agentJson["agent"];
                agentNode["entityId"] = agent->entityId;
                if (!agent->name.empty())
                {
                    agentNode["name"] = agent->name;
                }
                auto &locationJson = agentNode["location"];
                locationJson["id"] = agent->location.value;
                locationJson["name"] = nodeName;
                if (nodeInfo)
                {
                    locationJson["parent"] = nodeInfo->parent.value;
                    locationJson["kind"] = static_cast<unsigned int>(nodeInfo->kind);
                    locationJson["position"] = {{"x", nodeInfo->position.x}, {"y", nodeInfo->position.y}};
                }

                json needsJson = json::array();
                for (const auto &need : tick.needs)
                {
                    if (need.entityId == agent->entityId)
                    {
                        needsJson.push_back({
                            {"name", need.needName},
                            {"value", need.value},
                            {"critical", need.critical}});
                    }
                }
                if (!needsJson.empty())
                {
                    agentJson["needs"] = std::move(needsJson);
                }

                const genesis::telemetry::ActionSnapshot *actionPtr = nullptr;
                for (const auto &entry : tick.actions)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        actionPtr = &entry;
                        break;
                    }
                }
                if (actionPtr)
                {
                    json actionJson{
                        {"label", actionPtr->currentAction},
                        {"queueLength", actionPtr->queueLength},
                        {"target", actionPtr->target.value},
                        {"speed", actionPtr->speed},
                        {"resourceType", resourceTypeName(actionPtr->resource)},
                        {"amount", actionPtr->amount},
                        {"reliefPerUnit", actionPtr->reliefPerUnit}};
                    agentJson["action"] = std::move(actionJson);
                }

                const genesis::telemetry::PlannerSnapshot *plannerPtr = nullptr;
                for (const auto &entry : tick.plannerDecisions)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        plannerPtr = &entry;
                        break;
                    }
                }
                if (plannerPtr)
                {
                    agentJson["planner"] = {
                        {"target", plannerPtr->target.value},
                        {"travelCost", plannerPtr->travelCost},
                        {"score", plannerPtr->score}};
                }

                const genesis::telemetry::MovementProgressSnapshot *movementPtr = nullptr;
                for (const auto &entry : tick.movementProgress)
                {
                    if (entry.entityId == agent->entityId)
                    {
                        movementPtr = &entry;
                        break;
                    }
                }
                if (movementPtr)
                {
                    agentJson["movement"] = {
                        {"from", movementPtr->from.value},
                        {"to", movementPtr->to.value},
                        {"progress", movementPtr->t01}};
                }

                json resourcesJson = json::array();
                for (const auto &res : tick.resources)
                {
                    if (res.location.value == agent->location.value)
                    {
                        resourcesJson.push_back({
                            {"name", res.name},
                            {"type", resourceTypeName(res.type)},
                            {"current", res.current},
                            {"capacity", res.capacity}});
                    }
                }
                if (!resourcesJson.empty())
                {
                    agentJson["resourcesAtLocation"] = std::move(resourcesJson);
                }

                std::vector<std::uint32_t> peers;
                for (const auto &other : tick.agents)
                {
                    if (other.entityId != agent->entityId && other.location.value == agent->location.value)
                    {
                        peers.push_back(other.entityId);
                    }
                }
                if (!peers.empty())
                {
                    agentJson["otherAgentsAtLocation"] = peers;
                }

                std::string serialized = agentJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                pushToast("Agent snapshot copied", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
            }
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("ID: %u", agent->entityId);
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Location: #%u %s", agent->location.value, nodeName.c_str());

            ImGui::Separator();

            std::vector<const genesis::telemetry::NeedSnapshot *> needs;
            for (const auto &need : tick.needs)
            {
                if (need.entityId == agent->entityId)
                {
                    needs.push_back(&need);
                }
            }
            if (!needs.empty())
            {
                if (ImGui::BeginTable("NeedsTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Need");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableSetupColumn("Critical");
                    ImGui::TableHeadersRow();

                    for (const auto *need : needs)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(need->needName.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.2f", need->value);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(need->critical ? "Yes" : "No");
                    }

                    ImGui::EndTable();
                }
            }
            else
            {
                ImGui::TextUnformatted("No need data.");
            }

            const genesis::telemetry::ActionSnapshot *action = nullptr;
            for (const auto &entry : tick.actions)
            {
                if (entry.entityId == agent->entityId)
                {
                    action = &entry;
                    break;
                }
            }

            if (action)
            {
                ImGui::Separator();
                ImGui::Text("Current Action: %s", action->currentAction.c_str());
                ImGui::Text("Target Node: #%u", action->target.value);
                ImGui::Text("Queue Length: %u", action->queueLength);
                ImGui::Text("Speed: %.2f", action->speed);
                ImGui::Text("Resource: %s · Amount %u", resourceTypeName(action->resource), action->amount);
            }

            const genesis::telemetry::PlannerSnapshot *planner = nullptr;
            for (const auto &entry : tick.plannerDecisions)
            {
                if (entry.entityId == agent->entityId)
                {
                    planner = &entry;
                    break;
                }
            }
            if (planner)
            {
                ImGui::Separator();
                ImGui::Text("Planner Target: #%u", planner->target.value);
                ImGui::Text("Travel Cost: %.2f", planner->travelCost);
                ImGui::Text("Score: %.2f", planner->score);
            }

            const genesis::telemetry::MovementProgressSnapshot *movement = nullptr;
            for (const auto &entry : tick.movementProgress)
            {
                if (entry.entityId == agent->entityId)
                {
                    movement = &entry;
                    break;
                }
            }
            if (movement)
            {
                ImGui::Separator();
                ImGui::Text("Movement Progress: %u → %u (%.2f)", movement->from.value, movement->to.value, movement->t01);
            }

            if (snapshot.diff)
            {
                bool printedHeader = false;
                for (const auto &change : snapshot.diff->needChanges)
                {
                    const auto *afterNeed = change.after ? &(*change.after) : nullptr;
                    const auto *beforeNeed = change.before ? &(*change.before) : nullptr;
                    if ((afterNeed && afterNeed->entityId == agent->entityId) || (beforeNeed && beforeNeed->entityId == agent->entityId))
                    {
                        if (!printedHeader)
                        {
                            ImGui::Separator();
                            ImGui::TextUnformatted("Need changes this frame");
                            printedHeader = true;
                        }
                        char delta[160];
                        if (beforeNeed && afterNeed)
                        {
                            std::snprintf(delta, sizeof(delta), "%s: %.2f → %.2f", afterNeed->needName.c_str(), beforeNeed->value, afterNeed->value);
                        }
                        else if (afterNeed)
                        {
                            std::snprintf(delta, sizeof(delta), "%s: added %.2f", afterNeed->needName.c_str(), afterNeed->value);
                        }
                        else
                        {
                            std::snprintf(delta, sizeof(delta), "%s: removed (%.2f)", beforeNeed->needName.c_str(), beforeNeed->value);
                        }
                        ImGui::BulletText("%s", delta);
                    }
                }
            }
            break;
        }
        case InspectorSelectionType::Resource:
        {
            if (inspector_selected_primary_ >= tick.resources.size())
            {
                ImGui::TextUnformatted("Selected resource index is stale.");
                break;
            }

            const auto &resource = tick.resources[inspector_selected_primary_];
            std::string nodeName = "(Unknown)";
            const RuntimeBridge::WorldAtlas::Node *nodeInfo = nullptr;
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.id.value == resource.location.value)
                    {
                        nodeName = node.name;
                        nodeInfo = &node;
                        break;
                    }
                }
            }
            ImGui::Text("%s", resource.name.c_str());
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("定位地图##resourceFocus"))
            {
                main_view_active_tab_ = MainViewTab::Scene;
                browser_active_section_ = BrowserSection::Scene;
                scene_view_mode_ = SceneViewMode::Map;
                inspector_highlight_node_ = resource.location.value;
                map_selected_node_ = resource.location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("打开节点视图##resourceScene"))
            {
                main_view_active_tab_ = MainViewTab::Scene;
                browser_active_section_ = BrowserSection::Scene;
                scene_view_mode_ = SceneViewMode::Node;
                scene_selected_node_ = resource.location.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("Copy JSON##resourceCopy"))
            {
                json resourceJson;
                resourceJson["type"] = "resource";
                resourceJson["step"] = tick.step;
                resourceJson["snapshotVersion"] = snapshot.version;

                auto &resourceNode = resourceJson["resource"];
                resourceNode["name"] = resource.name;
                resourceNode["type"] = resourceTypeName(resource.type);
                resourceNode["current"] = resource.current;
                resourceNode["capacity"] = resource.capacity;

                auto &locationJson = resourceNode["location"];
                locationJson["id"] = resource.location.value;
                locationJson["name"] = nodeName;
                if (nodeInfo)
                {
                    locationJson["parent"] = nodeInfo->parent.value;
                    locationJson["kind"] = static_cast<unsigned int>(nodeInfo->kind);
                    locationJson["position"] = {{"x", nodeInfo->position.x}, {"y", nodeInfo->position.y}};
                }

                json spawnsJson = json::array();
                if (atlasPtr)
                {
                    for (const auto &spawn : atlasPtr->spawns)
                    {
                        if (spawn.resource.location.value == resource.location.value && spawn.resource.name == resource.name)
                        {
                            json spawnJson{
                                {"position", {{"x", spawn.position.x}, {"y", spawn.position.y}}},
                                {"capacity", spawn.resource.capacity},
                                {"ratePerStep", spawn.resource.ratePerStep}};
                            if (spawn.resource.local_coord.has_value())
                            {
                                spawnJson["localCoord"] = {
                                    {"x", spawn.resource.local_coord->first},
                                    {"y", spawn.resource.local_coord->second}};
                            }
                            spawnsJson.push_back(std::move(spawnJson));
                        }
                    }
                }
                if (!spawnsJson.empty())
                {
                    resourceJson["spawns"] = std::move(spawnsJson);
                }

                json consumers = json::array();
                for (const auto &action : tick.actions)
                {
                    if (action.target.value == resource.location.value)
                    {
                        consumers.push_back({
                            {"entityId", action.entityId},
                            {"action", action.currentAction}});
                    }
                }
                if (!consumers.empty())
                {
                    resourceJson["activeAgents"] = std::move(consumers);
                }

                std::string serialized = resourceJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                pushToast("Resource snapshot copied", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
            }
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("Node: #%u %s", resource.location.value, nodeName.c_str());

            ImGui::Text("Type: %s", resourceTypeName(resource.type));
            ImGui::Text("Inventory: %u / %u", resource.current, resource.capacity);
            break;
        }
        case InspectorSelectionType::Node:
        {
            if (!atlasPtr)
            {
                ImGui::TextUnformatted("暂无节点信息。");
                break;
            }

            const RuntimeBridge::WorldAtlas::Node *selectedNode = nullptr;
            for (const auto &node : atlasPtr->nodes)
            {
                if (node.id.value == inspector_selected_primary_)
                {
                    selectedNode = &node;
                    break;
                }
            }

            if (!selectedNode)
            {
                ImGui::Text("节点 #%u 不存在。", inspector_selected_primary_);
                break;
            }

            ImGui::Text("%s", selectedNode->name.c_str());
            ImGui::SameLine(0.0f, 12.0f);
            if (ImGui::Button("定位地图##nodeFocus"))
            {
                main_view_active_tab_ = MainViewTab::Scene;
                browser_active_section_ = BrowserSection::Scene;
                scene_view_mode_ = SceneViewMode::Map;
                inspector_highlight_node_ = selectedNode->id.value;
                map_selected_node_ = selectedNode->id.value;
            }
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button("Copy JSON##nodeCopy"))
            {
                json nodeJson;
                nodeJson["type"] = "node";
                nodeJson["step"] = tick.step;
                nodeJson["snapshotVersion"] = snapshot.version;

                auto &nodeObj = nodeJson["node"];
                nodeObj["id"] = selectedNode->id.value;
                nodeObj["name"] = selectedNode->name;
                nodeObj["parent"] = selectedNode->parent.value;
                nodeObj["kind"] = static_cast<unsigned int>(selectedNode->kind);
                nodeObj["position"] = {{"x", selectedNode->position.x}, {"y", selectedNode->position.y}};

                json children = json::array();
                for (const auto &n : atlasPtr->nodes)
                {
                    if (n.parent.value == selectedNode->id.value && n.id.value != selectedNode->id.value)
                    {
                        children.push_back({{"id", n.id.value}, {"name", n.name}});
                    }
                }
                if (!children.empty())
                {
                    nodeObj["children"] = std::move(children);
                }

                json edges = json::array();
                for (const auto &edge : atlasPtr->edges)
                {
                    if (edge.from.value == selectedNode->id.value || edge.to.value == selectedNode->id.value)
                    {
                        edges.push_back({
                            {"from", edge.from.value},
                            {"to", edge.to.value},
                            {"bidirectional", edge.bidirectional}});
                    }
                }
                if (!edges.empty())
                {
                    nodeJson["edges"] = std::move(edges);
                }

                json resourcesJson = json::array();
                for (const auto &res : tick.resources)
                {
                    if (res.location.value == selectedNode->id.value)
                    {
                        resourcesJson.push_back({
                            {"name", res.name},
                            {"type", resourceTypeName(res.type)},
                            {"current", res.current},
                            {"capacity", res.capacity}});
                    }
                }
                if (!resourcesJson.empty())
                {
                    nodeJson["resources"] = std::move(resourcesJson);
                }

                json agentsJson = json::array();
                for (const auto &agent : tick.agents)
                {
                    if (agent.location.value == selectedNode->id.value)
                    {
                        agentsJson.push_back({
                            {"entityId", agent.entityId},
                            {"name", agent.name}});
                    }
                }
                if (!agentsJson.empty())
                {
                    nodeJson["agents"] = std::move(agentsJson);
                }

                std::string serialized = nodeJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                pushToast("Node snapshot copied", ImVec4(0.62f, 0.84f, 0.58f, 1.0f));
            }
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::Text("ID: %u", selectedNode->id.value);

            ImGui::Text("Parent: %u", selectedNode->parent.value);
            ImGui::Text("Kind: %u", static_cast<unsigned int>(selectedNode->kind));
            break;
        }
        }

        ImGui::Separator();
        if (!snapshot.events.empty())
        {
            ImGui::TextUnformatted("Runtime events this frame");
            if (ImGui::BeginChild("InspectorEventsLog", ImVec2(0, 140.0f), true))
            {
                for (const auto &evt : snapshot.events)
                {
                    ImGui::TextColored(evt.success ? ImVec4(0.62f, 0.84f, 0.58f, 1.0f) : ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
                                       "[#%llu] %s", static_cast<unsigned long long>(evt.id), evt.label.c_str());
                    if (!evt.message.empty())
                    {
                        ImGui::BulletText("%s", evt.message.c_str());
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::End();
    }

void AppHost::drawSceneWorldMapContent()
    {
        if (!runtime_bridge_)
        {
            ImGui::TextUnformatted("RuntimeBridge 未就绪。");
            return;
        }

        const auto& atlas = runtime_bridge_->atlas();
        const auto* agentPositionsPtr = latest_snapshot_ ? &latest_snapshot_->agentPositions : nullptr;

        ImGui::Checkbox("显示实体##Agents", &show_agent_overlay_);
        ImGui::SameLine();
        ImGui::Checkbox("描绘轨迹##Trails", &show_agent_trails_);
        ImGui::SameLine();
        ImGui::Checkbox("插值平滑##Interpolate", &map_interpolate_);
        if (show_agent_trails_)
        {
            ImGui::SameLine();
            int trailSamples = static_cast<int>(agent_trail_samples_);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderInt("轨迹长度", &trailSamples, 4, 64))
            {
                agent_trail_samples_ = static_cast<std::size_t>(trailSamples);
                for (auto &[id, trail] : agent_trails_)
                {
                    while (trail.size() > agent_trail_samples_)
                    {
                        trail.pop_front();
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("重置视图"))
        {
            resetMapViewCamera();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("滚轮缩放｜右键拖拽｜Space 重置");

        const ImVec4 colorMove{0.30f, 0.63f, 0.96f, 1.0f};
        const ImVec4 colorConsume{0.97f, 0.62f, 0.24f, 1.0f};
        const ImVec4 colorIdle{0.66f, 0.66f, 0.66f, 1.0f};
        const ImVec4 colorUnknown{0.82f, 0.52f, 0.90f, 1.0f};
        const ImU32 highlightColor = ImGui::GetColorU32(ImVec4(0.98f, 0.83f, 0.37f, 1.0f));

        if (show_agent_overlay_)
        {
            ImGui::Spacing();
            auto legendEntry = [](const char* id, const char* text, const ImVec4& color)
            {
                ImGui::ColorButton(id, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(12.0f, 12.0f));
                ImGui::SameLine();
                ImGui::TextUnformatted(text);
            };
            legendEntry("##legend_move", "移动", colorMove);
            ImGui::SameLine();
            legendEntry("##legend_consume", "消耗", colorConsume);
            ImGui::SameLine();
            legendEntry("##legend_idle", "空闲", colorIdle);
            ImGui::SameLine();
            legendEntry("##legend_other", "其它", colorUnknown);
            ImGui::Separator();
        }
        else
        {
            ImGui::Spacing();
            ImGui::Separator();
        }

        std::unordered_map<std::uint32_t, std::vector<const genesis::telemetry::ResourceSnapshot *>> resourcesByLocation;
        if (latest_snapshot_)
        {
            for (const auto &resource : latest_snapshot_->telemetry.resources)
            {
                resourcesByLocation[resource.location.value].push_back(&resource);
            }
        }

        const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasExtent{std::max(120.0f, canvasAvail.x), std::max(120.0f, canvasAvail.y)};
        const ImVec2 canvasMax{canvasPos.x + canvasExtent.x, canvasPos.y + canvasExtent.y};

        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("MapView.Canvas", canvasExtent, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool canvasHovered = ImGui::IsItemHovered();
        const bool canvasActive = ImGui::IsItemActive();

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
        drawList->AddRectFilled(canvasPos, canvasMax, bgColor);
        drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));

        ImGuiIO &io = ImGui::GetIO();

        if (!atlas.nodes.empty())
        {
            const float padding = 28.0f;
            const float width = std::max(atlas.extent.x, 1.0f);
            const float height = std::max(atlas.extent.y, 1.0f);
            const float scaleX = (canvasExtent.x - padding * 2.0f) / width;
            const float scaleY = (canvasExtent.y - padding * 2.0f) / height;
            const ImVec2 canvasOrigin{canvasPos.x + padding, canvasPos.y + padding};
            const ImVec2 worldHalf{width * scaleX * 0.5f, height * scaleY * 0.5f};
            const ImVec2 baseCenter{canvasOrigin.x + worldHalf.x, canvasOrigin.y + worldHalf.y};

            if (canvasHovered && io.MouseWheel != 0.0f)
            {
                const float zoomStep = io.MouseWheel > 0.0f ? 1.1f : 0.9f;
                const float newZoom = std::clamp(map_zoom_ * zoomStep, 0.25f, 5.0f);

                ImVec2 delta{io.MousePos.x - (baseCenter.x + map_pan_x_), io.MousePos.y - (baseCenter.y + map_pan_y_)};
                ImVec2 worldDelta{delta.x / map_zoom_, delta.y / map_zoom_};

                map_zoom_ = newZoom;
                ImVec2 newPan{
                    io.MousePos.x - baseCenter.x - worldDelta.x * map_zoom_,
                    io.MousePos.y - baseCenter.y - worldDelta.y * map_zoom_};
                map_pan_x_ = newPan.x;
                map_pan_y_ = newPan.y;
            }

            if (canvasActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            {
                ImVec2 delta = io.MouseDelta;
                map_pan_x_ += delta.x;
                map_pan_y_ += delta.y;
            }

            if (canvasHovered && ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                resetMapViewCamera();
            }

            const float nodeLabelZoomThreshold = 0.75f;
            const float resourceOverlayZoomThreshold = 0.60f;
            const float agentLabelZoomThreshold = 0.90f;

            auto toScreen = [&](const RuntimeBridge::Vector2 &pos)
            {
                const ImVec2 base{pos.x * scaleX, pos.y * scaleY};
                const ImVec2 centered{(base.x - worldHalf.x) * map_zoom_, (base.y - worldHalf.y) * map_zoom_};
                return ImVec2(
                    baseCenter.x + map_pan_x_ + centered.x,
                    baseCenter.y + map_pan_y_ + centered.y);
            };

            std::unordered_map<std::uint32_t, ImVec2> nodePositions;
            nodePositions.reserve(atlas.nodes.size());
            for (const auto &node : atlas.nodes)
            {
                nodePositions.emplace(node.id.value, toScreen(node.position));
            }

            // Selected node edges (parent/children only)
            std::optional<std::uint32_t> selectedEdgeNode = map_selected_node_ ? map_selected_node_ : inspector_highlight_node_;
            const RuntimeBridge::WorldAtlas::Node *selectedNodeInfo = nullptr;
            if (selectedEdgeNode)
            {
                for (const auto &node : atlas.nodes)
                {
                    if (node.id.value == *selectedEdgeNode)
                    {
                        selectedNodeInfo = &node;
                        break;
                    }
                }
            }

            if (selectedNodeInfo)
            {
                auto selectedPosIt = nodePositions.find(selectedNodeInfo->id.value);
                if (selectedPosIt != nodePositions.end())
                {
                    const ImU32 parentEdgeColor = ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.35f, 1.0f));
                    const ImU32 childEdgeColor = ImGui::GetColorU32(ImVec4(0.38f, 0.72f, 0.96f, 1.0f));

                    if (selectedNodeInfo->parent.value != 0 && selectedNodeInfo->parent.value != selectedNodeInfo->id.value)
                    {
                        auto parentPosIt = nodePositions.find(selectedNodeInfo->parent.value);
                        if (parentPosIt != nodePositions.end())
                        {
                            drawList->AddLine(selectedPosIt->second, parentPosIt->second, parentEdgeColor, 2.4f);
                        }
                    }

                    for (const auto &node : atlas.nodes)
                    {
                        if (node.parent.value != selectedNodeInfo->id.value)
                        {
                            continue;
                        }
                        auto childPosIt = nodePositions.find(node.id.value);
                        if (childPosIt != nodePositions.end())
                        {
                            drawList->AddLine(selectedPosIt->second, childPosIt->second, childEdgeColor, 2.4f);
                        }
                    }
                }
            }

            // Prepare for label overlap avoidance
            std::vector<ImRect> placedLabels;
            placedLabels.reserve(atlas.nodes.size());

            auto nodeKindIcon = [](genesis::world::LocationKind kind) -> const char*
            {
                switch (kind)
                {
                case genesis::world::LocationKind::Region: return "R";
                case genesis::world::LocationKind::Building: return "B";
                case genesis::world::LocationKind::Room: return "r";
                case genesis::world::LocationKind::Point: return "."; // simple dot placeholder
                }
                return "?";
            };

            const float nodeRadius = 12.0f;

            for (const auto &node : atlas.nodes)
            {
                auto it = nodePositions.find(node.id.value);
                if (it == nodePositions.end())
                {
                    continue;
                }

                const float radius = nodeRadius;
                ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.56f, 0.63f, 0.87f, 0.90f));
                switch (node.kind)
                {
                case genesis::world::LocationKind::Region:
                    fillColor = ImGui::GetColorU32(ImVec4(0.36f, 0.62f, 0.51f, 0.90f));
                    break;
                case genesis::world::LocationKind::Building:
                    fillColor = ImGui::GetColorU32(ImVec4(0.70f, 0.54f, 0.34f, 0.90f));
                    break;
                case genesis::world::LocationKind::Room:
                    fillColor = ImGui::GetColorU32(ImVec4(0.83f, 0.68f, 0.43f, 0.90f));
                    break;
                case genesis::world::LocationKind::Point:
                    fillColor = ImGui::GetColorU32(ImVec4(0.56f, 0.63f, 0.87f, 0.90f));
                    break;
                }

                drawList->AddCircleFilled(it->second, radius, fillColor, 20);
                drawList->AddCircle(it->second, radius, ImGui::GetColorU32(ImGuiCol_Border), 20, 1.5f);

                const bool isSelected = map_selected_node_ && node.id.value == *map_selected_node_;
                const bool highlighted = (inspector_highlight_node_ && node.id.value == *inspector_highlight_node_) || isSelected;
                if (highlighted)
                {
                    drawList->AddCircle(it->second, radius + 4.0f, highlightColor, 24, 2.5f);
                }

                // Icon inside the node
                const char* icon = nodeKindIcon(node.kind);
                const ImVec2 iconSize = ImGui::CalcTextSize(icon);
                const ImVec2 iconPos{it->second.x - iconSize.x * 0.5f, it->second.y - iconSize.y * 0.5f};
                drawList->AddText(iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon);

                // Label to the right, avoid overlap; force when hovered/selected
                bool hovered = false;
                if (canvasHovered)
                {
                    const ImVec2 mouse = io.MousePos;
                    const float dx = mouse.x - it->second.x;
                    const float dy = mouse.y - it->second.y;
                    hovered = (dx * dx + dy * dy) <= (radius * radius);
                }
                const ImVec2 nameSize = ImGui::CalcTextSize(node.name.c_str());
                const ImVec2 labelPos{it->second.x + radius + 6.0f, it->second.y - nameSize.y * 0.5f};
                ImRect labelRect{labelPos, ImVec2(labelPos.x + nameSize.x, labelPos.y + nameSize.y)};
                bool overlaps = false;
                for (const auto& r : placedLabels)
                {
                    if (r.Overlaps(labelRect)) { overlaps = true; break; }
                }
                const bool forceLabel = hovered || highlighted;
                if ((map_zoom_ >= nodeLabelZoomThreshold || forceLabel) && (!overlaps || forceLabel))
                {
                    drawList->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), node.name.c_str());
                    placedLabels.push_back(labelRect);
                }

                // Click to open Scene View on this node
                if (canvasHovered)
                {
                    const ImVec2 mouse = io.MousePos;
                    const float dx = mouse.x - it->second.x;
                    const float dy = mouse.y - it->second.y;
                    if ((dx * dx + dy * dy) <= (radius * radius))
                    {
                        if (hovered)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", node.name.c_str());
                            ImGui::Separator();
                            ImGui::Text("ID: %u", node.id.value);
                            ImGui::Text("Kind: %u", static_cast<unsigned int>(node.kind));
                            ImGui::EndTooltip();
                        }
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            map_selected_node_ = node.id.value;
                            scene_selected_node_ = node.id.value;
                            main_view_active_tab_ = MainViewTab::Scene;
                            browser_active_section_ = BrowserSection::Scene;
                            scene_view_mode_ = SceneViewMode::Node;
                        }
                    }
                }
            }

            if (canvasHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            {
                const ImVec2 drag = io.MouseDragMaxDistanceAbs[ImGuiMouseButton_Right];
                if (drag.x < 4.0f && drag.y < 4.0f)
                {
                    map_selected_node_.reset();
                }
            }

            for (const auto &spawn : atlas.spawns)
            {
                auto nodePosIt = nodePositions.find(spawn.resource.location.value);
                ImVec2 markerBase = nodePosIt != nodePositions.end() ? nodePosIt->second : toScreen(spawn.position);
                markerBase = ImVec2(std::floor(markerBase.x) + 0.5f, std::floor(markerBase.y) + 0.5f);
                markerBase.y += 22.0f;
                const ImVec2 a{markerBase.x - 6.0f, markerBase.y};
                const ImVec2 b{markerBase.x + 6.0f, markerBase.y};
                const ImVec2 c{markerBase.x, markerBase.y + 10.0f};
                const ImU32 markerColor = ImGui::GetColorU32(ImVec4(0.92f, 0.66f, 0.27f, 1.0f));
                drawList->AddTriangleFilled(a, b, c, markerColor);
                drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.2f);
            }

            if (latest_snapshot_)
            {
                auto colorForResource = [](genesis::world::ResourceType type) -> ImU32
                {
                    switch (type)
                    {
                    case genesis::world::ResourceType::Food:
                        return ImGui::GetColorU32(ImVec4(0.95f, 0.61f, 0.27f, 1.0f));
                    case genesis::world::ResourceType::Drink:
                        return ImGui::GetColorU32(ImVec4(0.27f, 0.61f, 0.95f, 1.0f));
                    case genesis::world::ResourceType::Social:
                        return ImGui::GetColorU32(ImVec4(0.48f, 0.76f, 0.47f, 1.0f));
                    default:
                        return ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    }
                };

                const ImU32 barBackground = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.18f, 0.9f));
                const ImU32 barBorder = ImGui::GetColorU32(ImGuiCol_Border);

                for (const auto &[locationId, resources] : resourcesByLocation)
                {
                    auto posIt = nodePositions.find(locationId);
                    if (posIt == nodePositions.end())
                    {
                        continue;
                    }
                    const bool forceOverlay = (map_selected_node_ && *map_selected_node_ == locationId) ||
                                              (inspector_highlight_node_ && *inspector_highlight_node_ == locationId);
                    if (map_zoom_ < resourceOverlayZoomThreshold && !forceOverlay)
                    {
                        continue;
                    }

                    const ImVec2 basePos = ImVec2(std::floor(posIt->second.x) + 0.5f, std::floor(posIt->second.y) + 0.5f);
                    float offsetY = 20.0f;

                    for (const auto *resource : resources)
                    {
                        const float capacity = static_cast<float>(resource->capacity);
                        const float current = static_cast<float>(resource->current);
                        const float ratio = capacity > 0.0f ? std::clamp(current / capacity, 0.0f, 1.0f) : 0.0f;

                        const ImVec2 barMin{std::floor(basePos.x - 28.0f) + 0.5f, std::floor(basePos.y + offsetY) + 0.5f};
                        const ImVec2 barMax{std::floor(basePos.x + 28.0f) + 0.5f, std::floor(barMin.y + 7.5f) + 0.5f};
                        drawList->AddRectFilled(barMin, barMax, barBackground, 3.0f);
                        const ImVec2 fillMax{barMin.x + (barMax.x - barMin.x) * ratio, barMax.y};
                        drawList->AddRectFilled(barMin, fillMax, colorForResource(resource->type), 3.0f);
                        drawList->AddRect(barMin, barMax, barBorder, 3.0f);

                        char buffer[48];
                        std::snprintf(buffer, sizeof(buffer), "%-12s %2u/%2u",
                                      resource->name.c_str(), resource->current, resource->capacity);
                        const ImVec2 textPos{std::floor(barMin.x) + 0.5f, std::floor(barMax.y + 1.0f) + 0.5f};
                        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, ImGui::GetColorU32(ImGuiCol_Text), buffer);

                        offsetY += 22.0f;
                    }
                }
            }

            if (show_agent_overlay_ && latest_snapshot_)
            {
                // Build movement progress map for interpolation (optional)
                std::unordered_map<std::uint32_t, std::tuple<RuntimeBridge::Vector2, RuntimeBridge::Vector2, float>> progress;
                if (map_interpolate_ && !latest_snapshot_->telemetry.movementProgress.empty())
                {
                    for (const auto &mp : latest_snapshot_->telemetry.movementProgress)
                    {
                        auto fromPos = atlas.nodePosition(mp.from).value_or(RuntimeBridge::Vector2{});
                        auto toPos = atlas.nodePosition(mp.to).value_or(fromPos);
                        progress.emplace(mp.entityId, std::make_tuple(fromPos, toPos, std::clamp(mp.t01, 0.0f, 1.0f)));
                    }
                }
                enum class AgentState
                {
                    Idle,
                    Move,
                    Consume,
                    Other
                };

                std::unordered_map<std::uint32_t, AgentState> agentStates;
                agentStates.reserve(latest_snapshot_->telemetry.actions.size());
                for (const auto &action : latest_snapshot_->telemetry.actions)
                {
                    AgentState state = AgentState::Other;
                    if (action.currentAction == "MoveTo")
                    {
                        state = AgentState::Move;
                    }
                    else if (action.currentAction == "ConsumeResource")
                    {
                        state = AgentState::Consume;
                    }
                    else if (action.currentAction == "Idle")
                    {
                        state = AgentState::Idle;
                    }
                    agentStates[action.entityId] = state;
                }

                const ImU32 moveColor = ImGui::GetColorU32(colorMove);
                const ImU32 consumeColor = ImGui::GetColorU32(colorConsume);
                const ImU32 idleColor = ImGui::GetColorU32(colorIdle);
                const ImU32 otherColor = ImGui::GetColorU32(colorUnknown);
                const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Text);

                auto colorForState = [&](AgentState state) -> ImU32
                {
                    switch (state)
                    {
                    case AgentState::Move:
                        return moveColor;
                    case AgentState::Consume:
                        return consumeColor;
                    case AgentState::Idle:
                        return idleColor;
                    case AgentState::Other:
                    default:
                        return otherColor;
                    }
                };

                for (std::size_t i = 0; i < latest_snapshot_->telemetry.agents.size(); ++i)
                {
                    const auto &agent = latest_snapshot_->telemetry.agents[i];
                    RuntimeBridge::Vector2 agentPos = atlas.nodePosition(agent.location).value_or(RuntimeBridge::Vector2{});
                    if (agentPositionsPtr && i < agentPositionsPtr->size())
                    {
                        agentPos = (*agentPositionsPtr)[i];
                    }
                    if (map_interpolate_)
                    {
                        if (auto itp = progress.find(agent.entityId); itp != progress.end())
                        {
                            const auto &[fromP, toP, t] = itp->second;
                            agentPos.x = fromP.x + (toP.x - fromP.x) * t;
                            agentPos.y = fromP.y + (toP.y - fromP.y) * t;
                        }
                    }

                    ImVec2 screenPos = toScreen(agentPos);
                    screenPos = ImVec2(std::floor(screenPos.x) + 0.5f, std::floor(screenPos.y) + 0.5f);

                    auto posIt = nodePositions.find(agent.location.value);
                    const auto state = agentStates.contains(agent.entityId) ? agentStates[agent.entityId] : AgentState::Idle;
                    const ImU32 fillColor = colorForState(state);
                    drawList->AddCircleFilled(screenPos, 7.0f, fillColor, 16);
                    drawList->AddCircle(screenPos, 7.0f, borderColor, 16, 1.4f);

                    const bool agentSelected = inspector_selection_type_ == InspectorSelectionType::Agent && inspector_selected_primary_ == agent.entityId;
                    if (agentSelected)
                    {
                        drawList->AddCircle(screenPos, 11.0f, highlightColor, 24, 2.5f);
                    }

                    // Agent name label
                    if (!agent.name.empty() && (map_zoom_ >= agentLabelZoomThreshold || agentSelected))
                    {
                        const ImVec2 namePos{screenPos.x + 9.0f, screenPos.y - ImGui::GetTextLineHeight() * 0.5f};
                        drawList->AddText(namePos, ImGui::GetColorU32(ImGuiCol_Text), agent.name.c_str());
                    }

                    if (show_agent_trails_)
                    {
                        auto trailIt = agent_trails_.find(agent.entityId);
                        if (trailIt != agent_trails_.end() && trailIt->second.size() > 1)
                        {
                            const auto &trail = trailIt->second;
                            ImVec2 previous = toScreen(trail.front());
                            previous = ImVec2(std::floor(previous.x) + 0.5f, std::floor(previous.y) + 0.5f);
                            for (std::size_t i = 1; i < trail.size(); ++i)
                            {
                                ImVec2 current = toScreen(trail[i]);
                                current = ImVec2(std::floor(current.x) + 0.5f, std::floor(current.y) + 0.5f);
                                drawList->AddLine(previous, current, fillColor, 2.0f);
                                previous = current;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            ImGui::TextUnformatted("暂无世界数据。");
        }

        ImGui::Dummy(ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y));
    }
void AppHost::drawSceneNodeContent()
    {
        if (!runtime_bridge_)
        {
            ImGui::TextUnformatted("RuntimeBridge 未就绪。");
            return;
        }

        const auto& atlas = runtime_bridge_->atlas();

        // Node selector
        if (scene_selected_node_ == 0 && !atlas.nodes.empty())
        {
            scene_selected_node_ = atlas.nodes.front().id.value;
        }

        if (ImGui::BeginCombo("节点", [this, &atlas]()
                              {
            for (const auto& n : atlas.nodes)
            {
                if (n.id.value == scene_selected_node_)
                    return n.name.c_str();
            }
            return "(none)"; }()))
        {
            for (const auto &n : atlas.nodes)
            {
                const bool selected = (n.id.value == scene_selected_node_);
                if (ImGui::Selectable(n.name.c_str(), selected))
                {
                    scene_selected_node_ = n.id.value;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Toggles
        ImGui::Checkbox("网格##SceneGrid", &scene_show_grid_);
        ImGui::SameLine();
        ImGui::Checkbox("锚点##SceneAnchors", &scene_show_anchors_);
        ImGui::SameLine();
        ImGui::Checkbox("资源##SceneResources", &scene_show_resources_);

        // Canvas setup
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasMax{canvasPos.x + std::max(120.0f, canvasSize.x), canvasPos.y + std::max(120.0f, canvasSize.y)};
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_WindowBg);
        drawList->AddRectFilled(canvasPos, canvasMax, bgColor);
        drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));

        // Camera controls (mouse drag/scroll)
        ImGui::InvisibleButton("SceneCanvas", ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y), ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        ImGuiIO &io = ImGui::GetIO();
        if (hovered && io.MouseWheel != 0.0f)
        {
            const float zoomStep = 1.0f + (io.MouseWheel > 0.0f ? 0.1f : -0.1f);
            scene_cam_zoom_ = std::max(scene_cam_zoom_ * zoomStep, 0.05f);
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            scene_cam_offset_x_ += delta.x;
            scene_cam_offset_y_ += delta.y;
        }

        // Gather local points: resource spawns and anchors for selected node
        std::vector<ImVec2> resourcePts;
        for (const auto &sp : atlas.spawns)
        {
            if (sp.resource.location.value != scene_selected_node_)
                continue;
            if (sp.resource.local_coord.has_value())
            {
                resourcePts.emplace_back(static_cast<float>(sp.resource.local_coord->first), static_cast<float>(sp.resource.local_coord->second));
            }
        }
        std::vector<std::pair<ImVec2, std::string>> anchorPts; // pos, label
        for (const auto &e : atlas.edges)
        {
            if (e.from.value == scene_selected_node_ && e.anchorFrom.has_value())
            {
                anchorPts.emplace_back(ImVec2(e.anchorFrom->x, e.anchorFrom->y), std::string("→ ") + std::to_string(e.to.value));
            }
            if (e.to.value == scene_selected_node_ && e.anchorTo.has_value())
            {
                anchorPts.emplace_back(ImVec2(e.anchorTo->x, e.anchorTo->y), std::string("→ ") + std::to_string(e.from.value));
            }
        }

        // Determine grid bounds (prefer tilemap meta)
        int minX = 0, minY = 0, maxX = 9, maxY = 9; // default 10x10
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0)
            {
                minX = 0;
                minY = 0;
                maxX = tm.width - 1;
                maxY = tm.height - 1;
                break;
            }
        }
        auto incorporate = [&](const ImVec2 &p)
        {
            minX = std::min(minX, static_cast<int>(std::floor(p.x)));
            minY = std::min(minY, static_cast<int>(std::floor(p.y)));
            maxX = std::max(maxX, static_cast<int>(std::ceil(p.x)));
            maxY = std::max(maxY, static_cast<int>(std::ceil(p.y)));
        };
        // If no tilemap meta, extend from points
        bool hasMeta = false;
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.width > 0 && tm.height > 0)
            {
                hasMeta = true;
                break;
            }
        }
        if (!hasMeta)
        {
            for (const auto &p : resourcePts)
                incorporate(p);
            for (const auto &ap : anchorPts)
                incorporate(ap.first);
        }

        float basePx = 24.0f;
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId == scene_selected_node_ && tm.tileW > 0)
            {
                basePx = std::max(basePx, static_cast<float>(tm.tileW));
                break;
            }
        }
        const float cellPx = basePx * scene_cam_zoom_;
        auto toScreen = [&](float gx, float gy)
        {
            const float sx = canvasPos.x + scene_cam_offset_x_ + (gx - minX) * cellPx + 8.0f;
            const float sy = canvasPos.y + scene_cam_offset_y_ + (gy - minY) * cellPx + 8.0f;
            return ImVec2(std::floor(sx) + 0.5f, std::floor(sy) + 0.5f);
        };

        // Draw grid
        const int cols = (maxX - minX + 1);
        const int rows = (maxY - minY + 1);
        const ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.25f, 0.25f, 0.28f, 1.0f));

        // Draw tile coverage using solid colors as placeholder for actual textures
        const ImU32 tileColorA = ImGui::GetColorU32(ImVec4(0.18f, 0.25f, 0.32f, 0.65f));
        const ImU32 tileColorB = ImGui::GetColorU32(ImVec4(0.15f, 0.21f, 0.28f, 0.65f));
        for (const auto &tm : atlas.tilemaps)
        {
            if (tm.nodeId != scene_selected_node_ || tm.width <= 0 || tm.height <= 0)
            {
                continue;
            }

            for (int y = 0; y < tm.height; ++y)
            {
                for (int x = 0; x < tm.width; ++x)
                {
                    const float gx0 = static_cast<float>(minX + x);
                    const float gy0 = static_cast<float>(minY + y);
                    const float gx1 = gx0 + 1.0f;
                    const float gy1 = gy0 + 1.0f;
                    const ImVec2 cellMin = toScreen(gx0, gy0);
                    const ImVec2 cellMax = toScreen(gx1, gy1);
                    const ImU32 fill = ((x + y) % 2 == 0) ? tileColorA : tileColorB;
                    drawList->AddRectFilled(cellMin, cellMax, fill);
                }
            }
            break; // use the first matching tilemap per node
        }

        if (scene_show_grid_)
        {
            for (int x = 0; x <= cols; ++x)
            {
                const ImVec2 a = toScreen(static_cast<float>(minX + x), static_cast<float>(minY));
                const ImVec2 b = toScreen(static_cast<float>(minX + x), static_cast<float>(maxY + 1));
                drawList->AddLine(a, b, gridColor, 1.0f);
            }
            for (int y = 0; y <= rows; ++y)
            {
                const ImVec2 a = toScreen(static_cast<float>(minX), static_cast<float>(minY + y));
                const ImVec2 b = toScreen(static_cast<float>(maxX + 1), static_cast<float>(minY + y));
                drawList->AddLine(a, b, gridColor, 1.0f);
            }
        }

        // Draw resources
        const ImU32 foodColor = ImGui::GetColorU32(ImVec4(0.93f, 0.67f, 0.27f, 1.0f));
        if (scene_show_resources_)
        {
            for (const auto &p : resourcePts)
            {
                const ImVec2 center = toScreen(p.x + 0.5f, p.y + 0.5f);
                const float r = std::max(3.0f, cellPx * 0.25f);
                drawList->AddCircleFilled(center, r, foodColor, 12);
                drawList->AddCircle(center, r, ImGui::GetColorU32(ImGuiCol_Border), 12, 1.2f);
            }
        }

        // Draw anchors
        const ImU32 anchorColor = ImGui::GetColorU32(ImVec4(0.38f, 0.74f, 0.88f, 1.0f));
        if (scene_show_anchors_)
        {
            for (const auto &ap : anchorPts)
            {
                const ImVec2 base = toScreen(ap.first.x + 0.5f, ap.first.y + 0.5f);
                const float w = std::max(4.0f, cellPx * 0.2f);
                const ImVec2 a{base.x - w, base.y};
                const ImVec2 b{base.x + w, base.y};
                const ImVec2 c{base.x, base.y + w * 1.6f};
                drawList->AddTriangleFilled(a, b, c, anchorColor);
                drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
                const ImVec2 labelPos{base.x + w + 4.0f, base.y - ImGui::GetTextLineHeight() * 0.5f};
                drawList->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), ap.second.c_str());
            }
        }

        ImGui::Dummy(ImVec2(canvasMax.x - canvasPos.x, canvasMax.y - canvasPos.y));
    }
void AppHost::drawMonitorTabContent()
    {
        ImGui::TextUnformatted("运行概览");
        ImGui::SameLine(0.0f, 12.0f);
        if (latest_snapshot_)
        {
            const auto& tick = latest_snapshot_->telemetry;
            ImGui::Text("Step %llu | Agents %zu | Actions %zu",
                        static_cast<unsigned long long>(tick.step),
                        tick.agents.size(),
                        tick.actions.size());
        }
        else
        {
            ImGui::TextUnformatted("(等待快照)");
        }

        ImGui::Separator();

        const float totalHeight = std::max(240.0f, ImGui::GetContentRegionAvail().y);
        const float telemetryHeight = totalHeight * 0.45f;

        if (ImGui::BeginChild("MonitorTelemetry", ImVec2(0.0f, telemetryHeight), true))
        {
            drawMonitorTelemetryContent();
        }
        ImGui::EndChild();

        ImGui::Spacing();

        if (ImGui::BeginChild("MonitorLogs", ImVec2(0.0f, 0.0f), true))
        {
            drawMonitorLogContent();
        }
        ImGui::EndChild();
    }

void AppHost::drawSettingsTabContent()
    {
        ImGui::TextUnformatted("设计令牌预览");
        ImGui::Separator();

        const ImGuiStyle& style = ImGui::GetStyle();

        ImGui::Text("颜色样本");
        if (ImGui::BeginTable("SettingsColors", 4, ImGuiTableFlags_SizingFixedFit))
        {
            const std::array<std::pair<const char*, ImGuiCol>, 4> swatches = {
                std::pair{"WindowBg", ImGuiCol_WindowBg},
                std::pair{"Header", ImGuiCol_Header},
                std::pair{"Button", ImGuiCol_Button},
                std::pair{"Accent", ImGuiCol_TabActive}};
            for (const auto& [label, col] : swatches)
            {
                ImGui::TableNextColumn();
                const ImVec4 color = style.Colors[col];
                ImGui::ColorButton(label, color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(40.0f, 18.0f));
                ImGui::SameLine();
                ImGui::TextUnformatted(label);
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("间距设置");
        ImGui::Text("窗口内边距：%.1f / %.1f", style.WindowPadding.x, style.WindowPadding.y);
        ImGui::Text("元素间距：%.1f / %.1f", style.ItemSpacing.x, style.ItemSpacing.y);
        ImGui::Text("控件圆角：%.1f", style.FrameRounding);

        ImGui::Separator();
        ImGui::TextWrapped(
            "后续任务将补充：主题切换、布局预设管理、快捷键自定义等功能。当前阶段仅提供设计指标预览，方便在开发过程中校准 UI 令牌。");
    }

void AppHost::drawMonitorTelemetryContent()
    {
        if (!latest_snapshot_)
        {
            ImGui::TextUnformatted("等待监控数据…");
            return;
        }

        const auto& tick = latest_snapshot_->telemetry;
        ImGui::Text("步数：%llu", static_cast<unsigned long long>(tick.step));
        ImGui::Text("实体：%zu", tick.agents.size());
        ImGui::Text("执行命令：%zu", tick.actions.size());
        ImGui::Text("需求项：%zu", tick.needs.size());

        if (!tick.needs.empty())
        {
            float needSum = 0.0f;
            std::uint32_t critical = 0;
            for (const auto& need : tick.needs)
            {
                needSum += need.value;
                if (need.critical)
                {
                    ++critical;
                }
            }
            const float average = needSum / static_cast<float>(tick.needs.size());
            ImGui::Separator();
            ImGui::Text("平均需求值：%.2f", average);
            ImGui::Text("危急需求：%u", critical);
        }

        if (!tick.resources.empty())
        {
            ImGui::Separator();
            for (const auto& resource : tick.resources)
            {
                ImGui::Text("#%u %s (%u / %u)",
                            resource.location.value,
                            resource.name.c_str(),
                            resource.current,
                            resource.capacity);
            }
        }
    }

void AppHost::drawMonitorLogContent()
    {
        if (!log_sink_)
        {
            ImGui::TextUnformatted("日志缓冲不可用。");
            return;
        }

        ImGui::Checkbox("自动滚动", &log_auto_scroll_);
        ImGui::Separator();

        const auto lines = log_sink_->snapshot();

        ImGui::BeginChild("LogConsole.ScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        const bool stickToBottom = log_auto_scroll_ &&
                                   (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f || log_last_line_count_ == 0);

        for (const auto &line : lines)
        {
            ImGui::TextUnformatted(line.c_str());
        }

        if (stickToBottom && !lines.empty())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        log_last_line_count_ = lines.size();

        ImGui::EndChild();
    }
}
