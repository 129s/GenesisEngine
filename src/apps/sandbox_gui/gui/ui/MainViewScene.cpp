#include "sandbox/gui/ui/MainView.hpp"
#include "../CommandUiHelpers.hpp"
#include "../ImGuiLogSink.hpp"
#include "sandbox/gui/AppHost.hpp"
#include "sandbox/gui/style/DesignTokens.hpp"
#include "sandbox/gui/style/LayoutMetrics.hpp"
#include "sandbox/gui/ui/LayoutHelpers.hpp"
#include "MainViewHelpers.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace genesis::sandbox::gui
{
    using json = nlohmann::json;

    namespace
    {
        void DrawSceneMiniMap(UiContext &ctx, const std::optional<SceneViewportRenderState> &stateOpt)
        {
            const float miniHeight = 160.0f;
            ImGui::BeginChild("SceneMiniMap", ImVec2(0.0f, miniHeight), false, ImGuiWindowFlags_NoScrollbar);

            const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
            const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, miniHeight);
            ImGui::InvisibleButton("SceneMiniMap.Canvas",
                                   canvasSize,
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            const ImVec2 canvasMax = ImGui::GetItemRectMax();

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImU32 bgColor = ImGui::GetColorU32(ImVec4(0.08f, 0.10f, 0.14f, 0.92f));
            const ImU32 borderColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
            drawList->AddRectFilled(canvasMin, canvasMax, bgColor, 0.0f);
            drawList->AddRect(canvasMin, canvasMax, borderColor, 0.0f, 0, 1.2f);

            if (!stateOpt || !stateOpt->details)
            {
                const char *message = "无世界数据";
                const ImVec2 textSize = ImGui::CalcTextSize(message);
                const ImVec2 textPos{canvasMin.x + (canvasSize.x - textSize.x) * 0.5f,
                                     canvasMin.y + (canvasSize.y - textSize.y) * 0.5f};
                drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), message);
                ImGui::EndChild();
                return;
            }

            const SceneViewportRenderState &state = *stateOpt;

            const float margin = Style::DesignTokens::spacing(Style::SpacingToken::Sm);
            const ImVec2 innerMin(canvasMin.x + margin, canvasMin.y + margin);
            const ImVec2 innerMax(canvasMax.x - margin, canvasMax.y - margin);
            const float innerWidth = std::max(1.0f, innerMax.x - innerMin.x);
            const float innerHeight = std::max(1.0f, innerMax.y - innerMin.y);

            const SceneNodeGridInfo &grid = state.grid;
            const float viewCenterX = state.viewOriginX + state.viewWidthTiles * 0.5f;
            const float viewCenterY = state.viewOriginY + state.viewHeightTiles * 0.5f;
            const float halfWidth = state.viewWidthTiles * 2.0f;
            const float halfHeight = state.viewHeightTiles * 2.0f;

            float regionMinX = std::max(static_cast<float>(grid.minX), viewCenterX - halfWidth);
            float regionMinY = std::max(static_cast<float>(grid.minY), viewCenterY - halfHeight);
            float regionMaxX = std::min(static_cast<float>(grid.maxX + 1), viewCenterX + halfWidth);
            float regionMaxY = std::min(static_cast<float>(grid.maxY + 1), viewCenterY + halfHeight);
            if (regionMaxX <= regionMinX || regionMaxY <= regionMinY)
            {
                regionMinX = static_cast<float>(grid.minX);
                regionMinY = static_cast<float>(grid.minY);
                regionMaxX = static_cast<float>(grid.maxX + 1);
                regionMaxY = static_cast<float>(grid.maxY + 1);
            }

            const float regionWidth = regionMaxX - regionMinX;
            const float regionHeight = regionMaxY - regionMinY;
            const float scale = std::min(innerWidth / regionWidth, innerHeight / regionHeight);
            const float offsetX = (innerWidth - regionWidth * scale) * 0.5f;
            const float offsetY = (innerHeight - regionHeight * scale) * 0.5f;

            auto toScreen = [&](float gx, float gy) -> ImVec2 {
                return ImVec2(innerMin.x + offsetX + (gx - regionMinX) * scale,
                              innerMin.y + offsetY + (gy - regionMinY) * scale);
            };

            const ImVec2 regionTopLeft = toScreen(regionMinX, regionMinY);
            const ImVec2 regionBottomRight = toScreen(regionMaxX, regionMaxY);
            const ImU32 regionColor = ImGui::GetColorU32(ImVec4(0.24f, 0.32f, 0.54f, 0.22f));
            drawList->AddRectFilled(regionTopLeft, regionBottomRight, regionColor, 0.0f);
            drawList->AddRect(regionTopLeft, regionBottomRight, ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent)), 0.0f, 0, 1.4f);

            const int tileMinX = std::max(static_cast<int>(std::floor(regionMinX)), grid.minX);
            const int tileMaxX = std::min(static_cast<int>(std::ceil(regionMaxX)), grid.maxX + 1);
            const int tileMinY = std::max(static_cast<int>(std::floor(regionMinY)), grid.minY);
            const int tileMaxY = std::min(static_cast<int>(std::ceil(regionMaxY)), grid.maxY + 1);

            const ImU32 tileColorA = ImGui::GetColorU32(ImVec4(0.18f, 0.24f, 0.32f, 0.65f));
            const ImU32 tileColorB = ImGui::GetColorU32(ImVec4(0.15f, 0.20f, 0.28f, 0.65f));
            for (int y = tileMinY; y < tileMaxY; ++y)
            {
                for (int x = tileMinX; x < tileMaxX; ++x)
                {
                    if (x < grid.minX || x >= grid.maxX + 1 || y < grid.minY || y >= grid.maxY + 1)
                    {
                        continue;
                    }
                    const ImVec2 cellMin = toScreen(static_cast<float>(x), static_cast<float>(y));
                    const ImVec2 cellMax = toScreen(static_cast<float>(x + 1), static_cast<float>(y + 1));
                    const ImU32 fill = ((x + y) % 2 == 0) ? tileColorA : tileColorB;
                    drawList->AddRectFilled(cellMin, cellMax, fill, 0.0f);
                }
            }

            const ImU32 gridColor = ImGui::GetColorU32(ImVec4(0.36f, 0.44f, 0.56f, 0.35f));
            for (int x = tileMinX; x <= tileMaxX; ++x)
            {
                const ImVec2 a = toScreen(static_cast<float>(x), static_cast<float>(tileMinY));
                const ImVec2 b = toScreen(static_cast<float>(x), static_cast<float>(tileMaxY));
                drawList->AddLine(a, b, gridColor, 0.8f);
            }
            for (int y = tileMinY; y <= tileMaxY; ++y)
            {
                const ImVec2 a = toScreen(static_cast<float>(tileMinX), static_cast<float>(y));
                const ImVec2 b = toScreen(static_cast<float>(tileMaxX), static_cast<float>(y));
                drawList->AddLine(a, b, gridColor, 0.8f);
            }

            const float viewMinX = state.viewOriginX;
            const float viewMinY = state.viewOriginY;
            const float viewMaxX = viewMinX + state.viewWidthTiles;
            const float viewMaxY = viewMinY + state.viewHeightTiles;
            const ImVec2 cameraMin = toScreen(viewMinX, viewMinY);
            const ImVec2 cameraMax = toScreen(viewMaxX, viewMaxY);
            const ImU32 cameraColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Highlight));
            drawList->AddRectFilled(cameraMin, cameraMax, ImGui::GetColorU32(ImVec4(0.30f, 0.46f, 0.86f, 0.16f)), 0.0f);
            drawList->AddRect(cameraMin, cameraMax, cameraColor, 0.0f, 0, 2.0f);

            const ImVec2 viewCenter = toScreen(viewCenterX, viewCenterY);
            const float cross = 6.0f;
            drawList->AddLine(ImVec2(viewCenter.x - cross, viewCenter.y), ImVec2(viewCenter.x + cross, viewCenter.y), cameraColor, 1.2f);
            drawList->AddLine(ImVec2(viewCenter.x, viewCenter.y - cross), ImVec2(viewCenter.x, viewCenter.y + cross), cameraColor, 1.2f);

            if (ctx.state.scene_tile_selection && ctx.state.scene_tile_selection->nodeId == state.details->nodeId)
            {
                const auto &sel = *ctx.state.scene_tile_selection;
                const ImVec2 selMin = toScreen(static_cast<float>(sel.tileX), static_cast<float>(sel.tileY));
                const ImVec2 selMax = toScreen(static_cast<float>(sel.tileX + 1), static_cast<float>(sel.tileY + 1));
                drawList->AddRect(selMin, selMax, cameraColor, 0.0f, 0, 1.8f);
            }

            if (ctx.state.scene_show_resources)
            {
                const ImU32 foodColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Warning));
                const ImU32 drinkColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
                const ImU32 socialColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Success));
                for (const auto &resource : state.details->resources)
                {
                    if (resource.x < regionMinX || resource.x > regionMaxX || resource.y < regionMinY || resource.y > regionMaxY)
                    {
                        continue;
                    }
                    const ImVec2 point = toScreen(resource.x + 0.5f, resource.y + 0.5f);
                    const ImU32 color = (resource.type == genesis::world::ResourceType::Food)
                                            ? foodColor
                                            : (resource.type == genesis::world::ResourceType::Drink)
                                                  ? drinkColor
                                                  : socialColor;
                    drawList->AddCircleFilled(point, 3.2f, color, 10);
                    drawList->AddCircle(point, 3.2f, ImGui::GetColorU32(ImGuiCol_Border), 10, 1.0f);
                }
            }

        if (ctx.state.scene_show_anchors)
        {
            const ImU32 anchorColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::AccentActive));
            for (const auto &anchor : state.details->anchors)
            {
                    if (anchor.x < regionMinX || anchor.x > regionMaxX || anchor.y < regionMinY || anchor.y > regionMaxY)
                    {
                        continue;
                    }
                    const ImVec2 center = toScreen(anchor.x + 0.5f, anchor.y + 0.5f);
                    const float size = 4.0f;
                    const ImVec2 a{center.x, center.y - size};
                    const ImVec2 b{center.x + size, center.y};
                    const ImVec2 c{center.x, center.y + size};
                    const ImVec2 d{center.x - size, center.y};
                drawList->AddQuadFilled(a, b, c, d, anchorColor);
                drawList->AddQuad(a, b, c, d, ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
            }
        }

        if (ctx.state.scene_show_graph)
        {
            const ImU32 portalColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
            for (const auto &portal : state.details->portals)
            {
                if (portal.x < regionMinX || portal.x > regionMaxX || portal.y < regionMinY || portal.y > regionMaxY)
                {
                    continue;
                }
                const ImVec2 point = toScreen(portal.x + 0.5f, portal.y + 0.5f);
                drawList->AddCircleFilled(point, 3.5f, portalColor, 12);
                drawList->AddCircle(point, 3.5f, ImGui::GetColorU32(ImGuiCol_Border), 12, 1.0f);
            }
        }

        ImGui::EndChild();
    }
    } // namespace

