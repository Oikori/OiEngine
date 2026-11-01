#pragma once

#include <vector>

#include "OiEngine.h"

// 《永夜幸存者》的程序化几何库。
// 全部形状都是「位置 + 顶点色」网格，不依赖任何图片资源；绘制时通过实例变换复用。
namespace OiEngine::Survivors {

    struct SurvivorsArt {
        // 四个角色各一套程序化形象，进游戏时按开局选择的那一个来画
        Ref<VertexArray> playerWatchman;   // 守夜人：重甲 + 圆盾 + 短锤（最宽最厚重）
        Ref<VertexArray> playerHunter;     // 猎手：尖兜帽 + 短斗篷 + 长弓（细长）
        Ref<VertexArray> playerSister;     // 修女：头巾 + 米白长袍 + 十字杖（圆润明亮）
        Ref<VertexArray> playerWarlock;    // 术士：尖帽 + 紫黑长袍 + 悬浮法球（最瘦）

        Ref<VertexArray> bat;
        Ref<VertexArray> ghoul;
        Ref<VertexArray> wraith;
        Ref<VertexArray> brute;
        Ref<VertexArray> boss;

        Ref<VertexArray> blade;         // 月刃投射物
        Ref<VertexArray> scythe;        // 回旋镰投射物
        Ref<VertexArray> coin;          // 金币掉落物（击杀敌人后落在地上，刷新升级卡用）

        Ref<VertexArray> particle;      // 径向光点（u_Falloff 生效）
        Ref<VertexArray> disc;          // 实心圆（半径 1）
        Ref<VertexArray> ring;          // 圆环（外半径 1）
        Ref<VertexArray> quad;          // 单位方片
        Ref<VertexArray> vignette;      // 单边渐隐条（做顶部薄雾 / 光柱）
        Ref<VertexArray> vignetteDisc;  // 径向暗角（中心透明、边缘不透明）
        Ref<VertexArray> bolt;          // 闪电柱
        Ref<VertexArray> tombstone;     // 地面装饰：墓碑
        Ref<VertexArray> grass;         // 地面装饰：草丛
        Ref<VertexArray> fencePost;     // 场地围栏：立柱（带结界晶石）
        Ref<VertexArray> fenceRail;     // 场地围栏：横栏（一条，按段拉伸）
        Ref<VertexArray> sigil;         // 玩家脚下的法阵

        Ref<VertexArray> lineGrid;      // 世界网格线（用 RenderCommand::DrawLines 绘制）
        uint32_t lineGridVertexCount = 0;

        // ---- 贴图素材（可选）----
        // assets/textures/ 下存在对应 PNG 时用它替换上面的程序化剪影；文件缺失或解码失败
        // 就退回几何版本，游戏照常可玩（不影响联机/其它场景）。
        Ref<VertexArray> spriteQuad;      // 贴图方片：position + uv，原点在精灵中心
        Ref<Texture2D> playerSprite[4];   // 角色：守夜人 / 猎手 / 修女 / 术士
        Ref<Texture2D> enemySprite[5];    // 敌人：蝙蝠 / 食尸鬼 / 幽灵 / 蛮兽 / 使徒

        bool HasPlayerSprite(int index) const;
        bool HasEnemySprite(int index) const;

        // ---- 道具 / 武器 / 被动的图标（可选，同样缺图就留空，UI 自动跳过）----
        // 数组顺序由调用方给定（SurvivorsItems.cpp 按 ItemId / WeaponId / PassiveId 的枚举顺序传入），
        // 这样这一层不需要认识游戏层的枚举
        std::vector<Ref<Texture2D>> itemIcons;
        std::vector<Ref<Texture2D>> weaponIcons;
        std::vector<Ref<Texture2D>> passiveIcons;

        void LoadIcons(const std::vector<const char*>& itemFiles,
                       const std::vector<const char*>& weaponFiles,
                       const std::vector<const char*>& passiveFiles);

        void Load();
        void Unload();
    };
}
