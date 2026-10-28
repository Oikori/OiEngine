# 永夜幸存者（Survivors）

类《吸血鬼幸存者》的俯视角自动战斗生存游戏，与 SandBox、Platformer 一样是引用 `Engine` 的独立可执行工程。
全部美术都是程序化几何（顶点色网格），不依赖任何图片资源。

## 构建与运行

```powershell
$cmake = 'D:\Program Files\JetBrains\CLion 2025.2.1\bin\cmake\win\x64\bin\cmake.exe'
& $cmake --preset mingw-debug
& $cmake --build build/cmake-build-debug --target Survivors
```

产物：`output/Survivors.exe`（与 SandBox / Platformer 共用 `output/libEngine.dll`）。命令行直接运行需要 MinGW 运行时在 PATH 中：

```powershell
$env:PATH = "D:\Program Files\JetBrains\CLion 2025.2.1\bin\mingw\bin;$env:PATH"
.\output\Survivors.exe
```

## 操作

| 输入 | 作用 |
| --- | --- |
| Enter / 空格 | 标题界面开始游戏（也可以直接点按钮） |
| 游戏说明 / 操作 | 标题页与暂停菜单里的按钮，打开完整说明面板（ESC 关闭） |
| 设置 / 音量 | 标题页与暂停菜单里的按钮，调主音量 / 背景音乐 / 音效，可静音（ESC 关闭） |
| WASD / 方向键 | 移动（武器自动索敌开火，无需攻击键） |
| 鼠标点击卡片 / 1、2、3 | 升级时三选一 |
| P | 暂停 / 继续 |
| Esc | 暂停 / 继续（暂停菜单里可「退出游戏」，不会立刻退出） |
| R | 重开一局 |
| F4 | 调试面板（FPS、实体数、伤害统计） |

> 中文输入法开启时，数字键可能被输入法候选窗吞掉；升级界面优先用鼠标点卡片（按钮足够大），不会卡流程。

## 规则

- 启动后先进入标题界面（开始游戏 / 退出游戏），不会直接开打。
- 关卡制，共 **10 关**：第 1 关存活 30 秒，之后每关 +5 秒（第 10 关 75 秒），撑完第 10 关即胜利；生命归零则结束。
- 过关会清空场上敌人与弹丸，地上的宝石自动折算成经验；**第 5 / 10 关有「血月使徒」**，进关即登场。
- 怪物数值随关卡递增（血量 / 伤害 / 防御 / 经验），刷怪节奏与兵种也逐关加强。
- 击杀敌人掉落经验宝石，走近自动吸取；升级后从三张卡里选一项强化。
- 武器四种：月刃（默认）、圣光环（近身脉冲）、回旋镰（往返穿透）、雷暴（随机落雷）；
  被动九种：力量、急速、疾行、体魄、磁石、幸运、回春、铁壁（防御）、破甲（穿甲）；另有「血之祭」补给卡。

## 代码结构

| 文件 | 职责 |
| --- | --- |
| `main.cpp` | `Application` 入口，压入一个 `SurvivorsLayer` |
| `SurvivorsGame.h/.cpp` | 游戏层：状态机、战斗、刷怪、升级、音频、HUD 与渲染 |
| `SurvivorsArt.h/.cpp` | 程序化几何库：角色 / 五种敌人 / 投射物 / 宝石 / 特效 / 地面装饰 |
| `assets/audio/` | 2 首 BGM + 11 个音效（程序化生成，无外部素材） |
| `tools/generate_audio.py` | 音频生成脚本：`python Survivors/tools/generate_audio.py` |

实现要点：

- 音频：`LoadAudio()` 在 `OnAttach` 加载全部资源；`EnsureBgm(boss)` 管 BGM（标题页主曲 → Boss 关切曲 → 过关切回，
  阵亡 / 通关停曲再播长音效）；`PlaySfx()` 每槽位独立冷却 + 随机变调，扛住每秒几十次命中 / 拾取。
  F4 面板可静音与调主音量。
- 正交相机跟随玩家，世界单位与像素按 900 高度 1:1 映射；窗口改变宽高比时自动调整横向可视范围。
- 敌人用 **空间哈希**（96 单位网格）做邻近查询：分离、索敌、弹丸碰撞、光环与落雷都是 O(n·k)，
  默认上限 420 个敌人。
- 敌人跑出 1400 单位后从视野外重新入场，避免玩家直线逃跑时屏幕空无一人；Boss 单独处理。
- 深度测试为 `GL_LESS`，所有绘制按 z 递增排列；屏幕暗角用径向渐变网格，最后绘制以免出现矩形接缝。
- 场景每帧显式声明 `Renderer::SetHDREnabled(false)` / `SetLightCullingEnabled(false)`，避免引擎全局开关残留。
