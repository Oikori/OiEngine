// 《永夜幸存者》全部 ImGui 面板：标题 / 帮助 / 设置 / HUD / 升级卡 / 暂停 / 阵亡 / 通关。
//
// 面板之间没有共享状态，除了 m_state 与 m_show* 开关；升级卡必须能用鼠标点（见 ImGui与输入.md）。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ ImGui ============================

ImVec2 SurvivorsLayer::WorldToScreen(const glm::vec2& world) const
{
    const float scale = static_cast<float>(m_viewHeight) / (2.0f * VIEW_HALF_HEIGHT);
    return ImVec2(
        static_cast<float>(m_viewWidth) * 0.5f + (world.x - m_cameraFocus.x) * scale,
        static_cast<float>(m_viewHeight) * 0.5f - (world.y - m_cameraFocus.y) * scale);
}

void SurvivorsLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }
    if (m_state == State::Title) {
        RenderTitle();
    } else if (m_state == State::Setup) {
        RenderSetup();   // 开局选择：单独一屏，不显示 HUD
    } else {
        RenderHud();

        switch (m_state) {
            case State::LevelUp: RenderLevelUp(); break;
            case State::WeaponPick: RenderWeaponPick(); break;
            case State::Shop: RenderShop(); break;
            case State::Paused: RenderPause(); break;
            case State::GameOver: RenderGameOver(); break;
            case State::Victory: RenderVictory(); break;
            default: break;
        }
    }

    // 帮助 / 设置面板永远画在最上层（标题页 / 暂停菜单里打开）
    if (m_showHelp) {
        RenderHelp();
    }
    if (m_showOptions) {
        RenderOptions();
    }
}

// ============================ 图标 ============================

namespace {
    // 图标缺失时返回它（UI 拿到空引用就跳过绘制）
    const Ref<Texture2D> NO_ICON;
}

const Ref<Texture2D>& SurvivorsLayer::ItemIcon(ItemId id) const
{
    const size_t index = static_cast<size_t>(id);
    if (index >= m_art.itemIcons.size()) {
        return NO_ICON;
    }
    return m_art.itemIcons[index];
}

const Ref<Texture2D>& SurvivorsLayer::WeaponIcon(int weapon) const
{
    if (weapon < 0 || static_cast<size_t>(weapon) >= m_art.weaponIcons.size()) {
        return NO_ICON;
    }
    return m_art.weaponIcons[static_cast<size_t>(weapon)];
}

const Ref<Texture2D>& SurvivorsLayer::PassiveIcon(int passive) const
{
    if (passive < 0 || static_cast<size_t>(passive) >= m_art.passiveIcons.size()) {
        return NO_ICON;
    }
    return m_art.passiveIcons[static_cast<size_t>(passive)];
}

void SurvivorsLayer::DrawCardIcon(const Ref<Texture2D>& icon, float cardWidth, float size,
                                  const glm::vec4& glow)
{
    if (icon) {
        ImGui::SetCursorPosX((cardWidth - size) * 0.5f);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        // 底衬：暗色卡片上图标容易糊进背景，垫一层按稀有度取的淡光把人托出来
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(origin.x - 5.0f, origin.y - 5.0f),
            ImVec2(origin.x + size + 5.0f, origin.y + size + 5.0f),
            IM_COL32(static_cast<int>(glow.x * 96.0f), static_cast<int>(glow.y * 96.0f),
                     static_cast<int>(glow.z * 96.0f), 110),
            size * 0.24f);
        // 纹理是 stb_image 的 flip 约定（v = 0 在图像底部），ImGui 的 UV 要把 V 反过来
        ImGui::Image(static_cast<ImTextureID>(icon->GetRendererID()), ImVec2(size, size),
                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

void SurvivorsLayer::RenderTitle()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 3, 10, 150));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.38f, 0.72f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    if (ImGui::Begin("##survivors-title-screen", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(2.4f);
        ImGui::TextColored(ImVec4(0.95f, 0.90f, 0.72f, 1.0f), "永 夜 幸 存 者");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextColored(ImVec4(0.66f, 0.70f, 0.86f, 1.0f), "十关长夜，撑到黎明。");

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.46f, 0.38f, 0.72f, 0.45f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.34f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.46f, 0.19f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.57f, 0.23f, 1.0f));
        if (ImGui::Button("开始游戏", ImVec2(buttonWidth, 46.0f))) {
            ResetRun();   // → 选角色 + 武器
        }
        ImGui::PopStyleColor(3);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.20f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.30f, 0.46f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.40f, 0.58f, 1.0f));
        if (ImGui::Button("游戏说明 / 操作", ImVec2(buttonWidth, 40.0f))) {
            m_showOptions = false;
            m_showHelp = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.20f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.30f, 0.46f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.40f, 0.58f, 1.0f));
        if (ImGui::Button("设置 / 音量", ImVec2(buttonWidth, 40.0f))) {
            m_showHelp = false;
            m_showOptions = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 40.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);

        // 标题页不再有「开始游戏」的键盘快捷键：只能点按钮（避免误按 Enter / 空格直接开局）
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// ============================ 开局：选角色 + 武器 ============================

