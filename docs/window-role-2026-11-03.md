# 2026‑11‑03 窗口类型判定：对原逻辑的完善（未提交）

> 背景：昨天起按"影子判定（只打日志、不改行为）"的思路用真机数据驱动修正。
> 今天一天的结论是：**大部分"窗口类型不对"不是应用 bug，而是原实现的覆盖不全**——
> 原生桌面（xfwm4/KWin）有对应分支，我们没有。
> 下面按"判定规则 / 事实采集 / 生产路径 / 测试"四块记录改了什么、为什么改。

---

## 1. 判定规则（`WindowRole` / `WindowRolePolicy`）

### 1.1 新增正交维度 `hideFromDock`
`WindowRole` 增加 `hideFromDock`，表示"仍然是 Task，但不上 dock / 不进最近任务"。
它和 Identity/Containment/Decoration 一样是独立维度，`toString()` 里显示为 `/nodock`。

### 1.2 规则顺序：结构事实优先于客户端"意愿"
| 顺序 | 判据 | 结果 |
|---|---|---|
| 0 | `AppOverrideTable`（WPS / android_frame） | 应用级覆盖 |
| 1 | `override-redirect` | OVERLAY |
| 1b | KDE override + （无标准类型 或 `input == False`） | OVERLAY |
| 2 | 非 root 子窗口（`!rootChild`） | OVERLAY / IN_PARENT |
| 3 | 极小窗：root 顶层 + 非 OR + `0 < w,h <= 16` | OVERLAY（helper） |
| 3b | `present == 0`（一个属性都没有） | OVERLAY（helper） |
| 4 | `SYSTRAY` / `SYSTIP` / `SPLASH` | SYSTRAY / OVERLAY / TASK 无装饰 |
| 4b | 其余 aux 类型（UTILITY、菜单类…） | `input == False` → OVERLAY；否则 TASK 无装饰 |
| 5 | 未声明类型 + `SKIP_TASKBAR` | OVERLAY（面板/托盘） |
| 6 | `WM_HINTS.input == False` | OVERLAY |
| 7 | `DIALOG` | TASK / NONE，模态只认 `_NET_WM_STATE_MODAL` |
| 8 | `NORMAL` / 未声明 | TASK，装饰看 Motif / KDE override |

### 1.3 关键语义修正
- **`SKIP_TASKBAR` 不再改变身份**：有类型声明时只置 `hideFromDock`。例外：没声明类型
  （`typeCode == NONE`）时仍判浮层，用于面板/托盘。
- **KDE override 同理**：有标准类型时只影响"无装饰 + 不上 dock"。只有"只声明了 KDE atom"
  或 `input == False` 时才判浮层。
  - 理由：浮层宿主是 `FLAG_NOT_FOCUSABLE`，把要键盘的模态框做成浮层会"能看不能打字"。
- **`DIALOG` 的模态**只认 `_NET_WM_STATE_MODAL`；`WM_TRANSIENT_FOR` 只表示依附关系，
  以前"任何 transient 都算模态"会让父窗口平白变灰。
- **新增 `SPLASH`（277）**：启动画面 = 无装饰 Task + 不上 dock。以前落回 `NORMAL`，
  被当成普通窗口套标题栏。
- **UTILITY 等 aux 按 input 分流**：`input=False` → 浮层；`input=True`（WPS 账号登录）
  → Task 无标题栏。
- **Motif 装饰位**：抽出 `MWM_DECOR_TITLE_BITS`，明确 `BORDER/RESIZEH` 不算"要标题栏"
  （GTK CSD 窗口常只报这两位）。
- **极小窗守卫**：`w/h > 0` 才按尺寸判 helper；0×0 表示几何未定，继续按类型判。
- **无属性窗口**：ICCCM 要求属性在 map 前设好，`present == 0` 的顶层窗判
  `helper:no-properties`。
- **reason 标签**区分 `+motif-no-decoration` 与 `+kde-override-no-decoration`
  （以前 motif=0/0/0 也写 motif，误导排查）。

---

## 2. 事实采集（`window_facts.c/h` + `WindowFacts.java`）

- **多 atom 类型优先级**：具体类型优先，`NORMAL` 只作兜底。
  以前 `[NORMAL, TOOLTIP]` 会取到 NORMAL，把 TOOLTIP 的意图丢掉。
