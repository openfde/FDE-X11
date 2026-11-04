#!/usr/bin/env bash
# 窗口角色判定（WindowRolePolicy）真机采集工具，在连着设备的主机上跑。
#
#   tools/winrole.sh dump              打印一次全量判定（含已消失窗口）
#   tools/winrole.sh watch             跟踪 winrole / WM 事件日志
#   tools/winrole.sh collect [FILE]    每 20s dump 一次并同时收实时日志，默认 /tmp/winrole-<ts>.log
#   tools/winrole.sh enable-live       打开逐窗口实时判定（需要重启窗口服务）
#   tools/winrole.sh disable-live      关掉逐窗口实时判定（需要重启窗口服务）
#   tools/winrole.sh restart           重启窗口服务（= 重启设备上的 X server）
#   tools/winrole.sh status            看 live 开关 / 服务进程
#
# 为什么 dump 就够用：native 的登记表在窗口消失后仍保留记录（最多 128 条，满了先淘汰
# 最久以前消失的），所以菜单、tooltip 这种几百毫秒的窗口也能事后看到。live 模式只是
# 让"窗口产生的瞬间"也有一行，方便和时间点对齐。
set -u

PKG=com.fde.x11
SERVICE="$PKG/.XWindowService"
ACTION_DUMP="$PKG.DUMP_WINDOW_ROLES"
PROP=persist.sys.fde.window_role.live
TAG=winrole

require_adb() {
    command -v adb >/dev/null 2>&1 || { echo "找不到 adb，请先连好设备" >&2; exit 1; }
}

dump_once() {
    adb shell am broadcast -a "$ACTION_DUMP" -p "$PKG" >/dev/null 2>&1
    sleep 0.6
    # 从最后一次广播的标记开始截，避免把上一次 dump 的内容也打出来
    adb logcat -d -v time -s "$TAG:*" 2>/dev/null | awk '
        /received com\.fde\.x11\.DUMP_WINDOW_ROLES/ { buf = "" }
        { buf = buf $0 "\n" }
        END { printf "%s", buf }'
}

restart_service() {
    local pid
    pid=$(adb shell pidof com.fde.x11.xserver 2>/dev/null | tr -d '\r')
    if [ -n "$pid" ]; then
        adb shell kill "$pid" >/dev/null 2>&1 || true
        sleep 2
    fi
    adb shell am startservice -n "$SERVICE" >/dev/null 2>&1
    sleep 2
    adb shell pidof com.fde.x11.xserver 2>/dev/null | tr -d '\r'
}

usage() {
    sed -n '2,/^set -u$/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

case "${1:-}" in
    dump)
        require_adb
        dump_once
        ;;
    watch)
        require_adb
        adb logcat -v time -s "$TAG:*" cc_window_manager.cpp:*
        ;;
    collect)
        require_adb
        out=${2:-/tmp/winrole-$(date +%Y%m%d-%H%M%S).log}
        echo "写到 $out（Ctrl-C 结束）"
        adb logcat -v time -s "$TAG:*" >"$out" 2>&1 &
        logpid=$!
        trap 'kill "$logpid" 2>/dev/null' EXIT INT TERM
        while :; do
            {
                printf '\n===== dump %s =====\n' "$(date +%H:%M:%S)"
                dump_once
            } >>"$out"
            sleep 20
        done
        ;;
    enable-live)
        require_adb
        adb shell setprop "$PROP" 1
        echo "live=1，重启窗口服务后生效（X server 会重启，Linux 侧应用要重连）"
        echo "pid=$(restart_service)"
        ;;
    disable-live)
        require_adb
        adb shell setprop "$PROP" 0
        echo "live=0，重启窗口服务后生效"
        echo "pid=$(restart_service)"
        ;;
    restart)
        require_adb
        echo "pid=$(restart_service)"
        ;;
    status)
        require_adb
        echo "live=$(adb shell getprop "$PROP" | tr -d '\r')"
        echo "xserver pid=$(adb shell pidof com.fde.x11.xserver | tr -d '\r')"
        ;;
    *)
        usage
        exit 1
        ;;
esac
