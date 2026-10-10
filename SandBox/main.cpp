#include <iostream>

#include "OiEngine.h"

using namespace OiEngine;

int main(int argc, char *argv[])
{
    Application::Get().Init({
        "Application",
        "",
        {argc, argv}
    });

    Application::Get().Run();
    return 0;
}
