#include "OiEngine.h"

#include <imgui.h>

#include "Engine/Events/KeyEvent.h"

#include "PlaneGame.h"
#include "SolarSystem.h"
#include "Platformer/Platformer.h"

using namespace OiEngine;

namespace {
    // 场景切换器（overlay）：F1 飞机大战 / F2 太阳系 / F3 平台跳跃
    class SceneSwitcherLayer : public Layer {
    public:
        SceneSwitcherLayer(SandBox::PlaneGameLayer* planeGame, SolarSystemLayer* solarSystem,
                           SandBox::PlatformerLayer* platformer)
            : Layer("SceneSwitcher"), m_planeGame(planeGame), m_solarSystem(solarSystem), m_platformer(platformer)
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
            }
        }

        void OnImGuiRender() override
        {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 320.0f, 14.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.78f, 0.88f, 0.9f));
            if (ImGui::Begin("SceneSwitcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                    ImGuiWindowFlags_NoInputs)) {
                ImGui::Text("%s  |  F1 飞机大战  F2 太阳系  F3 平台跳跃", SceneName());
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }

    private:
        void SetScene(int scene)
        {
            m_current = scene;
            m_planeGame->SetActive(scene == 0);
            m_solarSystem->SetActive(scene == 1);
            m_platformer->SetActive(scene == 2);
        }

        const char* SceneName() const
        {
            switch (m_current) {
                case 0:  return "飞机大战";
                case 1:  return "太阳系";
                default: return "平台跳跃";
            }
        }

        SandBox::PlaneGameLayer* m_planeGame = nullptr;
        SolarSystemLayer* m_solarSystem = nullptr;
        SandBox::PlatformerLayer* m_platformer = nullptr;
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

    Application::Get().PushLayer(planeGame);
    Application::Get().PushLayer(solarSystem);
    Application::Get().PushLayer(platformer);
    Application::Get().PushOverlay(new SceneSwitcherLayer(planeGame, solarSystem, platformer));

    planeGame->SetActive(true);
    solarSystem->SetActive(false);
    platformer->SetActive(false);

    Application::Get().Run();
    return 0;
}
