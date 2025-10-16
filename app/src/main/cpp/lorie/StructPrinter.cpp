//
// Created by huyang on 2025/10/16.
//

#include "StructPrinter.h"


std::string StructPrinter::toString(const WindProperty &prop) {
    std::ostringstream oss;
    oss << "WindProperty {\n"
        << "  window: 0x" << std::hex << prop.window << std::dec << "\n"
        << "  transient: 0x" << std::hex << prop.transient << std::dec << "\n"
        << "  leader: 0x" << std::hex << prop.leader << std::dec << "\n"
        << "  window_type: 0x" << std::hex << prop.window_type << std::dec << "\n"
        << "  net_wm_name: " << (prop.net_wm_name ? prop.net_wm_name : "null") << "\n"
        << "  wm_name: " << (prop.wm_name ? prop.wm_name : "null") << "\n"
        << "  wm_class: " << (prop.wm_class ? prop.wm_class : "null") << "\n"
        << "  support_wm_delete: " << (prop.support_wm_delete ? "true" : "false") << "\n"
        << "  support_motif: " << (prop.support_motif ? "true" : "false") << "\n"
        << "  icon: " << prop.icon << "\n"
        << "  pid: " << prop.pid << "\n"
        << "}";
    return oss.str();
}
std::string StructPrinter::toString(const Widget &widget) {
    std::ostringstream oss;
    oss << "Widget {\n"
        << "  texture_id: " << widget.texture_id << "\n"
        << "  size: " << widget.width << "x" << widget.height << "\n"
        << "  offset: (" << widget.offset_x << ", " << widget.offset_y << ")\n"
        << "  task_to: 0x" << std::hex << widget.task_to << std::dec << "\n"
        << "  window: 0x" << std::hex << widget.window << std::dec << "\n"
        << "  pWin: " << widget.pWin << "\n"
        << "  inbounds: " << (widget.inbounds ? "true" : "false") << "\n"
        << "  sfc: " << widget.sfc << "\n"
        << "  discard: " << widget.discard << "\n"
        << "  status: " << widget.status << "\n"
        << "  override_window_type: " << widget.override_window_type << "\n"
        << "}";
    return oss.str();
}

 std::string StructPrinter::toString(const WindAttribute &attr) {
    std::ostringstream oss;
    oss << "WindAttribute {\n"
        << "  texture_id: " << attr.texture_id << "\n"
        << "  dri_texture_id: " << attr.dri_texture_id << "\n"
        << "  size: " << attr.width << "x" << attr.height << "\n"
        << "  offset: (" << attr.offset_x << ", " << attr.offset_y << ")\n"
        << "  dri_size: " << attr.dri_w << "x" << attr.dri_h << "\n"
        << "  dri_offset: (" << attr.dri_x << ", " << attr.dri_y << ")\n"
        << "  index: " << attr.index << "\n"
        << "  window: 0x" << std::hex << attr.window << std::dec << "\n"
        << "  child: 0x" << std::hex << attr.child << std::dec << "\n"
        << "  frame: 0x" << std::hex << attr.frame << std::dec << "\n"
        << "  pWin: " << attr.pWin << "\n"
        << "  dri_pWin: " << attr.dri_pWin << "\n"
        << "  sfc: " << attr.sfc << "\n"
        << "  widget: " << attr.widget << "\n"
        << "  widget_size: " << attr.widget_size << "\n"
        << "  discard: " << attr.discard << "\n"
        << "  level: " << attr.level << "\n"
        << "  system_tray: " << (attr.system_tray ? "true" : "false") << "\n"
        << "  dock_sent: " << (attr.dock_sent ? "true" : "false") << "\n"
        << "  status: " << attr.status << "\n"
        << "  override_window_type: " << attr.override_window_type << "\n"
        << "  android_component: " << attr.android_component << "\n"
        << "  WindProperty: {\n";

    // 内嵌打印WindProperty
    std::string propStr = toString(attr.prop);
    // 为内嵌结构体添加缩进
    size_t pos = 0;
    while ((pos = propStr.find('\n', pos)) != std::string::npos) {
        propStr.replace(pos, 1, "\n    ");
        pos += 5;
    }
    oss << "    " << propStr.substr(0, propStr.find('\n') + 1);
    oss << "  }\n}";

    return oss.str();
}


