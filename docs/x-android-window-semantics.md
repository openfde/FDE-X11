# X 窗口与 Android 窗口的语义同步（当前实现）

> 本文记录 FDE-X11 里"X 窗口语义 → Android 宿主"的映射规则，以及几何、生命周期的同步路径。
> 目标是让两端对"这是一个什么窗口"有同一套理解，而不是靠尺寸或经验猜。

## 1. 三层结构

| 层 | 位置 | 职责 |
|---|---|---|
| 事实采集 | `lorie/window_facts.c` | 把 ICCCM / EWMH / Motif 属性抓成一份快照（类型、状态、input、transient、motif、pid、几何），按 xid 登记 |
| 判定 | `app/.../window/WindowRolePolicy.java` | 只读快照，输出 `WindowRole`：身份 / 容纳方式 / 装饰 / 模态 / 是否上 dock，并带 reason 便于对账 |
| 生产路由 | `android.c` / `Xserver.java` / `XWindowService.java` | 按判定选择宿主：Activity、浮层 View、内嵌 widget；并负责几何与生命周期 |

判定目前是"影子模式 + 生产侧独立判据"并存：policy 的输出用于诊断和对账（`winrole` 日志），
生产路由用的是同一套语义的另一份实现。两边口径必须一致，否则会出现 `IDENTITY_DIFF` 一类告警。

**这两份实现暂不合并，是刻意决定，不是遗留问题。** 理由：

- 判定层是"独立的第二个判断者"。它能在不改变行为的前提下，把"规则认为应该是什么"和"生产实际做了什么"
  逐条对比（`IDENTITY_DIFF` / `DECOR_DIFF` / `CONTAINMENT_DIFF` 都来自这个差值）。合并进生产代码后，
  诊断会退化成"用实现解释实现"，失去参考价值；
- 排查现场问题时，这份纯函数判定可以随手改、跑单测验证，不会把正在运行的窗口管理改坏；
- 代价是两份口径要同步维护：**任何一侧的规则改动都必须在另一侧同步，并在提交说明里注明**。
  将来若要合并，前提是先有一套覆盖足够全的现场数据 + 单测能把生产行为固定下来，在那之前保持现状。

## 2. 身份映射（Identity）

| X 侧特征 | 身份 | Android 宿主 |
|---|---|---|
| override-redirect | OVERLAY | 浮层 View |
| KDE override 且（无标准类型 或 `input == False`） | OVERLAY | 浮层 View |
| 非 root 子窗口 | OVERLAY / IN_PARENT | 浮层 View（或内嵌 widget） |
| `SYSTRAY` / `SYSTIP` | SYSTRAY | 托盘浮层 |
| `SPLASH` | TASK（无装饰） | `MainActivity11`（无装饰 Activity） |
| `UTILITY` + 小尺寸 +（KDE override 或 SKIP_TASKBAR） | OVERLAY | 浮层 View |
| 其余 aux 类型（UTILITY、菜单类） | `input == False` → OVERLAY；否则 TASK 无装饰 | 浮层 View / Activity |
| KDE override + SKIP_TASKBAR | OVERLAY | 浮层 View |
| 未声明类型 + SKIP_TASKBAR | OVERLAY | 浮层 View |
| `input == False` | OVERLAY | 浮层 View |
| `DIALOG` | TASK（无装饰，模态只认 `_NET_WM_STATE_MODAL`） | Activity |
| `NORMAL` / 未声明 | TASK，装饰看 Motif / KDE override | Activity（有/无装饰） |

原则：

- **结构优先于意愿**：override-redirect、父子关系、无属性这些"硬事实"先判；
- **呈现意愿不改变身份**：`SKIP_TASKBAR`、KDE override 只影响"无装饰"和"是否上 dock"；
- **要键盘的窗口必须是 Task**：浮层宿主是 `FLAG_NOT_FOCUSABLE`，做成浮层就"能看不能打字"。

## 3. 宿主选择与内嵌规则

`android_redirect_window()` 按窗口属性分三条路：

1. **Activity**：`android_frame`（WM 托管）→ 起 `MainActivity11` / `MainActivity1`；
2. **内嵌 widget**：**带 `WM_TRANSIENT_FOR`** 的浮层，且父窗口已有 Android 宿主、且矩形落在父窗口内（允许 2px 溢出）
   → 画进父窗口的 Surface，层级跟随父窗口、超出部分被裁剪；
3. **独立浮层 View**：其余浮层（无 transient 的弹窗、菜单位于父窗口之外等）。

