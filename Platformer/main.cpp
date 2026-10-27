#include "OiEngine.h"

#include "Platformer.h"

int main(int argc, char* argv[])
{
    OiEngine::Application::Get().Init({
        "OiEngine Platformer - 平台跳跃",
        "",
        { argc, argv }
    });

    // 独立项目只有一个游戏层；ImGuiLayer 由 Application 内置为 overlay
    OiEngine::Application::Get().PushLayer(new OiEngine::Platformer::PlatformerLayer());

    OiEngine::Application::Get().Run();
    OiEngine::Application::Get().Shutdown();
    return 0;
}
