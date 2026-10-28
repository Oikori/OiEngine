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
    } else {
        RenderHud();

        switch (m_state) {
            case State::LevelUp: RenderLevelUp(); break;
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
        if (ImGui::Button("开始游戏   (Enter / 空格)", ImVec2(buttonWidth, 46.0f))) {
            ResetRun();
        }
        ImGui::PopStyleColor(3);

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

        // ImGui 可能把键盘事件标记为已处理，这里用 ImGui 的输入状态补一条开始路径
        if (!m_showHelp && !m_showOptions &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space))) {
            ResetRun();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void SurvivorsLayer::RenderHelp()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 190));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(640.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.38f, 0.72f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26.0f, 22.0f));
    if (ImGui::Begin("##survivors-help", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
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
        body("只有第 5 关与第 10 关会出现「血月使徒」。过关会清空场上敌人与弹丸，地上的金币自动收进钱包。");

        section("操作");
        body("WASD / 方向键移动   ·   武器自动索敌开火，无需攻击键");
        body("升级时点击卡片选择（或按 1 / 2 / 3）；面板下方的「刷新全部三张」花金币整组重抽");
        body("ESC / P 暂停   ·   R 重开一局   ·   F4 调试面板");

        section("成长");
        body("击杀敌人当场获得经验，并掉落金币（走近自动吸取，22 秒后散掉）。金币只用于刷新升级卡。");
        body("每次升级给三张卡；整组刷新会把三张一起换掉（不会重复刚才那三张），刷新价 = 4 + 3 × 本次已刷新次数。");
        body("武器：月刃（直线穿透）/ 圣光环（近身脉冲）/ 回旋镰（往返切割）/ 雷暴（随机落雷），各自最高 5 级。");
        body("属性：力量 / 急速 / 疾行 / 体魄 / 磁石 / 幸运 / 回春 / 铁壁 / 破甲，另有「血之祭」回血卡。");

        section("属性与品质");
        body("属性卡没有层数上限，可以一直叠；每张卡有品质——普通 / 高级 / 稀有 / 史诗，加成倍率 1.0 / 1.6 / 2.4 / 3.5。");
        body("等级越高，高品质的出现概率越高（普通让位给稀有 / 史诗）。补给卡的回血比例同样按品质放大。");
        body("防御：固定值，减伤率 = 防御 / (防御 + 100)。100 点 = 50%，越往上堆收益越钝，但永远不会免疫。");
        body("穿甲：百分比穿甲先按比例削减对方防御，固定穿甲再扣点；防御被扣到 0 为止，溢出部分不额外生效。");
        body("上限：移动速度最多 2 倍裸值，暴击率最多 100%，百分比穿甲最多 100%。其余属性只会线性叠加。");

        section("敌人");
        body("蝙蝠（无防御）· 食尸鬼（防御 5）· 怨灵（12）· 石魔（25）· 血月使徒（40）。");
        body("怪物数值随关卡递增：血量 +30%/关、伤害 +12%/关、防御 +8%/关、经验与金币 +10%/关。");

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("关 闭   (ESC)", ImVec2(buttonWidth, 38.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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
        if (ImGui::Button("关 闭   (ESC)", ImVec2(buttonWidth, 38.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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
        ImGui::SetWindowFontScale(1.9f);
        ImGui::TextColored(colText, "%s", FormatTime(stageLeft).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(bossStage ? ImVec4(1.0f, 0.48f, 0.50f, 0.95f) : colDim,
                           "第 %d / %d 关%s   ·   击杀 %d", m_stage, STAGE_COUNT,
                           bossStage ? " · 血月使徒" : "", m_kills);
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
        const int alpha = static_cast<int>(255.0f * (1.0f - t) * (1.0f - t));
        const ImU32 color = number.gold ? IM_COL32(255, 206, 92, alpha)
            : (number.crit ? IM_COL32(255, 224, 130, alpha) : IM_COL32(228, 236, 255, alpha));
        const float fontSize = number.gold ? 15.0f : (number.crit ? 23.0f : 16.0f);
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
            ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.90f, fade * 0.9f),
                               m_stageBannerBoss ? "血月使徒降临 · 撑过 %d 秒" : "存活 %d 秒",
                               static_cast<int>(m_stageDuration));
        }
        ImGui::End();
    }

    if (m_showDebug) {
        ImGui::SetNextWindowPos(ImVec2(18.0f, 150.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        if (ImGui::Begin("调试##survivors", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("FPS %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
            ImGui::Text("状态 %s", m_state == State::Playing ? "游戏中" : m_state == State::LevelUp ? "升级" :
                                 m_state == State::Paused ? "暂停" : m_state == State::GameOver ? "阵亡" : "胜利");
            ImGui::Text("关卡 %d / %d（%.1f / %.1f s）", m_stage, STAGE_COUNT, m_stageTime, m_stageDuration);
            ImGui::Text("总时间 %.1f s   击杀 %d", m_time, m_kills);
            ImGui::Text("敌人 %d   弹丸 %d   金币 %d   粒子 %d",
                        static_cast<int>(m_enemies.size()), static_cast<int>(m_projectiles.size()),
                        static_cast<int>(m_coinDrops.size()), static_cast<int>(m_particles.size()));
            ImGui::Text("钱袋 %d（累计 %d）   本次刷新 %d 次   下次刷新 %d 币",
                        m_coins, m_coinsEarned, m_rerollCount, RerollCost());
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
    const float cardHeight = 246.0f;
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
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.45f, canReroll ? 1.0f : 0.55f),
                           "金币 %d   ·   整组刷新 %d 币（每刷一次更贵）", m_coins, rerollCost);
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

        // 标签：属性卡 / 补给卡是品质名，武器卡是 Lv.N
        ImGui::TextColored(ImVec4(option.color.x, option.color.y, option.color.z, 1.0f), "%s", option.tag.c_str());
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
                      canReroll ? "刷新全部三张  (%d 币)" : "刷新全部三张  (需 %d 币)", rerollCost);
        if (ImGui::Button(rerollLabel, ImVec2(344.0f, 40.0f))) {
            rerollClicked = true;
        }
        ImGui::EndDisabled();
        ImGui::TextColored(ImVec4(0.62f, 0.64f, 0.78f, 0.9f), "重抽三张 · 本次已刷 %d 次", m_rerollCount);
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
        char bladesText[32];
        char defenseText[40];
        char penText[40];
        char coinText[40];
        std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_playerHp, m_playerMaxHp);
        std::snprintf(damageText, sizeof(damageText), "+%.0f%%", (DamageMul() - 1.0f) * 100.0f);
        std::snprintf(hasteText, sizeof(hasteText), "+%.0f%%", (AttackSpeedMul() - 1.0f) * 100.0f);
        std::snprintf(critText, sizeof(critText), "%.0f%%", CritChance() * 100.0f);
        std::snprintf(speedText, sizeof(speedText), "%.0f", MoveSpeed());
        std::snprintf(pickupText, sizeof(pickupText), "%.0f", PickupRadius());
        std::snprintf(regenText, sizeof(regenText), "+%.1f / 秒", m_baseStats.regenPerSecond);
        std::snprintf(bladesText, sizeof(bladesText), "%d 道",
                      BLADE.count[m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)]]);
        // 防御只有固定值，括号里换算成减伤率；穿甲是两档，分开显示
        std::snprintf(defenseText, sizeof(defenseText), "%.0f 点 · 减伤 %.0f%%",
                      m_baseStats.defense, DefenseReduction(m_baseStats.defense) * 100.0f);
        std::snprintf(penText, sizeof(penText), "%.0f 点 / %.0f%%",
                      m_baseStats.penFlat, m_baseStats.penPercent * 100.0f);
        std::snprintf(coinText, sizeof(coinText), "%d 枚 · 刷新 %d 币", m_coins, RerollCost());

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
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "飞刃");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", bladesText);

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
        for (int weapon = 0; weapon < 4; ++weapon) {
            const int level = m_weaponLevel[weapon];
            if (level <= 0) {
                continue;
            }
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(WEAPON_COLORS[weapon].x, WEAPON_COLORS[weapon].y,
                                      WEAPON_COLORS[weapon].z, 1.0f),
                               "%s %s", WEAPON_NAMES[weapon], ROMAN[level]);
        }
        ImGui::TextColored(statLabel, "祝福");
        bool anyPassive = false;
        for (int passive = 0; passive < static_cast<int>(PassiveId::Count); ++passive) {
            const int level = m_passive[passive];
            if (level <= 0) {
                continue;
            }
            anyPassive = true;
            ImGui::SameLine(0.0f, 10.0f);
            // 属性卡没有层数上限，用「×N」代替罗马数字
            ImGui::TextColored(ImVec4(0.72f, 0.82f, 0.98f, 0.95f), "%s ×%d", PASSIVE_NAMES[passive], level);
        }
        if (!anyPassive) {
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(0.50f, 0.54f, 0.64f, 0.9f), "暂无");
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        // 按钮宽度取内容区可用宽度，保证左右留白一致（写死宽度会因窗口更宽而右偏）
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("继续游戏  (ESC / P)", ImVec2(buttonWidth, 38.0f))) {
            m_state = State::Playing;
        }
        if (ImGui::Button("重新开始  (R)", ImVec2(buttonWidth, 38.0f))) {
            ResetRun();
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

        // 暂停菜单出现时 ImGui 可能吃掉键盘事件，这里直接用 ImGui 的输入状态补一条 ESC/P 继续；
        // 开面板的同一帧仍可能在队列里，用几帧宽限期避开；帮助 / 设置面板打开时 ESC/P 只关面板
        if (m_pauseGuardFrames > 0) {
            --m_pauseGuardFrames;
        } else if (!m_showHelp && !m_showOptions &&
                   (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_P))) {
            m_state = State::Playing;
        }
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
        if (ImGui::Button("再来一局  (R)", ImVec2(buttonWidth, 42.0f))) {
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
        if (ImGui::Button("再来一局  (R)", ImVec2(buttonWidth, 42.0f))) {
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

