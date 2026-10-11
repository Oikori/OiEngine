# OiEngine — 项目说明（供 AI 协作者）

自研 C++23 游戏引擎，架构参考 Hazel。Windows + MinGW GCC + Ninja + CMake（≥3.20）。

- `Engine`：动态库（产物 `output/libEngine.dll`）
- `SandBox`：演示程序（产物 `output/SandBox.exe`，`main.cpp` 当前是太阳系场景）
- 渲染后端 OpenGL（glad + glfw），日志 spdlog，图像加载 stb_image

## 目录结构

- `Engine/` — 库工程（include 根目录，见下）
  - `Engine/Engine/Core/` — `Base.h`（OI_API / OI_ASSERT / Scope / Ref）、`Log.h`、Application、Layer、Window、Input、Timer
  - `Engine/Engine/Events/` — 事件系统（Event、ApplicationEvent、KeyEvent、MouseEvent）
  - `Engine/Engine/Math/` — 数学库（当前迭代重点，见下文）；总入口是 `Engine/Engine/Math.h`
  - `Engine/Engine/Renderer/` — Renderer、Camera、Shader、Buffer、VertexArray、Framebuffer 等抽象
  - `Engine/Platform/` — `OpenGL/`、`Windows/` 平台实现
- `SandBox/` — demo 可执行程序
- `vendor/` — glad、glfw、spdlog、stb_image（glfw/spdlog 登记为 git submodule）
- `test/` — 数学库独立验证用例（不参与 CMake，被 .gitignore 忽略），用法见 `test/README.md`
- `output/`、`build/` — 构建产物，不入库

头文件包含：include 根是 `Engine/` 目录，写作 `#include "Engine/Core/Base.h"`、`#include "Platform/OpenGL/OpenGLContext.h"`。

## 构建

```powershell
# 日常构建（构建目录已配置好）
cmake --build build/cmake-build-debug

# 首次 / 修改 CMake 后重新配置（Ninja + CLion 自带 MinGW，见 CMakePresets.json）
cmake --preset mingw-debug      # 或 mingw-release
```

- 产物在 `output/`：`libEngine.dll`、`SandBox.exe`
- 编译器路径记录在 `build/cmake-build-debug/CMakeCache.txt`（当前为 CLion 自带 MinGW GCC 15）
- shell 是 Windows PowerShell 5.1：不支持 `&&` 和三元 `?:`；含逗号的链接参数（如 `-Wl,--out-implib,...`）需加引号

## 测试（数学库）

`test/` 下三个用例直接对头文件编译（不依赖 CMake）：`vector_ops_test.cpp`、`matrix_verify.cpp`、
`dll_client_test.cpp`。全部通过时：向量用例 exit 0，矩阵用例输出 `ALL CHECKS PASSED`。
具体命令见 `test/README.md`。改数学库后至少跑：

```powershell
c++ -std=c++23 -Wall -Wextra -I Engine -o "$env:TEMP\m.exe" test/matrix_verify.cpp; & "$env:TEMP\m.exe"
```

## 代码约定

- 命名：模板类型 `T` 前缀（`TVector3`、`TMatrix4x4`、`TQuaternion`）；宏 `OI_`；类成员 `m_` 前缀；类型别名 `Vector3f`、`Matrix4f`、`Point2f` 等
- 风格：4 空格缩进，大括号不换行，`namespace OiEngine::Math {`；数学库尽量 `constexpr`、header-only（.cpp 仅 Random/UUID/平台代码）
- 注释：中文为主，glm 等来源的注释保留英文原文
- `OI_API` 只用于**非模板类**（有 .cpp、需要跨 DLL 边界）；**模板类型不要加**——标了之后静态成员（`zero`/`identity`）在 DLL 客户端会因未导出而链接失败
- 调试断言用 `OI_ASSERT(条件)`

## 数学库要点（Engine/Engine/Math/）

设计仿 glm：成员运算符 + 同文件尾部的自由函数。

- 列主序；`TMatrix3x3/4x4` 内部是 `columns[N]`，`operator[]` 返回列
- 语义：`matrix * vector` 为列向量、`vector * matrix` 为行向量；所有 `*` 都是**逐分量乘法**；点积用 `Dot()`
- 运算符两层：
  - 成员：复合赋值（`+= -= *= /= %= &= |= ^= <<= >>=`，标量版 `template<U>` + 向量版）、`++/--` 前置后置
  - 自由函数：一元 `+ - ~`、二元 `+ - * / % & | ^ << >>` 各三种形式（`(v,scalar)`、`(scalar,v)`、`(v1,v2)`）、`== !=`
- 标量参数类型可与元素类型不同（`template<typename T, typename U>`，内部 `static_cast<T>(scalar)`）
- 几何函数 `Dot/Cross/Distance/Normalize` 仅浮点（`static_assert(std::numeric_limits<T>::is_iec559)`）
- 四元数×向量在 `QuaternionGeometric.h`（vec4 旋转 xyz 并保留 w）；`MatrixGeometric.h` 目前为空占位
- `Math.h` 为总入口：Vector2/3/4 → Matrix3x3/4x4 → Random/UUID → VectorGeometric → MatrixGeometric → QuaternionGeometric

### 已知陷阱（都实际踩过）

1. 自由运算符涉及自有类型（如矩阵）时，参数**必须写成可直接推导的类型**（`const TVectorN<T>&`），不能写 `typename TMatrix<T>::ColType` 这类非推导上下文——否则与泛型标量重载无法偏序比较，`m * v` / `v * m` 会二义（对应位置已有注释）
2. `zero` / `identity` 是 `static const`（不是 constexpr），**不能用于 static_assert**；常量场景用 `Matrix4(1.0f)` 构造
3. 向量 `operator[]` 实现为 `(&x)[i]`（指针算术），矩阵×向量内部用到它 → `m * v` **不能用于常量表达式**（`v * m`、`m * m` 可以）；需要全 constexpr 矩阵运算时先改 `operator[]` 实现
4. 模板类型不要加 `OI_API`（原因见「代码约定」）

## Git / 其他

- `.gitignore`：`.idea`、`output`、`test/*`；`build/` 由 CMake 生成的 .gitignore 覆盖
- `vendor/glfw`、`vendor/spdlog` 是 submodule（工作区可能显示为未跟踪，不要整体 `git add`）；`glad`、`stb_image` 直接入库
- 行尾为 LF，git 提示 "LF will be replaced by CRLF" 属正常

## 协作习惯

- 改动后做**验证**：全量构建通过 + 用 `test/` 用例数值校验；可编译期验证的尽量 `static_assert`
- 回复用中文，结论先行，专业名词保留英文
- 发现明显 bug 可以顺手修复并说明（用户接受「修复 + 报告」的方式）；改动保持与现有风格一致、尽量最小