判据要点：

- 内嵌只给"真子窗口"（有 transient）。无 transient 的弹窗内嵌进"当前焦点窗口"是错的：
  它在 surface manager 里只有 widget 记录、没有 `window_attrs` 登记，后续 configure/damage 都找不到它；
- `util_check_window_bounds()` 的容差加在**父窗口矩形**上，不能加在子窗口上；
- 无坐标的弹层由 `placePopupNearTransient()` 摆到父窗口右上角（应用没指定位置时）。

## 4. 几何同步（三条路径，各管一段）

| 窗口类型 | 谁负责几何 | 同步方式 |
|---|---|---|
| WM 托管的普通窗口 | WM 的 frame | `syncConfigureRequest(c->x/c->y/c->width/c->height)` → Java 调整宿主窗口 |
| 独立浮层（override-redirect / popup-like） | X server | `android_configure_window()` → `configureWidget()` → Java；同时更新 native `WindAttribute` |
| 内嵌 widget | X server | 同上，但更新的是父窗口 `widgets[]` 里的那一项 |

关键约束：

1. `OnConfigureRequest` **只认客户端原始掩码里的 `CWX/CWY`**。纯 `StackMode` 请求里 `e.x/e.y` 是 0，
   拿来当坐标会把已经摆好的窗口拽回 `(0,0)`；装饰高度修正补出来的 `CWY` 也不能参与判断。
2. 同步给 Android 的是 **WM 实际应用后的几何**（`c->x/c->y/c->w/c->h`），不是请求里的 `changes`。
3. 更新几何时必须同时更新"画多大"和"Surface 多大"所需的三份数据：`attr/widget` 的 `width/height`、
   `tex_w/tex_h`（强制重传）、Java 侧宿主窗口尺寸。

## 5. 生命周期同步

| X 事件 | Android 侧动作 |
|---|---|
| MapRequest（WM 托管） | `clientFrame` → 起 Activity |
| MapNotify（不托管） | `android_redirect_window` → 浮层 View / 内嵌 widget |
| UnmapNotify | **只隐藏**（`ACTION_UNMAP`）：Activity 保留在后台，重新 map 时拉回前台。以前是 `ACTION_DESTORY`，导致"隐藏再显示"的窗口（如 WPS 登录框）再也不出现 |
| DestroyNotify | 销毁宿主；若 Activity 尚未注册回调，记入 `shouldDestroyMap`，注册时补收尾，避免空壳窗口 |
| 子 widget 销毁 | 只摘除 widget，**不**把父窗口一起收掉（`XWindowService.isHostedWindow()` 闸门） |

## 6. dock / 最近任务

`Xserver.isHiddenFromDockWindow()` 是**唯一**驱动行为的地方（policy 里的 `hideFromDock` 只用于诊断）：

- 判据：只有 `_NET_WM_STATE_SKIP_TASKBAR`；
- 不算 `_KDE_NET_WM_WINDOW_TYPE_OVERRIDE`——WPS 给它所有窗口（含主窗）都设了这个 atom 当"不要装饰"用，
  算进去会让主窗从 dock 消失；
- 结果通过 Intent extra 传给宿主 Activity：跳过 `setTaskDescription`、加 `FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS`。

## 7. 已知限制

1. **没有合成器**：未宣告 `_NET_WM_CM_S0`，依赖 ARGB 的应用会退化成"不透明阴影"（详见 `compositor-argb-plan.md`）；
2. `_NET_WM_DESKTOP` 写入了异常值（实测 160），会影响按桌面分类的应用；
3. 判定层与生产层是两份实现（**刻意如此**，见第 1 节），口径靠测试和 `winrole` 对账保证；
4. 无 `WM_TRANSIENT_FOR` 的弹窗无法判断归属，只能作为独立浮层。

## 8. 相关文件

- 事实采集：`app/src/main/cpp/lorie/window_facts.c`、`window_facts.h`
- 判定：`app/src/main/java/com/fde/x11/window/WindowRolePolicy.java`、`WindowRole.java`、`WindowFacts.java`
- 诊断：`WindowRoleDiagnostics.java`（`adb shell am broadcast -a com.fde.x11.DUMP_WINDOW_ROLES`）
- 生产路由：`app/src/main/cpp/lorie/android.c`、`app/src/main/java/com/fde/x11/Xserver.java`、`XWindowService.java`
- WM：`fusionwindowmanager/src/main/cpp/window_manager.cpp`、`client.c`
- 单测：`app/src/test/java/com/fde/x11/window/`