void InspectorView::render(UiContext &ctx, const std::optional<SceneViewportRenderState> &viewportState)
    {
        if (!ctx.state.show_inspector)
        {
            return;
        }

        if (!ctx.latest_snapshot)
        {
            ImGui::TextUnformatted("等待快照…");
            return;
        }

        const auto &snapshot = *ctx.latest_snapshot;
        const auto &tick = snapshot.telemetry;
        const RuntimeBridge::WorldAtlas *atlasPtr = ctx.runtime_bridge ? &ctx.runtime_bridge->atlas() : nullptr;

        const auto inspectorCardLayout = Style::Layout::detailCard();
        Style::Layout::CardScope inspectorCard("InspectorContent",
                                               inspectorCardLayout,
                                               ImGuiWindowFlags_NoScrollbar);
        if (!inspectorCard.isOpen())
        {
            return;
        }
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        DrawSceneMiniMap(ctx, viewportState);
        ImGui::Dummy(ImVec2(0.0f, inspectorCardLayout.headerGap));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);

        const auto resourceTypeName = [](genesis::world::ResourceType type) -> const char *
        {
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
        switch (ctx.state.inspector_selection_type)
        {
        case UiState::InspectorSelectionType::None:
            ImGui::TextUnformatted("没有选择实体");
            break;
        case UiState::InspectorSelectionType::Agent:
        {
            const genesis::telemetry::AgentSnapshot *agent = nullptr;
            for (const auto &candidate : tick.agents)
            {
                if (candidate.entityId == ctx.state.inspector_selected_primary)
                {
                    agent = &candidate;
                    break;
                }
            }

            if (!agent)
            {
                ImGui::Text("Agent #%u is not present in the current snapshot.", ctx.state.inspector_selected_primary);
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
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##agentFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_focus_node_request = agent->location.value;
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("打开节点视图##agentScene"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Tile;
                ctx.state.scene_selected_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_tile_selection.reset();
                ctx.state.scene_focus_node_request = agent->location.value;
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            bool followChanged = ImGui::Checkbox("Follow##agentFollow", &ctx.state.inspector_follow_selection);
            Ui::applyClickableCursorToLastItem();
            if (followChanged && ctx.state.inspector_follow_selection)
            {
                ctx.state.scene_selected_node = agent->location.value;
                ctx.state.map_selected_node = agent->location.value;
                ctx.state.scene_tile_selection.reset();
            }
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
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
                        needsJson.push_back({{"name", need.needName},
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
                        resourcesJson.push_back({{"name", res.name},
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
                ctx.pushToast("Agent snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::Dummy(ImVec2(0.0f, inspectorCardLayout.lineGap));
            ImGui::Text("ID：%u", agent->entityId);
            ImGui::Text("位置：#%u %s", agent->location.value, nodeName.c_str());

            bool firstSection = true;
            CardSectionHeader(inspectorCardLayout, "需求概览", firstSection);

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
                CardSectionHeader(inspectorCardLayout, "当前行动", firstSection);
                ImGui::Text("行动：%s", action->currentAction.c_str());
                ImGui::Text("目标节点：#%u", action->target.value);
                ImGui::Text("队列长度：%u", action->queueLength);
                ImGui::Text("速度：%.2f", action->speed);
                ImGui::Text("资源：%s · 数量 %u", resourceTypeName(action->resource), action->amount);
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
                CardSectionHeader(inspectorCardLayout, "Planner 决策", firstSection);
                ImGui::Text("目标：#%u", planner->target.value);
                ImGui::Text("旅行成本：%.2f", planner->travelCost);
                ImGui::Text("评分：%.2f", planner->score);
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
                CardSectionHeader(inspectorCardLayout, "移动进度", firstSection);
                ImGui::Text("路径：%u → %u", movement->from.value, movement->to.value);
                ImGui::Text("进度：%.2f", movement->t01);
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
                            CardSectionHeader(inspectorCardLayout, "本帧需求变更", firstSection);
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
        case UiState::InspectorSelectionType::Resource:
        {
            if (ctx.state.inspector_selected_primary >= tick.resources.size())
            {
                ImGui::TextUnformatted("Selected resource index is stale.");
                break;
            }

            const auto &resource = tick.resources[ctx.state.inspector_selected_primary];
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

            std::vector<const RuntimeBridge::WorldAtlas::Spawn *> resourceSpawns;
            if (atlasPtr)
            {
                for (const auto &spawn : atlasPtr->spawns)
                {
                    if (spawn.resource.location.value == resource.location.value && spawn.resource.name == resource.name)
                    {
                        resourceSpawns.push_back(&spawn);
                    }
                }
            }

            std::vector<const genesis::telemetry::ActionSnapshot *> activeConsumers;
            for (const auto &action : tick.actions)
            {
                if (action.target.value == resource.location.value)
                {
                    activeConsumers.push_back(&action);
                }
            }
            ImGui::Text("%s", resource.name.c_str());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##resourceFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = resource.location.value;
                ctx.state.map_selected_node = resource.location.value;
                ctx.state.scene_focus_node_request = resource.location.value;
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
            if (ImGui::Button("打开节点视图##resourceScene"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Tile;
                ctx.state.scene_selected_node = resource.location.value;
                ctx.state.map_selected_node = resource.location.value;
                ctx.state.scene_tile_selection.reset();
                ctx.state.scene_focus_node_request = resource.location.value;
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
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
                for (const auto *spawn : resourceSpawns)
                {
                    json spawnJson{
                        {"position", {{"x", spawn->position.x}, {"y", spawn->position.y}}},
                        {"capacity", spawn->resource.capacity},
                        {"ratePerStep", spawn->resource.ratePerStep}};
                    if (spawn->resource.local_coord.has_value())
                    {
                        spawnJson["localCoord"] = {
                            {"x", spawn->resource.local_coord->first},
                            {"y", spawn->resource.local_coord->second}};
                    }
                    spawnsJson.push_back(std::move(spawnJson));
                }
                if (!spawnsJson.empty())
                {
                    resourceJson["spawns"] = std::move(spawnsJson);
                }

                json consumersJson = json::array();
                for (const auto *consumer : activeConsumers)
                {
                    consumersJson.push_back({{"entityId", consumer->entityId},
                                             {"action", consumer->currentAction}});
                }
                if (!consumersJson.empty())
                {
                    resourceJson["activeAgents"] = std::move(consumersJson);
                }

                std::string serialized = resourceJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                ctx.pushToast("Resource snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::Dummy(ImVec2(0.0f, inspectorCardLayout.lineGap));
            bool firstSection = true;
            CardSectionHeader(inspectorCardLayout, "资源概览", firstSection);
            ImGui::Text("节点：#%u %s", resource.location.value, nodeName.c_str());
            ImGui::Text("类型：%s", resourceTypeName(resource.type));
            ImGui::Text("库存：%u / %u", resource.current, resource.capacity);

            if (!resourceSpawns.empty())
            {
                CardSectionHeader(inspectorCardLayout, "关联生成点", firstSection);
                for (const auto *spawn : resourceSpawns)
                {
                    if (spawn->resource.local_coord.has_value())
                    {
                        ImGui::BulletText("坐标：(%g, %g) · 本地(%d, %d) · 速率 %.2f / 容量 %u",
                                          spawn->position.x,
                                          spawn->position.y,
                                          spawn->resource.local_coord->first,
                                          spawn->resource.local_coord->second,
                                          spawn->resource.ratePerStep,
                                          spawn->resource.capacity);
                    }
                    else
                    {
                        ImGui::BulletText("坐标：(%g, %g) · 速率 %.2f / 容量 %u",
                                          spawn->position.x,
                                          spawn->position.y,
                                          spawn->resource.ratePerStep,
                                          spawn->resource.capacity);
                    }
                }
            }

            if (!activeConsumers.empty())
            {
                CardSectionHeader(inspectorCardLayout, "活跃消耗者", firstSection);
                for (const auto *consumer : activeConsumers)
                {
                    ImGui::BulletText("Agent #%u · %s", consumer->entityId, consumer->currentAction.c_str());
                }
            }
            break;
        }
        case UiState::InspectorSelectionType::Node:
        {
            if (!atlasPtr)
            {
                ImGui::TextUnformatted("暂无节点信息。");
                break;
            }

            const RuntimeBridge::WorldAtlas::Node *selectedNode = nullptr;
            for (const auto &node : atlasPtr->nodes)
            {
                if (node.id.value == ctx.state.inspector_selected_primary)
                {
                    selectedNode = &node;
                    break;
                }
            }

            if (!selectedNode)
            {
                ImGui::Text("节点 #%u 不存在。", ctx.state.inspector_selected_primary);
                break;
            }

            std::vector<const RuntimeBridge::WorldAtlas::Node *> childNodes;
            if (atlasPtr)
            {
                for (const auto &node : atlasPtr->nodes)
                {
                    if (node.parent.value == selectedNode->id.value && node.id.value != selectedNode->id.value)
                    {
                        childNodes.push_back(&node);
                    }
                }
            }

            std::vector<const RuntimeBridge::WorldAtlas::Edge *> connectedEdges;
            if (atlasPtr)
            {
                for (const auto &edge : atlasPtr->edges)
                {
                    if (edge.from.value == selectedNode->id.value || edge.to.value == selectedNode->id.value)
                    {
                        connectedEdges.push_back(&edge);
                    }
                }
            }

            std::vector<const genesis::telemetry::ResourceSnapshot *> resourcesAtNode;
            for (const auto &res : tick.resources)
            {
                if (res.location.value == selectedNode->id.value)
                {
                    resourcesAtNode.push_back(&res);
                }
            }

            std::vector<const genesis::telemetry::AgentSnapshot *> agentsAtNode;
            for (const auto &agentSnapshot : tick.agents)
            {
                if (agentSnapshot.location.value == selectedNode->id.value)
                {
                    agentsAtNode.push_back(&agentSnapshot);
                }
            }

            ImGui::Text("%s", selectedNode->name.c_str());
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Md));
            if (ImGui::Button("定位地图##nodeFocus"))
            {
                ctx.state.main_view_active_tab = MainViewTab::Scene;
                ctx.state.scene_selection_tool = SceneSelectionTool::Node;
                ctx.state.scene_tile_selection.reset();
                ctx.state.inspector_highlight_node = selectedNode->id.value;
                ctx.state.map_selected_node = selectedNode->id.value;
                ctx.state.scene_focus_node_request = selectedNode->id.value;
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::SameLine(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm));
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
                for (const auto *child : childNodes)
                {
                    children.push_back({{"id", child->id.value}, {"name", child->name}});
                }
                if (!children.empty())
                {
                    nodeObj["children"] = std::move(children);
                }

                json edges = json::array();
                for (const auto *edge : connectedEdges)
                {
                    edges.push_back({{"from", edge->from.value},
                                     {"to", edge->to.value},
                                     {"bidirectional", edge->bidirectional}});
                }
                if (!edges.empty())
                {
                    nodeJson["edges"] = std::move(edges);
                }

                json resourcesJson = json::array();
                for (const auto *res : resourcesAtNode)
                {
                    resourcesJson.push_back({{"name", res->name},
                                             {"type", resourceTypeName(res->type)},
                                             {"current", res->current},
                                             {"capacity", res->capacity}});
                }
                if (!resourcesJson.empty())
                {
                    nodeJson["resources"] = std::move(resourcesJson);
                }

                json agentsJson = json::array();
                for (const auto *agentSnap : agentsAtNode)
                {
                    agentsJson.push_back({{"entityId", agentSnap->entityId},
                                          {"name", agentSnap->name}});
                }
                if (!agentsJson.empty())
                {
                    nodeJson["agents"] = std::move(agentsJson);
                }

                std::string serialized = nodeJson.dump(2);
                ImGui::SetClipboardText(serialized.c_str());
                ctx.pushToast("Node snapshot copied", Style::DesignTokens::color(Style::ColorToken::Success));
            }
            Ui::applyClickableCursorToLastItem();
            ImGui::Dummy(ImVec2(0.0f, inspectorCardLayout.lineGap));
            bool firstSection = true;
            CardSectionHeader(inspectorCardLayout, "节点信息", firstSection);
            ImGui::Text("ID：%u", selectedNode->id.value);
            ImGui::Text("父节点：%u", selectedNode->parent.value);
            ImGui::Text("类型：%u", static_cast<unsigned int>(selectedNode->kind));

            if (!childNodes.empty())
            {
                CardSectionHeader(inspectorCardLayout, "子节点", firstSection);
                for (const auto *child : childNodes)
                {
                    ImGui::BulletText("#%u %s", child->id.value, child->name.c_str());
                }
            }

            if (!connectedEdges.empty())
            {
                CardSectionHeader(inspectorCardLayout, "关联边", firstSection);
                for (const auto *edge : connectedEdges)
                {
                    ImGui::BulletText("%u %s %u",
                                      edge->from.value,
                                      edge->bidirectional ? "↔" : "→",
                                      edge->to.value);
                }
            }

            if (!resourcesAtNode.empty())
            {
                CardSectionHeader(inspectorCardLayout, "资源", firstSection);
                for (const auto *res : resourcesAtNode)
                {
                    ImGui::BulletText("%s · %s · %u/%u",
                                      res->name.c_str(),
                                      resourceTypeName(res->type),
                                      res->current,
                                      res->capacity);
                }
            }

            if (!agentsAtNode.empty())
            {
                CardSectionHeader(inspectorCardLayout, "在此节点的 Agent", firstSection);
                for (const auto *agentSnap : agentsAtNode)
                {
                    if (!agentSnap->name.empty())
                    {
                        ImGui::BulletText("#%u %s", agentSnap->entityId, agentSnap->name.c_str());
                    }
                    else
                    {
                        ImGui::BulletText("#%u", agentSnap->entityId);
                    }
                }
            }
            break;
        }
        }

        if (!snapshot.events.empty())
        {
            bool eventsSectionFirst = false;
            CardSectionHeader(inspectorCardLayout, "本帧运行事件", eventsSectionFirst);
            if (ImGui::BeginChild("InspectorEventsLog", ImVec2(0, 140.0f), true))
            {
                for (const auto &evt : snapshot.events)
                {
                    ImGui::TextColored(evt.success ? Style::DesignTokens::color(Style::ColorToken::Success)
                                                   : Style::DesignTokens::color(Style::ColorToken::Danger),
                                       "[#%llu] %s",
                                       static_cast<unsigned long long>(evt.id),
                                       evt.label.c_str());
                    if (!evt.message.empty())
                    {
                        ImGui::BulletText("%s", evt.message.c_str());
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::PopTextWrapPos();
    }

void MainView::drawSceneUnified(UiContext &ctx, InspectorView &inspector)
{
    ScenePresenterInput presenterInput{
        ctx.state,
        ctx.runtime_bridge ? &ctx.runtime_bridge->atlas() : nullptr,
        ctx.latest_snapshot ? &*ctx.latest_snapshot : nullptr};
    const RuntimeBridge::WorldAtlas *atlasPtr = presenterInput.atlas;

    if (ctx.state.scene_selected_node == 0 && atlasPtr && !atlasPtr->nodes.empty())
    {
        ctx.state.scene_selected_node = atlasPtr->nodes.front().id.value;
        if (ctx.state.inspector_selection_type == UiState::InspectorSelectionType::None)
        {
            ctx.state.inspector_selection_type = UiState::InspectorSelectionType::Node;
            ctx.state.inspector_selected_primary = ctx.state.scene_selected_node;
            ctx.state.inspector_selected_secondary = 0;
            ctx.state.inspector_highlight_node = ctx.state.scene_selected_node;
        }
    }

    SceneNodeViewModel nodeVm = scene_presenter_.buildNodeViewModel(presenterInput);
    if (!nodeVm.active && !nodeVm.nodes.empty())
    {
        ctx.state.scene_selected_node = nodeVm.nodes.front().id;
        nodeVm = scene_presenter_.buildNodeViewModel(presenterInput);
        if (ctx.state.inspector_selection_type == UiState::InspectorSelectionType::None)
        {
            ctx.state.inspector_selection_type = UiState::InspectorSelectionType::Node;
            ctx.state.inspector_selected_primary = ctx.state.scene_selected_node;
            ctx.state.inspector_selected_secondary = 0;
            ctx.state.inspector_highlight_node = ctx.state.scene_selected_node;
        }
    }

    const SceneNodeDetails *detailsPtr = nodeVm.active ? &*nodeVm.active : nullptr;

    ImGui::BeginChild("SceneTabRoot", ImVec2(0.0f, 0.0f), false);
    const bool resetRequested = drawSceneToolbar(ctx, detailsPtr);

    const bool showSidebar = true;
    const float spacing = Style::DesignTokens::spacing(Style::SpacingToken::Md);

    ImGui::BeginChild("SceneContentArea", ImVec2(0.0f, 0.0f), false);

    const ImVec2 totalAvail = ImGui::GetContentRegionAvail();
    float sidebarWidth = 0.0f;
    if (showSidebar)
    {
        sidebarWidth = std::clamp(totalAvail.x * 0.32f, 220.0f, 360.0f);
        if (sidebarWidth > totalAvail.x - 240.0f)
        {
            sidebarWidth = std::max(totalAvail.x - 240.0f, 200.0f);
        }
    }

    float viewportWidth = showSidebar ? std::max(totalAvail.x - sidebarWidth - spacing, 240.0f) : totalAvail.x;

    ImGui::BeginChild("SceneViewportPane", ImVec2(viewportWidth, 0.0f), false);
    auto viewportState = drawSceneViewport(ctx, presenterInput, nodeVm, resetRequested);
    ImGui::EndChild();

    if (showSidebar)
    {
        ImGui::SameLine(0.0f, spacing);
        ImGui::BeginChild("SceneSidebarPane", ImVec2(0.0f, 0.0f), false);
        if (ctx.state.show_inspector)
        {
            inspector.render(ctx, viewportState);
        }
        else
        {
            if (ImGui::Button("显示检查器"))
            {
                ctx.state.show_inspector = true;
            }
            Ui::applyClickableCursorToLastItem();
        }
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::EndChild();
    }

bool MainView::drawSceneToolbar(UiContext &ctx, const SceneNodeDetails *details)
    {
        const float toggleSpacing = Style::DesignTokens::spacing(Style::SpacingToken::Sm);
        auto drawToolButton = [&](const char *label, SceneSelectionTool tool) {
            const bool active = (ctx.state.scene_selection_tool == tool);
            PushActiveButtonStyle(active);
            if (ImGui::Button(label))
            {
                ctx.state.scene_selection_tool = tool;
            }
            Ui::applyClickableCursorToLastItem();
            PopActiveButtonStyle(active);
        };

        drawToolButton("任意", SceneSelectionTool::Any);
        ImGui::SameLine(0.0f, toggleSpacing);
        drawToolButton("节点", SceneSelectionTool::Node);
        ImGui::SameLine(0.0f, toggleSpacing);
        drawToolButton("瓦片", SceneSelectionTool::Tile);

        ImGui::SameLine(0.0f, toggleSpacing);
        ImGui::TextDisabled("图层：");
        ImGui::SameLine(0.0f, toggleSpacing);
        DrawOverlayToggleButton("SceneOverlayGrid", ctx.state.scene_show_grid, "网格", "Ctrl+G 切换网格显示");
        ImGui::SameLine(0.0f, toggleSpacing);
        DrawOverlayToggleButton("SceneOverlayPortals", ctx.state.scene_show_graph, "传送门", "Ctrl+1 切换传送门显示");
        ImGui::SameLine(0.0f, toggleSpacing);
        DrawOverlayToggleButton("SceneOverlayAnchors", ctx.state.scene_show_anchors, "锚点", "Ctrl+A 切换锚点显示");
        ImGui::SameLine(0.0f, toggleSpacing);
        DrawOverlayToggleButton("SceneOverlayResources", ctx.state.scene_show_resources, "资源", "Ctrl+R 切换资源叠加");
        ImGui::SameLine(0.0f, toggleSpacing);
        DrawOverlayToggleButton("SceneOverlayRuler", ctx.state.scene_show_ruler, "标尺", "Ctrl+6 切换标尺（左键设起点，右键清除）");

        bool resetRequested = false;
        ImGui::SameLine(0.0f, toggleSpacing);
        if (details)
        {
            if (ImGui::Button("重置视图"))
            {
                resetRequested = true;
            }
            Ui::applyClickableCursorToLastItem();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("重置视图");
            ImGui::EndDisabled();
        }
        ImGui::SameLine(0.0f, toggleSpacing);
        ImGui::TextDisabled("滚轮缩放｜右键拖拽");

        ImGui::Dummy(ImVec2(0.0f, Style::DesignTokens::spacing(Style::SpacingToken::Sm)));
        return resetRequested;
    }

std::optional<SceneViewportRenderState> MainView::drawSceneViewport(UiContext &ctx,
                                                                                  const ScenePresenterInput &presenterInput,
                                                                                  const SceneNodeViewModel &nodeVm,
                                                                                  bool resetRequested)
    {
        const RuntimeBridge::WorldAtlas *atlasPtr = presenterInput.atlas;
        const SceneNodeDetails *detailsPtr = nodeVm.active ? &*nodeVm.active : nullptr;
        const SceneNodeGridInfo *gridPtr = detailsPtr ? &detailsPtr->grid : nullptr;

        const ImVec2 canvasAvail = ImGui::GetContentRegionAvail();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasExtent{std::max(160.0f, canvasAvail.x), std::max(220.0f, canvasAvail.y)};
        const ImVec2 canvasMax{canvasPos.x + canvasExtent.x, canvasPos.y + canvasExtent.y};
        constexpr float canvasMargin = 24.0f;

        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("SceneCanvas", canvasExtent, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        auto drawCanvasBackground = [&]() {
            drawList->AddRectFilled(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_WindowBg));
            drawList->AddRect(canvasPos, canvasMax, ImGui::GetColorU32(ImGuiCol_Border));
        };
        auto drawCanvasMessage = [&](const char *text) -> std::optional<SceneViewportRenderState> {
            drawCanvasBackground();
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const ImVec2 textPos{
                canvasPos.x + (canvasExtent.x - textSize.x) * 0.5f,
                canvasPos.y + (canvasExtent.y - textSize.y) * 0.5f};
            drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), text);
            return std::nullopt;
        };

        if (!atlasPtr)
        {
            return drawCanvasMessage("RuntimeBridge 未就绪。");
        }

        if (nodeVm.nodes.empty())
        {
            return drawCanvasMessage("暂无地图节点。");
        }

        if (!detailsPtr || !gridPtr)
        {
            return drawCanvasMessage("请选择包含瓦片数据的节点。");
        }

        const SceneNodeDetails &details = *detailsPtr;
        const SceneNodeGridInfo &grid = *gridPtr;

        drawCanvasBackground();
        const bool canvasHovered = ImGui::IsItemHovered();
        const bool canvasActive = ImGui::IsItemActive();
        ImGuiIO &io = ImGui::GetIO();

        drawList->PushClipRect(canvasPos, canvasMax, true);

        bool shouldRecenter = resetRequested;
        if (ctx.state.scene_camera_node != details.nodeId)
        {
            ctx.state.scene_camera_node = details.nodeId;
            shouldRecenter = true;
        }
        if (ctx.state.scene_focus_node_request && *ctx.state.scene_focus_node_request == details.nodeId)
        {
            ctx.state.scene_focus_node_request.reset();
            shouldRecenter = true;
        }

        auto centerCamera = [&](float zoom)
        {
            const float cell = grid.baseTileSize * zoom;
            const float mapWidthPx = (grid.maxX - grid.minX + 1) * cell;
            const float mapHeightPx = (grid.maxY - grid.minY + 1) * cell;
            const float viewWidth = canvasExtent.x - canvasMargin * 2.0f;
            const float viewHeight = canvasExtent.y - canvasMargin * 2.0f;
            ctx.state.scene_cam_offset_x = (viewWidth - mapWidthPx) * 0.5f;
            ctx.state.scene_cam_offset_y = (viewHeight - mapHeightPx) * 0.5f;
        };

        auto enforceCameraBounds = [&]()
        {
            const float cell = grid.baseTileSize * ctx.state.scene_cam_zoom;
            const float mapWidthPx = (grid.maxX - grid.minX + 1) * cell;
            const float mapHeightPx = (grid.maxY - grid.minY + 1) * cell;
            const float viewWidth = canvasExtent.x - canvasMargin * 2.0f;
            const float viewHeight = canvasExtent.y - canvasMargin * 2.0f;

            auto clampOffset = [&](float mapSize, float viewSize, float &offset)
            {
                if (mapSize <= viewSize)
                {
                    offset = (viewSize - mapSize) * 0.5f;
                }
                else
                {
                    const float minOffset = viewSize - mapSize;
                    const float maxOffset = 0.0f;
                    if (offset < minOffset)
                    {
                        offset = minOffset;
                    }
                    else if (offset > maxOffset)
                    {
                        offset = maxOffset;
                    }
                }
            };

            clampOffset(mapWidthPx, viewWidth, ctx.state.scene_cam_offset_x);
            clampOffset(mapHeightPx, viewHeight, ctx.state.scene_cam_offset_y);
        };

        if (shouldRecenter)
        {
            ctx.state.scene_cam_zoom = 1.0f;
            centerCamera(ctx.state.scene_cam_zoom);
            enforceCameraBounds();
        }

        const float prevZoom = ctx.state.scene_cam_zoom;
        if (io.MouseWheel != 0.0f)
        {
            const float zoomFactor = (io.MouseWheel > 0.0f) ? 1.1f : 0.9f;
            float newZoom = prevZoom * zoomFactor;
            newZoom = std::clamp(newZoom, 0.25f, 6.0f);

            if (std::abs(newZoom - prevZoom) > 1e-4f)
            {
                const ImVec2 focus = canvasHovered ? io.MousePos : ImVec2(canvasPos.x + canvasExtent.x * 0.5f, canvasPos.y + canvasExtent.y * 0.5f);
                const float prevCell = grid.baseTileSize * prevZoom;
                const float newCell = grid.baseTileSize * newZoom;
                const float localX = focus.x - (canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x);
                const float localY = focus.y - (canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y);

                ctx.state.scene_cam_zoom = newZoom;
                ctx.state.scene_cam_offset_x = localX - (localX / prevCell) * newCell;
                ctx.state.scene_cam_offset_y = localY - (localY / prevCell) * newCell;
                enforceCameraBounds();
            }
        }

        if (canvasActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            const ImVec2 delta = io.MouseDelta;
            ctx.state.scene_cam_offset_x += delta.x;
            ctx.state.scene_cam_offset_y += delta.y;
            enforceCameraBounds();
        }

        const float cellPx = grid.baseTileSize * ctx.state.scene_cam_zoom;
        if (cellPx <= 0.01f)
        {
            drawList->AddText(canvasPos, ImGui::GetColorU32(ImGuiCol_Text), "瓦片尺寸异常。");
            drawList->PopClipRect();
            ImGui::Dummy(canvasExtent);
            return std::nullopt;
        }

        auto toScreenRaw = [&](float gx, float gy) -> ImVec2
        {
            const float px = canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x + (gx - static_cast<float>(grid.minX)) * cellPx;
            const float py = canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y + (gy - static_cast<float>(grid.minY)) * cellPx;
            return ImVec2(px, py);
        };
        auto toScreen = [&](float gx, float gy) -> ImVec2
        {
            ImVec2 raw = toScreenRaw(gx, gy);
            return ImVec2(std::floor(raw.x) + 0.5f, std::floor(raw.y) + 0.5f);
        };
        auto tileFromScreen = [&](const ImVec2 &screen) -> std::optional<std::pair<int, int>>
        {
            const float localX = screen.x - (canvasPos.x + canvasMargin + ctx.state.scene_cam_offset_x);
            const float localY = screen.y - (canvasPos.y + canvasMargin + ctx.state.scene_cam_offset_y);
            const float gx = localX / cellPx + static_cast<float>(grid.minX);
            const float gy = localY / cellPx + static_cast<float>(grid.minY);
            const int tileX = static_cast<int>(std::floor(gx));
            const int tileY = static_cast<int>(std::floor(gy));
            if (tileX < grid.minX || tileX > grid.maxX || tileY < grid.minY || tileY > grid.maxY)
            {
                return std::nullopt;
            }
            return std::pair{tileX, tileY};
        };

        if (details.tilemap)
        {
            const ImU32 tileColorA = ImGui::GetColorU32(ImVec4(0.18f, 0.25f, 0.32f, 0.65f));
            const ImU32 tileColorB = ImGui::GetColorU32(ImVec4(0.15f, 0.21f, 0.28f, 0.65f));
            for (int y = 0; y < details.tilemap->height; ++y)
            {
                for (int x = 0; x < details.tilemap->width; ++x)
                {
                    const float gx0 = static_cast<float>(grid.minX + x);
                    const float gy0 = static_cast<float>(grid.minY + y);
                    ImVec2 cellMin = toScreenRaw(gx0, gy0);
                    ImVec2 cellMax = toScreenRaw(gx0 + 1.0f, gy0 + 1.0f);
                    const ImU32 fill = ((x + y) % 2 == 0) ? tileColorA : tileColorB;
                    drawList->AddRectFilled(cellMin, cellMax, fill);
                }
            }
        }
        else
        {
            const ImVec2 min = toScreenRaw(static_cast<float>(grid.minX), static_cast<float>(grid.minY));
            const ImVec2 max = toScreenRaw(static_cast<float>(grid.maxX + 1), static_cast<float>(grid.maxY + 1));
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.14f, 0.16f, 0.20f, 0.65f)));
        }

        if (ctx.state.scene_show_grid)
        {
            const ImU32 gridColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::BorderSoft));
            for (int x = grid.minX; x <= grid.maxX + 1; ++x)
            {
                drawList->AddLine(toScreen(static_cast<float>(x), static_cast<float>(grid.minY)),
                                  toScreen(static_cast<float>(x), static_cast<float>(grid.maxY + 1)),
                                  gridColor,
                                  1.0f);
            }
            for (int y = grid.minY; y <= grid.maxY + 1; ++y)
            {
                drawList->AddLine(toScreen(static_cast<float>(grid.minX), static_cast<float>(y)),
                                  toScreen(static_cast<float>(grid.maxX + 1), static_cast<float>(y)),
                                  gridColor,
                                  1.0f);
            }
        }

        std::optional<std::pair<int, int>> hoveredTile;
        if (canvasHovered)
        {
            hoveredTile = tileFromScreen(io.MousePos);
        }

        const ImU32 highlightColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Highlight));
        if (ctx.state.scene_tile_selection && ctx.state.scene_tile_selection->nodeId == details.nodeId)
        {
            const auto &sel = *ctx.state.scene_tile_selection;
            ImVec2 selMin = toScreenRaw(static_cast<float>(sel.tileX), static_cast<float>(sel.tileY));
            ImVec2 selMax = toScreenRaw(static_cast<float>(sel.tileX + 1), static_cast<float>(sel.tileY + 1));
            drawList->AddRect(selMin, selMax, highlightColor, 0.0f, 0, 2.0f);
        }

        if (ctx.state.ui_fps_display > 0.0f)
        {
            const int fpsRounded = static_cast<int>(std::lround(ctx.state.ui_fps_display));
            char fpsBuffer[16];
            std::snprintf(fpsBuffer, sizeof(fpsBuffer), "FPS %d", fpsRounded);
            const ImVec2 textSize = ImGui::CalcTextSize(fpsBuffer);
            const ImVec2 padding{6.0f, 4.0f};
            const ImVec2 textPos{canvasPos.x + 10.0f, canvasPos.y + 10.0f};
            const ImVec2 bgMin{textPos.x - padding.x, textPos.y - padding.y};
            const ImVec2 bgMax{textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y};
            drawList->AddRectFilled(bgMin, bgMax, ImGui::GetColorU32(ImVec4(0.08f, 0.09f, 0.12f, 0.85f)), 4.0f);
            drawList->AddRect(bgMin, bgMax, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, ImDrawFlags_RoundCornersAll, 1.0f);
            drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), fpsBuffer);
        }

        if (hoveredTile)
        {
            ImVec2 hMin = toScreenRaw(static_cast<float>(hoveredTile->first), static_cast<float>(hoveredTile->second));
            ImVec2 hMax = toScreenRaw(static_cast<float>(hoveredTile->first + 1), static_cast<float>(hoveredTile->second + 1));
            drawList->AddRect(hMin, hMax, highlightColor, 0.0f, 0, 1.2f);
        }

        auto nodeNameById = [&](std::uint32_t id) -> std::string
        {
            const auto it = std::find_if(nodeVm.nodes.begin(), nodeVm.nodes.end(), [&](const SceneNodeSummary &entry) {
                return entry.id == id;
            });
            if (it != nodeVm.nodes.end())
            {
                return it->name.empty() ? ("节点 " + std::to_string(it->id)) : it->name;
            }
            for (const auto &node : atlasPtr->nodes)
            {
                if (node.id.value == id)
                {
                    return node.name.empty() ? ("节点 " + std::to_string(node.id.value)) : node.name;
                }
            }
            return "#" + std::to_string(id);
        };

        const bool nodeToolActive = ctx.state.scene_selection_tool == SceneSelectionTool::Any ||
                                    ctx.state.scene_selection_tool == SceneSelectionTool::Node;
        const bool tileToolActive = ctx.state.scene_selection_tool == SceneSelectionTool::Any ||
                                    ctx.state.scene_selection_tool == SceneSelectionTool::Tile;

        const float portalRadius = std::max(6.0f, cellPx * 0.25f);
        const ImU32 portalColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
        std::optional<SceneNodePortal> hoveredPortal;
        if (ctx.state.scene_show_graph)
        {
            for (const auto &portal : details.portals)
            {
                const ImVec2 center = toScreen(portal.x + 0.5f, portal.y + 0.5f);
                drawList->AddCircleFilled(center, portalRadius, portalColor, 24);
                drawList->AddCircle(center, portalRadius, ImGui::GetColorU32(ImGuiCol_Border), 24, 1.2f);

                const std::string label = "→ " + nodeNameById(portal.targetNodeId);
                drawList->AddText(ImVec2(center.x + portalRadius + 4.0f, center.y - ImGui::GetTextLineHeight() * 0.5f),
                                  ImGui::GetColorU32(ImGuiCol_Text),
                                  label.c_str());

                if (canvasHovered)
                {
                    const float dx = io.MousePos.x - center.x;
                    const float dy = io.MousePos.y - center.y;
                    if ((dx * dx + dy * dy) <= (portalRadius * portalRadius))
                    {
                        hoveredPortal = portal;
                        }
                    }
                }
            }

        if (hoveredPortal && nodeToolActive && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (hoveredPortal->targetNodeId != 0)
            {
                ctx.state.scene_selected_node = hoveredPortal->targetNodeId;
                ctx.state.map_selected_node = hoveredPortal->targetNodeId;
                ctx.state.scene_tile_selection.reset();
                ctx.state.scene_focus_node_request = hoveredPortal->targetNodeId;
                ctx.state.scene_camera_node = 0;
            }
        }

        if (hoveredPortal)
        {
            ImGui::BeginTooltip();
            ImGui::Text("前往：%s", nodeNameById(hoveredPortal->targetNodeId).c_str());
            ImGui::EndTooltip();
        }

        if (tileToolActive && hoveredTile && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hoveredPortal)
        {
            ctx.state.scene_tile_selection = SceneTileSelection{details.nodeId, hoveredTile->first, hoveredTile->second};
            ctx.state.scene_selected_node = details.nodeId;
        }

        if (!ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        {
            const ImVec2 drag = io.MouseDragMaxDistanceAbs[ImGuiMouseButton_Right];
            if (drag.x < 4.0f && drag.y < 4.0f)
            {
                ctx.state.scene_tile_selection.reset();
            }
        }

        if (ctx.state.scene_show_resources)
        {
            auto colorForResource = [](genesis::world::ResourceType type) -> ImU32
            {
                switch (type)
                {
                case genesis::world::ResourceType::Food:
                    return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Warning));
                case genesis::world::ResourceType::Drink:
                    return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
                case genesis::world::ResourceType::Social:
                    return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Success));
                default:
                    return ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Muted));
                }
            };

            for (const auto &resource : details.resources)
            {
                const ImVec2 center = toScreen(resource.x + 0.5f, resource.y + 0.5f);
                const float radius = std::max(3.5f, cellPx * 0.25f);
                const ImU32 fillColor = colorForResource(resource.type);
                drawList->AddCircleFilled(center, radius, fillColor, 16);
                drawList->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Border), 16, 1.1f);
            }
        }

        if (ctx.state.scene_show_anchors)
        {
            const ImU32 anchorColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::AccentActive));
            for (const auto &anchor : details.anchors)
            {
                const ImVec2 base = toScreen(anchor.x + 0.5f, anchor.y + 0.5f);
                const float size = std::max(4.0f, cellPx * 0.2f);
                const ImVec2 a{base.x - size, base.y};
                const ImVec2 b{base.x + size, base.y};
                const ImVec2 c{base.x, base.y + size * 1.6f};
                drawList->AddTriangleFilled(a, b, c, anchorColor);
                drawList->AddTriangle(a, b, c, ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
                const std::string label = std::string("↔ ") + nodeNameById(anchor.targetNodeId);
                drawList->AddText(ImVec2(base.x + size + 4.0f, base.y - ImGui::GetTextLineHeight() * 0.5f),
                                  ImGui::GetColorU32(ImGuiCol_Text),
                                  label.c_str());
            }
        }

        if (ctx.state.scene_show_ruler)
        {
            const ImU32 rulerColor = ImGui::GetColorU32(Style::DesignTokens::color(Style::ColorToken::Accent));
            const ImVec2 cursor = io.MousePos;
            const bool inside = cursor.x >= canvasPos.x && cursor.x <= canvasMax.x &&
                                cursor.y >= canvasPos.y && cursor.y <= canvasMax.y;
            if (inside)
            {
                drawList->AddLine(ImVec2(cursor.x, canvasPos.y), ImVec2(cursor.x, canvasMax.y), rulerColor, 1.0f);
                drawList->AddLine(ImVec2(canvasPos.x, cursor.y), ImVec2(canvasMax.x, cursor.y), rulerColor, 1.0f);
            }

            if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (auto tile = tileFromScreen(cursor))
                {
                    ctx.state.scene_ruler_anchor = RuntimeBridge::Vector2{
                        static_cast<float>(tile->first),
                        static_cast<float>(tile->second)};
                }
                else
                {
                    ctx.state.scene_ruler_anchor.reset();
                }
            }
            if (!ImGui::IsMouseDragging(ImGuiMouseButton_Right) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ctx.state.scene_ruler_anchor.reset();
            }

            if (ctx.state.scene_ruler_anchor)
            {
                const RuntimeBridge::Vector2 anchorTile = *ctx.state.scene_ruler_anchor;
                const ImVec2 anchorScreen = toScreen(anchorTile.x + 0.5f, anchorTile.y + 0.5f);
                drawList->AddCircleFilled(anchorScreen, 4.0f, rulerColor, 24);
                drawList->AddLine(anchorScreen, cursor, rulerColor, 1.3f);

                if (auto tile = tileFromScreen(cursor))
                {
                    const float dx = static_cast<float>(tile->first) - anchorTile.x;
                    const float dy = static_cast<float>(tile->second) - anchorTile.y;
                    const float dist = std::hypot(dx, dy);
                    char buffer[96];
                    std::snprintf(buffer, sizeof(buffer), "Δ (%d, %d)  dist %.2f", static_cast<int>(dx), static_cast<int>(dy), dist);
                    drawList->AddText(ImVec2(cursor.x + 12.0f, cursor.y - 18.0f),
                                      ImGui::GetColorU32(ImGuiCol_Text),
                                      buffer);
                }
            }
        }

        if (hoveredTile)
        {
            char coordBuffer[48];
            std::snprintf(coordBuffer, sizeof(coordBuffer), "Tile (%d, %d)", hoveredTile->first, hoveredTile->second);
            drawList->AddText(ImVec2(canvasPos.x + 10.0f, canvasMax.y - ImGui::GetTextLineHeightWithSpacing() - 6.0f),
                              ImGui::GetColorU32(ImGuiCol_TextDisabled),
                              coordBuffer);
        }

        drawList->PopClipRect();

        SceneViewportRenderState state{};
        state.details = &details;
        state.grid = grid;
        state.canvasPos = canvasPos;
        state.canvasExtent = canvasExtent;
        state.canvasMargin = canvasMargin;
        state.cellPx = cellPx;
        const float visibleWidth = canvasExtent.x - canvasMargin * 2.0f;
        const float visibleHeight = canvasExtent.y - canvasMargin * 2.0f;
        state.viewWidthTiles = std::max(1e-3f, visibleWidth / cellPx);
        state.viewHeightTiles = std::max(1e-3f, visibleHeight / cellPx);
        state.viewOriginX = static_cast<float>(grid.minX) - ctx.state.scene_cam_offset_x / cellPx;
        state.viewOriginY = static_cast<float>(grid.minY) - ctx.state.scene_cam_offset_y / cellPx;

        return state;
    }

} // namespace genesis::sandbox::gui
