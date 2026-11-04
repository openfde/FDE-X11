# 需求：宣告合成器（`_NET_WM_CM_S0`）与 ARGB 窗口支持

> 这是待开工的独立需求，和"窗口类型判定"那条线解耦。
> 新对话可以直接从这里开始，先读本文，再看下面列的源码位置。

## 1. 背景与动机

FDE-X11 现在**没有对外宣告合成器**，Qt/GTK 应用据此认为"没有合成器在跑"，于是：

- 不敢使用 32 位 ARGB 视觉；
- 需要阴影/圆角的弹窗会退化：自己把阴影画成**不透明黑色**。

### 实测证据（2026-11-04，ToDesk 二维码弹窗）

| 环境 | 窗口尺寸 | 内容尺寸（白卡片） |
|---|---|---|
| 原生 Linux 桌面（有合成器） | 376x241 | 376x241（严丝合缝，阴影半透明） |
| FDE-X11（无合成器） | 439x305 | 376x241（四周约 32px 黑边） |

- `xwininfo`：`439x305  Depth: 24  Visual 0x3bb`（24 位，无 alpha）；
- `import -window` 抓下来的 PNG 里，白色内容也只到 373 宽——**黑边在 X 的像素里就已存在**；
- 渲染链路已验证无误：`set_window_each` / `draw widget` 的 quad、EGL Surface、pixmap、X 窗口**四者都是 439x305**；
- WM 侧所有 `ConfigureRequest` 都只带 `X|Y`（从未请求过尺寸），尺寸由客户端在创建时决定。

结论：黑边是"无合成器导致的退化渲染"，不是尺寸/时序 bug。

### 反证：WPS 悬浮条

WPS 的 102x40 悬浮条自己指定了 ARGB（`Depth:32 / Visual 0x44`），在我们的服务器上显示正常——
说明"32 位视觉 + alpha 混合"这条链路是可行且已部分实现的。

## 2. 目标

1. X server 能正常提供 32 位 ARGB visual，应用可选到；
2. 带 alpha 的窗口在 Android 侧正确合成（透明区域透出下层内容，而不是黑）；
3. 最终由 WM 持有 `_NET_WM_CM_S0` 选择，并写进 `_NET_SUPPORTED`，让工具包切换行为；
4. 全程可开关：出问题能一键退回当前行为。

## 3. 分阶段计划（建议按序，不要跳步）

| 阶段 | 内容 | 风险 | 验收 |
|---|---|---|---|
| 1 | X server：32 位 visual 可用；渲染器对 `depth == 32` 的窗口按预乘 alpha 输出（**不宣告**） | 低 | WPS 悬浮条、ToDesk 弹窗（若选到 ARGB）阴影变半透明 |
| 2 | Android 承载 alpha：浮层 SurfaceView 用 `TRANSLUCENT` + 窗口格式 RGBA + Z 序正确 | 中 | 浮层透明区域透出父窗口，而不是黑 |
| 3 | WM：`XSetSelectionOwner(_NET_WM_CM_S0)` + 加入 `_NET_SUPPORTED`，带开关 | 高 | 全体 Qt/GTK 应用切 ARGB 后不出现大面积黑窗/全透明 |

开关建议：`persist.fde.x11.compositor=1/0`，默认 0，验证充分后再默认 1。

## 4. 现有可复用的代码

- `app/src/main/cpp/lorie/renderer.c`
  - `FRAGMENT_SHADER_ALPHA()` / `g_texture_program_alpha*`：输出 `vec4(rgb, a)`；
  - `draw_alpha()`：`glEnable(GL_BLEND)` + `glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`（XRender ARGB32 是预乘 alpha）；
  - widget 绘制处已按 `widget.has_alpha`（`pWin->drawable.depth == 32`）分流。
- `app/src/main/cpp/lorie/android.c`
  - `android_redirect_widget()` 里设置 `has_alpha`；
  - `android_update_widget_texture()` / `android_update_texture()`：纹理更新入口（widget 回退已加）。
- 之前试过又回退的改动（在 git 历史里可找回）：
  - `widgetView.setZOrderOnTop(true)` + `getHolder().setFormat(PixelFormat.TRANSLUCENT)`（`MainActivity.addFloatView`、`XWindowService` 的托盘/提示浮层）。
  - 回退原因：当时着色器强制 `alpha=1`，透明路径没有真正生效，且与"白底"问题混在一起。

## 5. 需要先确认的问题

1. 我们的 X server 里 composite 扩展是否完整启用（`compwindow.c` / `compalloc.c` 在树里，但 `CompositeExtensionInit` 是否被调用、是否 `CompositeRegisterAlternateVisuals`）；
2. 32 位 visual 是否已经在 `XVisualInfo` 列表里（`Depth 32 / Visual 0x44` 这类已经出现过，说明至少有）；
3. 宣告 selection 后，Qt 具体会切哪些行为（是否强制 ARGB、是否改变阴影绘制策略），需要用一个最小 Qt 程序验证；
4. 阶段 3 的开关放在 WM 还是 X server（推荐 WM，因为 selection 由它持有）。

## 6. 验收标准（整体）

- ToDesk 二维码弹窗：阴影为半透明，窗口内容 376x241 与原生一致；
- WPS 悬浮条：仍然正常（不回归）；
- 普通 24 位窗口：行为完全不变；
- 关闭开关后：回到当前行为，无残留。
