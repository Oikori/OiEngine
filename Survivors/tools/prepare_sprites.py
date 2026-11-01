#!/usr/bin/env python3
"""把生图产出的白底立绘切成游戏可直接用的精灵图。

    # 单张立绘 → 一个精灵
    python prepare_sprites.py single <源图> <输出.png>

    # 图集（白底上按网格排开的多个角色/怪物）→ 每格一个精灵
    python prepare_sprites.py sheet <图集> <输出目录> --names a,b,c,d --cols 2 --rows 2

处理流程：从图像四边 flood fill 掉纯白背景 → alpha 收缩 1px 削掉描边白边 → 按 alpha
包围盒裁切 → 等比缩放进 512x512 透明画布居中。统一画布是为了让游戏里所有精灵的
世界尺寸一致（绘制端只按一个 Scale 缩放即可）。
"""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CANVAS = 512          # 输出精灵边长（2 的幂，便于后续做 mipmap / 图集）
MARGIN = 0.045        # 画布四周留白比例，避免描边贴边被 mipmap 采样吃掉
BG_FLOOD_THRESH = 34  # 与白色种子的通道差阈值：太小去不干净，太大会吃掉浅色衣料


def cut_background(image: Image.Image) -> Image.Image:
    """边缘连通的白色区域置为透明，返回 RGBA。"""
    original = image.convert("RGB")
    work = original.copy()
    width, height = work.size

    # 沿四边取样做种子：白色背景总是与边缘连通，角色内部的白色高光被描边隔开、不受影响
    step = max(8, min(width, height) // 48)
    seeds = []
    for x in range(0, width, step):
        seeds.append((x, 0))
        seeds.append((x, height - 1))
    for y in range(1, height - 1, step):
        seeds.append((0, y))
        seeds.append((width - 1, y))

    marker = (255, 0, 255)   # flood fill 的标记色，最后按它生成 alpha
    for seed in seeds:
        r, g, b = work.getpixel(seed)
        if r > 232 and g > 232 and b > 232:
            ImageDraw.floodfill(work, seed, marker, thresh=BG_FLOOD_THRESH)

    data = np.asarray(work).astype(np.int16)
    filled = (data[:, :, 0] == 255) & (data[:, :, 1] == 0) & (data[:, :, 2] == 255)

    # 贴着背景的那一圈像素：雾 / 光晕 / 抗锯齿都是「颜色按 alpha 混到白底上」的产物，
    # 以白色为假设反解出 alpha 与本色，白边和毛刺就一起消失了（内部像素完全不动）
    background = Image.fromarray(np.where(filled, 255, 0).astype(np.uint8), "L")
    near_background = (np.asarray(background.filter(ImageFilter.MaxFilter(5))) > 0) & (~filled)

    rgb = np.asarray(original).astype(np.float32)
    alpha = np.where(filled, 0.0, 1.0).astype(np.float32)
    estimate = np.clip(1.0 - rgb.min(axis=2) / 255.0, 0.0, 1.0)
    alpha = np.where(near_background, np.minimum(alpha, estimate), alpha)
    unblended = np.clip((rgb - (1.0 - alpha)[..., None] * 255.0)
                        / np.maximum(alpha, 1e-3)[..., None], 0.0, 255.0)
    rgb = np.where(near_background[..., None], unblended, rgb)

    alpha_image = Image.fromarray((alpha * 255.0).astype(np.uint8), "L")
    alpha_image = alpha_image.filter(ImageFilter.GaussianBlur(0.6))

    result = Image.fromarray(rgb.astype(np.uint8), "RGB").convert("RGBA")
    result.putalpha(alpha_image)
    return result


def normalize(rgba: Image.Image, canvas: int = CANVAS) -> Image.Image:
    """按 alpha 包围盒裁切，等比缩放后居中放进 canvas 画布。"""
    bbox = rgba.split()[3].getbbox()
    if bbox is None:
        return Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))

    content = rgba.crop(bbox)
    limit = int(round(canvas * (1.0 - 2.0 * MARGIN)))
    scale = min(limit / content.width, limit / content.height)
    size = (max(1, int(round(content.width * scale))),
            max(1, int(round(content.height * scale))))
    content = content.resize(size, Image.LANCZOS)

    surface = Image.new("RGBA", (canvas, canvas), (0, 0, 0, 0))
    surface.paste(content, ((canvas - size[0]) // 2, (canvas - size[1]) // 2), content)
    return surface


def save_single(src: Path, dst: Path, canvas: int = CANVAS) -> None:
    sprite = normalize(cut_background(Image.open(src)), canvas)
    dst.parent.mkdir(parents=True, exist_ok=True)
    sprite.save(dst)
    print(f"{dst}  ({sprite.width}x{sprite.height})")


def save_sheet(src: Path, outdir: Path, names, cols: int, rows: int, canvas: int) -> None:
    sheet = Image.open(src).convert("RGB")
    cell_w = sheet.width // cols
    cell_h = sheet.height // rows
    outdir.mkdir(parents=True, exist_ok=True)
    for index, name in enumerate(names):
        row, col = divmod(index, cols)
        if row >= rows:
            print(f"跳过 {name}：网格 {cols}x{rows} 放不下第 {index + 1} 个")
            continue
        cell = sheet.crop((col * cell_w, row * cell_h, (col + 1) * cell_w, (row + 1) * cell_h))
        save_single_from_image(cell, outdir / f"{name}.png", canvas)


def save_single_from_image(image: Image.Image, dst: Path, canvas: int = CANVAS) -> None:
    sprite = normalize(cut_background(image), canvas)
    sprite.save(dst)
    print(f"{dst}  ({sprite.width}x{sprite.height})")


def main() -> None:
    parser = argparse.ArgumentParser(description="白底立绘 → 透明精灵")
    subparsers = parser.add_subparsers(dest="mode", required=True)

    single = subparsers.add_parser("single")
    single.add_argument("src")
    single.add_argument("dst")

    sheet = subparsers.add_parser("sheet")
    sheet.add_argument("src")
    sheet.add_argument("outdir")
    sheet.add_argument("--names", required=True, help="逗号分隔，按从左到右、从上到下排列")
    sheet.add_argument("--cols", type=int, default=2)
    sheet.add_argument("--rows", type=int, default=1)

    for sub in (single, sheet):
        # 画布边长：立绘用 512，图标这类小物件用 256 就够（显存与加载都快一截）
        sub.add_argument("--size", type=int, default=CANVAS)

    args = parser.parse_args()
    if args.mode == "single":
        save_single(Path(args.src), Path(args.dst), args.size)
    else:
        save_sheet(Path(args.src), Path(args.outdir),
                   [name.strip() for name in args.names.split(",") if name.strip()],
                   args.cols, args.rows, args.size)


if __name__ == "__main__":
    main()