- **`_KDE_NET_WM_WINDOW_TYPE_OVERRIDE` 记为事实**：`WF_FLAG_KDE_OVERRIDE`（bit 12）。
- **`SPLASH` 类型码 277**，atom 名映射。
- **按 xid 取 flags**：`window_facts_flags()` + JNI `nativeWindowFactsFlags`，
  给"要不要上 dock"用（只读登记表，任意线程可调）。
- 诊断行新增 `flags=0x%x`。

---

## 3. 生产路径（原逻辑需要完善的地方）

### 3.1 尺寸启发式：删除
`Xserver.convert2AndroidType()`（`w < 400 || h < 250` → 把 NORMAL 改判 DIALOG）整条删除。
现场：mate-terminal 映射时 361×76，62 ms 后才被拉到 654×440，按映射那一刻的尺寸判身份
→ 普通窗口永久没有标题栏（resize 不会重新判定宿主）。

### 3.2 类型分派：不再静默丢窗口
`startOrUpdateWindow()` 的 Activity 分支只认识 `0/NORMAL/DIALOG/SYSTRAY/SYSTIP`，
其它类型落到 `default: break;` → **不创建任何宿主**（ToDesk 悬浮条就是这样"完全没显示"）。
现在：
- 菜单类 / UTILITY 等 aux 类型走 `X_START_VIEW`（浮层）；
- `SPLASH` 走 `X_START_ACTIVITY_WINDOW`（无装饰 Activity）；
- `actualHostKind()` 同步修正（aux → WIDGET/VIEW，SPLASH → ACTIVITY_NODECOR），
  以前它会对所有未知类型硬贴 `ACTIVITY_NODECOR`，诊断里的"actual"是假的。

### 3.3 装饰：生产侧也要认 KDE override
`android.c` 的两个类型解析函数遇到 `_KDE_NET_WM_WINDOW_TYPE_OVERRIDE` 就置
`support_motif = 1`（老 Java 侧用它表示"不要标题栏"）。
否则 splash / 登录框会被 `MainActivity1` 套上标题栏（DECOR_DIFF）。

### 3.4 生产侧识别 SPLASH
`android.c` 新增 `_NET_WM_WINDOW_TYPE_SPLASH = 277`，两个解析函数都在 NORMAL 之前匹配。

### 3.5 popup-like 判据扩展（WM 与 X server 必须同源）
`checkPopupLikeWindow()`（`client.c`）与 `window_is_popup_like()`（`android.c`）现在一致：
1. `_NET_WM_WINDOW_TYPE` 属于菜单/提示类（MENU/POPUP_MENU/DROPDOWN_MENU/COMBO/TOOLTIP/DND）；
2. `UTILITY` 且 `WM_HINTS.input == False`；
3. `KDE override + SKIP_TASKBAR`（且 `input == False`）。

第 3 条的 `input == False` 是今天补的：WPS 的"账号登录"是 UTILITY + KDE override + input=1，
按老判据会被塞进浮层，登录框就"能看不能打字"。

### 3.6 无属性窗口不重定向
`android_redirect_window()` 的 Activity 分支：属性源窗口 `userProps == NULL` → 直接返回，
不再为一个 0.9 s 的裸窗口闪一个空宿主。

### 3.7 父窗口匹配：不再用 leader
同一个应用所有窗口共享一个 leader，WPS 的登录框/工具栏因此被挂到了 splash 上。
顺序改为 **transient → 当前焦点窗口 → pid**。

### 3.8 浮层几何同步
`android_configure_window()` 原来只对 override-redirect 放行；现在 override-redirect
**或 popup-like** 都会把几何推给浮层宿主（否则浮层停在旧尺寸，出现黑边）。

### 3.9 WM：悬浮工具条自定位 + ConfigureRequest 掩码修正
- 新增 `WindowManager::isSelfPositionedToolbar()`：`UTILITY + SKIP_TASKBAR + ≤400×200`；
- `OnConfigureRequest`：**只有 `value_mask` 真的带 `CWX/CWY` 时才用 `e.x/e.y`**。
  纯 `StackMode` 请求里 `e.x/e.y` 是无意义值，今天真机上就是它把已经摆好的工具条
  拽回 `(0,44)`；