void SurvivorsLayer::RenderSetup()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 3, 10, 210));

    const ImVec4 colTitle(0.98f, 0.87f, 0.60f, 1.0f);
    const ImVec4 colText(0.92f, 0.94f, 1.00f, 1.0f);
    const ImVec4 colBody(0.74f, 0.78f, 0.90f, 1.0f);
    const ImVec4 colDim(0.62f, 0.66f, 0.80f, 0.95f);

    const int characterCount = std::max(1, CharacterCount());
    // 武器卡的文案要跟着「当前高亮的角色」走（术士的雷暴是范围伤害、间隔也少 0.5 秒）。
    // 这里只同步展示用的角色标识，真正的属性结算仍在 BeginRun 的 ApplyCharacter
    m_character = CharacterItem(std::clamp(m_setupCharacter, 0, characterCount - 1));
    const float cardWidth = 260.0f;
    const float gap = 18.0f;
    // 角色卡多了「特性」两行说明（左上角那行「角色 · 史诗」已去掉，净高还是不够，给到 336）
    const float charHeight = 336.0f;
    // 武器卡要比角色卡矮，但至少装得下「玩法 + 主数值 + 攻击间隔」三到四行说明（窗口里固定放按钮）
    const float weaponHeight = 272.0f;

    const float rowWidth = cardWidth * static_cast<float>(characterCount) +
                           gap * static_cast<float>(characterCount - 1);
    const float startX = (display.x - rowWidth) * 0.5f;
    const float titleY = std::max(12.0f, display.y * 0.5f - 350.0f);
    const float charRowY = titleY + 70.0f;
    const float weaponLabelY = charRowY + charHeight + 20.0f;
    const float weaponRowY = weaponLabelY + 34.0f;
    // 底部「开始战斗 / 回到标题」的位置：正常窗口里紧跟武器行，窗口矮的时候
    // **钳进屏幕内**（宁可压在武器卡下沿，也不能被挤出屏幕——那样等于没有确认按钮）
    const float actionY = std::min(weaponRowY + weaponHeight + 16.0f, display.y - 64.0f);

    // ---- 标题 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, titleY), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-setup-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(colTitle, "选 择 开 局");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(colDim, "角色决定初始属性与专属特性 · 武器决定起手手段（之后每 %d 级还能再选一次）",
                           WEAPON_PICK_STEP);
    }
    ImGui::End();

    // ---- 角色卡（四个角色，点「选择」切换）----
    for (int i = 0; i < characterCount; ++i) {
        const ItemId character = CharacterItem(i);
        const glm::vec4 color = ItemColor(character);
        const bool selected = m_setupCharacter == i;

        std::string name;
        std::string description;
        DescribeItem(character, name, description);

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##survivors-setup-char%d", i);
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(i) * (cardWidth + gap), charRowY),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, charHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, selected ? ImVec4(0.10f, 0.09f, 0.16f, 0.97f)
                                                          : ImVec4(0.055f, 0.055f, 0.11f, 0.94f));
        ImGui::PushStyleColor(ImGuiCol_Border, selected
            ? ImVec4(colTitle.x, colTitle.y, colTitle.z, 0.95f)
            : ImVec4(color.x, color.y, color.z, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.34f, color.y * 0.34f, color.z * 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.54f, color.y * 0.54f, color.z * 0.54f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.74f, color.y * 0.74f, color.z * 0.74f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, selected ? 2.5f : 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::Begin(identifier, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        // 立绘：开局是玩家第一次见到「自己要扮演谁」，有贴图就铺一张（没有则留白占位，保持卡片等高）
        constexpr float PORTRAIT = 132.0f;
        if (m_art.HasPlayerSprite(i)) {
            // 居中：SetCursorPosX 是**窗口坐标**（0 = 窗口左边缘，不含内边距），
            // 所以要从当前光标（= 左内边距）再加「可用宽度 − 立绘宽度」的一半；
            // 用 GetContentRegionAvail 而不是手算 cardWidth − 32，滚动条出现时也不会偏
            const float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - PORTRAIT) * 0.5f);
            // 底衬：暗色卡片上直接铺深色角色会糊成一片，垫一层角色色的淡光把人形托出来
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(origin.x - 6.0f, origin.y - 6.0f),
                ImVec2(origin.x + PORTRAIT + 6.0f, origin.y + PORTRAIT + 6.0f),
                IM_COL32(static_cast<int>(color.x * 88.0f), static_cast<int>(color.y * 88.0f),
                         static_cast<int>(color.z * 88.0f), 120),
                10.0f);
            // 纹理是 stb_image 的 flip 约定（v = 0 在图像底部），ImGui 默认 (0,0) 在左上 ——
            // 这里要把 V 反过来，否则立绘上下颠倒
            ImGui::Image(static_cast<ImTextureID>(m_art.playerSprite[i]->GetRendererID()),
                         ImVec2(PORTRAIT, PORTRAIT), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        } else {
            ImGui::Dummy(ImVec2(0.0f, PORTRAIT));
        }

        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(colText, "%s", name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, colBody);
        ImGui::TextWrapped("%s", description.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(charHeight - 28.0f - 30.0f);
        if (ImGui::Button(selected ? "已选择" : "选择", ImVec2(cardWidth - 32.0f, 30.0f)) && !selected) {
            m_setupCharacter = i;
            PlaySfx(SfxId::Pickup, 0.35f, 1.32f);
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    // ---- 武器标题 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, weaponLabelY), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-setup-weapon-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(colTitle, "起 手 武 器");
    }
    ImGui::End();

    // ---- 武器卡 ----
    for (int weapon = 0; weapon < WEAPON_COUNT; ++weapon) {
        const glm::vec4 color = WEAPON_COLORS[weapon];
        const bool selected = m_setupWeapon == weapon;

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##survivors-setup-weapon%d", weapon);
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(weapon) * (cardWidth + gap), weaponRowY),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, weaponHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, selected ? ImVec4(0.10f, 0.09f, 0.16f, 0.97f)
                                                          : ImVec4(0.055f, 0.055f, 0.11f, 0.94f));
        ImGui::PushStyleColor(ImGuiCol_Border, selected
            ? ImVec4(colTitle.x, colTitle.y, colTitle.z, 0.95f)
            : ImVec4(color.x, color.y, color.z, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.34f, color.y * 0.34f, color.z * 0.34f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.54f, color.y * 0.54f, color.z * 0.54f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.74f, color.y * 0.74f, color.z * 0.74f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, selected ? 2.5f : 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
        ImGui::Begin(identifier, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        DrawCardIcon(WeaponIcon(weapon), cardWidth, 52.0f, color);
        ImGui::TextColored(ImVec4(color.x, color.y, color.z, 1.0f), "武器 · Lv.1");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::SetWindowFontScale(1.35f);
        ImGui::TextColored(colText, "%s", WEAPON_NAMES[weapon]);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, colBody);
        ImGui::TextWrapped("%s", DescribeWeapon(weapon, 0).c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(weaponHeight - 24.0f - 32.0f);
        if (ImGui::Button(selected ? "已选择" : "选择", ImVec2(cardWidth - 32.0f, 32.0f)) && !selected) {
            m_setupWeapon = weapon;
            PlaySfx(SfxId::Pickup, 0.35f, 1.32f);
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    // ---- 开始战斗 / 返回（固定在底部，窗口再矮也看得见）----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, actionY), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##survivors-setup-go", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.34f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58f, 0.47f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.72f, 0.59f, 0.24f, 1.0f));
        if (ImGui::Button("开始战斗   (Enter)", ImVec2(300.0f, 44.0f))) {
            BeginRun();
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.20f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.30f, 0.46f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.40f, 0.58f, 1.0f));
        if (ImGui::Button("回到标题   (ESC)", ImVec2(180.0f, 44.0f))) {
            ReturnToTitle();
        }
        ImGui::PopStyleColor(3);

        // ImGui 可能吃掉键盘事件，这里用 ImGui 的输入状态补一条开打路径
        if (!m_showHelp && !m_showOptions &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space))) {
            BeginRun();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void SurvivorsLayer::RenderHelp()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 190));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    // 内容比屏幕长：给一个固定的最大高度，超出部分靠滚轮滚动（ESC 也能关）
    ImGui::SetNextWindowSize(ImVec2(700.0f, std::max(360.0f, std::min(display.y - 70.0f, 860.0f))),
                             ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.38f, 0.72f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f, 22.0f));
    if (ImGui::Begin("##survivors-help", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const ImVec4 colTitle(0.98f, 0.87f, 0.60f, 1.0f);
        const ImVec4 colKey(0.62f, 0.70f, 0.92f, 1.0f);
        const ImVec4 colText(0.86f, 0.88f, 0.95f, 1.0f);
        const ImVec4 colDim(0.60f, 0.64f, 0.78f, 0.95f);

        ImGui::SetWindowFontScale(1.7f);
        ImGui::TextColored(colTitle, "游 戏 说 明");
        ImGui::SetWindowFontScale(1.0f);

        // 段落：小标题 + 正文（正文自动按窗口宽度换行）
        const auto section = [&](const char* name) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextColored(colKey, "%s", name);
        };
        const auto body = [&](const char* text) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(colText, "%s", text);
            ImGui::PopTextWrapPos();
        };

        section("关卡与目标");
        body("共 10 关：第 1 关存活 30 秒，之后每关 +5 秒（第 10 关 75 秒）；撑完第 10 关即通关，生命归零则本局结束。");
        body("只有第 5 关与第 10 关会出现「血月使徒」。第 10 关的使徒是最终战：击杀它当场通关，不必再撑完剩余时间。");
        body("场地四周有围栏：角色只能在围栏内移动，敌人可以越过栏杆；围栏外的阴影里敌人无敌，要等它走进场地才打得到。");
        body("过关会清空场上敌人与弹丸，地上的金币自动收进钱包，然后开商店。");

        section("开局");
        body("先选一个角色（角色类道具，品质固定史诗，给初始属性）和一把武器（1 级起手）。");
        body("之后每 5 级可以从四把武器里再选一次：选没拿过的武器 = 解锁 1 级，选已有的 = 升 1 级（最高 5 级）。");

        section("角色特性");
        body("守夜人：每 1 点防御 +1% 伤害；生命降到 50% 及以下时，防御与每秒回复翻倍。");
        body("猎手：月刃与回旋镰伤害 +40%；月刃的穿透改为弹射 —— 命中后飞向离弹丸最近的、还没打过的敌人。");
        body("修女：圣光环伤害 +50%；受到伤害后 3 秒内回复与攻击速度翻倍。");
        body("术士：全部伤害 +40%；雷暴从单体改为范围伤害，基础攻击间隔 -0.75 秒。");

        section("操作");
        body("WASD / 方向键移动   ·   武器自动索敌开火，无需攻击键");
        body("升级时点击卡片选择（或按 1 / 2 / 3）；面板下方的「刷新」花金币整组重抽（快捷键 R）");
        body("选武器点卡片（或按 1 / 2 / 3 / 4）；商店点「购买」，R 刷新货位，点「离开商店」回战斗（没有键盘离开键）");
        body("ESC 暂停 / 继续：战斗、升级选卡、商店三个阶段都能按，继续时回到暂停前的界面");
        body("F4 打开调试面板（FPS / 实体数 / 伤害统计 / 快速验证按钮）");

        section("成长：经验、升级卡、武器");
        body("击杀敌人当场获得经验，并掉落金币（走近自动吸取，金币**不会消失**，过关 / 通关时全地图的金币自动收进钱包）。金币用于刷新升级卡、刷新商店、买道具。");
        body("每次升级给三张属性卡三选一（选完回到战斗有 1 秒无敌）；刷新价 = 4 + 3 × 本次已刷新次数。武器与「立即回复」已不在升级卡池里。");
        body("属性卡九种：力量 / 急速 / 疾行 / 体魄 / 暴击 / 爆伤 / 回春 / 铁壁 / 破甲，没有层数上限。");
        body("武器四把：月刃（直线穿透，同一枚飞刃对同一敌人只结算一次）/ 圣光环（近身脉冲）/ 回旋镰（一把锁最近敌人、其余等分圆抛出，飞出后折返切割，去程与回程各结算一次）/ 雷暴（单体落雷：一道必打最近的敌人、一道优先打 Boss、其余随机，术士变范围伤害），各自最高 5 级。");

        section("道具");
        body("每关通关后开商店，一次摆 4 件道具（角色不卖），可以刷新（(6 + 4 × 本次已刷新次数) × 关卡系数）。");
        body("关卡系数 = 1 + 0.4 × (已过关数 − 1)：第一次开商店是原价，越往后刷新越贵。");
        body("种类决定能不能重复拿：普通类（无上限，可叠）· 限制类（每件有持有上限 1~3，原「独特」道具也在这里）· 角色（开局选）。");
        body("稀有度是每件道具固定的档位（普通 / 高级 / 稀有 / 史诗）：铁片永远是高级、血袋永远是普通，");
        body("它只决定价格与刷出来的概率，效果是道具自身的固定数值。");
        body("价格 12 / 22 / 40 / 66 起：前 5 关每关 +20%，之后涨价加速；越稀有的越难出现在货架上。");
        body("档位按关卡解锁：第 3 关起才卖稀有档，第 5 关起才卖史诗档（前两关的货架只有普通 / 高级）。");

        section("事件与事件道具");
        body("游戏里会发生八类事件：击杀敌人、拾取金币、受到伤害、通过一关、暴击命中、计时（每 8 秒）、花费金币、升级。");
        body("击杀 · 哀嚎钟（每 12 杀音波）/ 血契（每次击杀回血）；拾金 · 铸币者之锤 / 拾荒护符 / 贪欲之链；");
        body("受伤 · 受难者圣痕；过关 · 黎明誓约；暴击 · 淬血匕首（每次暴击回血）/ 破晓之刃（每 8 次暴击爆发）；");
        body("计时 · 沙漏（回血）/ 节拍器（对最近敌人打一击）；消费 · 贪婪契约（回血）/ 商人之瞳（永久 +伤害）；");
        body("升级 · 成长之书（回血）/ 星辉残页（+固定穿甲）。阈值型道具的进度与八类事件的累计次数都在暂停菜单的「事件」一栏。");

        section("属性与品质");
        body("升级卡（属性卡）有品质：普通 / 高级 / 稀有 / 史诗，加成倍率 1.0 / 1.6 / 2.4 / 3.5，等级越高越容易出高品质。");
        body("属性卡没有层数上限，可以一直叠。");
        body("防御：固定值，减伤率 = 防御 / (防御 + 100)。100 点 = 50%，越往上堆收益越钝，但永远不会免疫。");
        body("穿甲：固定穿甲直接扣对方这么多点防御（「破甲」不再附带百分比穿甲）；防御被扣到 0 为止，溢出部分不额外生效。");
        body("上限：只有暴击率封顶 100%；移动速度不设上限，其余属性只会线性叠加。");

        section("敌人");
        body("蝙蝠（无防御）· 食尸鬼（防御 10）· 怨灵（24）· 石魔（50）· 血月使徒（100）。");
        body("怪物数值随关卡递增：小怪血量每关 +30%、防御每关 +20%（线性）；伤害是三次曲线（1 + (n−1)³/81），");
        body("第 10 关的伤害约为第 1 关的 10 倍。经验与金币不随关卡变化。");
        body("血月使徒的血量另算：第 5 关 2 倍、第 10 关 40 倍基础值（伤害与防御与小怪同一套）。");
        body("血月使徒会冲刺：靠近到一定距离后锁定方向、蓄力 1 秒，再沿这条直线高速冲一段；");
        body("蓄力时地上那道血线就是它的冲刺轨迹，横向拉开就能躲开（冲刺撞到围栏会提前停下）。");

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("关 闭   (ESC)", ImVec2(buttonWidth, 38.0f))) {   // ESC 由 PollPauseKeys 处理
            m_showHelp = false;
        }
        ImGui::TextColored(colDim, "标题界面与暂停菜单都可以再次打开本说明");
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void SurvivorsLayer::RenderOptions()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 190));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.38f, 0.72f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f, 22.0f));
    if (ImGui::Begin("##survivors-options", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const ImVec4 colTitle(0.98f, 0.87f, 0.60f, 1.0f);
        const ImVec4 colLabel(0.62f, 0.70f, 0.92f, 1.0f);
        const ImVec4 colText(0.86f, 0.88f, 0.95f, 1.0f);
        const ImVec4 colDim(0.60f, 0.64f, 0.78f, 0.95f);

        ImGui::SetWindowFontScale(1.7f);
        ImGui::TextColored(colTitle, "设 置");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::TextColored(colLabel, "音量");
        // 滑条右侧要留标签位置，否则标签会被挤出窗口
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 96.0f);
        if (ImGui::SliderFloat("主音量", &m_masterVolume, 0.0f, 1.0f, "%.2f")) {
            AudioEngine::SetMasterVolume(m_masterVolume);
        }
        if (ImGui::SliderFloat("背景音乐", &m_bgmVolume, 0.0f, 1.0f, "%.2f")) {
            ApplyBgmVolume();
        }
        if (ImGui::SliderFloat("音效", &m_sfxVolume, 0.0f, 1.0f, "%.2f")) {
            // 拖动过程不响，松手时播一声做确认（顺便听到新音量）
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                PlaySfx(SfxId::Pickup);
            }
        }
        ImGui::PopItemWidth();

        if (ImGui::Checkbox("静音（主音量不变，只关输出）", &m_audioMuted)) {
            AudioEngine::SetMuted(m_audioMuted);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(colDim, "主音量作用于全局输出；背景音乐 / 音效再各自乘一层。");
        ImGui::TextColored(colDim, "设置只在本次运行内有效，不会写入磁盘。");

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("关 闭   (ESC)", ImVec2(buttonWidth, 38.0f))) {   // ESC 由 PollPauseKeys 处理
            m_showOptions = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void SurvivorsLayer::RenderHud()
{
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    ImDrawList* background = ImGui::GetBackgroundDrawList();

    const ImVec4 colText(0.90f, 0.92f, 0.98f, 1.0f);
    const ImVec4 colDim(0.60f, 0.64f, 0.78f, 0.85f);
    const ImVec4 colHp(0.88f, 0.26f, 0.32f, 1.0f);

    // 顶部经验条
    const float expFraction = m_expNext > 0
        ? std::clamp(static_cast<float>(m_exp) / static_cast<float>(m_expNext), 0.0f, 1.0f)
        : 0.0f;
    background->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x, 8.0f), IM_COL32(10, 10, 22, 220));
    background->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x * expFraction, 8.0f),
                              IM_COL32(84, 196, 255, 255));

    // ---- 左上：生命 / 等级 ----
    ImGui::SetNextWindowPos(ImVec2(18.0f, 22.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.55f));
    if (ImGui::Begin("##survivors-left", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(colDim, "生命");
        char hpText[64];
        std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_playerHp, m_playerMaxHp);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, colHp);
        ImGui::ProgressBar(m_playerMaxHp > 0.0f ? m_playerHp / m_playerMaxHp : 0.0f,
                           ImVec2(244.0f, 18.0f), hpText);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, colText);
        ImGui::Text("等级 %d", m_level);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(colDim, "经验 %d / %d", m_exp, m_expNext);

        // 金币：数字前面补画一枚小金币（不用图标字体，直接画两个圆）
        const ImVec2 coinRow = ImGui::GetCursorScreenPos();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "    金币 %d", m_coins);
        {
            ImDrawList* windowDraw = ImGui::GetWindowDrawList();
            const ImVec2 center(coinRow.x + 6.0f, coinRow.y + ImGui::GetTextLineHeight() * 0.5f);
            windowDraw->AddCircleFilled(center, 6.5f, IM_COL32(150, 108, 30, 255), 20);
            windowDraw->AddCircleFilled(center, 5.0f, IM_COL32(255, 206, 92, 255), 20);
        }

        // 角色 + 道具件数：HUD 只给一个概览，明细在暂停面板里
        int itemTotal = 0;
        for (const int count : m_itemCount) {
            itemTotal += count;
        }
        const glm::vec4 characterColor = ItemColor(m_character);
        ImGui::TextColored(ImVec4(characterColor.x, characterColor.y, characterColor.z, 0.95f), "%s",
                           ItemName(m_character));
        ImGui::SameLine();
        ImGui::TextColored(colDim, "· 道具 %d 件", itemTotal);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // ---- 顶部中央：关卡计时 / 击杀 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, 18.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    if (ImGui::Begin("##survivors-timer", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        const float stageLeft = std::max(0.0f, m_stageDuration - m_stageTime);
        const bool bossStage = HasBossStage(m_stage);
        const bool finalStage = m_stage >= STAGE_COUNT;
        ImGui::SetWindowFontScale(1.9f);
        ImGui::TextColored(colText, "%s", FormatTime(stageLeft).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(bossStage ? ImVec4(1.0f, 0.48f, 0.50f, 0.95f) : colDim,
                           "第 %d / %d 关%s   ·   击杀 %d", m_stage, STAGE_COUNT,
                           bossStage ? (finalStage ? " · 最终关 · 击杀使徒即通关" : " · 血月使徒") : "",
                           m_kills);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                              bossStage ? ImVec4(0.86f, 0.20f, 0.24f, 1.0f) : ImVec4(0.86f, 0.70f, 0.36f, 1.0f));
        ImGui::ProgressBar(m_stageDuration > 0.0f ? m_stageTime / m_stageDuration : 0.0f,
                           ImVec2(250.0f, 6.0f), "");
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ---- 血月使徒血条（直接扫描存活的 Boss，避免多只同名 Boss 时血条丢失） ----
    {
        const Enemy* boss = nullptr;
        for (const Enemy& enemy : m_enemies) {
            if (enemy.alive && enemy.type == EnemyType::Boss) {
                boss = &enemy;
                break;
            }
        }
        if (boss != nullptr) {
            ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, 96.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.02f, 0.04f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.86f, 0.16f, 0.22f, 1.0f));
            if (ImGui::Begin("##survivors-boss", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoInputs)) {
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "血月使徒");
                ImGui::ProgressBar(boss->maxHp > 0.0f ? boss->hp / boss->maxHp : 0.0f,
                                   ImVec2(430.0f, 14.0f), "");
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }
    }

    // ---- 伤害数字 / 金币提示 ----
    ImFont* font = ImGui::GetFont();
    for (const DamageNumber& number : m_damageNumbers) {
        const float t = std::min(number.age / number.lifetime, 1.0f);
        const ImVec2 screen = WorldToScreen(number.position);
        // 前 55% 保持不透明，之后线性淡出：1 秒的停留里先「看清」再「消失」，
        // 而不是从出生就开始衰减（那样刚跳出来就已经灰了）
        const float fadeT = std::max(0.0f, (t - 0.55f) / 0.45f);
        const int alpha = static_cast<int>(255.0f * (1.0f - fadeT));
        const ImU32 color = number.gold ? IM_COL32(255, 206, 92, alpha)
            : (number.crit ? IM_COL32(255, 224, 130, alpha) : IM_COL32(228, 236, 255, alpha));
        // 字号：金币最小、暴击最大（都上抬了一档，暗场里更容易看清）
        const float fontSize = number.gold ? 19.0f : (number.crit ? 30.0f : 21.0f);
        char text[32];
        // 注意：格式串与实参类型必须一致——伤害是 float（%.0f），金币是 int（+%d），
        // 不能把两者塞进一个三元格式串，否则 %f 读不到 int 参数（可变参数类型不匹配，会打印 0）
        if (number.gold) {
            std::snprintf(text, sizeof(text), "+%d", static_cast<int>(number.value));
        } else {
            std::snprintf(text, sizeof(text), "%.0f", static_cast<double>(number.value));
        }
        const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        background->AddText(font, fontSize, ImVec2(screen.x - size.x * 0.5f, screen.y), color, text);
    }

    // ---- 进关横幅：普通关金色、Boss 关血色，2.2 秒内淡出 ----
    if (m_stageBanner > 0.0f) {
        const float fade = std::min(1.0f, m_stageBanner / 0.6f);
        const float pulse = m_stageBannerBoss ? (0.55f + 0.45f * std::sin(m_time * 8.0f)) : 1.0f;
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.30f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        if (ImGui::Begin("##survivors-stage-banner", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoInputs)) {
            ImGui::SetWindowFontScale(1.9f);
            ImGui::TextColored(m_stageBannerBoss ? ImVec4(1.0f, 0.32f, 0.34f, fade * pulse)
                                                 : ImVec4(0.98f, 0.84f, 0.52f, fade),
                               "第 %d 关", m_stage);
            ImGui::SetWindowFontScale(1.0f);
            // 最终关的胜利条件换成「击杀使徒」，横幅得说清楚，否则玩家会以为还是要硬撑计时
            const bool finalStage = m_stage >= STAGE_COUNT;
            ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.90f, fade * 0.9f),
                               m_stageBannerBoss
                                   ? (finalStage ? "血月使徒降临 · 击杀使徒即通关"
                                                 : "血月使徒降临 · 撑过 %d 秒")
                                   : "存活 %d 秒",
                               static_cast<int>(m_stageDuration));
        }
        ImGui::End();
    }

    if (m_state == State::Playing) {
        // 右上角暂停按钮：ESC 之外的鼠标路径（面板里都要能点，不只靠键盘）。
        // **不能让它抢焦点**：ImGui 的窗口一旦成为 NavWindow，`io.WantCaptureKeyboard` 就常真，
        // ImGuiLayer 会把所有键盘事件标成 handled，游戏层连 F4 / R 都收不到
        // （这个按钮每帧都在，等于把整个游戏期间的键盘事件全吞掉）。
        ImGui::SetNextWindowPos(ImVec2(display.x - 18.0f, 18.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::Begin("##survivors-pause-button", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus)) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.20f, 0.32f, 0.92f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.30f, 0.46f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.40f, 0.58f, 1.0f));
            if (ImGui::Button("暂停  (ESC)", ImVec2(124.0f, 34.0f))) {
                OpenPauseMenu();
            }
            ImGui::PopStyleColor(3);
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (m_showDebug) {
        ImGui::SetNextWindowPos(ImVec2(18.0f, 150.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        if (ImGui::Begin("调试##survivors", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("FPS %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
            const char* stateName = "游戏中";
            switch (m_state) {
                case State::Title: stateName = "标题"; break;
                case State::Setup: stateName = "选角"; break;
                case State::LevelUp: stateName = "升级"; break;
                case State::WeaponPick: stateName = "选武器"; break;
                case State::Shop: stateName = "商店"; break;
                case State::Paused: stateName = "暂停"; break;
                case State::GameOver: stateName = "阵亡"; break;
                case State::Victory: stateName = "胜利"; break;
                default: break;
            }
            ImGui::Text("状态 %s", stateName);
            // ImGui 抢占键盘时游戏层收不到键盘事件（F4 / R 会失效），这里给个一眼可见的指示
            ImGui::Text("ImGui 键盘捕获 %d · 导航 %d", static_cast<int>(io.WantCaptureKeyboard),
                        static_cast<int>(io.NavActive));
            ImGui::Text("关卡 %d / %d（%.1f / %.1f s）", m_stage, STAGE_COUNT, m_stageTime, m_stageDuration);
            ImGui::Text("总时间 %.1f s   击杀 %d", m_time, m_kills);
            ImGui::Text("敌人 %d   弹丸 %d   金币 %d   粒子 %d",
                        static_cast<int>(m_enemies.size()), static_cast<int>(m_projectiles.size()),
                        static_cast<int>(m_coinDrops.size()), static_cast<int>(m_particles.size()));
            ImGui::Text("钱袋 %d（累计 %d）   本次刷新 %d 次   下次刷新 %d 币",
                        m_coins, m_coinsEarned, m_rerollCount, RerollCost());
            ImGui::Text("待处理：升级 %d · 选武器 %d · 商店 %s", m_pendingLevelUps, m_pendingWeaponPicks,
                        m_shopPending ? "待开" : "无");
            ImGui::Text("角色 %s   道具 %zu 种", ItemName(m_character), m_itemCount.size());
            ImGui::Text("事件：击杀 %d · 拾币 %d 枚（%d 次）· 受伤 %d · 过关 %d",
                        m_eventStats.kills, m_eventStats.coinValue, m_eventStats.coinPickups,
                        m_eventStats.hurts, m_eventStats.stageClears);
            ImGui::Text("伤害总计 %.0f", m_damageDealt);
            ImGui::Text("玩家 (%.0f, %.0f)  HP %.0f", m_playerPosition.x, m_playerPosition.y, m_playerHp);
            ImGui::Separator();
            if (!AudioEngine::IsInitialized()) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "音频设备未初始化");
            } else {
                if (ImGui::Checkbox("静音", &m_audioMuted)) {
                    AudioEngine::SetMuted(m_audioMuted);
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(130.0f);
                if (ImGui::SliderFloat("主音量", &m_masterVolume, 0.0f, 1.0f, "%.2f")) {
                    AudioEngine::SetMasterVolume(m_masterVolume);
                }
            }
            ImGui::Separator();
            // 快速验证用：只改待办标记 / 直接加经验，真正的面板切换仍走 ContinueRun 的排队
            if (ImGui::Button("升一级")) {
                GainExp(m_expNext - m_exp);
            }
            ImGui::SameLine();
            if (ImGui::Button("+50 金币")) {
                m_coins += 50;
            }
            ImGui::SameLine();
            if (ImGui::Button("立刻过关")) {
                m_stageTime = m_stageDuration;
            }
            // 事件道具的快速验证：直接把事件类道具塞进背包（无视商店与持有上限）
            if (ImGui::Button("送事件道具")) {
                const ItemId eventItems[] = {
                    ItemId::WailingBell, ItemId::CoinHammer, ItemId::ScavengerCharm,
                    ItemId::GreedChain, ItemId::MartyrStigma, ItemId::DawnPact,
                    ItemId::DawnBlade, ItemId::BloodDagger, ItemId::Hourglass, ItemId::Metronome,
                    ItemId::GreedPact, ItemId::MerchantEye, ItemId::TomeOfGrowth, ItemId::StarlightPage,
                };
                for (const ItemId id : eventItems) {
                    AcquireItem(id);
                }
            }
            if (ImGui::Button("重新开始")) {
                ResetRun();
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭调试 (F4)")) {
                m_showDebug = false;
            }
        }
        ImGui::End();
    }
}

void SurvivorsLayer::RenderLevelUp()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 205));

    const int rerollCost = RerollCost();
    const bool canReroll = m_coins >= rerollCost;

    const float cardWidth = 300.0f;
    const float cardHeight = 300.0f;
    const float startY = display.y * 0.5f - cardHeight * 0.5f + 18.0f;

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY - 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-levelup-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "等 级 提 升");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.66f, 0.70f, 0.86f, 0.95f), "点击卡片选择一项祝福（也可按 1 / 2 / 3）");
    }
    ImGui::End();

    const float gap = 22.0f;
    const int count = static_cast<int>(m_options.size());
    const float totalWidth = cardWidth * static_cast<float>(count) + gap * static_cast<float>(count - 1);
    const float startX = (display.x - totalWidth) * 0.5f;
    const float contentHeight = cardHeight - 32.0f;   // 窗口内边距上下各 16

    int picked = -1;
    bool rerollClicked = false;
    for (int i = 0; i < count; ++i) {
        const UpgradeOption& option = m_options[i];
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(i) * (cardWidth + gap), startY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.11f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(option.color.x, option.color.y, option.color.z, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(option.color.x * 0.42f, option.color.y * 0.42f,
                                                      option.color.z * 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(option.color.x * 0.62f, option.color.y * 0.62f,
                                                             option.color.z * 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(option.color.x * 0.80f, option.color.y * 0.80f,
                                                            option.color.z * 0.80f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##card%d", i);
        ImGui::Begin(identifier, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        // 品级不写字：卡框 / 图标底衬 / 按钮都用品质色，靠颜色区分
        DrawCardIcon(PassiveIcon(option.index), cardWidth, 76.0f, option.color);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SetWindowFontScale(1.45f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", option.title.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.80f, 0.92f, 1.0f));
        ImGui::TextWrapped("%s", option.description.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(contentHeight - 40.0f);
        char buttonLabel[32];
        std::snprintf(buttonLabel, sizeof(buttonLabel), "选择  (%d)", i + 1);
        if (ImGui::Button(buttonLabel, ImVec2(cardWidth - 36.0f, 40.0f))) {
            picked = i;
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    // ---- 整组刷新：三张一起换，价格 4 + 3 × 本次已刷新次数 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY + cardHeight + 18.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.26f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.40f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.68f, 0.53f, 0.16f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##survivors-levelup-reroll", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::BeginDisabled(!canReroll);
        char rerollLabel[64];
        std::snprintf(rerollLabel, sizeof(rerollLabel),
                      canReroll ? "刷新  (R · %d 币)" : "刷新  (需 %d 币)", rerollCost);
        if (ImGui::Button(rerollLabel, ImVec2(344.0f, 40.0f))) {
            rerollClicked = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (rerollClicked) {
        RerollOptions();   // 三张全部换新；下一帧就是新手牌，不消耗升级次数
        return;
    }
    if (picked >= 0) {
        ApplyUpgrade(picked);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_1) && count > 0) {
        ApplyUpgrade(0);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2) && count > 1) {
        ApplyUpgrade(1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_3) && count > 2) {
        ApplyUpgrade(2);
    }
}

// ============================ 每 5 级：从全部武器里选 ============================

void SurvivorsLayer::RenderWeaponPick()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 205));

    const float cardWidth = 292.0f;
    // 卡里固定四行说明（玩法 / 主数值 / 攻击间隔，长的会折行）+ 底部按钮，326 会压到按钮，给到 352
    const float cardHeight = 352.0f;
    const float gap = 20.0f;
    const float startY = display.y * 0.5f - cardHeight * 0.5f + 16.0f;
    const float totalWidth = cardWidth * static_cast<float>(WEAPON_COUNT) +
                             gap * static_cast<float>(WEAPON_COUNT - 1);
    const float startX = (display.x - totalWidth) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY - 78.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-weaponpick-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(0.72f, 0.88f, 1.0f, 1.0f), "第 %d 级 · 选 择 武 器", m_level);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.66f, 0.70f, 0.86f, 0.95f),
                           "新武器解锁 Lv.1，已有武器 +1 级（点卡片，或按 1 / 2 / 3 / 4）");
        if (m_pendingWeaponPicks > 1) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "还有 %d 次武器选择待处理", m_pendingWeaponPicks);
        }
    }
    ImGui::End();

    int picked = -1;
    for (int weapon = 0; weapon < WEAPON_COUNT; ++weapon) {
        const int level = m_weaponLevel[static_cast<size_t>(weapon)];
        const bool maxed = level >= WEAPON_MAX_LEVEL;
        const glm::vec4 color = WEAPON_COLORS[weapon];

        std::string tag;
        if (maxed) {
            tag = "已满级";
        } else if (level <= 0) {
            tag = "新武器 · Lv.1";
        } else {
            tag = std::format("Lv.{} → Lv.{}", level, level + 1);
        }

        std::string title = WEAPON_NAMES[weapon];
        if (level > 0) {
            title += std::string(" ") + ROMAN[std::min(level, 5)];
        }

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##survivors-weaponpick%d", weapon);
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(weapon) * (cardWidth + gap), startY),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.11f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, maxed
            ? ImVec4(0.34f, 0.36f, 0.44f, 0.85f)
            : ImVec4(color.x, color.y, color.z, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.42f, color.y * 0.42f, color.z * 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.62f, color.y * 0.62f, color.z * 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.80f, color.y * 0.80f, color.z * 0.80f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
        ImGui::Begin(identifier, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        DrawCardIcon(WeaponIcon(weapon), cardWidth, 76.0f, maxed ? glm::vec4(0.4f, 0.4f, 0.5f, 1.0f) : color);
        ImGui::TextColored(maxed ? ImVec4(0.56f, 0.58f, 0.66f, 1.0f)
                                 : ImVec4(color.x, color.y, color.z, 1.0f), "%s", tag.c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SetWindowFontScale(1.45f);
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 1.0f, 1.0f), "%s", title.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.80f, 0.92f, 1.0f));
        ImGui::TextWrapped("%s", DescribeWeapon(weapon, level).c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(cardHeight - 32.0f - 40.0f);
        ImGui::BeginDisabled(maxed);
        char buttonLabel[32];
        std::snprintf(buttonLabel, sizeof(buttonLabel), maxed ? "已 满 级" : "选择  (%d)", weapon + 1);
        if (ImGui::Button(buttonLabel, ImVec2(cardWidth - 36.0f, 40.0f))) {
            picked = weapon;
        }
        ImGui::EndDisabled();

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    if (picked >= 0) {
        ChooseWeapon(picked);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
        ChooseWeapon(0);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
        ChooseWeapon(1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
        ChooseWeapon(2);
    } else if (ImGui::IsKeyPressed(ImGuiKey_4, false)) {
        ChooseWeapon(3);
    }
}

// ============================ 每关通关后的商店 ============================

void SurvivorsLayer::RenderShop()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 5, 10, 212));

    const float cardWidth = 272.0f;
    const float cardHeight = 320.0f;
    const float gap = 18.0f;
    const int count = static_cast<int>(m_shopOffers.size());
    const float startY = display.y * 0.5f - cardHeight * 0.5f + 8.0f;
    const float totalWidth = cardWidth * static_cast<float>(std::max(1, count)) +
                             gap * static_cast<float>(std::max(0, count - 1));
    const float startX = (display.x - totalWidth) * 0.5f;
    const int rerollCost = ShopRerollCost();

    // ---- 标题 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY - 104.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-shop-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "商 店");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.86f, 0.88f, 0.95f, 1.0f), "第 %d 关已通过", m_stage - 1);
    }
    ImGui::End();

    // ---- 货位 ----
    int buySlot = -1;
    for (int slot = 0; slot < count; ++slot) {
        const ShopOffer& offer = m_shopOffers[static_cast<size_t>(slot)];
        const Rarity rarity = ItemRarity(offer.id);   // 稀有度是道具的固定属性
        const glm::vec4 rarityColor = RARITY_COLORS[static_cast<int>(rarity)];
        const bool affordable = m_coins >= offer.price;
        const bool acquirable = CanAcquireItem(offer.id);
        const bool buyable = !offer.sold && affordable && acquirable;

        std::string name;
        std::string description;
        DescribeItem(offer.id, name, description);

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##survivors-shop%d", slot);
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(slot) * (cardWidth + gap), startY),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, offer.sold ? ImVec4(0.04f, 0.04f, 0.07f, 0.94f)
                                                            : ImVec4(0.055f, 0.055f, 0.11f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(rarityColor.x, rarityColor.y, rarityColor.z,
                                                      offer.sold ? 0.28f : 0.80f));
        // 购买按钮跟卡片同色（按稀有度取色，只做明度变体）：稀有档的卡片是紫色，按钮也就是紫色
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(rarityColor.x * 0.36f, rarityColor.y * 0.36f, rarityColor.z * 0.36f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(rarityColor.x * 0.56f, rarityColor.y * 0.56f, rarityColor.z * 0.56f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(rarityColor.x * 0.76f, rarityColor.y * 0.76f, rarityColor.z * 0.76f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::Begin(identifier, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        // 种类 / 品质不写字（靠图标底衬与卡框的颜色区分）；持有上限挪到卡片右上角
        DrawCardIcon(ItemIcon(offer.id), cardWidth, 76.0f, rarityColor);
        const int held = ItemCount(offer.id);
        if (ItemCap(offer.id) > 0) {
            char capLabel[32];
            std::snprintf(capLabel, sizeof(capLabel), "上限(%d/%d)", held, ItemCap(offer.id));
            const ImVec2 textSize = ImGui::CalcTextSize(capLabel);
            const ImVec2 cardPos = ImGui::GetWindowPos();
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(cardPos.x + cardWidth - 16.0f - textSize.x, cardPos.y + 14.0f),
                IM_COL32(128, 136, 162, 235), capLabel);
        }

        ImGui::SetWindowFontScale(1.4f);
        ImGui::TextColored(offer.sold ? ImVec4(0.52f, 0.54f, 0.62f, 1.0f) : ImVec4(0.95f, 0.95f, 1.0f, 1.0f),
                           "%s", name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.80f, 0.92f, 1.0f));
        ImGui::TextWrapped("%s", description.c_str());
        ImGui::PopStyleColor();

        // 已持有件数 / 到上限的提示
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (held > 0) {
            ImGui::TextColored(ImVec4(0.56f, 0.62f, 0.76f, 1.0f), "已持有 %d 件", held);
        } else if (!acquirable && !offer.sold) {
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "已达持有上限");
        } else {
            ImGui::TextColored(ImVec4(0.56f, 0.62f, 0.76f, 1.0f), " ");
        }

        ImGui::SetCursorPosY(cardHeight - 28.0f - 56.0f);
        char priceLabel[48];
        if (offer.sold) {
            std::snprintf(priceLabel, sizeof(priceLabel), "已 售 出");
        } else {
            std::snprintf(priceLabel, sizeof(priceLabel), "购买   %d 币", offer.price);
        }
        ImGui::BeginDisabled(!buyable);
        if (ImGui::Button(priceLabel, ImVec2(cardWidth - 32.0f, 34.0f))) {
            buySlot = slot;
        }
        ImGui::EndDisabled();
        if (!offer.sold && !affordable) {
            ImGui::TextColored(ImVec4(0.85f, 0.42f, 0.38f, 0.95f), "金币不足");
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    if (count <= 0) {
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY + 40.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.6f);
        if (ImGui::Begin("##survivors-shop-empty", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.80f, 1.0f), "今天没有可卖的货了（道具都已到持有上限）");
        }
        ImGui::End();
    }

    // ---- 底部：整组刷新 / 离开 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, startY + cardHeight + 16.0f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("##survivors-shop-bar", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.26f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.40f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.68f, 0.53f, 0.16f, 1.0f));
        ImGui::BeginDisabled(m_coins < rerollCost);
        char rerollLabel[64];
        std::snprintf(rerollLabel, sizeof(rerollLabel),
                      m_coins >= rerollCost ? "刷新  (R · %d 币)" : "刷新  (需 %d 币)", rerollCost);
        const bool rerollClicked = ImGui::Button(rerollLabel, ImVec2(300.0f, 42.0f));
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.0f, 12.0f);
        // 离开商店只留这一个按钮：Enter / 空格不再是快捷键，ESC 在商店里改成打开暂停面板
        const bool leaveClicked = ImGui::Button("离开商店", ImVec2(220.0f, 42.0f));

        if (rerollClicked) {
            RerollShop();   // 整组换新；货位数量可能变化，下一帧再画
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }
        if (leaveClicked) {
            ImGui::End();
            ImGui::PopStyleVar();
            ContinueRun();
            return;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (buySlot >= 0) {
        BuyShopOffer(buySlot);
    }
}

