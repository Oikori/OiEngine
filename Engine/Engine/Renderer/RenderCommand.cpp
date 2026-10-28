#include "RenderCommand.h"

namespace OiEngine {
	Scope<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}