- 同步给 Android 的几何改成 **WM 实际应用后的 `c->x/c->y/c->width/c->height`**，
  不再用请求里的 `changes`。

### 3.10 dock / 最近任务
- `Xserver.isHiddenFromDockWindow()`：`SKIP_TASKBAR | KDE_OVERRIDE`；
- `XWindowService` 启动宿主时把结果塞进 Intent extra，并加
  `FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS`；
- `MainActivity` 据 extra 跳过全部 4 处 `setTaskDescription`（不写 label，
  DockAppsProvider 的 `taskDescription?.label ?: return` 就会跳过它）。
  注意：native 的登记表在 xserver 进程，Activity 在 app 进程，所以只能在
  `XWindowService` 里算好、随 Intent 传过去。

### 3.11 宿主生命周期
- 窗口销毁时若 Activity 还没注册回调，记入 `shouldDestroyMap`，等
  `registerActivityCallback` 时直接 finish（否则留下空壳窗口）；
- `startActLikeWindow` 时清掉该标记（XID 会被复用）。

### 3.12 浮层视图的测量
`LorieView` 新增 `setFloatingPopup(true)`：浮层的 `onMeasure` 不再走
"按全局分辨率等比裁剪 + setFixedSize"，Surface 尺寸 = 布局尺寸 = X 窗口尺寸。
否则 439×305 的弹窗会被量成 376×241，和窗口对不上。

---

## 4. 测试

- `app/build.gradle` 增加 `testImplementation 'junit:junit:4.13.2'`；
- `app/src/test/java/com/fde/x11/window/`：
  - `WindowRolePolicyTest`：**35 个用例**，覆盖 override-redirect / KDE override /
    client-child / 极小窗 / 无属性 / SYSTRAY / aux+input / SKIP_TASKBAR / input=false /
    DIALOG 模态 / Motif 三态 / WPS 与 android_frame 覆盖 / hostKind 映射；
  - `WindowFactsTest`：**4 个用例**，锁 native 编码格式与 Java 解析的契约。
- 运行：`gradlew testVersioncDebugUnitTest --tests "com.fde.x11.window.*"`

---

## 5. 已知未完成（明天的入口）

1. **WPS 悬浮工具条位置**：今天的掩码修复已编译、未装机验证。
   明天装最新包后复现，确认日志里 `syncConfigureRequest` 只出现 `1259 241 …`，
   不再有 `0 44 …`。
2. **类型解析两层不一致**：事实层（`window_facts.c`）是"具体类型优先、NORMAL 兜底"，
   产出层（`android.c: property_get_type_info`）还是"扫到哪个算哪个"，
   尾部 `NORMAL` 会覆盖 `UTILITY`。应改成同一套优先级。
3. **合成器宣告**：WM 没有持有 `_NET_WM_CM_S0`，应用会自绘不透明阴影
   （ToDesk 二维码弹窗的黑边）。要么宣告合成器并支持 32 位 alpha，要么接受。
4. **短命辅助窗口的宿主创建**：WPS 反复创建/销毁 102×40 窗口，每个都开一个宿主。
   可以考虑延迟创建/复用宿主。
5. **类型表还缺**：`DOCK / DESKTOP / TOOLBAR / NOTIFICATION` 等，仍会落回 NORMAL。
6. **containmentDiff 误报**：菜单/DND 的 `WM_TRANSIENT_FOR` 指向客户端窗口 id，
   登记表里只有 frame id，`resolveParent` 找不到 → 诊断里 3 条 `CONTAINMENT_DIFF` 是假阳性。

---

## 6. 不在本线、但仍在工作区的改动

这些是之前会话留下的，和"窗口类型判定"无关，提交时建议单独成一个 commit：

- `lorie/renderer.c`：去掉根纹理/窗口纹理上传前的临时 malloc+memcpy；
- `fusionwindowmanager`：多处 `XWindowChanges = {0}` 初始化、`device.c` 死代码、
  `netwm.c` 的 `FALLTHROUGH` 修正；
- `app/src/main/AndroidManifest.xml`：打开 MainActivity 的 freeform 默认布局（760×920 dp）；
- `MainActivity` 的 `applyInitialTaskDescription()`（dock 依赖 `taskDescription.label`
  识别 X11 窗口，没有 `_NET_WM_ICON` 的客户端以前进不了 dock）。