void SurvivorsLayer::RenderPause()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 3, 10, 170));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.12f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-pause", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 1.0f, 1.0f), "已 暂 停");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.80f, 1.0f), "第 %d / %d 关   ·   存活 %s   ·   击杀 %d   ·   等级 %d",
                           m_stage, STAGE_COUNT, FormatTime(m_time).c_str(), m_kills, m_level);

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.46f, 0.38f, 0.72f, 0.45f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // ---- 当前属性（基础属性：开局裸值 + 全部成长）----
        ImGui::TextColored(ImVec4(0.58f, 0.62f, 0.76f, 1.0f), "当前属性");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        char hpText[48];
        char damageText[32];
        char hasteText[32];
        char critText[32];
        char speedText[32];
        char pickupText[32];
        char regenText[32];
        char critDamageText[32];
        char defenseText[40];
        char penText[40];
        char coinText[40];
        std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_playerHp, m_playerMaxHp);
        std::snprintf(damageText, sizeof(damageText), "+%.0f%%", (DamageMul() - 1.0f) * 100.0f);
        std::snprintf(hasteText, sizeof(hasteText), "+%.0f%%", (AttackSpeedMul() - 1.0f) * 100.0f);
        std::snprintf(critText, sizeof(critText), "%.0f%%", CritChance() * 100.0f);
        std::snprintf(speedText, sizeof(speedText), "%.0f", MoveSpeed());
        std::snprintf(pickupText, sizeof(pickupText), "%.0f", PickupRadius());
        std::snprintf(regenText, sizeof(regenText), "+%.1f / 秒", RegenPerSecond());
        // 暴击伤害倍率：基础 2.0×，猎月瞳等道具会继续抬
        std::snprintf(critDamageText, sizeof(critDamageText), "%.1f 倍", CritMultiplier());
        // 防御只有固定值，括号里换算成减伤率；穿甲也只有固定值（点数）。
        // 这里显示的是**结算用**的防御（守夜人低血时翻倍，见 DefenseStat）
        std::snprintf(defenseText, sizeof(defenseText), "%.0f 点 · 减伤 %.0f%%",
                      DefenseStat(), DefenseReduction(DefenseStat()) * 100.0f);
        std::snprintf(penText, sizeof(penText), "%.0f 点", m_baseStats.penFlat);
        std::snprintf(coinText, sizeof(coinText), "%d 枚", m_coins);

        const ImVec4 statLabel(0.58f, 0.62f, 0.76f, 1.0f);
        const ImVec4 statValue(0.92f, 0.94f, 1.00f, 1.0f);
        if (ImGui::BeginTable("##survivors-stats", 4, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("##sl1", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("##sv1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("##sl2", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("##sv2", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "生命");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", hpText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "伤害");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", damageText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "攻速");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", hasteText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "暴击");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", critText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "移速");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", speedText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "拾取");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", pickupText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "回复");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", regenText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "暴伤");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", critDamageText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "防御");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", defenseText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "穿甲");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", penText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(0.86f, 0.74f, 0.42f, 1.0f), "金币");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, 1.0f), "%s", coinText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "累计");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%d 枚", m_coinsEarned);
            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextColored(statLabel, "武器");
        for (int weapon = 0; weapon < WEAPON_COUNT; ++weapon) {
            const int level = m_weaponLevel[static_cast<size_t>(weapon)];
            if (level <= 0) {
                continue;
            }
            ImGui::SameLine(0.0f, 10.0f);
            if (const Ref<Texture2D>& icon = WeaponIcon(weapon)) {
                ImGui::Image(static_cast<ImTextureID>(icon->GetRendererID()), ImVec2(18.0f, 18.0f),
                             ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                ImGui::SameLine(0.0f, 4.0f);
            }
            ImGui::TextColored(ImVec4(WEAPON_COLORS[weapon].x, WEAPON_COLORS[weapon].y,
                                      WEAPON_COLORS[weapon].z, 1.0f),
                               "%s %s", WEAPON_NAMES[weapon], ROMAN[level]);
        }
        // 「祝福」（属性卡）清单不再显示：数值已经全部体现在上面的「当前属性」表里

        // 角色 + 道具背包（角色是固定史诗的角色类道具，单独一行；其余按种类分色、后面跟件数）
        {
            const glm::vec4 characterColor = ItemColor(m_character);
            ImGui::TextColored(statLabel, "角色");
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(characterColor.x, characterColor.y, characterColor.z, 1.0f), "%s",
                               ItemName(m_character));

            ImGui::TextColored(statLabel, "道具");
            int shown = 0;
            for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
                const ItemId id = static_cast<ItemId>(i);
                if (m_itemCount[i] <= 0 || ItemCategoryOf(id) == ItemCategory::Character) {
                    continue;
                }
                if (shown % 3 != 0) {
                    ImGui::SameLine(0.0f, 12.0f);
                }
                if (const Ref<Texture2D>& icon = ItemIcon(id)) {
                    ImGui::Image(static_cast<ImTextureID>(icon->GetRendererID()), ImVec2(18.0f, 18.0f),
                                 ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                    ImGui::SameLine(0.0f, 4.0f);
                }
                // 颜色取道具自己的固定稀有度
                const glm::vec4 color = RARITY_COLORS[static_cast<int>(ItemRarity(id))];
                ImGui::TextColored(ImVec4(color.x, color.y, color.z, 0.95f), "%s ×%d", ItemName(id), m_itemCount[i]);
                ++shown;
            }
            if (shown <= 0) {
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::TextColored(ImVec4(0.50f, 0.54f, 0.64f, 0.9f), "暂无（每关通关后到商店买）");
            }
        }

        // 事件：八类事件的累计次数 + 已持事件道具的触发进度（阈值型道具才显示 N / 阈值）
        {
            ImGui::TextColored(statLabel, "事件");
            for (const std::string& line : EventSummary()) {
                ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.90f, 0.95f), "   %s", line.c_str());
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        // 按钮宽度取内容区可用宽度，保证左右留白一致（写死宽度会因窗口更宽而右偏）
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("继续游戏  (ESC)", ImVec2(buttonWidth, 38.0f))) {
            m_state = m_pauseReturnState;   // 从商店 / 升级卡暂停的，继续时回到那一屏
        }
        if (ImGui::Button("回到标题", ImVec2(buttonWidth, 38.0f))) {
            ReturnToTitle();   // 清局并回标题（不再「就地重开」：要换角色 / 武器就回标题重新开始）
        }
        if (ImGui::Button("游戏说明 / 操作", ImVec2(buttonWidth, 38.0f))) {
            m_showOptions = false;
            m_showHelp = true;
        }
        if (ImGui::Button("设置 / 音量", ImVec2(buttonWidth, 38.0f))) {
            m_showHelp = false;
            m_showOptions = true;
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 38.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);
        // ESC 的「继续」不在这里做：PollPauseKeys() 每帧轮询物理按键（ImGui 抢焦点时也生效），
        // 边沿检测保证按一次只切一次，所以不需要在这里再补一条、也不需要宽限时间。
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void SurvivorsLayer::RenderGameOver()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(8, 2, 4, 200));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.03f, 0.05f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-gameover", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.28f, 0.32f, 1.0f), "你 被 黑 暗 吞 噬");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "到达关卡   %d / %d", m_stage, STAGE_COUNT);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "存活时间   %s", FormatTime(m_time).c_str());
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "击杀数量   %d", m_kills);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "达到等级   %d", m_level);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "获得金币   %d", m_coinsEarned);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "造成伤害   %.0f", m_damageDealt);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("再来一局", ImVec2(buttonWidth, 42.0f))) {
            ResetRun();
        }
        if (ImGui::Button("回到标题", ImVec2(buttonWidth, 38.0f))) {
            ReturnToTitle();
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 38.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void SurvivorsLayer::RenderVictory()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 8, 18, 185));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.07f, 0.13f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-victory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "黎 明 降 临");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.95f, 1.0f), "十关尽破，你走出了漫漫长夜。");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "通关关卡   %d / %d", m_stage, STAGE_COUNT);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "总耗时     %s", FormatTime(m_time).c_str());
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "击杀数量   %d", m_kills);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "达到等级   %d", m_level);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "获得金币   %d", m_coinsEarned);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "造成伤害   %.0f", m_damageDealt);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("再来一局", ImVec2(buttonWidth, 42.0f))) {
            ResetRun();
        }
        if (ImGui::Button("回到标题", ImVec2(buttonWidth, 38.0f))) {
            ReturnToTitle();
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 38.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

