#include "OiEngine.h"

#include "SurvivorsGame.h"

int main(int argc, char* argv[])
{
    OiEngine::Application::Get().Init({
        "Nocturne Survivors - 永夜幸存者",
        "",
        { argc, argv }
    });

    // 只有一个场景：游戏层直接压在根上，ImGui 调试层由 Application 内置为 overlay
    OiEngine::Application::Get().PushLayer(new OiEngine::Survivors::SurvivorsLayer());

    OiEngine::Application::Get().Run();
    OiEngine::Application::Get().Shutdown();
    return 0;
}
