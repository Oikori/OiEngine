#pragma once

#include "OiEngine.h"

// 《永夜幸存者》的程序化几何库。
// 全部形状都是「位置 + 顶点色」网格，不依赖任何图片资源；绘制时通过实例变换复用。
namespace OiEngine::Survivors {

    struct SurvivorsArt {
        Ref<VertexArray> player;        // 兜帽法师
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
        Ref<VertexArray> sigil;         // 玩家脚下的法阵

        Ref<VertexArray> lineGrid;      // 世界网格线（用 RenderCommand::DrawLines 绘制）
        uint32_t lineGridVertexCount = 0;

        void Load();
        void Unload();
    };
}
