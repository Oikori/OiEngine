#include "OiEngine.h"

#include <imgui.h>

#include "Engine/Events/KeyEvent.h"

#include "PlaneGame.h"
#include "SolarSystem.h"
#include "ShadowDemo.h"
#include "IndoorDemo.h"
#include "VolumetricCloudDemo.h"
#include "Platformer/Platformer.h"

using namespace OiEngine;

namespace {
    // 场景切换器（overlay）：F1 飞机大战 / F2 太阳系 / F3 平台跳跃 / F5 阴影演示 / F6 室内多光源 / F7 体积云
    constexpr int SceneCount = 6;
    struct SceneEntry {
        const char* key;
        const char* name;
    };
    const SceneEntry SCENES[SceneCount] = {
        { "F1", "飞机大战" },
        { "F2", "太阳系" },
        { "F3", "平台跳跃" },
        { "F5", "阴影演示" },
        { "F6", "室内多光源" },
        { "F7", "体积云地平线" },
    };

    class SceneSwitcherLayer : public Layer {
    public:
        SceneSwitcherLayer(SandBox::PlaneGameLayer* planeGame, SolarSystemLayer* solarSystem,
                           SandBox::PlatformerLayer* platformer, ShadowDemoLayer* shadowDemo,
                           IndoorDemoLayer* indoorDemo, VolumetricCloudLayer* volumetricCloud)
            : Layer("SceneSwitcher"), m_planeGame(planeGame), m_solarSystem(solarSystem),
              m_platformer(platformer), m_shadowDemo(shadowDemo), m_indoorDemo(indoorDemo),
              m_volumetricCloud(volumetricCloud)
        {
        }

        void OnEvent(Event& event) override
        {
            if (event.GetEventType() != EventType::KeyPressed) {
                return;
            }

            const auto key = static_cast<KeyPressedEvent&>(event).GetKeyCode();
            if (key == Key::F1 && !m_planeGame->IsActive()) {
                SetScene(0);
                event.handled = true;
            } else if (key == Key::F2 && !m_solarSystem->IsActive()) {
                SetScene(1);
                event.handled = true;
            } else if (key == Key::F3 && !m_platformer->IsActive()) {
                SetScene(2);
                event.handled = true;
            } else if (key == Key::F5 && !m_shadowDemo->IsActive()) {
                SetScene(3);
                event.handled = true;
            } else if (key == Key::F6 && !m_indoorDemo->IsActive()) {
                SetScene(4);
                event.handled = true;
            } else if (key == Key::F7 && !m_volumetricCloud->IsActive()) {
                SetScene(5);
                event.handled = true;
            }
        }

        void OnImGuiRender() override
        {
            // 右上角竖向排列的场景列表：当前场景高亮并带 ">" 标记
            // （窗口宽度由内容决定，Begin 之后按实际尺寸贴右上角；半透明底板保证亮背景下也看得清）
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.45f));
            if (ImGui::Begin("SceneSwitcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                    ImGuiWindowFlags_NoInputs)) {
                for (int i = 0; i < SceneCount; ++i) {
                    const bool active = (i == m_current);
                    ImGui::PushStyleColor(ImGuiCol_Text, active
                        ? ImVec4(1.00f, 0.92f, 0.60f, 1.00f)
                        : ImVec4(0.80f, 0.86f, 0.94f, 1.00f));
                    ImGui::Text("%s %s  %s", active ? ">" : " ", SCENES[i].key, SCENES[i].name);
                    ImGui::PopStyleColor();
                }

                const ImVec2 display = ImGui::GetIO().DisplaySize;
                const ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::SetWindowPos(ImVec2(display.x - windowSize.x - 16.0f, 14.0f));
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }

    private:
        void SetScene(int scene)
        {
            m_current = scene;
            m_planeGame->SetActive(scene == 0);
            m_solarSystem->SetActive(scene == 1);
            m_platformer->SetActive(scene == 2);
            m_shadowDemo->SetActive(scene == 3);
            m_indoorDemo->SetActive(scene == 4);
            m_volumetricCloud->SetActive(scene == 5);
        }

        SandBox::PlaneGameLayer* m_planeGame = nullptr;
        SolarSystemLayer* m_solarSystem = nullptr;
        SandBox::PlatformerLayer* m_platformer = nullptr;
        ShadowDemoLayer* m_shadowDemo = nullptr;
        IndoorDemoLayer* m_indoorDemo = nullptr;
        VolumetricCloudLayer* m_volumetricCloud = nullptr;
        int m_current = 0;
    };
}

int main(int argc, char *argv[])
{
    Application::Get().Init({
        "OiEngine SandBox",
        "",
        {argc, argv}
    });

    auto* planeGame = new SandBox::PlaneGameLayer();
    auto* solarSystem = new SolarSystemLayer();
    auto* platformer = new SandBox::PlatformerLayer();
    auto* shadowDemo = new ShadowDemoLayer();
    auto* indoorDemo = new IndoorDemoLayer();
    auto* volumetricCloud = new VolumetricCloudLayer();

    Application::Get().PushLayer(planeGame);
    Application::Get().PushLayer(solarSystem);
    Application::Get().PushLayer(platformer);
    Application::Get().PushLayer(shadowDemo);
    Application::Get().PushLayer(indoorDemo);
    Application::Get().PushLayer(volumetricCloud);
    Application::Get().PushOverlay(new SceneSwitcherLayer(planeGame, solarSystem, platformer, shadowDemo,
                                                          indoorDemo, volumetricCloud));

    planeGame->SetActive(true);
    solarSystem->SetActive(false);
    platformer->SetActive(false);
    shadowDemo->SetActive(false);
    indoorDemo->SetActive(false);
    volumetricCloud->SetActive(false);

    Application::Get().Run();
    Application::Get().Shutdown();
    return 0;
}
