#include "OiEngine.h"

#include <imgui.h>

#include "Engine/Events/KeyEvent.h"

#include "PlaneGame.h"
#include "SolarSystem.h"

using namespace OiEngine;

namespace {
    // 场景切换器（overlay）：F1 飞机大战 / F2 太阳系
    class SceneSwitcherLayer : public Layer {
    public:
        SceneSwitcherLayer(SandBox::PlaneGameLayer* planeGame, SolarSystemLayer* solarSystem)
            : Layer("SceneSwitcher"), m_planeGame(planeGame), m_solarSystem(solarSystem)
        {
        }

        void OnEvent(Event& event) override
        {
            if (event.GetEventType() != EventType::KeyPressed) {
                return;
            }

            const auto key = static_cast<KeyPressedEvent&>(event).GetKeyCode();
            if (key == Key::F1 && !m_planeGame->IsActive()) {
                m_planeGame->SetActive(true);
                m_solarSystem->SetActive(false);
                event.handled = true;
            } else if (key == Key::F2 && !m_solarSystem->IsActive()) {
                m_planeGame->SetActive(false);
                m_solarSystem->SetActive(true);
                event.handled = true;
            }
        }

        void OnImGuiRender() override
        {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220.0f, 14.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.78f, 0.88f, 0.9f));
            if (ImGui::Begin("SceneSwitcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                    ImGuiWindowFlags_NoInputs)) {
                ImGui::Text("%s  |  F1 飞机大战  F2 太阳系", m_planeGame->IsActive() ? "飞机大战" : "太阳系");
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }

    private:
        SandBox::PlaneGameLayer* m_planeGame = nullptr;
        SolarSystemLayer* m_solarSystem = nullptr;
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

    Application::Get().PushLayer(planeGame);
    Application::Get().PushLayer(solarSystem);
    Application::Get().PushOverlay(new SceneSwitcherLayer(planeGame, solarSystem));

    planeGame->SetActive(true);
    solarSystem->SetActive(false);

    Application::Get().Run();
    return 0;
}
