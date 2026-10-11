#include "OiEngine.h"

#include "SolarSystem.h"

using namespace OiEngine;

int main(int argc, char *argv[])
{
    Application::Get().Init({
        "Application",
        "",
        {argc, argv}
    });
    Application::Get().PushLayer(new SolarSystemLayer());
    Application::Get().Run();
    return 0;
}
