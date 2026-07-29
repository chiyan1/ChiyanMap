#include "state/LanguageManager.h"
#include "state/MapRenderState.h"
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <nlohmann/json.hpp>
#include <ll/api/i18n/I18n.h>
#include <mutex>

using json = nlohmann::json;

namespace LanguageManager {
    std::string g_currentLanguage = "en";
    std::vector<std::pair<std::string, std::string>> g_availableLanguages;
    static std::unordered_map<std::string, std::string> g_translationCache;
    static std::mutex g_cacheMutex;

    static std::unordered_map<std::string, std::string> g_defaultJsonFiles = {
        {"en", R"json({
        "BIGMAP_TITLE": "Chiyan Big Map | Zoom: %.1fx",
        "BIGMAP_HELP": "[Drag] Pan    [Scroll] Zoom    [Esc] Close Map",
        "CURSOR_POS": "Cursor: X: %d  Z: %d",
        "BIOME_LABEL": "Biome: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Player Status ]",
        "PLAYER_POS_X": "Player X: %d",
        "PLAYER_POS_Y": "Player Y: %d",
        "PLAYER_POS_Z": "Player Z: %d",
        "SIDEBAR_OPS": "[ Settings ]",
        "SHOW_MINIMAP": "Show Minimap",
        "SQUARE_MINIMAP": "Square Minimap",
        "CENTER_CAMERA": "Center Camera to Player",
        "NETHER_WARNING": "[ Nether magnetic field is too strong to draw map ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "E",
        "COMPASS_W": "W",
        "CONTEXT_TITLE": "Select Action",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Block: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Copy Coordinates",
        "CREATE_WAYPOINT": "Create Waypoint",
        "TELEPORT_HERE": "Teleport Here",
        "OPEN_WP_MENU": "Open Waypoint Manager",
        "RENAME_WP": "Rename Waypoint",
        "DELETE_WP": "Delete Waypoint",
        "TELEPORT_WP": "Teleport to Waypoint",
        "WP_MANAGER_TITLE": "Waypoint Manager (Press 'U' or 'Esc' to Close)##WP",
        "SEARCH_HINT": "Enter name to search waypoints...",
        "NEW_WP_BUTTON": " + New Waypoint",
        "NEW_WP_TITLE": "New Waypoint##Popup",
        "WP_LIST_SHOW": "Show",
        "WP_LIST_RENAME": "Rename",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Delete",
        "WP_SAVE": "Save",
        "WP_CANCEL": "Cancel",
        "WP_NAME": "Name",
        "WP_COLOR": "Color",
        "WP_DEFAULT_NAME": "New Waypoint",
        "EDIT_WP_TITLE": "Edit Waypoint##Popup",
        "EDIT_WP": "Edit",
        "WP_SHOW_ON_MAP": "Show on Map",
        "LANG_SELECT": "Language",
        "LOG_VERSION_MISMATCH": "[CRITICAL] Game version mismatch! Current client version",
        "LOG_VERSION_STRICT": "[CRITICAL] ChiyanMap strictly supports version 1.26.20.04 only!",
        "LOG_VERSION_ABORT": "[CRITICAL] Mod loading aborted to prevent Access Violation crashes.",
        "LOG_VERSION_PASS": "Game client version verification passed",
        "LOG_VERSION_UNKNOWN": "Unable to identify game executable version, attempting to force load...",
        "CAVE_SETTINGS": "Cave Map Settings",
        "CAVE_MODE_OFF": "Off",
        "CAVE_MODE_LAYERED": "On",
        "CAVE_MODE_TYPE": "Cave Mode",
        "CAVE_MODE_DESC": "Select the cave map rendering mode",
        "CAVE_ACTIVE": "Cave mode: ON",
        "CAVE_DEPTH": "Render depth",
        "CAVE_DEPTH_DESC": "Number of layers rendered downward",
        "CAVE_INACTIVE": "Cave mode: OFF",
        "CAVE_LEGIBLE": "High contrast",
        "CAVE_LEGIBLE_DESC": "Improve readability of ambiguous underground blocks",
        "CAVE_TOP_Y": "Top height",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "In auto mode, the top height is computed from the player's current layer",
        "CAVE_TOP_Y_MANUAL": "Manual",
        "CAVE_TOP_Y_MODE": "Top height mode",
        "HOTKEY_ACTION": "Action",
        "HOTKEY_CLEAR": "Clear waypoints",
        "HOTKEY_DISABLED": "Disabled",
        "HOTKEY_KEY": "Key",
        "HOTKEY_OPEN_BIGMAP": "Open big map",
        "HOTKEY_OPEN_WPMGR": "Open waypoint manager",
        "HOTKEY_RESET": "Reset view",
        "HOTKEY_RESET_ALL": "Reset all",
        "HOTKEY_SETTINGS": "Hotkeys",
        "HOTKEY_SETTINGS_TITLE": "Hotkey settings",
        "HOTKEY_STATUS_CLEARED": "Waypoints cleared",
        "HOTKEY_STATUS_RESET": "View reset",
        "HOTKEY_STATUS_UNDONE": "Undone",
        "HOTKEY_TOGGLE_MINIMAP": "Toggle minimap",
        "HOTKEY_TOGGLE_ROTATION": "Toggle rotation",
        "HOTKEY_TOGGLE_SHAPE": "Toggle shape",
        "HOTKEY_UNDO": "Undo",
        "MINIMAP_ZOOM_RADIUS": "Zoom radius",
        "RESET": "Reset",
        "SHOW_WAYPOINTS_MINIMAP": "Show waypoints on minimap",
        "TELEPORT_FAILED": "Teleport failed",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Teleport failed: timed out or rejected by server",
        "TELEPORT_LOADING": "Teleporting…",
        "TELEPORT_LOADING_HINT": "Requesting teleport from the server, please wait",
        "TELEPORT_TIMEOUT_MSG": "Teleport timed out: no response from server",
        "WP_DELETE_SELECTED": "Delete selected",
        "WP_DESELECT_ALL": "Deselect all",
        "WP_SELECT_ALL": "Select all"
    })json"},
        {"zh_CN", R"json({
        "BIGMAP_TITLE": "\u8d64\u7130\u5168\u5c40\u5927\u5730\u56fe | \u7f29\u653e: %.1fx",
        "BIGMAP_HELP": "[\u62d6\u62fd] \u5e73\u79fb    [\u6eda\u8f6e] \u7f29\u653e    [Esc] \u5173\u95ed\u5730\u56fe",
        "CURSOR_POS": "\u5149\u6807\u4f4d\u7f6e: X: %d  Z: %d",
        "BIOME_LABEL": "\u751f\u7269\u7fa4\u7cfb: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u73a9\u5bb6\u72b6\u6001 ]",
        "PLAYER_POS_X": "\u73a9\u5bb6 X: %d",
        "PLAYER_POS_Y": "\u73a9\u5bb6 Y: %d",
        "PLAYER_POS_Z": "\u73a9\u5bb6 Z: %d",
        "SIDEBAR_OPS": "[ \u64cd\u4f5c\u9762\u677f ]",
        "SHOW_MINIMAP": "\u663e\u793a\u53f3\u4e0a\u89d2\u5c0f\u5730\u56fe",
        "SQUARE_MINIMAP": "\u4f7f\u7528\u65b9\u5f62\u5c0f\u5730\u56fe",
        "CENTER_CAMERA": "\u89c6\u89d2\u56de\u4e2d\u81f3\u73a9\u5bb6",
        "NETHER_WARNING": "\u3010 \u4e0b\u754c\u78c1\u573a\u5e72\u6270\u8fc7\u5f3a\uff0c\u65e0\u6cd5\u7ed8\u5236\u5730\u56fe \u3011",
        "COMPASS_N": "\u5317",
        "COMPASS_S": "\u5357",
        "COMPASS_E": "\u4e1c",
        "COMPASS_W": "\u897f",
        "CONTEXT_TITLE": "\u9009\u62e9\u64cd\u4f5c",
        "CHUNK_POS": "\u533a\u5757: (%d; %d)",
        "BLOCK_POS": "\u5750\u6807: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u590d\u5236\u5750\u6807",
        "CREATE_WAYPOINT": "\u521b\u5efa\u8def\u5f84\u70b9",
        "TELEPORT_HERE": "\u4f20\u9001\u5230\u6b64\u5730",
        "OPEN_WP_MENU": "\u6253\u5f00\u8def\u5f84\u70b9\u83dc\u5355",
        "RENAME_WP": "\u91cd\u547d\u540d\u8def\u5f84\u70b9",
        "DELETE_WP": "\u5220\u9664\u6b64\u8def\u5f84\u70b9",
        "TELEPORT_WP": "\u4f20\u9001\u5230\u6b64\u8def\u5f84\u70b9",
        "WP_MANAGER_TITLE": "\u8def\u5f84\u70b9\u7ba1\u7406\u5668 (\u6309 'U' \u6216 'Esc' \u5173\u95ed)##WP",
        "SEARCH_HINT": "\u5728\u6b64\u8f93\u5165\u540d\u79f0\u4ee5\u641c\u7d22\u8def\u5f84\u70b9...",
        "NEW_WP_BUTTON": " + \u65b0\u5efa\u8def\u5f84\u70b9",
        "NEW_WP_TITLE": "\u65b0\u5efa\u8def\u5f84\u70b9##Popup",
        "WP_LIST_SHOW": "\u663e\u793a",
        "WP_LIST_RENAME": "\u91cd\u547d\u540d",
        "WP_LIST_TELEPORT": "\u4f20\u9001",
        "WP_LIST_DELETE": "\u5220\u9664",
        "WP_SAVE": "\u4fdd\u5b58",
        "WP_CANCEL": "\u53d6\u6d88",
        "WP_NAME": "名称",
        "WP_COLOR": "颜色",
        "WP_DEFAULT_NAME": "\u65b0\u5730\u6807",
        "EDIT_WP_TITLE": "\u7f16\u8f91\u8def\u5f84\u70b9##Popup",
        "EDIT_WP": "\u7f16\u8f91",
        "WP_SHOW_ON_MAP": "\u5728\u5730\u56fe\u4e0a\u663e\u793a",
        "LANG_SELECT": "\u8bed\u8a00",
        "LOG_VERSION_MISMATCH": "\u3010\u4e25\u91cd\u8b66\u544a\u3011\u6e38\u620f\u7248\u672c\u4e0d\u9002\u914d\uff01\u5f53\u524d\u5ba2\u6237\u7aef\u7248\u672c\u4e3a",
        "LOG_VERSION_STRICT": "\u3010\u4e25\u91cd\u8b66\u544a\u3011\u8d64\u7130\u5730\u56fe (ChiyanMap) \u5e95\u5c42\u62e6\u622a\u5668\u5f53\u524d\u4e25\u683c\u9650\u5b9a\u4ec5\u517c\u5bb9 1.26.20.04 \u7248\u672c\uff01",
        "LOG_VERSION_ABORT": "\u3010\u4e25\u91cd\u8b66\u544a\u3011\u4e3a\u9632\u6b62\u52a0\u8f7d\u8fdb\u5165\u4e16\u754c\u65f6\u53d1\u751f Access Violation \u5d29\u6e83\uff0c\u6a21\u7ec4\u5df2\u4e3b\u52a8\u4e2d\u6b62\u52a0\u8f7d\u3002",
        "LOG_VERSION_PASS": "\u6e38\u620f\u5ba2\u6237\u7aef\u7248\u672c\u9a8c\u8bc1\u901a\u8fc7",
        "LOG_VERSION_UNKNOWN": "\u65e0\u6cd5\u8bc6\u522b\u5f53\u524d\u6e38\u620f\u53ef\u6267\u884c\u6587\u4ef6\u7684\u7248\u672c\u4fe1\u606f\uff0c\u6a21\u7ec4\u5c06\u5c1d\u8bd5\u5f3a\u884c\u52a0\u8f7d...",
        "CAVE_SETTINGS": "洞穴地图设置",
        "CAVE_MODE_OFF": "关闭",
        "CAVE_MODE_LAYERED": "开启",
        "CAVE_MODE_TYPE": "洞穴模式",
        "CAVE_MODE_DESC": "选择洞穴地图的渲染模式",
        "CAVE_ACTIVE": "洞穴模式：开启",
        "CAVE_DEPTH": "显示深度",
        "CAVE_DEPTH_DESC": "向下渲染的层数",
        "CAVE_INACTIVE": "洞穴模式：关闭",
        "CAVE_LEGIBLE": "高对比度",
        "CAVE_LEGIBLE_DESC": "提升地下不明方块的可读性",
        "CAVE_TOP_Y": "顶部高度",
        "CAVE_TOP_Y_AUTO": "自动",
        "CAVE_TOP_Y_DESC": "自动模式下，顶部高度根据玩家当前所在层自动计算",
        "CAVE_TOP_Y_MANUAL": "手动",
        "CAVE_TOP_Y_MODE": "顶部高度模式",
        "HOTKEY_ACTION": "操作",
        "HOTKEY_CLEAR": "清除路点",
        "HOTKEY_DISABLED": "已禁用",
        "HOTKEY_KEY": "按键",
        "HOTKEY_OPEN_BIGMAP": "打开全屏大地图",
        "HOTKEY_OPEN_WPMGR": "打开路径点管理器",
        "HOTKEY_RESET": "重置视图",
        "HOTKEY_RESET_ALL": "重置全部",
        "HOTKEY_SETTINGS": "快捷键",
        "HOTKEY_SETTINGS_TITLE": "快捷键设置",
        "HOTKEY_STATUS_CLEARED": "已清除路点",
        "HOTKEY_STATUS_RESET": "视图已重置",
        "HOTKEY_STATUS_UNDONE": "已撤销",
        "HOTKEY_TOGGLE_MINIMAP": "开启/关闭小地图",
        "HOTKEY_TOGGLE_ROTATION": "开启/关闭小地图旋转",
        "HOTKEY_TOGGLE_SHAPE": "切换小地图形状",
        "HOTKEY_UNDO": "撤销",
        "MINIMAP_ZOOM_RADIUS": "缩放半径",
        "RESET": "重置",
        "SHOW_WAYPOINTS_MINIMAP": "小地图显示路点",
        "TELEPORT_FAILED": "传送失败",
        "TELEPORT_FAILED_DISMISS": "知道了",
        "TELEPORT_FAILED_MSG": "传送失败：超时或被服务器拒绝",
        "TELEPORT_LOADING": "传送中…",
        "TELEPORT_LOADING_HINT": "正在向服务器请求传送，请稍候",
        "TELEPORT_TIMEOUT_MSG": "传送超时：服务器无响应",
        "WP_DELETE_SELECTED": "删除选中",
        "WP_DESELECT_ALL": "取消全选",
        "WP_SELECT_ALL": "全选"
    })json"},
        {"zh_TW", R"json({
        "BIGMAP_TITLE": "\u8d64\u7130\u5168\u5c40\u5730\u5716 | \u7e2e\u653e: %.1fx",
        "BIGMAP_HELP": "[\u62d6\u62fd] \u5e73\u79fb    [\u6efe\u8f2a] \u7e2e\u653e    [Esc] \u95dc\u9589\u5730\u5716",
        "CURSOR_POS": "\u6e38\u6a19\u4f4d\u7f6e: X: %d  Z: %d",
        "BIOME_LABEL": "\u751f\u614b\u7cfb: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u73a9\u5bb6\u72c0\u614b ]",
        "PLAYER_POS_X": "\u73a9\u5bb6 X: %d",
        "PLAYER_POS_Y": "\u73a9\u5bb6 Y: %d",
        "PLAYER_POS_Z": "\u73a9\u5bb6 Z: %d",
        "SIDEBAR_OPS": "[ \u64cd\u4f5c\u9762\u677f ]",
        "SHOW_MINIMAP": "\u986f\u793a\u53f3\u4e0a\u89d2\u5c0f\u5730\u5716",
        "SQUARE_MINIMAP": "\u4f7f\u7528\u65b9\u5f62\u5c0f\u5730\u5716",
        "CENTER_CAMERA": "\u8996\u89d2\u56de\u4e2d\u81f3\u73a9\u5bb6",
        "NETHER_WARNING": "\u3010 \u4e0b\u754c\u78c1\u5834\u5e72\u64fe\u904e\u5f37\uff0c\u7121\u6cd5\u7e6a\u88fd\u5730\u5716 \u3011",
        "COMPASS_N": "\u5317",
        "COMPASS_S": "\u5357",
        "COMPASS_E": "\u6771",
        "COMPASS_W": "\u897f",
        "CONTEXT_TITLE": "\u9078\u64c7\u64cd\u4f5c",
        "CHUNK_POS": "\u5340\u584a: (%d; %d)",
        "BLOCK_POS": "\u5ea7\u6a19: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u8907\u88fd\u5ea7\u6a19",
        "CREATE_WAYPOINT": "\u5efa\u7acb\u8def\u5f91\u9ede",
        "TELEPORT_HERE": "\u50b3\u9001\u5230\u6b64\u5730",
        "OPEN_WP_MENU": "\u958b\u555f\u8def\u5f91\u9ede\u9078\u55ae",
        "RENAME_WP": "\u91cd\u547d\u540d\u8def\u5f91\u9ede",
        "DELETE_WP": "\u522a\u9664\u6b64\u8def\u5f91\u9ede",
        "TELEPORT_WP": "\u50b3\u9001\u5230\u6b64\u8def\u5f91\u9ede",
        "WP_MANAGER_TITLE": "\u8def\u5f91\u9ede\u7ba1\u7406\u5668 (\u6309 'U' \u6216 'Esc' \u95dc\u9589)##WP",
        "SEARCH_HINT": "\u5728\u6b64\u8f38\u5165\u540d\u7a31\u4ee5\u641c\u5c0b\u8def\u5f91\u9ede...",
        "NEW_WP_BUTTON": " + \u65b0\u5efa\u8def\u5f91\u9ede",
        "NEW_WP_TITLE": "\u65b0\u5efa\u8def\u5f91\u9ede##Popup",
        "WP_LIST_SHOW": "\u986f\u793a",
        "WP_LIST_RENAME": "\u91cd\u547d\u540d",
        "WP_LIST_TELEPORT": "\u50b3\u9001",
        "WP_LIST_DELETE": "\u522a\u9664",
        "WP_SAVE": "\u5132\u5b58",
        "WP_CANCEL": "\u53d6\u6d88",
        "WP_NAME": "名稱",
        "WP_COLOR": "顏色",
        "WP_DEFAULT_NAME": "\u65b0\u5730\u6a19",
        "EDIT_WP_TITLE": "\u7de8\u8f2f\u8def\u5f91\u9ede##Popup",
        "EDIT_WP": "\u7de8\u8f2f",
        "WP_SHOW_ON_MAP": "\u5728\u5730\u5716\u4e0a\u986f\u793a",
        "LANG_SELECT": "\u8a9e\u8a00",
        "LOG_VERSION_MISMATCH": "\u3010\u56b4\u91cd\u8b66\u544a\u3011\u904a\u6232\u7248\u672c\u4e0d\u7b26\u5408\uff01\u7576\u524d\u5ba2\u6236\u7aef\u7248\u672c\u70ba",
        "LOG_VERSION_STRICT": "\u3010\u56b4\u91cd\u8b66\u544a\u3011\u8d64\u7130\u5730\u5716 (ChiyanMap) \u5e95\u5c64\u62e6\u622a\u5668\u7576\u524d\u56b4\u683c\u9650\u5b9a\u50c5\u76f8\u5bb9 1.26.20.04 \u7248\u672c\uff01",
        "LOG_VERSION_ABORT": "\u3010\u56b4\u91cd\u8b66\u544a\u3011\u70ba\u9632\u6b62\u8f09\u5165\u9032\u5165\u4e16\u754c\u6642\u767c\u751f Access Violation \u5d29\u6e83\uff0c\u6a21\u7d44\u5df2\u4e3b\u52d5\u7d42\u6b62\u8f09\u5165\u3002",
        "LOG_VERSION_PASS": "\u904a\u6232\u5ba2\u6236\u7aef\u7248\u672c\u9a57\u8b49\u901a\u904e",
        "LOG_VERSION_UNKNOWN": "\u7121\u6cd5\u8b58\u5225\u7576\u524d\u904a\u6232\u57f7\u884c\u6a94\u7684\u7248\u672c\u8cc7\u8a0a\uff0c\u6a21\u7d44\u5c07\u5617\u8a66\u5f37\u884c\u8f09\u5165...",
        "CAVE_SETTINGS": "洞穴地圖設置",
        "CAVE_MODE_OFF": "關閉",
        "CAVE_MODE_LAYERED": "開啟",
        "CAVE_MODE_TYPE": "洞穴模式",
        "CAVE_MODE_DESC": "選擇洞穴地圖的渲染模式",
        "CAVE_ACTIVE": "洞穴模式：開啟",
        "CAVE_DEPTH": "顯示深度",
        "CAVE_DEPTH_DESC": "向下渲染的層數",
        "CAVE_INACTIVE": "洞穴模式：關閉",
        "CAVE_LEGIBLE": "高對比度",
        "CAVE_LEGIBLE_DESC": "提升地下不明方塊的可讀性",
        "CAVE_TOP_Y": "頂部高度",
        "CAVE_TOP_Y_AUTO": "自動",
        "CAVE_TOP_Y_DESC": "自動模式下，頂部高度根據玩家當前所在層自動計算",
        "CAVE_TOP_Y_MANUAL": "手動",
        "CAVE_TOP_Y_MODE": "頂部高度模式",
        "HOTKEY_ACTION": "操作",
        "HOTKEY_CLEAR": "清除路點",
        "HOTKEY_DISABLED": "已停用",
        "HOTKEY_KEY": "按鍵",
        "HOTKEY_OPEN_BIGMAP": "打開大地圖",
        "HOTKEY_OPEN_WPMGR": "打開路點管理器",
        "HOTKEY_RESET": "重置視圖",
        "HOTKEY_RESET_ALL": "重置全部",
        "HOTKEY_SETTINGS": "快捷鍵",
        "HOTKEY_SETTINGS_TITLE": "快捷鍵設定",
        "HOTKEY_STATUS_CLEARED": "已清除路點",
        "HOTKEY_STATUS_RESET": "視圖已重置",
        "HOTKEY_STATUS_UNDONE": "已撤銷",
        "HOTKEY_TOGGLE_MINIMAP": "切換小地圖",
        "HOTKEY_TOGGLE_ROTATION": "切換旋轉",
        "HOTKEY_TOGGLE_SHAPE": "切換形狀",
        "HOTKEY_UNDO": "撤銷",
        "MINIMAP_ZOOM_RADIUS": "縮放半徑",
        "RESET": "重置",
        "SHOW_WAYPOINTS_MINIMAP": "小地圖顯示路點",
        "TELEPORT_FAILED": "傳送失敗",
        "TELEPORT_FAILED_DISMISS": "知道了",
        "TELEPORT_FAILED_MSG": "傳送失敗：逾時或被伺服器拒絕",
        "TELEPORT_LOADING": "傳送中…",
        "TELEPORT_LOADING_HINT": "正在向伺服器請求傳送，請稍候",
        "TELEPORT_TIMEOUT_MSG": "傳送逾時：伺服器無回應",
        "WP_DELETE_SELECTED": "刪除選中",
        "WP_DESELECT_ALL": "取消全選",
        "WP_SELECT_ALL": "全選"
    })json"},
        {"de", R"json({
        "BIGMAP_TITLE": "Chiyan Weltkarte | Zoom: %.1fx",
        "BIGMAP_HELP": "[Ziehen] Verschieben    [Mausrad] Zoom    [Esc] Karte schlie\u00dfen",
        "CURSOR_POS": "Cursor: X: %d  Z: %d",
        "BIOME_LABEL": "Biom: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Spielerstatus ]",
        "PLAYER_POS_X": "Spieler X: %d",
        "PLAYER_POS_Y": "Spieler Y: %d",
        "PLAYER_POS_Z": "Spieler Z: %d",
        "SIDEBAR_OPS": "[ Einstellungen ]",
        "SHOW_MINIMAP": "Minimap anzeigen",
        "SQUARE_MINIMAP": "Eckige Minimap",
        "CENTER_CAMERA": "Kamera auf Spieler zentrieren",
        "NETHER_WARNING": "[ Nether-Magnetfeld ist zu stark, um Karte zu zeichnen ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "O",
        "COMPASS_W": "W",
        "CONTEXT_TITLE": "Aktion ausw\u00e4hlen",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Block: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Koordinaten kopieren",
        "CREATE_WAYPOINT": "Wegpunkt erstellen",
        "TELEPORT_HERE": "Hierhin teleportieren",
        "OPEN_WP_MENU": "Wegpunkt-Manager \u00f6ffnen",
        "RENAME_WP": "Wegpunkt umbenennen",
        "DELETE_WP": "Wegpunkt l\u00f6schen",
        "TELEPORT_WP": "Zu Wegpunkt teleportieren",
        "WP_MANAGER_TITLE": "Wegpunkt-Manager (Dr\u00fccke 'U' oder 'Esc' zum Schlie\u00dfen)##WP",
        "SEARCH_HINT": "Name eingeben, um Wegpunkte zu suchen...",
        "NEW_WP_BUTTON": " + Neuer Wegpunkt",
        "NEW_WP_TITLE": "Neuer Wegpunkt##Popup",
        "WP_LIST_SHOW": "Anzeigen",
        "WP_LIST_RENAME": "Umbenennen",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "L\u00f6schen",
        "WP_SAVE": "Speichern",
        "WP_CANCEL": "Abbrechen",
        "WP_NAME": "Name",
        "WP_COLOR": "Farbe",
        "WP_DEFAULT_NAME": "Neuer Wegpunkt",
        "LANG_SELECT": "Sprache",
        "LOG_VERSION_MISMATCH": "[KRITISCH] Spielversion stimmt nicht überein! Aktuelle Client-Version",
        "LOG_VERSION_STRICT": "[KRITISCH] ChiyanMap unterstützt strikt nur die Version 1.26.20.04!",
        "LOG_VERSION_ABORT": "[KRITISCH] Mod-Ladevorgang abgebrochen, um Access Violation-Abstürze zu verhindern.",
        "LOG_VERSION_PASS": "Überprüfung der Spiel-Client-Version bestanden",
        "LOG_VERSION_UNKNOWN": "Die Version der ausführbaren Spieldatei konnte nicht identifiziert werden. Es wird versucht, das Laden zu erzwingen...",
        "CAVE_SETTINGS": "Höhlenkarten-Einstellungen",
        "CAVE_MODE_OFF": "Aus",
        "CAVE_MODE_LAYERED": "AN",
        "CAVE_MODE_TYPE": "Höhlenmodus",
        "CAVE_MODE_DESC": "Wählen Sie den Render-Modus der Höhlenkarte",
        "CAVE_ACTIVE": "Höhlenmodus: AN",
        "CAVE_DEPTH": "Render-Tiefe",
        "CAVE_DEPTH_DESC": "Anzahl der nach unten gerenderten Ebenen",
        "CAVE_INACTIVE": "Höhlenmodus: AUS",
        "CAVE_LEGIBLE": "Hoher Kontrast",
        "CAVE_LEGIBLE_DESC": "Verbessert die Lesbarkeit mehrdeutiger Untergrund-Blöcke",
        "CAVE_TOP_Y": "Obere Höhe",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "Im Auto-Modus wird die obere Höhe aus der aktuellen Ebene des Spielers berechnet",
        "CAVE_TOP_Y_MANUAL": "Manuell",
        "CAVE_TOP_Y_MODE": "Obere Höhen-Modus",
        "HOTKEY_ACTION": "Aktion",
        "HOTKEY_CLEAR": "Wegpunkte löschen",
        "HOTKEY_DISABLED": "Deaktiviert",
        "HOTKEY_KEY": "Taste",
        "HOTKEY_OPEN_BIGMAP": "Große Karte öffnen",
        "HOTKEY_OPEN_WPMGR": "Wegpunkt-Manager öffnen",
        "HOTKEY_RESET": "Ansicht zurücksetzen",
        "HOTKEY_RESET_ALL": "Alles zurücksetzen",
        "HOTKEY_SETTINGS": "Tastenkürzel",
        "HOTKEY_SETTINGS_TITLE": "Tastenkürzel-Einstellungen",
        "HOTKEY_STATUS_CLEARED": "Wegpunkte gelöscht",
        "HOTKEY_STATUS_RESET": "Ansicht zurückgesetzt",
        "HOTKEY_STATUS_UNDONE": "Rückgängig gemacht",
        "HOTKEY_TOGGLE_MINIMAP": "Minimap umschalten",
        "HOTKEY_TOGGLE_ROTATION": "Rotation umschalten",
        "HOTKEY_TOGGLE_SHAPE": "Form umschalten",
        "HOTKEY_UNDO": "Rückgängig",
        "MINIMAP_ZOOM_RADIUS": "Zoomradius",
        "RESET": "Zurücksetzen",
        "SHOW_WAYPOINTS_MINIMAP": "Wegpunkte auf der Minimap anzeigen",
        "TELEPORT_FAILED": "Teleport fehlgeschlagen",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Teleport fehlgeschlagen: Zeitüberschreitung oder vom Server abgelehnt",
        "TELEPORT_LOADING": "Teleportieren…",
        "TELEPORT_LOADING_HINT": "Teleport beim Server anfragen, bitte warten",
        "TELEPORT_TIMEOUT_MSG": "Teleport-Zeitüberschreitung: keine Antwort vom Server",
        "WP_DELETE_SELECTED": "Ausgewählte löschen",
        "WP_DESELECT_ALL": "Auswahl aufheben",
        "WP_SELECT_ALL": "Alle auswählen"
    })json"},
        {"fr", R"json({
        "BIGMAP_TITLE": "Carte globale de Chiyan | Zoom : %.1fx",
        "BIGMAP_HELP": "[Glisser] Panoramique    [Molette] Zoom    [Esc] Fermer la carte",
        "CURSOR_POS": "Curseur : X: %d  Z: %d",
        "BIOME_LABEL": "Biome : %s",
        "SIDEBAR_PLAYER_STATUS": "[ Statut du joueur ]",
        "PLAYER_POS_X": "Joueur X: %d",
        "PLAYER_POS_Y": "Joueur Y: %d",
        "PLAYER_POS_Z": "Joueur Z: %d",
        "SIDEBAR_OPS": "[ Param\u00e8tres ]",
        "SHOW_MINIMAP": "Afficher la minimap",
        "SQUARE_MINIMAP": "Minimap carr\u00e9e",
        "CENTER_CAMERA": "Centrer la cam\u00e9ra sur le joueur",
        "NETHER_WARNING": "[ Le champ magn\u00e9tique du Nether est trop fort pour dessiner la carte ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "O",
        "COMPASS_W": "O",
        "CONTEXT_TITLE": "S\u00e9lectionner une action",
        "CHUNK_POS": "Chunk : (%d, %d)",
        "BLOCK_POS": "Bloc : X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Copier les coordonn\u00e9es",
        "CREATE_WAYPOINT": "Cr\u00e9er un point de rep\u00e8re",
        "TELEPORT_HERE": "Se t\u00e9l\u00e9porter ici",
        "OPEN_WP_MENU": "Ouvrir le gestionnaire de rep\u00e8res",
        "RENAME_WP": "Renommer le rep\u00e8re",
        "DELETE_WP": "Supprimer le rep\u00e8re",
        "TELEPORT_WP": "Se t\u00e9l\u00e9porter au rep\u00e8re",
        "WP_MANAGER_TITLE": "Gestionnaire de rep\u00e8res (Appuyez sur 'U' ou 'Esc' pour fermer)##WP",
        "SEARCH_HINT": "Entrez un nom pour rechercher...",
        "NEW_WP_BUTTON": " + Nouveau rep\u00e8re",
        "NEW_WP_TITLE": "Nouveau rep\u00e8re##Popup",
        "WP_LIST_SHOW": "Afficher",
        "WP_LIST_RENAME": "Renommer",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Supprimer",
        "WP_SAVE": "Sauvegarder",
        "WP_CANCEL": "Annuler",
        "WP_NAME": "Nom",
        "WP_COLOR": "Couleur",
        "WP_DEFAULT_NAME": "Nouveau rep\u00e8re",
        "LANG_SELECT": "Langue",
        "LOG_VERSION_MISMATCH": "[CRITIQUE] Incompatibilité de la version du jeu ! Version actuelle du client",
        "LOG_VERSION_STRICT": "[CRITIQUE] ChiyanMap prend strictement en charge uniquement la version 1.26.20.04 !",
        "LOG_VERSION_ABORT": "[CRITIQUE] Chargement du mod annulé pour éviter les plantages (Access Violation).",
        "LOG_VERSION_PASS": "Vérification de la version du client de jeu réussie",
        "LOG_VERSION_UNKNOWN": "Impossible d'identifier la version de l'exécutable du jeu, tentative de chargement forcé...",
        "CAVE_SETTINGS": "Paramètres de la carte des grottes",
        "CAVE_MODE_OFF": "Désactivé",
        "CAVE_MODE_LAYERED": "ACTIVÉ",
        "CAVE_MODE_TYPE": "Mode grotte",
        "CAVE_MODE_DESC": "Sélectionnez le mode de rendu de la carte des grottes",
        "CAVE_ACTIVE": "Mode grotte : ACTIVÉ",
        "CAVE_DEPTH": "Profondeur de rendu",
        "CAVE_DEPTH_DESC": "Nombre de couches rendues vers le bas",
        "CAVE_INACTIVE": "Mode grotte : DÉSACTIVÉ",
        "CAVE_LEGIBLE": "Contraste élevé",
        "CAVE_LEGIBLE_DESC": "Améliore la lisibilité des blocs souterrains ambigus",
        "CAVE_TOP_Y": "Hauteur supérieure",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "En mode auto, la hauteur supérieure est calculée selon la couche actuelle du joueur",
        "CAVE_TOP_Y_MANUAL": "Manuel",
        "CAVE_TOP_Y_MODE": "Mode de hauteur supérieure",
        "HOTKEY_ACTION": "Action",
        "HOTKEY_CLEAR": "Effacer les waypoints",
        "HOTKEY_DISABLED": "Désactivé",
        "HOTKEY_KEY": "Touche",
        "HOTKEY_OPEN_BIGMAP": "Ouvrir la grande carte",
        "HOTKEY_OPEN_WPMGR": "Ouvrir le gestionnaire de waypoints",
        "HOTKEY_RESET": "Réinitialiser la vue",
        "HOTKEY_RESET_ALL": "Tout réinitialiser",
        "HOTKEY_SETTINGS": "Raccourcis",
        "HOTKEY_SETTINGS_TITLE": "Paramètres des raccourcis",
        "HOTKEY_STATUS_CLEARED": "Waypoints effacés",
        "HOTKEY_STATUS_RESET": "Vue réinitialisée",
        "HOTKEY_STATUS_UNDONE": "Annulé",
        "HOTKEY_TOGGLE_MINIMAP": "Activer/désactiver la minimap",
        "HOTKEY_TOGGLE_ROTATION": "Activer/désactiver la rotation",
        "HOTKEY_TOGGLE_SHAPE": "Activer/désactiver la forme",
        "HOTKEY_UNDO": "Annuler",
        "MINIMAP_ZOOM_RADIUS": "Rayon de zoom",
        "RESET": "Réinitialiser",
        "SHOW_WAYPOINTS_MINIMAP": "Afficher les waypoints sur la minimap",
        "TELEPORT_FAILED": "Échec de la téléportation",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Échec de la téléportation : délai dépassé ou refusé par le serveur",
        "TELEPORT_LOADING": "Téléportation…",
        "TELEPORT_LOADING_HINT": "Demande de téléportation au serveur, veuillez patienter",
        "TELEPORT_TIMEOUT_MSG": "Délai de téléportation dépassé : pas de réponse du serveur",
        "WP_DELETE_SELECTED": "Supprimer la sélection",
        "WP_DESELECT_ALL": "Tout désélectionner",
        "WP_SELECT_ALL": "Tout sélectionner"
    })json"},
        {"id", R"json({
        "BIGMAP_TITLE": "Peta Besar Chiyan | Zoom: %.1fx",
        "BIGMAP_HELP": "[Seret] Geser    [Gulir] Zoom    [Esc] Tutup Peta",
        "CURSOR_POS": "Kursor: X: %d  Z: %d",
        "BIOME_LABEL": "Bioma: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Status Pemain ]",
        "PLAYER_POS_X": "Pemain X: %d",
        "PLAYER_POS_Y": "Pemain Y: %d",
        "PLAYER_POS_Z": "Pemain Z: %d",
        "SIDEBAR_OPS": "[ Pengaturan ]",
        "SHOW_MINIMAP": "Tampilkan Minimap",
        "SQUARE_MINIMAP": "Minimap Kotak",
        "CENTER_CAMERA": "Pusatkan Kamera ke Pemain",
        "NETHER_WARNING": "[ Medan magnet Nether terlalu kuat untuk menggambar peta ]",
        "COMPASS_N": "U",
        "COMPASS_S": "S",
        "COMPASS_E": "T",
        "COMPASS_W": "B",
        "CONTEXT_TITLE": "Pilih Tindakan",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Blok: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Salin Koordinat",
        "CREATE_WAYPOINT": "Buat Titik Jalan",
        "TELEPORT_HERE": "Teleportasi ke Sini",
        "OPEN_WP_MENU": "Buka Pengelola Titik Jalan",
        "RENAME_WP": "Ubah Nama Titik Jalan",
        "DELETE_WP": "Hapus Titik Jalan",
        "TELEPORT_WP": "Teleportasi ke Titik Jalan",
        "WP_MANAGER_TITLE": "Pengelola Titik Jalan (Tekan 'U' atau 'Esc' untuk Tutup)##WP",
        "SEARCH_HINT": "Masukkan nama untuk mencari...",
        "NEW_WP_BUTTON": " + Titik Jalan Baru",
        "NEW_WP_TITLE": "Titik Jalan Baru##Popup",
        "WP_LIST_SHOW": "Tampilkan",
        "WP_LIST_RENAME": "Ubah Nama",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Hapus",
        "WP_SAVE": "Simpan",
        "WP_CANCEL": "Batal",
        "WP_NAME": "Nama",
        "WP_COLOR": "Warna",
        "WP_DEFAULT_NAME": "Titik Jalan Baru",
        "LANG_SELECT": "Bahasa",
        "LOG_VERSION_MISMATCH": "[KRITIS] Ketidakcocokan versi game! Versi klien saat ini",
        "LOG_VERSION_STRICT": "[KRITIS] ChiyanMap secara ketat hanya mendukung versi 1.26.20.04!",
        "LOG_VERSION_ABORT": "[KRITIS] Pemuatan mod dibatalkan untuk mencegah crash Access Violation.",
        "LOG_VERSION_PASS": "Verifikasi versi klien game berhasil",
        "LOG_VERSION_UNKNOWN": "Tidak dapat mengidentifikasi versi eksekusi game, mencoba memaksakan pemuatan...",
        "CAVE_SETTINGS": "Pengaturan peta gua",
        "CAVE_MODE_OFF": "Mati",
        "CAVE_MODE_LAYERED": "HIDUP",
        "CAVE_MODE_TYPE": "Mode gua",
        "CAVE_MODE_DESC": "Pilih mode render peta gua",
        "CAVE_ACTIVE": "Mode gua: HIDUP",
        "CAVE_DEPTH": "Kedalaman render",
        "CAVE_DEPTH_DESC": "Jumlah lapisan yang dirender ke bawah",
        "CAVE_INACTIVE": "Mode gua: MATI",
        "CAVE_LEGIBLE": "Kontras tinggi",
        "CAVE_LEGIBLE_DESC": "Tingkatkan keterbacaan blok bawah tanah yang ambigu",
        "CAVE_TOP_Y": "Ketinggian atas",
        "CAVE_TOP_Y_AUTO": "Otomatis",
        "CAVE_TOP_Y_DESC": "Dalam mode otomatis, ketinggian atas dihitung dari lapisan pemain saat ini",
        "CAVE_TOP_Y_MANUAL": "Manual",
        "CAVE_TOP_Y_MODE": "Mode ketinggian atas",
        "HOTKEY_ACTION": "Tindakan",
        "HOTKEY_CLEAR": "Hapus waypoint",
        "HOTKEY_DISABLED": "Nonaktif",
        "HOTKEY_KEY": "Tombol",
        "HOTKEY_OPEN_BIGMAP": "Buka peta besar",
        "HOTKEY_OPEN_WPMGR": "Buka pengelola waypoint",
        "HOTKEY_RESET": "Atur ulang tampilan",
        "HOTKEY_RESET_ALL": "Atur ulang semua",
        "HOTKEY_SETTINGS": "Tombol pintas",
        "HOTKEY_SETTINGS_TITLE": "Pengaturan tombol pintas",
        "HOTKEY_STATUS_CLEARED": "Waypoint dihapus",
        "HOTKEY_STATUS_RESET": "Tampilan diatur ulang",
        "HOTKEY_STATUS_UNDONE": "Dibatalkan",
        "HOTKEY_TOGGLE_MINIMAP": "Alihkan minimap",
        "HOTKEY_TOGGLE_ROTATION": "Alihkan rotasi",
        "HOTKEY_TOGGLE_SHAPE": "Alihkan bentuk",
        "HOTKEY_UNDO": "Urungkan",
        "MINIMAP_ZOOM_RADIUS": "Radius zoom",
        "RESET": "Atur ulang",
        "SHOW_WAYPOINTS_MINIMAP": "Tampilkan waypoint di minimap",
        "TELEPORT_FAILED": "Teleport gagal",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Teleport gagal: waktu habis atau ditolak oleh server",
        "TELEPORT_LOADING": "Teleportasi…",
        "TELEPORT_LOADING_HINT": "Meminta teleport dari server, harap tunggu",
        "TELEPORT_TIMEOUT_MSG": "Teleport waktu habis: tidak ada respons dari server",
        "WP_DELETE_SELECTED": "Hapus yang dipilih",
        "WP_DESELECT_ALL": "Batalkan pilihan semua",
        "WP_SELECT_ALL": "Pilih semua"
    })json"},
        {"it", R"json({
        "BIGMAP_TITLE": "Mappa globale di Chiyan | Zoom: %.1fx",
        "BIGMAP_HELP": "[Trascina] Pan    [Scorri] Zoom    [Esc] Chiudi mappa",
        "CURSOR_POS": "Cursore: X: %d  Z: %d",
        "BIOME_LABEL": "Bioma: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Stato del giocatore ]",
        "PLAYER_POS_X": "Giocatore X: %d",
        "PLAYER_POS_Y": "Giocatore Y: %d",
        "PLAYER_POS_Z": "Giocatore Z: %d",
        "SIDEBAR_OPS": "[ Impostazioni ]",
        "SHOW_MINIMAP": "Mostra Minimappa",
        "SQUARE_MINIMAP": "Minimappa quadrata",
        "CENTER_CAMERA": "Centra la visuale sul giocatore",
        "NETHER_WARNING": "[ Il campo magnetico del Nether \u00e8 troppo forte per disegnare la mappa ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "O",
        "COMPASS_W": "O",
        "CONTEXT_TITLE": "Seleziona azione",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Blocco: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Copia coordinate",
        "CREATE_WAYPOINT": "Crea waypoint",
        "TELEPORT_HERE": "Teleportati qui",
        "OPEN_WP_MENU": "Apri gestione waypoint",
        "RENAME_WP": "Rinomina waypoint",
        "DELETE_WP": "Elimina waypoint",
        "TELEPORT_WP": "Teleportati al waypoint",
        "WP_MANAGER_TITLE": "Gestore waypoint (Premi 'U' o 'Esc' per chiudere)##WP",
        "SEARCH_HINT": "Inserisci il nome per cercare...",
        "NEW_WP_BUTTON": " + Nuovo waypoint",
        "NEW_WP_TITLE": "Nuovo waypoint##Popup",
        "WP_LIST_SHOW": "Mostra",
        "WP_LIST_RENAME": "Rinomina",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Elimina",
        "WP_SAVE": "Salva",
        "WP_CANCEL": "Annulla",
        "WP_NAME": "Nome",
        "WP_COLOR": "Colore",
        "WP_DEFAULT_NAME": "Nuovo waypoint",
        "LANG_SELECT": "Lingua",
        "LOG_VERSION_MISMATCH": "[CRITICO] Mancata corrispondenza della versione del gioco! Versione attuale del client",
        "LOG_VERSION_STRICT": "[CRITICO] ChiyanMap supporta rigorosamente solo la versione 1.26.20.04!",
        "LOG_VERSION_ABORT": "[CRITICO] Caricamento della mod interrotto per prevenire crash (Access Violation).",
        "LOG_VERSION_PASS": "Verifica della versione del client di gioco superata",
        "LOG_VERSION_UNKNOWN": "Impossibile identificare la versione dell'eseguibile del gioco, tentativo di caricamento forzato in corso...",
        "CAVE_SETTINGS": "Impostazioni mappa delle grotte",
        "CAVE_MODE_OFF": "Disattivato",
        "CAVE_MODE_LAYERED": "ATTIVA",
        "CAVE_MODE_TYPE": "Modalità grotta",
        "CAVE_MODE_DESC": "Seleziona la modalità di rendering della mappa delle grotte",
        "CAVE_ACTIVE": "Modalità caverna: ATTIVA",
        "CAVE_DEPTH": "Profondità di rendering",
        "CAVE_DEPTH_DESC": "Numero di livelli renderizzati verso il basso",
        "CAVE_INACTIVE": "Modalità caverna: DISATTIVA",
        "CAVE_LEGIBLE": "Alto contrasto",
        "CAVE_LEGIBLE_DESC": "Migliora la leggibilità dei blocchi sotterranei ambigui",
        "CAVE_TOP_Y": "Altezza superiore",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "In modalità auto, l'altezza superiore è calcolata dal livello attuale del giocatore",
        "CAVE_TOP_Y_MANUAL": "Manuale",
        "CAVE_TOP_Y_MODE": "Modalità altezza superiore",
        "HOTKEY_ACTION": "Azione",
        "HOTKEY_CLEAR": "Cancella waypoint",
        "HOTKEY_DISABLED": "Disattivato",
        "HOTKEY_KEY": "Tasto",
        "HOTKEY_OPEN_BIGMAP": "Apri mappa grande",
        "HOTKEY_OPEN_WPMGR": "Apri gestore waypoint",
        "HOTKEY_RESET": "Reimposta vista",
        "HOTKEY_RESET_ALL": "Reimposta tutto",
        "HOTKEY_SETTINGS": "Tasti",
        "HOTKEY_SETTINGS_TITLE": "Impostazioni tasti",
        "HOTKEY_STATUS_CLEARED": "Waypoint cancellati",
        "HOTKEY_STATUS_RESET": "Vista reimpostata",
        "HOTKEY_STATUS_UNDONE": "Annullato",
        "HOTKEY_TOGGLE_MINIMAP": "Attiva/disattiva minimappa",
        "HOTKEY_TOGGLE_ROTATION": "Attiva/disattiva rotazione",
        "HOTKEY_TOGGLE_SHAPE": "Attiva/disattiva forma",
        "HOTKEY_UNDO": "Annulla",
        "MINIMAP_ZOOM_RADIUS": "Raggio zoom",
        "RESET": "Reimposta",
        "SHOW_WAYPOINTS_MINIMAP": "Mostra waypoint sulla minimappa",
        "TELEPORT_FAILED": "Teletrasporto fallito",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Teletrasporto fallito: timeout o rifiutato dal server",
        "TELEPORT_LOADING": "Teletrasporto…",
        "TELEPORT_LOADING_HINT": "Richiesta di teletrasporto al server, attendere",
        "TELEPORT_TIMEOUT_MSG": "Timeout teletrasporto: nessuna risposta dal server",
        "WP_DELETE_SELECTED": "Elimina selezionati",
        "WP_DESELECT_ALL": "Deseleziona tutto",
        "WP_SELECT_ALL": "Seleziona tutto"
    })json"},
        {"ja", R"json({
        "BIGMAP_TITLE": "\u8d64\u7130\u5168\u4f53\u30de\u30c3\u30d7 | \u30ba\u30fc\u30e0: %.1fx",
        "BIGMAP_HELP": "[\u30c9\u30e9\u30c3\u30b0] \u30b9\u30af\u30ed\u30fc\u30eb    [\u30db\u30a4\u30fc\u30eb] \u30ba\u30fc\u30e0    [Esc] \u30de\u30c3\u30d7\u3092\u9589\u3058\u308b",
        "CURSOR_POS": "\u30ab\u30fc\u30bd\u30eb\u4f4d\u7f6e: X: %d  Z: %d",
        "BIOME_LABEL": "\u30d0\u30a4\u30aa\u30fc\u30e0: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u30d7\u30ec\u30a4\u30e4\u30fc\u306e\u30b9\u30c6\u30fc\u30bf\u30b9 ]",
        "PLAYER_POS_X": "\u30d7\u30ec\u30a4\u30e4\u30fc X: %d",
        "PLAYER_POS_Y": "\u30d7\u30ec\u30a4\u30e4\u30fc Y: %d",
        "PLAYER_POS_Z": "\u30d7\u30ec\u30a4\u30e4\u30fc Z: %d",
        "SIDEBAR_OPS": "[ \u64cd\u4f5c\u30d1\u30cd\u30eb ]",
        "SHOW_MINIMAP": "\u30df\u30cb\u30de\u30c3\u30d7\u3092\u8868\u793a",
        "SQUARE_MINIMAP": "\u56db\u89d2\u3044\u30df\u30cb\u30de\u30c3\u30d7\u3092\u4f7f\u7528",
        "CENTER_CAMERA": "\u30d7\u30ec\u30a4\u30e4\u30fc\u3092\u753b\u9762\u4e2d\u592e\u306b",
        "NETHER_WARNING": "\u3010 \u30cd\u30b6\u30fc\u306e\u78c1\u5834\u304c\u5f37\u3059\u304e\u308b\u305f\u3081\u3001\u30de\u30c3\u30d7\u3092\u63cf\u753b\u3067\u304d\u307e\u305b\u3093 \u3011",
        "COMPASS_N": "\u5317",
        "COMPASS_S": "\u5357",
        "COMPASS_E": "\u6771",
        "COMPASS_W": "\u897f",
        "CONTEXT_TITLE": "\u64cd\u4f5c\u3092\u9078\u629e",
        "CHUNK_POS": "\u30c1\u30e3\u30f3\u30af: (%d, %d)",
        "BLOCK_POS": "\u5ea7\u6a19: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u5ea7\u6a19\u3092\u30b3\u30d4\u30fc",
        "CREATE_WAYPOINT": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u3092\u4f5c\u6210",
        "TELEPORT_HERE": "\u3053\u3053\u306b\u30c6\u30ec\u30dd\u30fc\u30c8",
        "OPEN_WP_MENU": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u30e1\u30cb\u30e5\u30fc\u3092\u958b\u304f",
        "RENAME_WP": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u306e\u540d\u524d\u5909\u66f4",
        "DELETE_WP": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u3092\u524a\u9664",
        "TELEPORT_WP": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u3078\u30c6\u30ec\u30dd\u30fc\u30c8",
        "WP_MANAGER_TITLE": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u7ba1\u7406 ( 'U' \u30ad\u30fc\u307e\u305f\u306f 'Esc' \u3067\u9589\u3058\u308b)##WP",
        "SEARCH_HINT": "\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8\u540d\u3092\u5165\u529b\u3057\u3066\u691c\u7d22...",
        "NEW_WP_BUTTON": " + \u65b0\u898f\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8",
        "NEW_WP_TITLE": "\u65b0\u898f\u30a6\u30a7\u30a4\u30dd\u30a4\u30f3\u30c8##Popup",
        "WP_LIST_SHOW": "\u8868\u793a",
        "WP_LIST_RENAME": "\u540d\u524d\u5909\u66f4",
        "WP_LIST_TELEPORT": "\u30c6\u30ec\u30dd\u30fc\u30c8",
        "WP_LIST_DELETE": "\u524a\u9664",
        "WP_SAVE": "\u4fdd\u5b58",
        "WP_CANCEL": "\u30ad\u30e3\u30f3\u30bb\u30eb",
        "WP_NAME": "名前",
        "WP_COLOR": "色",
        "WP_DEFAULT_NAME": "\u65b0\u898f\u30dd\u30a4\u30f3\u30c8",
        "LANG_SELECT": "\u8a00\u8a9e",
        "LOG_VERSION_MISMATCH": "\u3010\u91cd\u5927\u306a\u30a8\u30e9\u30fc\u3011\u30b2\u30fc\u30e0\u306e\u30d0\u30fc\u30b8\u30e7\u30f3\u304c\u4e00\u81f4\u3057\u307e\u305b\u3093\uff01\u73fe\u5728\u306e\u30af\u30e9\u30a4\u30a2\u30f3\u30c8\u30d0\u30fc\u30b8\u30e7\u30f3",
        "LOG_VERSION_STRICT": "\u3010\u91cd\u5927\u306a\u30a8\u30e9\u30fc\u3011ChiyanMap\u306f\u30d0\u30fc\u30b8\u30e7\u30f3 1.26.20.04 \u306e\u307f\u3092\u53b3\u5bc6\u306b\u30b5\u30dd\u30fc\u30c8\u3057\u3066\u3044\u307e\u3059\uff01",
        "LOG_VERSION_ABORT": "\u3010\u91cd\u5927\u306a\u30a8\u30e9\u30fc\u3011Access Violation\u306e\u30af\u30e9\u30c3\u30b7\u30e5\u3092\u9632\u3050\u305f\u3081\u3001Mod\u306e\u30ed\u30fc\u30c9\u3092\u4e2d\u6b62\u3057\u307e\u3057\u305f\u3002",
        "LOG_VERSION_PASS": "\u30b2\u30fc\u30e0\u30af\u30e9\u30a4\u30a2\u30f3\u30c8\u306e\u30d0\u30fc\u30b8\u30e7\u30f3\u78ba\u8a8d\u3092\u901a\u904e\u3057\u307e\u3057\u305f",
        "LOG_VERSION_UNKNOWN": "\u30b2\u30fc\u30e0\u306e\u5b9f\u884c\u30d5\u30a1\u30a4\u30eb\u30d0\u30fc\u30b8\u30e7\u30f3\u3092\u8b58\u5225\u3067\u304d\u307e\u305b\u3093\u3002\u5f37\u5236\u30ed\u30fc\u30c9\u3092\u8a66\u307f\u307e\u3059...",
        "CAVE_SETTINGS": "洞窟マップ設定",
        "CAVE_MODE_OFF": "オフ",
        "CAVE_MODE_LAYERED": "オン",
        "CAVE_MODE_TYPE": "洞窟モード",
        "CAVE_MODE_DESC": "洞窟マップの描画モードを選択",
        "CAVE_ACTIVE": "洞穴モード：オン",
        "CAVE_DEPTH": "描画深度",
        "CAVE_DEPTH_DESC": "下方向に描画する層数",
        "CAVE_INACTIVE": "洞穴モード：オフ",
        "CAVE_LEGIBLE": "高コントラスト",
        "CAVE_LEGIBLE_DESC": "地下の判別しにくいブロックの視認性を向上",
        "CAVE_TOP_Y": "上端高さ",
        "CAVE_TOP_Y_AUTO": "自動",
        "CAVE_TOP_Y_DESC": "自動モードでは、上端高さはプレイヤーの現在層から計算されます",
        "CAVE_TOP_Y_MANUAL": "手動",
        "CAVE_TOP_Y_MODE": "上端高さモード",
        "HOTKEY_ACTION": "アクション",
        "HOTKEY_CLEAR": "ウェイポイント消去",
        "HOTKEY_DISABLED": "無効",
        "HOTKEY_KEY": "キー",
        "HOTKEY_OPEN_BIGMAP": "大型マップを開く",
        "HOTKEY_OPEN_WPMGR": "ウェイポイント管理を開く",
        "HOTKEY_RESET": "表示リセット",
        "HOTKEY_RESET_ALL": "すべてリセット",
        "HOTKEY_SETTINGS": "ショートカット",
        "HOTKEY_SETTINGS_TITLE": "ショートカット設定",
        "HOTKEY_STATUS_CLEARED": "ウェイポイントを消去しました",
        "HOTKEY_STATUS_RESET": "表示をリセットしました",
        "HOTKEY_STATUS_UNDONE": "元に戻しました",
        "HOTKEY_TOGGLE_MINIMAP": "ミニマップ切替",
        "HOTKEY_TOGGLE_ROTATION": "回転切替",
        "HOTKEY_TOGGLE_SHAPE": "形状切替",
        "HOTKEY_UNDO": "元に戻す",
        "MINIMAP_ZOOM_RADIUS": "ズーム半径",
        "RESET": "リセット",
        "SHOW_WAYPOINTS_MINIMAP": "ミニマップにウェイポイントを表示",
        "TELEPORT_FAILED": "転送失敗",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "転送失敗：タイムアウトまたはサーバーに拒否されました",
        "TELEPORT_LOADING": "転送中…",
        "TELEPORT_LOADING_HINT": "サーバーに転送を要求しています、お待ちください",
        "TELEPORT_TIMEOUT_MSG": "転送タイムアウト：サーバーから応答がありません",
        "WP_DELETE_SELECTED": "選択を削除",
        "WP_DESELECT_ALL": "選択解除",
        "WP_SELECT_ALL": "すべて選択"
    })json"},
        {"ko", R"json({
        "BIGMAP_TITLE": "\uc9c0\uc5f0 \uc804\uccb4 \uc9c0\ub3c4 | \ubc30\uc2a8: %.1fx",
        "BIGMAP_HELP": "[\ub4dc\ub798\uadf8] \uc774\ub3d9    [\uc2a4\ud06c\ub864] \ud655\ub300/\ucd95\uc18c    [Esc] \uc9c0\ub3c4 \ub2eb\uae30",
        "CURSOR_POS": "\ucee4\uc11c \uc704\uce58: X: %d  Z: %d",
        "BIOME_LABEL": "\uc0dd\ubb3c\uad70\uacc4: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \ud50c\ub808\uc774\uc5b4 \uc0c1\ud0dc ]",
        "PLAYER_POS_X": "\ud50c\ub808\uc774\uc5b4 X: %d",
        "PLAYER_POS_Y": "\ud50c\ub808\uc774\uc5b4 Y: %d",
        "PLAYER_POS_Z": "\ud50c\ub808\uc774\uc5b4 Z: %d",
        "SIDEBAR_OPS": "[ \uc124\uc815 \ud328\ub110 ]",
        "SHOW_MINIMAP": "\ubbf8\ub2c8\ub9f6 \ud45c\uc2dc",
        "SQUARE_MINIMAP": "\uc0ac\uac01\ud615 \ubbf8\ub2c8\ub9f6 \uc0ac\uc6a9",
        "CENTER_CAMERA": "\uce74\uba54\ub77c\ub97c \ud50c\ub808\uc774\uc5b4\uc5d0 \uc911\uc559",
        "NETHER_WARNING": "\u3010 \uc9c0\uc625\uc758 \uc790\uae30\uc7a5\uc774 \ub108\ubb34 \uac15\ud574\uc11c \uc9c0\ub3c4\ub97c \uadf8\ub9b4 \uc218 \uc5c6\uc2b5\ub2c8\ub2e4 \u3011",
        "COMPASS_N": "\ubd81",
        "COMPASS_S": "\ub0a8",
        "COMPASS_E": "\ub3d9",
        "COMPASS_W": "\uc11c",
        "CONTEXT_TITLE": "\uc791\uc5c5 \uc120\ud0dd",
        "CHUNK_POS": "\uccad\ud06c: (%d, %d)",
        "BLOCK_POS": "\ube14\ub85d: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\uc88c\ud45c \ubcf5\uc0ac",
        "CREATE_WAYPOINT": "\uc6e8\uc774\ud3ec\uc778\ud2b8 \uc0dd\uc131",
        "TELEPORT_HERE": "\uc5ec\uae30\ub85c \ud154\ub808\ud3ec\ud2b8",
        "OPEN_WP_MENU": "\uc6e8\uc774\ud3ec\uc778\ud2b8 \uad00\ub9ac \uc5f4\uae30",
        "RENAME_WP": "\uc6e8\uc774\ud3ec\uc778\ud2b8 \uc774\ub984 \ubcc0\uacbd",
        "DELETE_WP": "\uc6e8\uc774\ud3ec\uc778\ud2b8 \uc0ad\uc81c",
        "TELEPORT_WP": "\uc6e8\uc774\ud3ec\uc778\ud2b8\ub85c \ud154\ub808\ud3ec\ud2b8",
        "WP_MANAGER_TITLE": "\uc6e8\uc774\ud3ec\uc778\ud2b8 \uad00\ub9ac\uc790 ('U' \ub610\ub294 'Esc' \ub85c \ub2eb\uae30)##WP",
        "SEARCH_HINT": "\uc774\ub984\uc744 \uc785\ub825\ud558\uc5ec \uac80\uc0c9...",
        "NEW_WP_BUTTON": " + \uc0c8 \uc6e8\uc774\ud3ec\uc778\ud2b8",
        "NEW_WP_TITLE": "\uc0c8 \uc6e8\uc774\ud3ec\uc778\ud2b8##Popup",
        "WP_LIST_SHOW": "\ud45c\uc2dc",
        "WP_LIST_RENAME": "\uc774\ub984 \ubcc0\uacbd",
        "WP_LIST_TELEPORT": "\ud154\ub808\ud3ec\ud2b8",
        "WP_LIST_DELETE": "\uc0ad\uc81c",
        "WP_SAVE": "\uc800\uc7a5",
        "WP_CANCEL": "\ucde8\uc18c",
        "WP_NAME": "이름",
        "WP_COLOR": "색",
        "WP_DEFAULT_NAME": "\uc0c8 \uc704\uce58",
        "LANG_SELECT": "\uc5b8\uc5b4",
        "LOG_VERSION_MISMATCH": "[\uce58\uba85\uc801 \uc624\ub958] \uac8c\uc784 \ubc84\uc804 \ubd88\uc77c\uce58! \ud604\uc7ac \ud074\ub77c\uc774\uc5b8\ud2b8 \ubc84\uc804",
        "LOG_VERSION_STRICT": "[\uce58\uba85\uc801 \uc624\ub958] ChiyanMap\uc740 1.26.20.04 \ubc84\uc804\ub9cc \uc5c4\uaca9\ud558\uac8c \uc9c0\uc6d0\ud569\ub2c8\ub2e4!",
        "LOG_VERSION_ABORT": "[\uce58\uba85\uc801 \uc624\ub958] Access Violation \ucda9\ub3cc\uc744 \ubc29\uc9c0\ud558\uae30 \uc704\ud574 \ubaa8\ub4dc \ub85c\ub4dc\uac00 \uc911\ub2e8\ub418\uc5c8\uc2b5\ub2c8\ub2e4.",
        "LOG_VERSION_PASS": "\uac8c\uc784 \ud074\ub77c\uc774\uc5b8\ud2b8 \ubc84\uc804 \uac80\uc99d \ud1b5\uacfc",
        "LOG_VERSION_UNKNOWN": "\uac8c\uc784 \uc2e4\ud589 \ud30c\uc77c \ubc84\uc804\uc744 \uc2dd\ubcc4\ud560 \uc218 \uc5c6\uc2b5\ub2c8\ub2e4. \uac15\uc81c \ub85c\ub4dc\ub97c \uc2dc\ub3c4\ud569\ub2c8\ub2e4...",
        "CAVE_SETTINGS": "동굴 지도 설정",
        "CAVE_MODE_OFF": "끄기",
        "CAVE_MODE_LAYERED": "켜짐",
        "CAVE_MODE_TYPE": "동굴 모드",
        "CAVE_MODE_DESC": "동굴 지도 렌더링 모드 선택",
        "CAVE_ACTIVE": "동굴 모드: 켜짐",
        "CAVE_DEPTH": "표시 깊이",
        "CAVE_DEPTH_DESC": "아래로 렌더링되는 층 수",
        "CAVE_INACTIVE": "동굴 모드: 꺼짐",
        "CAVE_LEGIBLE": "고대비",
        "CAVE_LEGIBLE_DESC": "지하의 불분명한 블록 가독성 향상",
        "CAVE_TOP_Y": "상단 높이",
        "CAVE_TOP_Y_AUTO": "자동",
        "CAVE_TOP_Y_DESC": "자동 모드에서는 상단 높이가 플레이어의 현재 층에서 계산됩니다",
        "CAVE_TOP_Y_MANUAL": "수동",
        "CAVE_TOP_Y_MODE": "상단 높이 모드",
        "HOTKEY_ACTION": "동작",
        "HOTKEY_CLEAR": "위치 점 지우기",
        "HOTKEY_DISABLED": "비활성화",
        "HOTKEY_KEY": "키",
        "HOTKEY_OPEN_BIGMAP": "큰 지도 열기",
        "HOTKEY_OPEN_WPMGR": "위치 점 관리 열기",
        "HOTKEY_RESET": "보기 초기화",
        "HOTKEY_RESET_ALL": "전체 초기화",
        "HOTKEY_SETTINGS": "단축키",
        "HOTKEY_SETTINGS_TITLE": "단축키 설정",
        "HOTKEY_STATUS_CLEARED": "위치 점을 지웠습니다",
        "HOTKEY_STATUS_RESET": "보기를 초기화했습니다",
        "HOTKEY_STATUS_UNDONE": "실행 취소됨",
        "HOTKEY_TOGGLE_MINIMAP": "미니맵 켜기/끄기",
        "HOTKEY_TOGGLE_ROTATION": "회전 켜기/끄기",
        "HOTKEY_TOGGLE_SHAPE": "모양 켜기/끄기",
        "HOTKEY_UNDO": "실행 취소",
        "MINIMAP_ZOOM_RADIUS": "확대 반경",
        "RESET": "초기화",
        "SHOW_WAYPOINTS_MINIMAP": "미니맵에 위치 표시",
        "TELEPORT_FAILED": "전송 실패",
        "TELEPORT_FAILED_DISMISS": "확인",
        "TELEPORT_FAILED_MSG": "전송 실패: 시간 초과 또는 서버 거부",
        "TELEPORT_LOADING": "전송 중…",
        "TELEPORT_LOADING_HINT": "서버에 전송을 요청하는 중입니다. 잠시만 기다려 주세요",
        "TELEPORT_TIMEOUT_MSG": "전송 시간 초과: 서버 응답 없음",
        "WP_DELETE_SELECTED": "선택 삭제",
        "WP_DESELECT_ALL": "선택 해제",
        "WP_SELECT_ALL": "전체 선택"
    })json"},
        {"pt_BR", R"json({
        "BIGMAP_TITLE": "Mapa Geral de Chiyan | Zoom: %.1fx",
        "BIGMAP_HELP": "[Arrastar] Mover    [Scroll] Zoom    [Esc] Fechar Mapa",
        "CURSOR_POS": "Cursor: X: %d  Z: %d",
        "BIOME_LABEL": "Bioma: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Status do Jogador ]",
        "PLAYER_POS_X": "Jogador X: %d",
        "PLAYER_POS_Y": "Jogador Y: %d",
        "PLAYER_POS_Z": "Jogador Z: %d",
        "SIDEBAR_OPS": "[ Painel de Op\u00e7\u00f5es ]",
        "SHOW_MINIMAP": "Mostrar Minimapa",
        "SQUARE_MINIMAP": "Minimapa Quadrado",
        "CENTER_CAMERA": "Centralizar C\u00e2mera no Jogador",
        "NETHER_WARNING": "[ O campo magn\u00e9tico do Nether \u00e9 muito forte para desenhar o mapa ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "L",
        "COMPASS_W": "O",
        "CONTEXT_TITLE": "Selecionar A\u00e7\u00e3o",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Bloco: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Copiar Coordenadas",
        "CREATE_WAYPOINT": "Criar Marcador",
        "TELEPORT_HERE": "Teleportar para c\u00e1",
        "OPEN_WP_MENU": "Abrir Gerenciador de Marcadores",
        "RENAME_WP": "Renomear Marcador",
        "DELETE_WP": "Excluir Marcador",
        "TELEPORT_WP": "Teleportar para Marcador",
        "WP_MANAGER_TITLE": "Gerenciador de Marcadores (Pressione 'U' ou 'Esc' para fechar)##WP",
        "SEARCH_HINT": "Digite o nome para buscar...",
        "NEW_WP_BUTTON": " + Novo Marcador",
        "NEW_WP_TITLE": "Novo Marcador##Popup",
        "WP_LIST_SHOW": "Mostrar",
        "WP_LIST_RENAME": "Renomear",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Excluir",
        "WP_SAVE": "Salvar",
        "WP_CANCEL": "Cancelar",
        "WP_NAME": "Nome",
        "WP_COLOR": "Cor",
        "WP_DEFAULT_NAME": "Novo Marcador",
        "LANG_SELECT": "Idioma",
        "LOG_VERSION_MISMATCH": "[CRÍTICO] Incompatibilidade da versão do jogo! Versão atual do cliente",
        "LOG_VERSION_STRICT": "[CRÍTICO] ChiyanMap suporta estritamente apenas a versão 1.26.20.04!",
        "LOG_VERSION_ABORT": "[CRÍTICO] Carregamento do mod abortado para evitar travamentos de Access Violation.",
        "LOG_VERSION_PASS": "Verificação da versão do cliente do jogo concluída com sucesso",
        "LOG_VERSION_UNKNOWN": "Não foi possível identificar a versão do executável do jogo, tentando forçar o carregamento...",
        "CAVE_SETTINGS": "Configurações do mapa de cavernas",
        "CAVE_MODE_OFF": "Desativado",
        "CAVE_MODE_LAYERED": "LIGADO",
        "CAVE_MODE_TYPE": "Modo caverna",
        "CAVE_MODE_DESC": "Selecione o modo de renderização do mapa de cavernas",
        "CAVE_ACTIVE": "Modo caverna: LIGADO",
        "CAVE_DEPTH": "Profundidade de renderização",
        "CAVE_DEPTH_DESC": "Número de camadas renderizadas para baixo",
        "CAVE_INACTIVE": "Modo caverna: DESLIGADO",
        "CAVE_LEGIBLE": "Alto contraste",
        "CAVE_LEGIBLE_DESC": "Melhora a legibilidade de blocos subterrâneos ambíguos",
        "CAVE_TOP_Y": "Altura superior",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "No modo automático, a altura superior é calculada a partir da camada atual do jogador",
        "CAVE_TOP_Y_MANUAL": "Manual",
        "CAVE_TOP_Y_MODE": "Modo de altura superior",
        "HOTKEY_ACTION": "Ação",
        "HOTKEY_CLEAR": "Limpar waypoints",
        "HOTKEY_DISABLED": "Desativado",
        "HOTKEY_KEY": "Tecla",
        "HOTKEY_OPEN_BIGMAP": "Abrir mapa grande",
        "HOTKEY_OPEN_WPMGR": "Abrir gerenciador de waypoints",
        "HOTKEY_RESET": "Redefinir vista",
        "HOTKEY_RESET_ALL": "Redefinir tudo",
        "HOTKEY_SETTINGS": "Atalhos",
        "HOTKEY_SETTINGS_TITLE": "Configurações de atalhos",
        "HOTKEY_STATUS_CLEARED": "Waypoints limpos",
        "HOTKEY_STATUS_RESET": "Vista redefinida",
        "HOTKEY_STATUS_UNDONE": "Desfeito",
        "HOTKEY_TOGGLE_MINIMAP": "Alternar minimapa",
        "HOTKEY_TOGGLE_ROTATION": "Alternar rotação",
        "HOTKEY_TOGGLE_SHAPE": "Alternar forma",
        "HOTKEY_UNDO": "Desfazer",
        "MINIMAP_ZOOM_RADIUS": "Raio de zoom",
        "RESET": "Redefinir",
        "SHOW_WAYPOINTS_MINIMAP": "Mostrar waypoints no minimapa",
        "TELEPORT_FAILED": "Falha no teleporte",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Falha no teleporte: tempo esgotado ou recusado pelo servidor",
        "TELEPORT_LOADING": "Teleportando…",
        "TELEPORT_LOADING_HINT": "Solicitando teleporte ao servidor, aguarde",
        "TELEPORT_TIMEOUT_MSG": "Tempo de teleporte esgotado: sem resposta do servidor",
        "WP_DELETE_SELECTED": "Excluir selecionados",
        "WP_DESELECT_ALL": "Desmarcar tudo",
        "WP_SELECT_ALL": "Selecionar tudo"
    })json"},
        {"ru", R"json({
        "BIGMAP_TITLE": "\u0411\u043e\u043b\u044c\u0448\u0430\u044f \u043a\u0430\u0440\u0442\u0430 Chiyan | \u041c\u0430\u0441\u0448\u0442\u0430\u0431: %.1fx",
        "BIGMAP_HELP": "[\u041f\u0435\u0440\u0435\u0442\u044f\u0433\u0438\u0432\u0430\u043d\u0438\u0435] \u0421\u0434\u0432\u0438\u0433    [\u041f\u0440\u043e\u043a\u0440\u0443\u0442\u043a\u0430] \u041c\u0430\u0441\u0448\u0442\u0430\u0431    [Esc] \u0417\u0430\u043a\u0440\u044b\u0442\u044c \u043a\u0430\u0440\u0442\u0443",
        "CURSOR_POS": "\u041a\u0443\u0440\u0441\u043e\u0440: X: %d  Z: %d",
        "BIOME_LABEL": "\u0411\u0438\u043e\u043c: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u0421\u0442\u0430\u0442\u0443\u0441 \u0438\u0433\u0440\u043e\u043a\u0430 ]",
        "PLAYER_POS_X": "\u0418\u0433\u0440\u043e\u043a X: %d",
        "PLAYER_POS_Y": "\u0418\u0433\u0440\u043e\u043a Y: %d",
        "PLAYER_POS_Z": "\u0418\u0433\u0440\u043e\u043a Z: %d",
        "SIDEBAR_OPS": "[ \u041d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 ]",
        "SHOW_MINIMAP": "\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u043c\u0438\u043d\u0438\u043a\u0430\u0440\u0442\u0443",
        "SQUARE_MINIMAP": "\u041a\u0432\u0430\u0434\u0440\u0430\u0442\u043d\u0430\u044f \u043c\u0438\u043d\u0438\u043a\u0430\u0440\u0442\u0430",
        "CENTER_CAMERA": "\u0426\u0435\u043d\u0442\u0440\u0438\u0440\u043e\u0432\u0430\u0442\u044c \u043d\u0430 \u0438\u0433\u0440\u043e\u043a\u0435",
        "NETHER_WARNING": "[\u041c\u0430\u0433\u043d\u0438\u0442\u043d\u043e\u0435 \u043f\u043e\u043b\u0435 \u041d\u0435\u0437\u0435\u0440\u0430 \u0441\u043b\u0438\u0448\u043a\u043e\u043c \u0441\u0438\u043b\u044c\u043d\u043e\u0435 \u0434\u043b\u044f \u043e\u0442\u0440\u0438\u0441\u043e\u0432\u043a\u0438 \u043a\u0430\u0440\u0442\u044b]",
        "COMPASS_N": "\u0421",
        "COMPASS_S": "\u042e",
        "COMPASS_E": "\u0412",
        "COMPASS_W": "\u0417",
        "CONTEXT_TITLE": "\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u0434\u0435\u0439\u0441\u0442\u0432\u0438\u0435",
        "CHUNK_POS": "\u0427\u0430\u043d\u043a: (%d, %d)",
        "BLOCK_POS": "\u0411\u043b\u043e\u043a: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u041a\u043e\u043f\u0438\u0440\u043e\u0432\u0430\u0442\u044c \u043a\u043e\u043e\u0440\u0434\u0438\u043d\u0430\u0442\u044b",
        "CREATE_WAYPOINT": "\u0421\u043e\u0437\u0434\u0430\u0442\u044c \u0442\u043e\u0447\u043a\u0443 \u043f\u0443\u0442\u0438",
        "TELEPORT_HERE": "\u0422\u0435\u043b\u0435\u043f\u043e\u0440\u0442\u0438\u0440\u043e\u0432\u0430\u0442\u044c\u0441\u044f \u0441\u044e\u0434\u0430",
        "OPEN_WP_MENU": "\u041e\u0442\u043a\u0440\u044b\u0442\u044c \u0441\u043f\u0438\u0441\u043e\u043a \u0442\u043e\u0447\u0435\u043a \u043f\u0443\u0442\u0438",
        "RENAME_WP": "\u041f\u0435\u0440\u0435\u0438\u043c\u0435\u043d\u043e\u0432\u0430\u0442\u044c \u0442\u043e\u0447\u043a\u0443 \u043f\u0443\u0442\u0438",
        "DELETE_WP": "\u0423\u0434\u0430\u043b\u0438\u0442\u044c \u0442\u043e\u0447\u043a\u0443 \u043f\u0443\u0442\u0438",
        "TELEPORT_WP": "\u0422\u0435\u043b\u0435\u043f\u043e\u0440\u0442 \u043a \u0442\u043e\u0447\u043a\u0435 \u043f\u0443\u0442\u0438",
        "WP_MANAGER_TITLE": "\u041c\u0435\u043d\u0435\u0434\u0436\u0435\u0440 \u0442\u043e\u0447\u0435\u043a \u043f\u0443\u0442\u0438 (\u041d\u0430\u0436\u043c\u0438\u0442\u0435 'U' \u0438\u043b\u0438 'Esc' \u0434\u043b\u044f \u0437\u0430\u043a\u0440\u044b\u0442\u0438\u044f)##WP",
        "SEARCH_HINT": "\u0412\u0432\u0435\u0434\u0438\u0442\u0435 \u0438\u043c\u044f \u0434\u043b\u044f \u043f\u043e\u0438\u0441\u043a\u0430...",
        "NEW_WP_BUTTON": " + \u041d\u043e\u0432\u0430\u044f \u0442\u043e\u0447\u043a\u0430",
        "NEW_WP_TITLE": "\u041d\u043e\u0432\u0430\u044f \u0442\u043e\u0447\u043a\u0430 \u043f\u0443\u0442\u0438##Popup",
        "WP_LIST_SHOW": "\u041f\u043e\u043a\u0430\u0437\u0430\u0442\u044c",
        "WP_LIST_RENAME": "\u041f\u0435\u0440\u0435\u0438\u043c\u0435\u043d\u043e\u0432\u0430\u0442\u044c",
        "WP_LIST_TELEPORT": "\u0422\u041f",
        "WP_LIST_DELETE": "\u0423\u0434\u0430\u043b\u0438\u0442\u044c",
        "WP_SAVE": "\u0421\u043e\u0445\u0440\u0430\u043d\u0438\u0442\u044c",
        "WP_CANCEL": "\u041e\u0442\u043c\u0435\u043d\u0430",
        "WP_NAME": "Имя",
        "WP_COLOR": "Цвет",
        "WP_DEFAULT_NAME": "\u041d\u043e\u0432\u0430\u044f \u0442\u043e\u0447\u043a\u0430",
        "LANG_SELECT": "\u042f\u0437\u044b\u043a",
        "LOG_VERSION_MISMATCH": "[\u041a\u0420\u0418\u0422\u0418\u0427\u0415\u0421\u041a\u0410\u042f \u041e\u0428\u0418\u0411\u041a\u0410] \u041d\u0435\u0441\u043e\u0432\u043f\u0430\u0434\u0435\u043d\u0438\u0435 \u0432\u0435\u0440\u0441\u0438\u0438 \u0438\u0433\u0440\u044b! \u0422\u0435\u043a\u0443\u0449\u0430\u044f \u0432\u0435\u0440\u0441\u0438\u044f \u043a\u043b\u0438\u0435\u043d\u0442\u0430",
        "LOG_VERSION_STRICT": "[\u041a\u0420\u0418\u0422\u0418\u0427\u0415\u0421\u041a\u0410\u042f \u041e\u0428\u0418\u0411\u041a\u0410] ChiyanMap \u0441\u0442\u0440\u043e\u0433\u043e \u043f\u043e\u0434\u0434\u0435\u0440\u0436\u0438\u0432\u0430\u0435\u0442 \u0442\u043e\u043b\u044c\u043a\u043e \u0432\u0435\u0440\u0441\u0438\u044e 1.26.20.04!",
        "LOG_VERSION_ABORT": "[\u041a\u0420\u0418\u0422\u0418\u0427\u0415\u0421\u041a\u0410\u042f \u041e\u0428\u0418\u0411\u041a\u0410] \u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430 \u043c\u043e\u0434\u0430 \u043f\u0440\u0435\u0440\u0432\u0430\u043d\u0430 \u0434\u043b\u044f \u043f\u0440\u0435\u0434\u043e\u0442\u0432\u0440\u0430\u0449\u0435\u043d\u0438\u044f \u0441\u0431\u043e\u0435\u0432 (Access Violation).",
        "LOG_VERSION_PASS": "\u041f\u0440\u043e\u0432\u0435\u0440\u043a\u0430 \u0432\u0435\u0440\u0441\u0438\u0438 \u043a\u043b\u0438\u0435\u043d\u0442\u0430 \u0438\u0433\u0440\u044b \u043f\u0440\u043e\u0439\u0434\u0435\u043d\u0430",
        "LOG_VERSION_UNKNOWN": "\u041d\u0435\u0432\u043e\u0437\u043c\u043e\u0436\u043d\u043e \u043e\u043f\u0440\u0435\u0434\u0435\u043b\u0438\u0442\u044c \u0432\u0435\u0440\u0441\u0438\u044e \u0438\u0441\u043f\u043e\u043b\u043d\u044f\u0435\u043c\u043e\u0433\u043e \u0444\u0430\u0439\u043b\u0430 \u0438\u0433\u0440\u044b, \u043f\u043e\u043f\u044b\u0442\u043a\u0430 \u043f\u0440\u0438\u043d\u0443\u0434\u0438\u0442\u0435\u043b\u044c\u043d\u043e\u0439 \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0438...",
        "CAVE_SETTINGS": "Настройки карты пещер",
        "CAVE_MODE_OFF": "Выкл.",
        "CAVE_MODE_LAYERED": "ВКЛ",
        "CAVE_MODE_TYPE": "Режим пещеры",
        "CAVE_MODE_DESC": "Выберите режим отрисовки карты пещер",
        "CAVE_ACTIVE": "Режим пещеры: ВКЛ",
        "CAVE_DEPTH": "Глубина отрисовки",
        "CAVE_DEPTH_DESC": "Количество слоёв, отрисовываемых вниз",
        "CAVE_INACTIVE": "Режим пещеры: ВЫКЛ",
        "CAVE_LEGIBLE": "Высокая контрастность",
        "CAVE_LEGIBLE_DESC": "Повышает читаемость неоднозначных подземных блоков",
        "CAVE_TOP_Y": "Верхняя высота",
        "CAVE_TOP_Y_AUTO": "Авто",
        "CAVE_TOP_Y_DESC": "В авторежиме верхняя высота вычисляется по текущему слою игрока",
        "CAVE_TOP_Y_MANUAL": "Вручную",
        "CAVE_TOP_Y_MODE": "Режим верхней высоты",
        "HOTKEY_ACTION": "Действие",
        "HOTKEY_CLEAR": "Очистить точки",
        "HOTKEY_DISABLED": "Отключено",
        "HOTKEY_KEY": "Клавиша",
        "HOTKEY_OPEN_BIGMAP": "Открыть большую карту",
        "HOTKEY_OPEN_WPMGR": "Открыть менеджер точек",
        "HOTKEY_RESET": "Сбросить вид",
        "HOTKEY_RESET_ALL": "Сбросить всё",
        "HOTKEY_SETTINGS": "Горячие клавиши",
        "HOTKEY_SETTINGS_TITLE": "Настройки горячих клавиш",
        "HOTKEY_STATUS_CLEARED": "Точки очищены",
        "HOTKEY_STATUS_RESET": "Вид сброшен",
        "HOTKEY_STATUS_UNDONE": "Отменено",
        "HOTKEY_TOGGLE_MINIMAP": "Переключить миникарту",
        "HOTKEY_TOGGLE_ROTATION": "Переключить поворот",
        "HOTKEY_TOGGLE_SHAPE": "Переключить форму",
        "HOTKEY_UNDO": "Отменить",
        "MINIMAP_ZOOM_RADIUS": "Радиус зума",
        "RESET": "Сброс",
        "SHOW_WAYPOINTS_MINIMAP": "Показывать точки на миникарте",
        "TELEPORT_FAILED": "Ошибка телепортации",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Ошибка телепортации: тайм-аут или отклонено сервером",
        "TELEPORT_LOADING": "Телепортация…",
        "TELEPORT_LOADING_HINT": "Запрос телепортации у сервера, пожалуйста, подождите",
        "TELEPORT_TIMEOUT_MSG": "Тайм-аут телепортации: нет ответа от сервера",
        "WP_DELETE_SELECTED": "Удалить выбранные",
        "WP_DESELECT_ALL": "Снять выбор",
        "WP_SELECT_ALL": "Выбрать все"
    })json"},
        {"th", R"json({
        "BIGMAP_TITLE": "\u0e41\u0e1c\u0e19\u0e17\u0e35\u0e48\u0e42\u0e25\u0e01 Chiyan | \u0e0b\u0e39\u0e21: %.1fx",
        "BIGMAP_HELP": "[\u0e25\u0e32\u0e01] \u0e40\u0e25\u0e37\u0e48\u0e2d\u0e19    [\u0e2a\u0e01\u0e34\u0e25] \u0e0b\u0e39\u0e21    [Esc] \u0e1b\u0e34\u0e14\u0e41\u0e1c\u0e19\u0e17\u0e35\u0e48",
        "CURSOR_POS": "\u0e15\u0e33\u0e41\u0e2b\u0e19\u0e48\u0e07\u0e40\u0e1b\u0e49\u0e32\u0e40\u0e25\u0e47\u0e07: X: %d  Z: %d",
        "BIOME_LABEL": "\u0e44\u0e1a\u0e42\u0e2d\u0e21: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u0e2a\u0e16\u0e32\u0e19\u0e30\u0e1c\u0e39\u0e49\u0e40\u0e25\u0e48\u0e19 ]",
        "PLAYER_POS_X": "\u0e1c\u0e39\u0e49\u0e40\u0e25\u0e48\u0e19 X: %d",
        "PLAYER_POS_Y": "\u0e1c\u0e39\u0e49\u0e40\u0e25\u0e48\u0e19 Y: %d",
        "PLAYER_POS_Z": "\u0e1c\u0e39\u0e49\u0e40\u0e25\u0e48\u0e19 Z: %d",
        "SIDEBAR_OPS": "[ \u0e41\u0e1c\u0e07\u0e15\u0e31\u0e49\u0e07\u0e04\u0e48\u0e32 ]",
        "SHOW_MINIMAP": "\u0e41\u0e2a\u0e14\u0e07\u0e41\u0e1c\u0e19\u0e17\u0e35\u0e48\u0e22\u0e48\u0e2d",
        "SQUARE_MINIMAP": "\u0e43\u0e0a\u0e49\u0e41\u0e1c\u0e19\u0e17\u0e35\u0e48\u0e22\u0e48\u0e2d\u0e2a\u0e35\u0e48\u0e40\u0e2b\u0e25\u0e35\u0e48\u0e22\u0e21",
        "CENTER_CAMERA": "\u0e40\u0e25\u0e37\u0e48\u0e2d\u0e19\u0e01\u0e25\u0e49\u0e2d\u0e07\u0e44\u0e1b\u0e17\u0e35\u0e48\u0e1c\u0e39\u0e49\u0e40\u0e25\u0e48\u0e19",
        "NETHER_WARNING": "[\u0e2a\u0e19\u0e32\u0e21\u0e41\u0e21\u0e48\u0e40\u0e2b\u0e25\u0e47\u0e01\u0e40\u0e19\u0e18\u0e40\u0e2d\u0e23\u0e4c\u0e41\u0e23\u0e07\u0e21\u0e32\u0e01 \u0e44\u0e21\u0e48\u0e2a\u0e32\u0e21\u0e32\u0e23\u0e16\u0e27\u0e32\u0e14\u0e41\u0e1c\u0e19\u0e17\u0e35\u0e48\u0e44\u0e14\u0e49]",
        "COMPASS_N": "\u0e40\u0e2b\u0e19\u0e37\u0e2d",
        "COMPASS_S": "\u0e43\u0e15\u0e49",
        "COMPASS_E": "\u0e15\u0e30\u0e27\u0e31\u0e19\u0e2d\u0e2d\u0e01",
        "COMPASS_W": "\u0e15\u0e30\u0e27\u0e31\u0e19\u0e15\u0e01",
        "CONTEXT_TITLE": "\u0e40\u0e25\u0e37\u0e2d\u0e01\u0e01\u0e32\u0e23\u0e01\u0e23\u0e30\u0e17\u0e33",
        "CHUNK_POS": "\u0e0a\u0e31\u0e48\u0e07\u0e04\u0e4c: (%d, %d)",
        "BLOCK_POS": "\u0e1a\u0e25\u0e47\u0e2d\u0e01: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u0e04\u0e31\u0e14\u0e25\u0e2d\u0e01\u0e1e\u0e34\u0e01\u0e31\u0e14",
        "CREATE_WAYPOINT": "\u0e2a\u0e23\u0e49\u0e32\u0e07\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07",
        "TELEPORT_HERE": "\u0e40\u0e17\u0e40\u0e25\u0e1e\u0e2d\u0e23\u0e4c\u0e15\u0e21\u0e32\u0e17\u0e35\u0e48\u0e19\u0e35\u0e48",
        "OPEN_WP_MENU": "\u0e40\u0e1b\u0e34\u0e14\u0e40\u0e21\u0e19\u0e39\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07",
        "RENAME_WP": "\u0e40\u0e1b\u0e25\u0e35\u0e48\u0e22\u0e19\u0e0a\u0e37\u0e48\u0e2d\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07",
        "DELETE_WP": "\u0e25\u0e1a\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07\u0e19\u0e35\u0e49",
        "TELEPORT_WP": "\u0e40\u0e17\u0e40\u0e25\u0e1e\u0e2d\u0e23\u0e4c\u0e15\u0e44\u0e1b\u0e17\u0e35\u0e48\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07",
        "WP_MANAGER_TITLE": "\u0e15\u0e31\u0e27\u0e08\u0e31\u0e14\u0e01\u0e32\u0e23\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07 (\u0e01\u0e14 'U' \u0e2b\u0e23\u0e37\u0e2d 'Esc' \u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e1b\u0e34\u0e14)##WP",
        "SEARCH_HINT": "\u0e1b\u0e49\u0e2d\u0e19\u0e0a\u0e37\u0e48\u0e2d\u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e04\u0e49\u0e19\u0e2b\u0e32...",
        "NEW_WP_BUTTON": " + \u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07\u0e43\u0e2b\u0e21\u0e48",
        "NEW_WP_TITLE": "\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07\u0e43\u0e2b\u0e21\u0e48##Popup",
        "WP_LIST_SHOW": "\u0e41\u0e2a\u0e14\u0e07",
        "WP_LIST_RENAME": "\u0e40\u0e1b\u0e25\u0e35\u0e48\u0e22\u0e19\u0e0a\u0e37\u0e48\u0e2d",
        "WP_LIST_TELEPORT": "\u0e42\u0e17\u0e40\u0e25\u0e1e\u0e2d\u0e23\u0e4c\u0e15",
        "WP_LIST_DELETE": "\u0e25\u0e1a",
        "WP_SAVE": "\u0e1a\u0e31\u0e19\u0e17\u0e36\u0e01",
        "WP_CANCEL": "\u0e22\u0e01\u0e40\u0e25\u0e34\u0e01",
        "WP_NAME": "ชื่อ",
        "WP_COLOR": "สี",
        "WP_DEFAULT_NAME": "\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07\u0e43\u0e2b\u0e21\u0e48",
        "LANG_SELECT": "\u0e20\u0e32\u0e29\u0e32",
        "LOG_VERSION_MISMATCH": "[\u0e27\u0e34\u0e01\u0e24\u0e15] \u0e40\u0e27\u0e2d\u0e23\u0e4c\u0e0a\u0e31\u0e19\u0e40\u0e01\u0e21\u0e44\u0e21\u0e48\u0e15\u0e23\u0e07\u0e01\u0e31\u0e19! \u0e40\u0e27\u0e2d\u0e23\u0e4c\u0e0a\u0e31\u0e19\u0e44\u0e04\u0e25\u0e40\u0e2d\u0e19\u0e15\u0e4c\u0e1b\u0e31\u0e08\u0e08\u0e38\u0e1a\u0e31\u0e19",
        "LOG_VERSION_STRICT": "[\u0e27\u0e34\u0e01\u0e24\u0e15] ChiyanMap \u0e23\u0e2d\u0e07\u0e23\u0e31\u0e1a\u0e40\u0e09\u0e1e\u0e32\u0e30\u0e40\u0e27\u0e2d\u0e23\u0e4c\u0e0a\u0e31\u0e19 1.26.20.04 \u0e40\u0e17\u0e48\u0e32\u0e19\u0e31\u0e49\u0e19!",
        "LOG_VERSION_ABORT": "[\u0e27\u0e34\u0e01\u0e24\u0e15] \u0e22\u0e01\u0e40\u0e25\u0e34\u0e01\u0e01\u0e32\u0e23\u0e42\u0e2b\u0e25\u0e14\u0e21\u0e2d\u0e14\u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e1b\u0e49\u0e2d\u0e07\u0e01\u0e31\u0e19\u0e01\u0e32\u0e23\u0e41\u0e04\u0e23\u0e0a\u0e41\u0e1a\u0e1a Access Violation",
        "LOG_VERSION_PASS": "\u0e1c\u0e48\u0e32\u0e19\u0e01\u0e32\u0e23\u0e15\u0e23\u0e27\u0e08\u0e2a\u0e2d\u0e1a\u0e40\u0e27\u0e2d\u0e23\u0e4c\u0e0a\u0e31\u0e19\u0e02\u0e2d\u0e07\u0e44\u0e04\u0e25\u0e40\u0e2d\u0e19\u0e15\u0e4c\u0e40\u0e01\u0e21\u0e41\u0e25\u0e49\u0e27",
        "LOG_VERSION_UNKNOWN": "\u0e44\u0e21\u0e48\u0e2a\u0e32\u0e21\u0e32\u0e23\u0e16\u0e23\u0e30\u0e1a\u0e38\u0e40\u0e27\u0e2d\u0e23\u0e4c\u0e0a\u0e31\u0e19\u0e02\u0e2d\u0e07\u0e44\u0e1f\u0e25\u0e4c\u0e1b\u0e23\u0e30\u0e21\u0e27\u0e25\u0e1c\u0e25\u0e40\u0e01\u0e21\u0e44\u0e14\u0e49 \u0e01\u0e33\u0e25\u0e31\u0e07\u0e1e\u0e22\u0e32\u0e22\u0e32\u0e21\u0e1a\u0e31\u0e07\u0e04\u0e31\u0e1a\u0e42\u0e2b\u0e25\u0e14...",
        "CAVE_SETTINGS": "การตั้งค่าแผนที่ถ้ำ",
        "CAVE_MODE_OFF": "ปิด",
        "CAVE_MODE_LAYERED": "เปิด",
        "CAVE_MODE_TYPE": "โหมดถ้ำ",
        "CAVE_MODE_DESC": "เลือกโหมดการแสดงผลแผนที่ถ้ำ",
        "CAVE_ACTIVE": "โหมดถ้ำ: เปิด",
        "CAVE_DEPTH": "ความลึกการเรนเดอร์",
        "CAVE_DEPTH_DESC": "จำนวนชั้นที่เรนเดอร์ลงไปด้านล่าง",
        "CAVE_INACTIVE": "โหมดถ้ำ: ปิด",
        "CAVE_LEGIBLE": "ความเปรียบต่างสูง",
        "CAVE_LEGIBLE_DESC": "ปรับปรุงความอ่านง่ายของบล็อกใต้ดินที่คลุมเครือ",
        "CAVE_TOP_Y": "ความสูงด้านบน",
        "CAVE_TOP_Y_AUTO": "อัตโนมัติ",
        "CAVE_TOP_Y_DESC": "ในโหมดอัตโนมัติ ความสูงด้านบนจะคำนวณจากชั้นปัจจุบันของผู้เล่น",
        "CAVE_TOP_Y_MANUAL": "ด้วยตนเอง",
        "CAVE_TOP_Y_MODE": "โหมดความสูงด้านบน",
        "HOTKEY_ACTION": "การกระทำ",
        "HOTKEY_CLEAR": "ลบเวย์พอยต์",
        "HOTKEY_DISABLED": "ปิดใช้งาน",
        "HOTKEY_KEY": "ปุ่ม",
        "HOTKEY_OPEN_BIGMAP": "เปิดแผนที่ใหญ่",
        "HOTKEY_OPEN_WPMGR": "เปิดตัวจัดการเวย์พอยต์",
        "HOTKEY_RESET": "รีเซ็ตมุมมอง",
        "HOTKEY_RESET_ALL": "รีเซ็ตทั้งหมด",
        "HOTKEY_SETTINGS": "ปุ่มลัด",
        "HOTKEY_SETTINGS_TITLE": "การตั้งค่าปุ่มลัด",
        "HOTKEY_STATUS_CLEARED": "ลบเวย์พอยต์แล้ว",
        "HOTKEY_STATUS_RESET": "รีเซ็ตมุมมองแล้ว",
        "HOTKEY_STATUS_UNDONE": "ยกเลิกการทำ",
        "HOTKEY_TOGGLE_MINIMAP": "สลับแผนที่ย่อ",
        "HOTKEY_TOGGLE_ROTATION": "สลับการหมุน",
        "HOTKEY_TOGGLE_SHAPE": "สลับรูปทรง",
        "HOTKEY_UNDO": "เลิกทำ",
        "MINIMAP_ZOOM_RADIUS": "รัศมีการซูม",
        "RESET": "รีเซ็ต",
        "SHOW_WAYPOINTS_MINIMAP": "แสดงเวย์พอยต์บนแผนที่ย่อ",
        "TELEPORT_FAILED": "การเทเลพอร์ตล้มเหลว",
        "TELEPORT_FAILED_DISMISS": "ตกลง",
        "TELEPORT_FAILED_MSG": "การเทเลพอร์ตล้มเหลว: หมดเวลาหรือถูกปฏิเสธโดยเซิร์ฟเวอร์",
        "TELEPORT_LOADING": "กำลังเทเลพอร์ต…",
        "TELEPORT_LOADING_HINT": "กำลังขอการเทเลพอร์ตจากเซิร์ฟเวอร์ โปรดรอ",
        "TELEPORT_TIMEOUT_MSG": "การเทเลพอร์ตหมดเวลา: ไม่มีการตอบกลับจากเซิร์ฟเวอร์",
        "WP_DELETE_SELECTED": "ลบที่เลือก",
        "WP_DESELECT_ALL": "ยกเลิกการเลือกทั้งหมด",
        "WP_SELECT_ALL": "เลือกทั้งหมด"
    })json"},
        {"tr", R"json({
        "BIGMAP_TITLE": "Chiyan Genel Haritasi | Yakinlastirma: %.1fx",
        "BIGMAP_HELP": "[Surukle] Kaydir    [Tekerlek] Yakinlastir    [Esc] Haritayi Kapat",
        "CURSOR_POS": "Imlec: X: %d  Z: %d",
        "BIOME_LABEL": "Biyom: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Oyuncu Durumu ]",
        "PLAYER_POS_X": "Oyuncu X: %d",
        "PLAYER_POS_Y": "Oyuncu Y: %d",
        "PLAYER_POS_Z": "Oyuncu Z: %d",
        "SIDEBAR_OPS": "[ Ayarlar Paneli ]",
        "SHOW_MINIMAP": "Mini Haritayi Goster",
        "SQUARE_MINIMAP": "Kare Mini Harita Kullan",
        "CENTER_CAMERA": "Kamerayi Oyuncuya Ortala",
        "NETHER_WARNING": "[ Nether manyetik alani harita cizmek icin cok guclu ]",
        "COMPASS_N": "K",
        "COMPASS_S": "G",
        "COMPASS_E": "D",
        "COMPASS_W": "B",
        "CONTEXT_TITLE": "Eylem Sec",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Blok: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Koordinatlari Kopyala",
        "CREATE_WAYPOINT": "Isaret Noktasi Olustur",
        "TELEPORT_HERE": "Buraya Isinlan",
        "OPEN_WP_MENU": "Isaret Noktasi Menusunu Ac",
        "RENAME_WP": "Isaret Noktasini Yeniden Adlandir",
        "DELETE_WP": "Isaret Noktasini Sil",
        "TELEPORT_WP": "Isaret Noktasina Isinlan",
        "WP_MANAGER_TITLE": "Isaret Noktasi Yoneticisi (Kapatmak icin 'U' veya 'Esc' basin)##WP",
        "SEARCH_HINT": "Aramak icin isim girin...",
        "NEW_WP_BUTTON": " + Yeni Isaret Noktasi",
        "NEW_WP_TITLE": "Yeni Isaret Noktasi##Popup",
        "WP_LIST_SHOW": "Goster",
        "WP_LIST_RENAME": "Yeniden Adlandir",
        "WP_LIST_TELEPORT": "Isinlan",
        "WP_LIST_DELETE": "Sil",
        "WP_SAVE": "Kaydet",
        "WP_CANCEL": "Iptal",
        "WP_NAME": "Ad",
        "WP_COLOR": "Renk",
        "WP_DEFAULT_NAME": "Yeni Isaret",
        "LANG_SELECT": "Dil",
        "LOG_VERSION_MISMATCH": "[KR\u0130T\u0130K] Oyun s\u00fcr\u00fcm\u00fc uyu\u015fmazl\u0131\u011f\u0131! Mevcut istemci s\u00fcr\u00fcm\u00fc",
        "LOG_VERSION_STRICT": "[KR\u0130T\u0130K] ChiyanMap kesinlikle yaln\u0131zca 1.26.20.04 s\u00fcr\u00fcm\u00fcn\u00fc destekler!",
        "LOG_VERSION_ABORT": "[KR\u0130T\u0130K] Access Violation \u00e7\u00f6kmelerini \u00f6nlemek i\u00e7in mod y\u00fcklemesi iptal edildi.",
        "LOG_VERSION_PASS": "Oyun istemcisi s\u00fcr\u00fcm do\u011frulamas\u0131 ba\u015far\u0131l\u0131",
        "LOG_VERSION_UNKNOWN": "Oyun y\u00fcr\u00fct\u00fclebilir s\u00fcr\u00fcm\u00fc tan\u0131mlanam\u0131yor, zorla y\u00fckleme deneniyor...",
        "CAVE_SETTINGS": "Mağara haritası ayarları",
        "CAVE_MODE_OFF": "Kapalı",
        "CAVE_MODE_LAYERED": "AÇIK",
        "CAVE_MODE_TYPE": "Mağara modu",
        "CAVE_MODE_DESC": "Mağara haritası işleme modunu seçin",
        "CAVE_ACTIVE": "Mağara modu: AÇIK",
        "CAVE_DEPTH": "İşleme derinliği",
        "CAVE_DEPTH_DESC": "Aşağı doğru işlenen katman sayısı",
        "CAVE_INACTIVE": "Mağara modu: KAPALI",
        "CAVE_LEGIBLE": "Yüksek kontrast",
        "CAVE_LEGIBLE_DESC": "Belirsiz yeraltı bloklarının okunabilirliğini artırın",
        "CAVE_TOP_Y": "Üst yükseklik",
        "CAVE_TOP_Y_AUTO": "Otomatik",
        "CAVE_TOP_Y_DESC": "Otomatik modda üst yükseklik, oyuncunun mevcut katmanından hesaplanır",
        "CAVE_TOP_Y_MANUAL": "Manuel",
        "CAVE_TOP_Y_MODE": "Üst yükseklik modu",
        "HOTKEY_ACTION": "Eylem",
        "HOTKEY_CLEAR": "Yol noktalarını temizle",
        "HOTKEY_DISABLED": "Devre dışı",
        "HOTKEY_KEY": "Tuş",
        "HOTKEY_OPEN_BIGMAP": "Büyük haritayı aç",
        "HOTKEY_OPEN_WPMGR": "Yol noktası yöneticisini aç",
        "HOTKEY_RESET": "Görünümü sıfırla",
        "HOTKEY_RESET_ALL": "Tümünü sıfırla",
        "HOTKEY_SETTINGS": "Kısayollar",
        "HOTKEY_SETTINGS_TITLE": "Kısayol ayarları",
        "HOTKEY_STATUS_CLEARED": "Yol noktaları temizlendi",
        "HOTKEY_STATUS_RESET": "Görünüm sıfırlandı",
        "HOTKEY_STATUS_UNDONE": "Geri alındı",
        "HOTKEY_TOGGLE_MINIMAP": "Mini haritayı aç/kapat",
        "HOTKEY_TOGGLE_ROTATION": "Döndürmeyi aç/kapat",
        "HOTKEY_TOGGLE_SHAPE": "Şekli değiştir",
        "HOTKEY_UNDO": "Geri al",
        "MINIMAP_ZOOM_RADIUS": "Yakınlaştırma yarıçapı",
        "RESET": "Sıfırla",
        "SHOW_WAYPOINTS_MINIMAP": "Mini haritada yol noktalarını göster",
        "TELEPORT_FAILED": "Işınlanma başarısız",
        "TELEPORT_FAILED_DISMISS": "Tamam",
        "TELEPORT_FAILED_MSG": "Işınlanma başarısız: sunucu tarafından zaman aşımı veya reddedildi",
        "TELEPORT_LOADING": "Işınlanıyor…",
        "TELEPORT_LOADING_HINT": "Sunucudan ışınlanma isteniyor, lütfen bekleyin",
        "TELEPORT_TIMEOUT_MSG": "Işınlanma zaman aşımı: sunucudan yanıt yok",
        "WP_DELETE_SELECTED": "Seçileni sil",
        "WP_DESELECT_ALL": "Tüm seçimi kaldır",
        "WP_SELECT_ALL": "Tümünü seç"
    })json"},
        {"uk", R"json({
        "BIGMAP_TITLE": "\u0412\u0435\u043b\u0438\u043a\u0430 \u043a\u0430\u0440\u0442\u0430 Chiyan | \u041c\u0430\u0441\u0448\u0442\u0430\u0431: %.1fx",
        "BIGMAP_HELP": "[\u041f\u0435\u0440\u0435\u0442\u044f\u0433\u0443\u0432\u0430\u043d\u043d\u044f] \u0417\u0441\u0443\u0432    [\u041f\u0440\u043e\u043a\u0440\u0443\u0442\u043a\u0430] \u041c\u0430\u0441\u0448\u0442\u0430\u0431    [Esc] \u0417\u0430\u043a\u0440\u0438\u0442\u0438 \u043a\u0430\u0440\u0442\u0443",
        "CURSOR_POS": "\u041a\u0443\u0440\u0441\u043e\u0440: X: %d  Z: %d",
        "BIOME_LABEL": "\u0411\u0456\u043e\u043c: %s",
        "SIDEBAR_PLAYER_STATUS": "[ \u0421\u0442\u0430\u0442\u0443\u0441 \u0433\u0440\u0430\u0432\u0446\u044f ]",
        "PLAYER_POS_X": "\u0413\u0440\u0430\u0432\u0435\u0446\u044c X: %d",
        "PLAYER_POS_Y": "\u0413\u0440\u0430\u0432\u0435\u0446\u044c Y: %d",
        "PLAYER_POS_Z": "\u0413\u0440\u0430\u0432\u0435\u0446\u044c Z: %d",
        "SIDEBAR_OPS": "[ \u041d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f ]",
        "SHOW_MINIMAP": "\u041f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u043c\u0456\u043d\u0456\u043a\u0430\u0440\u0442\u0443",
        "SQUARE_MINIMAP": "\u041a\u0432\u0430\u0434\u0440\u0430\u0442\u043d\u0430 \u043c\u0456\u043d\u0456\u043a\u0430\u0440\u0442\u0430",
        "CENTER_CAMERA": "\u0426\u0435\u043d\u0442\u0440\u0443\u0432\u0430\u0442\u0438 \u043d\u0430 \u0433\u0440\u0430\u0432\u0446\u0435\u0432\u0456",
        "NETHER_WARNING": "[\u041c\u0430\u0433\u043d\u0456\u0442\u043d\u0435 \u043f\u043e\u043b\u0435 \u041d\u0435\u0437\u0435\u0440\u0443 \u0437\u0430\u043d\u0430\u0434\u0442\u043e \u0441\u0438\u043b\u044c\u043d\u0435 \u0434\u043b\u044f \u043a\u0430\u0440\u0442\u0438]",
        "COMPASS_N": "\u041f\u043d",
        "COMPASS_S": "\u041f\u0434",
        "COMPASS_E": "\u0421\u0445",
        "COMPASS_W": "\u0417\u0445",
        "CONTEXT_TITLE": "\u041e\u0431\u0435\u0440\u0456\u0442\u044c \u0434\u0456\u044e",
        "CHUNK_POS": "\u0427\u0430\u043d\u043a: (%d, %d)",
        "BLOCK_POS": "\u0411\u043b\u043e\u043a: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "\u041a\u043e\u043f\u0456\u044e\u0432\u0430\u0442\u0438 \u043a\u043e\u043e\u0440\u0434\u0438\u043d\u0430\u0442\u0438",
        "CREATE_WAYPOINT": "\u0421\u0442\u0432\u043e\u0440\u0438\u0442\u0438 \u0442\u043e\u0447\u043a\u0443 \u0448\u043b\u044f\u0445\u0443",
        "TELEPORT_HERE": "\u0422\u0435\u043b\u0435\u043f\u043e\u0440\u0442\u0443\u0432\u0430\u0442\u0438\u0441\u044f \u0441\u044e\u0434\u0438",
        "OPEN_WP_MENU": "\u0412\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u043c\u0435\u043d\u0435\u0434\u0436\u0435\u0440 \u0442\u043e\u0447\u043e\u043a \u0448\u043b\u044f\u0445\u0443",
        "RENAME_WP": "\u041f\u0435\u0440\u0435\u0439\u043c\u0435\u043d\u0443\u0432\u0430\u0442\u0438 \u0442\u043e\u0447\u043a\u0443 \u0448\u043b\u044f\u0445\u0443",
        "DELETE_WP": "\u0412\u0438\u0434\u0430\u043b\u0438\u0442\u0438 \u0442\u043e\u0447\u043a\u0443 \u0448\u043b\u044f\u0445\u0443",
        "TELEPORT_WP": "\u0422\u0435\u043b\u0435\u043f\u043e\u0440\u0442 \u0434\u043e \u0442\u043e\u0447\u043a\u0438 \u0448\u043b\u044f\u0445\u0443",
        "WP_MANAGER_TITLE": "\u041c\u0435\u043d\u0435\u0434\u0436\u0435\u0440 \u0442\u043e\u0447\u043e\u043a \u0448\u043b\u044f\u0445\u0443 (\u041d\u0430\u0442\u0438\u0441\u043d\u0456\u0442\u044c 'U' \u0430\u0431\u043e 'Esc' \u0434\u043b\u044f \u0437\u0430\u043a\u0440\u0438\u0442\u0442\u044f)##WP",
        "SEARCH_HINT": "\u0412\u0432\u0435\u0434\u0456\u0442\u044c \u043d\u0430\u0437\u0432\u0443 \u0434\u043b\u044f \u043f\u043e\u0448\u0443\u043a\u0443...",
        "NEW_WP_BUTTON": " + \u041d\u043e\u0432\u0430 \u0442\u043e\u0447\u043a\u0430 \u0448\u043b\u044f\u0445\u0443",
        "NEW_WP_TITLE": "\u041d\u043e\u0432\u0430 \u0442\u043e\u0447\u043a\u0430 \u0448\u043b\u044f\u0445\u0443##Popup",
        "WP_LIST_SHOW": "\u041f\u043e\u043a\u0430\u0437\u0430\u0442\u0438",
        "WP_LIST_RENAME": "\u0417\u043c\u0456\u043d\u0438\u0442\u0438 \u043d\u0430\u0437\u0432\u0443",
        "WP_LIST_TELEPORT": "\u0422\u041f",
        "WP_LIST_DELETE": "\u0412\u0438\u0434\u0430\u043b\u0438\u0442\u0438",
        "WP_SAVE": "\u0417\u0431\u0435\u0440\u0435\u0433\u0442\u0438",
        "WP_CANCEL": "\u0421\u043a\u0430\u0441\u0443\u0432\u0430\u0442\u0438",
        "WP_NAME": "Ім'я",
        "WP_COLOR": "Колір",
        "WP_DEFAULT_NAME": "\u041d\u043e\u0432\u0430 \u0442\u043e\u0447\u043a\u0430",
        "LANG_SELECT": "\u041c\u043e\u0432\u0430",
        "LOG_VERSION_MISMATCH": "[\u041a\u0420\u0418\u0422\u0418\u0427\u041d\u0410 \u041f\u041e\u041c\u0418\u041b\u041a\u0410] \u041d\u0435\u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u043d\u0456\u0441\u0442\u044c \u0432\u0435\u0440\u0441\u0456\u0457 \u0433\u0440\u0438! \u041f\u043e\u0442\u043e\u0447\u043d\u0430 \u0432\u0435\u0440\u0441\u0456\u044f \u043a\u043b\u0456\u0454\u043d\u0442\u0430",
        "LOG_VERSION_STRICT": "[\u041a\u0420\u0418\u0422\u0418\u0427\u041d\u0410 \u041f\u041e\u041c\u0418\u041b\u041a\u0410] ChiyanMap \u0441\u0443\u0432\u043e\u0440\u043e \u043f\u0456\u0434\u0442\u0440\u0438\u043c\u0443\u0454 \u043b\u0438\u0448\u0435 \u0432\u0435\u0440\u0441\u0456\u044e 1.26.20.04!",
        "LOG_VERSION_ABORT": "[\u041a\u0420\u0418\u0422\u0418\u0427\u041d\u0410 \u041f\u041e\u041c\u0418\u041b\u041a\u0410] \u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f \u043c\u043e\u0434\u0443 \u043f\u0435\u0440\u0435\u0440\u0432\u0430\u043d\u043e \u0434\u043b\u044f \u0437\u0430\u043f\u043e\u0431\u0456\u0433\u0430\u043d\u043d\u044f \u0437\u0431\u043e\u044f\u043c (Access Violation).",
        "LOG_VERSION_PASS": "\u041f\u0435\u0440\u0435\u0432\u0456\u0440\u043a\u0443 \u0432\u0435\u0440\u0441\u0456\u0457 \u043a\u043b\u0456\u0454\u043d\u0442\u0430 \u0433\u0440\u0438 \u043f\u0440\u043e\u0439\u0434\u0435\u043d\u043e",
        "LOG_VERSION_UNKNOWN": "\u041d\u0435\u043c\u043e\u0436\u043b\u0438\u0432\u043e \u0432\u0438\u0437\u043d\u0430\u0447\u0438\u0442\u0438 \u0432\u0435\u0440\u0441\u0456\u044e \u0432\u0438\u043a\u043e\u043d\u0443\u0432\u0430\u043d\u043e\u0433\u043e \u0444\u0430\u0439\u043b\u0443 \u0433\u0440\u0438, \u0441\u043f\u0440\u043e\u0431\u0430 \u043f\u0440\u0438\u043c\u0443\u0441\u043e\u0432\u043e\u0433\u043e \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f...",
        "CAVE_SETTINGS": "Налаштування карти печер",
        "CAVE_MODE_OFF": "Вимк.",
        "CAVE_MODE_LAYERED": "УВІМК.",
        "CAVE_MODE_TYPE": "Режим печери",
        "CAVE_MODE_DESC": "Виберіть режим відображення карти печер",
        "CAVE_ACTIVE": "Режим печери: УВІМК.",
        "CAVE_DEPTH": "Глибина відображення",
        "CAVE_DEPTH_DESC": "Кількість шарів, що відображаються вниз",
        "CAVE_INACTIVE": "Режим печери: ВИМК.",
        "CAVE_LEGIBLE": "Висока контрастність",
        "CAVE_LEGIBLE_DESC": "Покращте читабельність неоднозначних підземних блоків",
        "CAVE_TOP_Y": "Верхня висота",
        "CAVE_TOP_Y_AUTO": "Авто",
        "CAVE_TOP_Y_DESC": "У автоматичному режимі верхня висота обчислюється з поточного шару гравця",
        "CAVE_TOP_Y_MANUAL": "Вручну",
        "CAVE_TOP_Y_MODE": "Режим верхньої висоти",
        "HOTKEY_ACTION": "Дія",
        "HOTKEY_CLEAR": "Очистити шляхові точки",
        "HOTKEY_DISABLED": "Вимкнено",
        "HOTKEY_KEY": "Клавіша",
        "HOTKEY_OPEN_BIGMAP": "Відкрити велику карту",
        "HOTKEY_OPEN_WPMGR": "Відкрити менеджер шляхових точок",
        "HOTKEY_RESET": "Скинути вигляд",
        "HOTKEY_RESET_ALL": "Скинути все",
        "HOTKEY_SETTINGS": "Гарячі клавіші",
        "HOTKEY_SETTINGS_TITLE": "Налаштування гарячих клавіш",
        "HOTKEY_STATUS_CLEARED": "Шляхові точки очищено",
        "HOTKEY_STATUS_RESET": "Вигляд скинуто",
        "HOTKEY_STATUS_UNDONE": "Скасовано",
        "HOTKEY_TOGGLE_MINIMAP": "Перемкнути міні-карту",
        "HOTKEY_TOGGLE_ROTATION": "Перемкнути обертання",
        "HOTKEY_TOGGLE_SHAPE": "Перемкнути форму",
        "HOTKEY_UNDO": "Скасувати",
        "MINIMAP_ZOOM_RADIUS": "Радіус масштабу",
        "RESET": "Скинути",
        "SHOW_WAYPOINTS_MINIMAP": "Показувати шляхові точки на міні-карті",
        "TELEPORT_FAILED": "Телепортація не вдалася",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Телепортація не вдалася: час вичерпано або відхилено сервером",
        "TELEPORT_LOADING": "Телепортація…",
        "TELEPORT_LOADING_HINT": "Запит телепортації від сервера, зачекайте",
        "TELEPORT_TIMEOUT_MSG": "Час телепортації вичерпано: немає відповіді від сервера",
        "WP_DELETE_SELECTED": "Видалити вибране",
        "WP_DESELECT_ALL": "Зняти виділення з усього",
        "WP_SELECT_ALL": "Виділити все"
    })json"},
        {"vi", R"json({
        "BIGMAP_TITLE": "B\u1ea3n \u0111\u1ed3 l\u1edbn Chiyan | Thu ph\u00f3ng: %.1fx",
        "BIGMAP_HELP": "[K\u00e9o] Di chuy\u1ec3n    [Cu\u1ed9n] Thu ph\u00f3ng    [Esc] \u0110\u00f3ng b\u1ea3n \u0111\u1ed3",
        "CURSOR_POS": "Con tr\u1ecf: X: %d  Z: %d",
        "BIOME_LABEL": "Sinh c\u1ea3nh: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Tr\u1ea1ng th\u00e1i ng\u01b0\u1eddi ch\u01a1i ]",
        "PLAYER_POS_X": "Ng\u01b0\u1eddi ch\u01a1i X: %d",
        "PLAYER_POS_Y": "Ng\u01b0\u1eddi ch\u01a1i Y: %d",
        "PLAYER_POS_Z": "Ng\u01b0\u1eddi ch\u01a1i Z: %d",
        "SIDEBAR_OPS": "[ B\u1ea3ng t\u00f9y ch\u1ecdn ]",
        "SHOW_MINIMAP": "Hi\u1ec3n th\u1ecb b\u1ea3n \u0111\u1ed3 nh\u1ecf",
        "SQUARE_MINIMAP": "S\u1eed d\u1ee5ng b\u1ea3n \u0111\u1ed3 nh\u1ecf vu\u00f4ng",
        "CENTER_CAMERA": "C\u0103n gi\u1eefa camera v\u00e0o ng\u01b0\u1eddi ch\u01a1i",
        "NETHER_WARNING": "[ T\u1eeb tr\u01b0\u1eddng Nether qu\u00e1 m\u1ea1nh \u0111\u1ec3 v\u1ebd b\u1ea3n \u0111\u1ed3 ]",
        "COMPASS_N": "B\u1eafc",
        "COMPASS_S": "Nam",
        "COMPASS_E": "\u0110\u00f4ng",
        "COMPASS_W": "T\u00e2y",
        "CONTEXT_TITLE": "Ch\u1ecdn thao t\u00e1c",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "T\u1ecda \u0111\u1ed9: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Sao ch\u00e9p t\u1ecda \u0111\u1ed9",
        "CREATE_WAYPOINT": "T\u1ea1o \u0111i\u1ec3m m\u1ed1c",
        "TELEPORT_HERE": "D\u1ecbch chuy\u1ec3n t\u1edbi \u0111\u00e2y",
        "OPEN_WP_MENU": "M\u1edf danh s\u00e1ch \u0111i\u1ec3m m\u1ed1c",
        "RENAME_WP": "\u0110\u1ed5i t\u00ean \u0111i\u1ec3m m\u1ed1c",
        "DELETE_WP": "X\u00f3a \u0111i\u1ec3m m\u1ed1c n\u00e0y",
        "TELEPORT_WP": "D\u1ecbch chuy\u1ec3n t\u1edbi \u0111i\u1ec3m m\u1ed1c",
        "WP_MANAGER_TITLE": "Qu\u1ea3n l\u00fd \u0111i\u1ec3m m\u1ed1c (Nh\u1ea5n 'U' ho\u1eb7c 'Esc' \u0111\u1ec3 \u0111\u00f3ng)##WP",
        "SEARCH_HINT": "Nh\u1eadp t\u00ean \u0111\u1ec3 t\u00ecm ki\u1ebfm...",
        "NEW_WP_BUTTON": " + T\u1ea1o \u0111i\u1ec3m m\u1ed1c m\u1edbi",
        "NEW_WP_TITLE": "T\u1ea1o \u0111i\u1ec3m m\u1ed1c m\u1edbi##Popup",
        "WP_LIST_SHOW": "Hi\u1ec3n th\u1ecb",
        "WP_LIST_RENAME": "\u0110\u1ed5i t\u00ean",
        "WP_LIST_TELEPORT": "D\u1ecbch chuy\u1ec3n",
        "WP_LIST_DELETE": "X\u00f3a",
        "WP_SAVE": "L\u01b0u",
        "WP_CANCEL": "H\u1ee7y",
        "WP_NAME": "Tên",
        "WP_COLOR": "Màu",
        "WP_DEFAULT_NAME": "M\u1ed1c m\u1edbi",
        "LANG_SELECT": "Ng\u00f4n ng\u1eef",
        "LOG_VERSION_MISMATCH": "[NGHI\u00caM TR\u1eccNG] Phi\u00ean b\u1ea3n tr\u00f2 ch\u01a1i kh\u00f4ng kh\u1edbp! Phi\u00ean b\u1ea3n m\u00e1y kh\u00e1ch hi\u1ec7n t\u1ea1i",
        "LOG_VERSION_STRICT": "[NGHI\u00caM TR\u1eccNG] ChiyanMap ch\u1ec9 h\u1ed7 tr\u1ee3 nghi\u00eam ng\u1eb7t phi\u00ean b\u1ea3n 1.26.20.04!",
        "LOG_VERSION_ABORT": "[NGHI\u00caM TR\u1eccNG] Qu\u00e1 tr\u00ecnh t\u1ea3i mod \u0111\u00e3 b\u1ecb h\u1ee7y \u0111\u1ec3 ng\u0103n s\u1ef1 c\u1ed1 Access Violation.",
        "LOG_VERSION_PASS": "\u0110\u00e3 v\u01b0\u1ee3t qua x\u00e1c minh phi\u00ean b\u1ea3n m\u00e1y kh\u00e1ch tr\u00f2 ch\u01a1i",
        "LOG_VERSION_UNKNOWN": "Kh\u00f4ng th\u1ec3 x\u00e1c \u0111\u1ecbnh phi\u00ean b\u1ea3n t\u1ec7p th\u1ef1c thi c\u1ee7a tr\u00f2 ch\u01a1i, \u0111ang c\u1ed1 g\u1eafng bu\u1ed9c t\u1ea3i...",
        "CAVE_SETTINGS": "Cài đặt bản đồ hang động",
        "CAVE_MODE_OFF": "Tắt",
        "CAVE_MODE_LAYERED": "BẬT",
        "CAVE_MODE_TYPE": "Chế độ hang động",
        "CAVE_MODE_DESC": "Chọn chế độ hiển thị bản đồ hang động",
        "CAVE_ACTIVE": "Chế độ hang động: BẬT",
        "CAVE_DEPTH": "Độ sâu hiển thị",
        "CAVE_DEPTH_DESC": "Số lớp được hiển thị xuống dưới",
        "CAVE_INACTIVE": "Chế độ hang động: TẮT",
        "CAVE_LEGIBLE": "Tương phản cao",
        "CAVE_LEGIBLE_DESC": "Cải thiện khả năng đọc của các khối ngầm không rõ ràng",
        "CAVE_TOP_Y": "Chiều cao đỉnh",
        "CAVE_TOP_Y_AUTO": "Tự động",
        "CAVE_TOP_Y_DESC": "Ở chế độ tự động, chiều cao đỉnh được tính từ lớp hiện tại của người chơi",
        "CAVE_TOP_Y_MANUAL": "Thủ công",
        "CAVE_TOP_Y_MODE": "Chế độ chiều cao đỉnh",
        "HOTKEY_ACTION": "Hành động",
        "HOTKEY_CLEAR": "Xóa điểm đường",
        "HOTKEY_DISABLED": "Đã tắt",
        "HOTKEY_KEY": "Phím",
        "HOTKEY_OPEN_BIGMAP": "Mở bản đồ lớn",
        "HOTKEY_OPEN_WPMGR": "Mở trình quản lý điểm đường",
        "HOTKEY_RESET": "Đặt lại chế độ xem",
        "HOTKEY_RESET_ALL": "Đặt lại tất cả",
        "HOTKEY_SETTINGS": "Phím tắt",
        "HOTKEY_SETTINGS_TITLE": "Cài đặt phím tắt",
        "HOTKEY_STATUS_CLEARED": "Đã xóa điểm đường",
        "HOTKEY_STATUS_RESET": "Đã đặt lại chế độ xem",
        "HOTKEY_STATUS_UNDONE": "Đã hoàn tác",
        "HOTKEY_TOGGLE_MINIMAP": "Bật/tắt bản đồ thu nhỏ",
        "HOTKEY_TOGGLE_ROTATION": "Bật/tắt xoay",
        "HOTKEY_TOGGLE_SHAPE": "Chuyển đổi hình dạng",
        "HOTKEY_UNDO": "Hoàn tác",
        "MINIMAP_ZOOM_RADIUS": "Bán kính thu phóng",
        "RESET": "Đặt lại",
        "SHOW_WAYPOINTS_MINIMAP": "Hiển thị điểm đường trên bản đồ thu nhỏ",
        "TELEPORT_FAILED": "Dịch chuyển thất bại",
        "TELEPORT_FAILED_DISMISS": "OK",
        "TELEPORT_FAILED_MSG": "Dịch chuyển thất bại: hết thời gian hoặc bị máy chủ từ chối",
        "TELEPORT_LOADING": "Đang dịch chuyển…",
        "TELEPORT_LOADING_HINT": "Đang yêu cầu dịch chuyển từ máy chủ, vui lòng chờ",
        "TELEPORT_TIMEOUT_MSG": "Dịch chuyển hết thời gian: không có phản hồi từ máy chủ",
        "WP_DELETE_SELECTED": "Xóa mục đã chọn",
        "WP_DESELECT_ALL": "Bỏ chọn tất cả",
        "WP_SELECT_ALL": "Chọn tất cả"
    })json"},
        {"es", R"json({
        "BIGMAP_TITLE": "Mapa grande de Chiyan | Zoom: %.1fx",
        "BIGMAP_HELP": "[Arrastrar] Desplazar    [Rueda] Zoom    [Esc] Cerrar mapa",
        "CURSOR_POS": "Cursor: X: %d  Z: %d",
        "BIOME_LABEL": "Bioma: %s",
        "SIDEBAR_PLAYER_STATUS": "[ Estado del jugador ]",
        "PLAYER_POS_X": "Jugador X: %d",
        "PLAYER_POS_Y": "Jugador Y: %d",
        "PLAYER_POS_Z": "Jugador Z: %d",
        "SIDEBAR_OPS": "[ Ajustes ]",
        "SHOW_MINIMAP": "Mostrar minimapa",
        "SQUARE_MINIMAP": "Minimapa cuadrado",
        "CENTER_CAMERA": "Centrar cámara en el jugador",
        "NETHER_WARNING": "[ El campo magnético del Inframundo es demasiado fuerte para dibujar el mapa ]",
        "COMPASS_N": "N",
        "COMPASS_S": "S",
        "COMPASS_E": "E",
        "COMPASS_W": "O",
        "CONTEXT_TITLE": "Seleccionar acción",
        "CHUNK_POS": "Chunk: (%d, %d)",
        "BLOCK_POS": "Bloque: X: %d, Y: %d, Z: %d",
        "COPY_COORDS": "Copiar coordenadas",
        "CREATE_WAYPOINT": "Crear waypoint",
        "TELEPORT_HERE": "Teletransportar aquí",
        "OPEN_WP_MENU": "Abrir gestor de waypoints",
        "RENAME_WP": "Renombrar waypoint",
        "DELETE_WP": "Eliminar waypoint",
        "TELEPORT_WP": "Teletransportar al waypoint",
        "WP_MANAGER_TITLE": "Gestor de waypoints (Pulsa 'U' o 'Esc' para cerrar)##WP",
        "SEARCH_HINT": "Introduce un nombre para buscar waypoints...",
        "NEW_WP_BUTTON": " + Nuevo waypoint",
        "NEW_WP_TITLE": "Nuevo waypoint##Popup",
        "WP_LIST_SHOW": "Mostrar",
        "WP_LIST_RENAME": "Renombrar",
        "WP_LIST_TELEPORT": "TP",
        "WP_LIST_DELETE": "Eliminar",
        "WP_SAVE": "Guardar",
        "WP_CANCEL": "Cancelar",
        "WP_NAME": "Nombre",
        "WP_COLOR": "Color",
        "WP_DEFAULT_NAME": "Nuevo waypoint",
        "LANG_SELECT": "Idioma",
        "LOG_VERSION_MISMATCH": "[CRÍTICO] ¡Incompatibilidad de versión del juego! Versión de cliente actual",
        "LOG_VERSION_STRICT": "[CRÍTICO] ChiyanMap solo admite la versión 1.26.20.04",
        "LOG_VERSION_ABORT": "[CRÍTICO] Carga del mod abortada para evitar fallos de Access Violation.",
        "LOG_VERSION_PASS": "Verificación de la versión del cliente superada",
        "LOG_VERSION_UNKNOWN": "No se pudo identificar la versión del ejecutable, intentando carga forzada...",
        "CAVE_SETTINGS": "Ajustes del mapa de cuevas",
        "CAVE_MODE_OFF": "Apagado",
        "CAVE_MODE_LAYERED": "ACTIVADO",
        "CAVE_MODE_TYPE": "Modo cueva",
        "CAVE_MODE_DESC": "Selecciona el modo de renderizado del mapa de cuevas",
        "CAVE_ACTIVE": "Modo cueva: ACTIVADO",
        "CAVE_DEPTH": "Profundidad de renderizado",
        "CAVE_DEPTH_DESC": "Número de capas renderizadas hacia abajo",
        "CAVE_INACTIVE": "Modo cueva: DESACTIVADO",
        "CAVE_LEGIBLE": "Alto contraste",
        "CAVE_LEGIBLE_DESC": "Mejora la legibilidad de bloques subterráneos ambiguos",
        "CAVE_TOP_Y": "Altura superior",
        "CAVE_TOP_Y_AUTO": "Auto",
        "CAVE_TOP_Y_DESC": "En modo auto, la altura superior se calcula según la capa actual del jugador",
        "CAVE_TOP_Y_MANUAL": "Manual",
        "CAVE_TOP_Y_MODE": "Modo de altura superior",
        "HOTKEY_ACTION": "Acción",
        "HOTKEY_CLEAR": "Borrar waypoints",
        "HOTKEY_DISABLED": "Desactivado",
        "HOTKEY_KEY": "Tecla",
        "HOTKEY_OPEN_BIGMAP": "Abrir mapa grande",
        "HOTKEY_OPEN_WPMGR": "Abrir gestor de waypoints",
        "HOTKEY_RESET": "Restablecer vista",
        "HOTKEY_RESET_ALL": "Restablecer todo",
        "HOTKEY_SETTINGS": "Atajos",
        "HOTKEY_SETTINGS_TITLE": "Configuración de atajos",
        "HOTKEY_STATUS_CLEARED": "Waypoints borrados",
        "HOTKEY_STATUS_RESET": "Vista restablecida",
        "HOTKEY_STATUS_UNDONE": "Deshecho",
        "HOTKEY_TOGGLE_MINIMAP": "Alternar minimapa",
        "HOTKEY_TOGGLE_ROTATION": "Alternar rotación",
        "HOTKEY_TOGGLE_SHAPE": "Alternar forma",
        "HOTKEY_UNDO": "Deshacer",
        "MINIMAP_ZOOM_RADIUS": "Radio de zoom",
        "RESET": "Restablecer",
        "SHOW_WAYPOINTS_MINIMAP": "Mostrar waypoints en el minimapa",
        "TELEPORT_FAILED": "Teletransporte fallido",
        "TELEPORT_FAILED_DISMISS": "Aceptar",
        "TELEPORT_FAILED_MSG": "Teletransporte fallido: tiempo de espera agotado o rechazado por el servidor",
        "TELEPORT_LOADING": "Teletransportando…",
        "TELEPORT_LOADING_HINT": "Solicitando teletransporte al servidor, por favor espera",
        "TELEPORT_TIMEOUT_MSG": "Tiempo de teletransporte agotado: sin respuesta del servidor",
        "WP_DELETE_SELECTED": "Eliminar seleccionados",
        "WP_DESELECT_ALL": "Deseleccionar todo",
        "WP_SELECT_ALL": "Seleccionar todo"
    })json"},
    };

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/lang");

        // Always (re)write the bundled language files so the embedded
        // translations take effect. Previously these were only written when
        // missing, which let stale on-disk files (from an older build) keep
        // shadowing the translations and show English text.
        for (const auto& [langCode, jsonContent] : g_defaultJsonFiles) {
            std::string filePath = "mods/ChiyanMap/lang/" + langCode + ".json";
            std::ofstream out(filePath, std::ios::trunc);
            if (out.is_open()) {
                out << jsonContent;
                out.close();
            }
        }

        if (auto res = ll::i18n::getInstance().load("mods/ChiyanMap/lang"); !res) {
            printf("[ChiyanMap] i18n load failed!\n");
        }

        ScanLanguages();
        LoadConfig();
    }

    void ScanLanguages() {
        g_availableLanguages.clear();
        g_availableLanguages.push_back({"en", "English"});

        std::unordered_map<std::string, std::string> knownLangs = {
            {"de", "Deutsch"},
            {"en", "English"},
            {"fr", "Fran\u00e7ais"},
            {"id", "Bahasa Indonesia"},
            {"it", "Italiano"},
            {"ja", "\u65e5\u672c\u8a9e"},
            {"ko", "\ud55c\uad6d\uc5b4"},
            {"pt_BR", "Portugu\u00eas (Brasil)"},
            {"ru", "\u0420\u0443\u0441\u0441\u043a\u0438\u0439"},
            {"th", "\u0e44\u0e17\u0e22"},
            {"tr", "T\u00fcrk\u00e7e"},
            {"uk", "\u0423\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430"},
            {"vi", "Ti\u1ebfng Vi\u1ec7t"},
            {"zh_CN", "\u7b80\u4f53\u4e2d\u6587"},
            {"zh_TW", "\u7e41\u9ad4\u4e2d\u6587"}
        };

        try {
            for (const auto& entry : std::filesystem::directory_iterator("mods/ChiyanMap/lang")) {
                if (entry.path().extension() == ".json") {
                    std::string stem = entry.path().stem().string();
                    if (knownLangs.find(stem) != knownLangs.end()) {
                        bool already = false;
                        for (const auto& p : g_availableLanguages) {
                            if (p.first == stem) { already = true; break; }
                        }
                        if (!already && stem != "en") {
                            g_availableLanguages.push_back({stem, knownLangs[stem]});
                        }
                    } else {
                        g_availableLanguages.push_back({stem, stem});
                    }
                }
            }
        } catch (...) {}
    }

    void LoadLanguage(const std::string& langCode) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_currentLanguage = langCode;
        g_translationCache.clear();
    }

    void LoadConfig() {
        std::string filePath = "mods/ChiyanMap/config.json";
        if (!std::filesystem::exists(filePath)) {
            LANGID langId = GetUserDefaultUILanguage();
            WORD primary = PRIMARYLANGID(langId);
            if (primary == LANG_CHINESE) {
                WORD sub = (WORD)(langId & 0x3ff);
                if (sub == 0x0404 || sub == 0x0c04 || sub == 0x1404) {
                    g_currentLanguage = "zh_TW";
                } else {
                    g_currentLanguage = "zh_CN";
                }
            }
            else if (primary == LANG_GERMAN) g_currentLanguage = "de";
            else if (primary == LANG_FRENCH) g_currentLanguage = "fr";
            else if (primary == LANG_INDONESIAN) g_currentLanguage = "id";
            else if (primary == LANG_ITALIAN) g_currentLanguage = "it";
            else if (primary == LANG_JAPANESE) g_currentLanguage = "ja";
            else if (primary == LANG_KOREAN) g_currentLanguage = "ko";
            else if (primary == LANG_PORTUGUESE) g_currentLanguage = "pt_BR";
            else if (primary == LANG_RUSSIAN) g_currentLanguage = "ru";
            else if (primary == LANG_THAI) g_currentLanguage = "th";
            else if (primary == LANG_TURKISH) g_currentLanguage = "tr";
            else if (primary == LANG_UKRAINIAN) g_currentLanguage = "uk";
            else if (primary == LANG_VIETNAMESE) g_currentLanguage = "vi";
            else g_currentLanguage = "en";

            SaveConfig();
            LoadLanguage(g_currentLanguage);
            return;
        }

        std::ifstream in(filePath);
        if (in.is_open()) {
            try {
                json j;
                in >> j;
                g_currentLanguage = j.value("language", "en");
                MapRenderState::showMiniMap = j.value("showMiniMap", true);
                MapRenderState::isSquareMap = j.value("isSquareMap", false);
                MapRenderState::rotateMiniMap = j.value("rotateMiniMap", false);
                MapRenderState::uiTextScale = j.value("uiTextScale", 1.0f);
                MapRenderState::miniMapScale = j.value("miniMapScale", 1.0f);
                MapRenderState::miniMapOffsetX = j.value("miniMapOffsetX", 0.0f);
                MapRenderState::miniMapOffsetY = j.value("miniMapOffsetY", 0.0f);
            } catch (...) {
                g_currentLanguage = "en";
            }
            in.close();
        }
        LoadLanguage(g_currentLanguage);
    }

    void SaveConfig() {
        std::string filePath = "mods/ChiyanMap/config.json";
        json j;
        j["language"] = g_currentLanguage;
        j["showMiniMap"] = MapRenderState::showMiniMap;
        j["isSquareMap"] = MapRenderState::isSquareMap;
        j["rotateMiniMap"] = MapRenderState::rotateMiniMap;
        j["uiTextScale"] = MapRenderState::uiTextScale;
        j["miniMapScale"] = MapRenderState::miniMapScale;
        j["miniMapOffsetX"] = MapRenderState::miniMapOffsetX;
        j["miniMapOffsetY"] = MapRenderState::miniMapOffsetY;

        std::ofstream out(filePath);
        if (out.is_open()) {
            out << j.dump(4);
            out.close();
        }
    }

    const char* GetText(const std::string& key) {
        if (key.length() >= 6 && key.substr(0, 6) == "BIOME_") {
            static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> biomeDict = {
            {"en", {{ {"BIOME_PLAINS","Plains"}, {"BIOME_DESERT","Desert"}, {"BIOME_EXTREME_HILLS","Windswept Hills"}, {"BIOME_FOREST","Forest"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Mangrove Swamp"}, {"BIOME_SWAMP","Swamp"}, {"BIOME_RIVER","River"}, {"BIOME_HELL","Nether Wastes"}, {"BIOME_THE_END","The End"}, {"BIOME_FROZEN_OCEAN","Frozen Ocean"}, {"BIOME_WARM_OCEAN","Warm Ocean"}, {"BIOME_COLD_OCEAN","Cold Ocean"}, {"BIOME_OCEAN","Ocean"}, {"BIOME_SNOWY_PLAINS","Snowy Plains"}, {"BIOME_ICE_SPIKES","Ice Spikes"}, {"BIOME_MUSHROOM","Mushroom Fields"}, {"BIOME_BEACH","Beach"}, {"BIOME_BAMBOO_JUNGLE","Bamboo Jungle"}, {"BIOME_JUNGLE","Jungle"}, {"BIOME_BIRCH_FOREST","Birch Forest"}, {"BIOME_DARK_FOREST","Dark Forest"}, {"BIOME_SAVANNA","Savanna"}, {"BIOME_MESA","Badlands"}, {"BIOME_CHERRY","Cherry Grove"}, {"BIOME_CRIMSON_FOREST","Crimson Forest"}, {"BIOME_WARPED_FOREST","Warped Forest"}, {"BIOME_SOUL_SAND_VALLEY","Soul Sand Valley"}, {"BIOME_BASALT_DELTAS","Basalt Deltas"}, {"BIOME_MEADOW","Meadow"}, {"BIOME_GROVE","Grove"}, {"BIOME_SNOWY_SLOPES","Snowy Slopes"}, {"BIOME_JAGGED_PEAKS","Jagged Peaks"}, {"BIOME_FROZEN_PEAKS","Frozen Peaks"}, {"BIOME_STONY_PEAKS","Stony Peaks"}, {"BIOME_DEEP_DARK","Deep Dark"}, {"BIOME_PALE_GARDEN","Pale Garden"}, {"BIOME_UNKNOWN","Unknown Biome"}, {"BIOME_LUKEWARM_OCEAN","Lukewarm Ocean"}, {"BIOME_DEEP_OCEAN","Deep Ocean"}, {"BIOME_DEEP_FROZEN_OCEAN","Deep Frozen Ocean"}, {"BIOME_DEEP_COLD_OCEAN","Deep Cold Ocean"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Deep Lukewarm Ocean"}, {"BIOME_DEEP_WARM_OCEAN","Deep Warm Ocean"}, {"BIOME_FROZEN_RIVER","Frozen River"}, {"BIOME_STONY_SHORE","Stony Shore"}, {"BIOME_SNOWY_BEACH","Snowy Beach"}, {"BIOME_WINDSWEPT_FOREST","Windswept Forest"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Windswept Gravelly Hills"}, {"BIOME_WINDSWEPT_SAVANNA","Windswept Savanna"}, {"BIOME_SAVANNA_PLATEAU","Savanna Plateau"}, {"BIOME_SPARSE_JUNGLE","Sparse Jungle"}, {"BIOME_SNOWY_TAIGA","Snowy Taiga"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Old Growth Pine Taiga"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Old Growth Spruce Taiga"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Old Growth Birch Forest"}, {"BIOME_FLOWER_FOREST","Flower Forest"}, {"BIOME_SUNFLOWER_PLAINS","Sunflower Plains"}, {"BIOME_ERODED_BADLANDS","Eroded Badlands"}, {"BIOME_WOODED_BADLANDS","Wooded Badlands"}, {"BIOME_LUSH_CAVES","Lush Caves"}, {"BIOME_DRIPSTONE_CAVES","Dripstone Caves"} }}},
            {"zh_CN", {{ {"BIOME_PLAINS","平原"}, {"BIOME_DESERT","沙漠"}, {"BIOME_EXTREME_HILLS","风袭丘陵"}, {"BIOME_FOREST","森林"}, {"BIOME_TAIGA","针叶林"}, {"BIOME_MANGROVE_SWAMP","红树林沼泽"}, {"BIOME_SWAMP","沼泽"}, {"BIOME_RIVER","河流"}, {"BIOME_HELL","下界荒地"}, {"BIOME_THE_END","末地"}, {"BIOME_FROZEN_OCEAN","冻洋"}, {"BIOME_WARM_OCEAN","暖水海洋"}, {"BIOME_COLD_OCEAN","冷水海洋"}, {"BIOME_OCEAN","海洋"}, {"BIOME_SNOWY_PLAINS","雪原"}, {"BIOME_ICE_SPIKES","冰刺之地"}, {"BIOME_MUSHROOM","蘑菇岛"}, {"BIOME_BEACH","沙滩"}, {"BIOME_BAMBOO_JUNGLE","竹林"}, {"BIOME_JUNGLE","丛林"}, {"BIOME_BIRCH_FOREST","桦木森林"}, {"BIOME_DARK_FOREST","黑森林"}, {"BIOME_SAVANNA","热带草原"}, {"BIOME_MESA","恶地"}, {"BIOME_CHERRY","樱花树林"}, {"BIOME_CRIMSON_FOREST","绯红森林"}, {"BIOME_WARPED_FOREST","诡异森林"}, {"BIOME_SOUL_SAND_VALLEY","灵魂沙峡谷"}, {"BIOME_BASALT_DELTAS","玄武岩三角洲"}, {"BIOME_MEADOW","草甸"}, {"BIOME_GROVE","雪林"}, {"BIOME_SNOWY_SLOPES","积雪山坡"}, {"BIOME_JAGGED_PEAKS","尖峭山峰"}, {"BIOME_FROZEN_PEAKS","冰封山峰"}, {"BIOME_STONY_PEAKS","裸岩山峰"}, {"BIOME_DEEP_DARK","深暗之域"}, {"BIOME_PALE_GARDEN","苍白之园"}, {"BIOME_UNKNOWN","未知群系"}, {"BIOME_LUKEWARM_OCEAN","温水海洋"}, {"BIOME_DEEP_OCEAN","深海"}, {"BIOME_DEEP_FROZEN_OCEAN","冰冻深海"}, {"BIOME_DEEP_COLD_OCEAN","冷水深海"}, {"BIOME_DEEP_LUKEWARM_OCEAN","温水深海"}, {"BIOME_DEEP_WARM_OCEAN","暖水深海"}, {"BIOME_FROZEN_RIVER","冻河"}, {"BIOME_STONY_SHORE","石岸"}, {"BIOME_SNOWY_BEACH","积雪沙滩"}, {"BIOME_WINDSWEPT_FOREST","风袭森林"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","风袭沙砾丘陵"}, {"BIOME_WINDSWEPT_SAVANNA","风袭热带草原"}, {"BIOME_SAVANNA_PLATEAU","热带高原"}, {"BIOME_SPARSE_JUNGLE","稀疏丛林"}, {"BIOME_SNOWY_TAIGA","积雪针叶林"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","原始松木针叶林"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","原始云杉针叶林"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","原始桦木森林"}, {"BIOME_FLOWER_FOREST","繁花森林"}, {"BIOME_SUNFLOWER_PLAINS","向日葵平原"}, {"BIOME_ERODED_BADLANDS","风蚀恶地"}, {"BIOME_WOODED_BADLANDS","疏林恶地"} , {"BIOME_LUSH_CAVES","繁茂洞穴"}, {"BIOME_DRIPSTONE_CAVES","溶洞"}}}},
            {"zh_TW", {{ {"BIOME_PLAINS","平原"}, {"BIOME_DESERT","沙漠"}, {"BIOME_EXTREME_HILLS","風蝕丘陵"}, {"BIOME_FOREST","森林"}, {"BIOME_TAIGA","針葉林"}, {"BIOME_MANGROVE_SWAMP","紅樹林沼澤"}, {"BIOME_SWAMP","沼澤"}, {"BIOME_RIVER","河流"}, {"BIOME_HELL","地獄荒原"}, {"BIOME_THE_END","終界"}, {"BIOME_FROZEN_OCEAN","寒凍海洋"}, {"BIOME_WARM_OCEAN","溫暖海洋"}, {"BIOME_COLD_OCEAN","寒冷海洋"}, {"BIOME_OCEAN","海洋"}, {"BIOME_SNOWY_PLAINS","雪原"}, {"BIOME_ICE_SPIKES","冰刺"}, {"BIOME_MUSHROOM","蘑菇地"}, {"BIOME_BEACH","沙灘"}, {"BIOME_BAMBOO_JUNGLE","竹林"}, {"BIOME_JUNGLE","叢林"}, {"BIOME_BIRCH_FOREST","樺木森林"}, {"BIOME_DARK_FOREST","黑森林"}, {"BIOME_SAVANNA","莽原"}, {"BIOME_MESA","惡地"}, {"BIOME_CHERRY","櫻花樹林"}, {"BIOME_CRIMSON_FOREST","緋紅森林"}, {"BIOME_WARPED_FOREST","扭曲森林"}, {"BIOME_SOUL_SAND_VALLEY","靈魂砂谷"}, {"BIOME_BASALT_DELTAS","玄武岩三角洲"}, {"BIOME_MEADOW","草甸"}, {"BIOME_GROVE","雪林"}, {"BIOME_SNOWY_SLOPES","雪坡"}, {"BIOME_JAGGED_PEAKS","尖峭山峰"}, {"BIOME_FROZEN_PEAKS","霜凍山峰"}, {"BIOME_STONY_PEAKS","裸岩山峰"}, {"BIOME_DEEP_DARK","深淵"}, {"BIOME_PALE_GARDEN","蒼白之園"}, {"BIOME_UNKNOWN","未知群系"}, {"BIOME_LUKEWARM_OCEAN","溫和海洋"}, {"BIOME_DEEP_OCEAN","深海"}, {"BIOME_DEEP_FROZEN_OCEAN","寒凍深海"}, {"BIOME_DEEP_COLD_OCEAN","寒冷深海"}, {"BIOME_DEEP_LUKEWARM_OCEAN","溫和深海"}, {"BIOME_DEEP_WARM_OCEAN","溫暖深海"}, {"BIOME_FROZEN_RIVER","寒凍河流"}, {"BIOME_STONY_SHORE","石岸"}, {"BIOME_SNOWY_BEACH","冰雪沙灘"}, {"BIOME_WINDSWEPT_FOREST","風蝕森林"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","風蝕礫質丘陵"}, {"BIOME_WINDSWEPT_SAVANNA","風蝕莽原"}, {"BIOME_SAVANNA_PLATEAU","莽原高地"}, {"BIOME_SPARSE_JUNGLE","稀疏叢林"}, {"BIOME_SNOWY_TAIGA","冰雪針葉林"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","原生松木針葉林"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","原生杉木針葉林"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","原生樺木森林"}, {"BIOME_FLOWER_FOREST","繁花森林"}, {"BIOME_SUNFLOWER_PLAINS","向日葵平原"}, {"BIOME_ERODED_BADLANDS","侵蝕惡地"}, {"BIOME_WOODED_BADLANDS","疏林惡地"}, {"BIOME_LUSH_CAVES","蒼鬱洞窟"}, {"BIOME_DRIPSTONE_CAVES","鐘乳石洞窟"} }}},
            {"ja", {{ {"BIOME_PLAINS","平原"}, {"BIOME_DESERT","砂漠"}, {"BIOME_EXTREME_HILLS","吹きさらしの丘"}, {"BIOME_FOREST","森林"}, {"BIOME_TAIGA","タイガ"}, {"BIOME_MANGROVE_SWAMP","マングローブの沼地"}, {"BIOME_SWAMP","沼地"}, {"BIOME_RIVER","河川"}, {"BIOME_HELL","ネザーの荒地"}, {"BIOME_THE_END","ジ・エンド"}, {"BIOME_FROZEN_OCEAN","凍った海"}, {"BIOME_WARM_OCEAN","暖かい海"}, {"BIOME_COLD_OCEAN","冷たい海"}, {"BIOME_OCEAN","海洋"}, {"BIOME_SNOWY_PLAINS","雪原"}, {"BIOME_ICE_SPIKES","氷樹"}, {"BIOME_MUSHROOM","キノコ島"}, {"BIOME_BEACH","砂浜"}, {"BIOME_BAMBOO_JUNGLE","竹林"}, {"BIOME_JUNGLE","ジャングル"}, {"BIOME_BIRCH_FOREST","シラカバの森"}, {"BIOME_DARK_FOREST","暗い森"}, {"BIOME_SAVANNA","サバンナ"}, {"BIOME_MESA","荒野"}, {"BIOME_CHERRY","サクラの林"}, {"BIOME_CRIMSON_FOREST","真紅の森"}, {"BIOME_WARPED_FOREST","歪んだ森"}, {"BIOME_SOUL_SAND_VALLEY","ソウルサンドの谷"}, {"BIOME_BASALT_DELTAS","玄武岩の三角州"}, {"BIOME_MEADOW","草地"}, {"BIOME_GROVE","林"}, {"BIOME_SNOWY_SLOPES","雪の斜面"}, {"BIOME_JAGGED_PEAKS","尖った山頂"}, {"BIOME_FROZEN_PEAKS","凍った山頂"}, {"BIOME_STONY_PEAKS","石だらけの山頂"}, {"BIOME_DEEP_DARK","ディープダーク"}, {"BIOME_PALE_GARDEN","ペールガーデン"}, {"BIOME_UNKNOWN","未知のバイオーム"}, {"BIOME_LUKEWARM_OCEAN","ぬるい海"}, {"BIOME_DEEP_OCEAN","深海"}, {"BIOME_DEEP_FROZEN_OCEAN","凍った深海"}, {"BIOME_DEEP_COLD_OCEAN","冷たい深海"}, {"BIOME_DEEP_LUKEWARM_OCEAN","ぬるい深海"}, {"BIOME_DEEP_WARM_OCEAN","暖かい深海"}, {"BIOME_FROZEN_RIVER","凍った川"}, {"BIOME_STONY_SHORE","石だらけの海岸"}, {"BIOME_SNOWY_BEACH","雪の砂浜"}, {"BIOME_WINDSWEPT_FOREST","吹きさらしの森"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","吹きさらしの砂利の丘"}, {"BIOME_WINDSWEPT_SAVANNA","吹きさらしのサバンナ"}, {"BIOME_SAVANNA_PLATEAU","サバンナの高原"}, {"BIOME_SPARSE_JUNGLE","まばらなジャングル"}, {"BIOME_SNOWY_TAIGA","雪のタイガ"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","マツの原生林"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","トウヒの原生林"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","シラカバの原生林"}, {"BIOME_FLOWER_FOREST","花の森"}, {"BIOME_SUNFLOWER_PLAINS","ヒマワリ平原"}, {"BIOME_ERODED_BADLANDS","侵食された荒野"}, {"BIOME_WOODED_BADLANDS","森のある荒野"}, {"BIOME_LUSH_CAVES","繁茂した洞窟"}, {"BIOME_DRIPSTONE_CAVES","鍾乳洞"} }}},
            {"ko", {{ {"BIOME_PLAINS","평원"}, {"BIOME_DESERT","사막"}, {"BIOME_EXTREME_HILLS","바람이 세찬 언덕"}, {"BIOME_FOREST","숲"}, {"BIOME_TAIGA","타이가"}, {"BIOME_MANGROVE_SWAMP","맹그로브 늪"}, {"BIOME_SWAMP","늪"}, {"BIOME_RIVER","강"}, {"BIOME_HELL","네더 황무지"}, {"BIOME_THE_END","엔드"}, {"BIOME_FROZEN_OCEAN","얼어붙은 바다"}, {"BIOME_WARM_OCEAN","따뜻한 바다"}, {"BIOME_COLD_OCEAN","차가운 바다"}, {"BIOME_OCEAN","바다"}, {"BIOME_SNOWY_PLAINS","눈 덮인 평원"}, {"BIOME_ICE_SPIKES","역고드름"}, {"BIOME_MUSHROOM","버섯 들판"}, {"BIOME_BEACH","해변"}, {"BIOME_BAMBOO_JUNGLE","대나무 정글"}, {"BIOME_JUNGLE","정글"}, {"BIOME_BIRCH_FOREST","자작나무 숲"}, {"BIOME_DARK_FOREST","어두운 숲"}, {"BIOME_SAVANNA","사바나"}, {"BIOME_MESA","악지"}, {"BIOME_CHERRY","벚나무 숲"}, {"BIOME_CRIMSON_FOREST","진홍빛 숲"}, {"BIOME_WARPED_FOREST","뒤틀린 숲"}, {"BIOME_SOUL_SAND_VALLEY","영혼 모래 골짜기"}, {"BIOME_BASALT_DELTAS","현무암 삼각주"}, {"BIOME_MEADOW","목초지"}, {"BIOME_GROVE","산림"}, {"BIOME_SNOWY_SLOPES","눈 덮인 비탈"}, {"BIOME_JAGGED_PEAKS","뾰족한 봉우리"}, {"BIOME_FROZEN_PEAKS","얼어붙은 봉우리"}, {"BIOME_STONY_PEAKS","돌 봉우리"}, {"BIOME_DEEP_DARK","깊은 어둠"}, {"BIOME_PALE_GARDEN","창백한 정원"}, {"BIOME_UNKNOWN","알 수 없는 생물군계"}, {"BIOME_LUKEWARM_OCEAN","미지근한 바다"}, {"BIOME_DEEP_OCEAN","깊은 바다"}, {"BIOME_DEEP_FROZEN_OCEAN","깊고 얼어붙은 바다"}, {"BIOME_DEEP_COLD_OCEAN","깊고 차가운 바다"}, {"BIOME_DEEP_LUKEWARM_OCEAN","깊고 미지근한 바다"}, {"BIOME_DEEP_WARM_OCEAN","깊고 따뜻한 바다"}, {"BIOME_FROZEN_RIVER","얼어붙은 강"}, {"BIOME_STONY_SHORE","돌 해안"}, {"BIOME_SNOWY_BEACH","눈 덮인 해변"}, {"BIOME_WINDSWEPT_FOREST","바람이 세찬 숲"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","바람이 세찬 자갈투성이 언덕"}, {"BIOME_WINDSWEPT_SAVANNA","바람이 세찬 사바나"}, {"BIOME_SAVANNA_PLATEAU","사바나 고원"}, {"BIOME_SPARSE_JUNGLE","듬성듬성한 정글"}, {"BIOME_SNOWY_TAIGA","눈 덮인 타이가"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","소나무 원시 타이가"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","가문비나무 원시 타이가"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","자작나무 원시림"}, {"BIOME_FLOWER_FOREST","꽃 숲"}, {"BIOME_SUNFLOWER_PLAINS","해바라기 평원"}, {"BIOME_ERODED_BADLANDS","침식된 악지"}, {"BIOME_WOODED_BADLANDS","나무가 우거진 악지"}, {"BIOME_LUSH_CAVES","무성한 동굴"}, {"BIOME_DRIPSTONE_CAVES","점적석 동굴"} }}},
            {"ru", {{ {"BIOME_PLAINS","Равнины"}, {"BIOME_DESERT","Пустыня"}, {"BIOME_EXTREME_HILLS","Выветренные холмы"}, {"BIOME_FOREST","Лес"}, {"BIOME_TAIGA","Тайга"}, {"BIOME_MANGROVE_SWAMP","Мангровое болото"}, {"BIOME_SWAMP","Болото"}, {"BIOME_RIVER","Река"}, {"BIOME_HELL","Пустоши Незера"}, {"BIOME_THE_END","Энд"}, {"BIOME_FROZEN_OCEAN","Замёрзший океан"}, {"BIOME_WARM_OCEAN","Тёплый океан"}, {"BIOME_COLD_OCEAN","Холодный океан"}, {"BIOME_OCEAN","Океан"}, {"BIOME_SNOWY_PLAINS","Заснеженные равнины"}, {"BIOME_ICE_SPIKES","Ледяные пики"}, {"BIOME_MUSHROOM","Грибные поля"}, {"BIOME_BEACH","Пляж"}, {"BIOME_BAMBOO_JUNGLE","Бамбуковые заросли"}, {"BIOME_JUNGLE","Джунгли"}, {"BIOME_BIRCH_FOREST","Березняк"}, {"BIOME_DARK_FOREST","Тёмный лес"}, {"BIOME_SAVANNA","Саванна"}, {"BIOME_MESA","Бесплодные земли"}, {"BIOME_CHERRY","Вишнёвая роща"}, {"BIOME_CRIMSON_FOREST","Багровый лес"}, {"BIOME_WARPED_FOREST","Искажённый лес"}, {"BIOME_SOUL_SAND_VALLEY","Долина песка душ"}, {"BIOME_BASALT_DELTAS","Базальтовые дельты"}, {"BIOME_MEADOW","Луг"}, {"BIOME_GROVE","Роща"}, {"BIOME_SNOWY_SLOPES","Заснеженные склоны"}, {"BIOME_JAGGED_PEAKS","Зубчатые вершины"}, {"BIOME_FROZEN_PEAKS","Замёрзшие вершины"}, {"BIOME_STONY_PEAKS","Каменные вершины"}, {"BIOME_DEEP_DARK","Тёмные подземелья"}, {"BIOME_PALE_GARDEN","Бледный сад"}, {"BIOME_UNKNOWN","Неизвестный биом"}, {"BIOME_LUKEWARM_OCEAN","Тепловатый океан"}, {"BIOME_DEEP_OCEAN","Глубокий океан"}, {"BIOME_DEEP_FROZEN_OCEAN","Глубокий замёрзший океан"}, {"BIOME_DEEP_COLD_OCEAN","Глубокий холодный океан"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Глубокий тепловатый океан"}, {"BIOME_DEEP_WARM_OCEAN","Глубокий тёплый океан"}, {"BIOME_FROZEN_RIVER","Замёрзшая река"}, {"BIOME_STONY_SHORE","Каменистый берег"}, {"BIOME_SNOWY_BEACH","Заснеженный пляж"}, {"BIOME_WINDSWEPT_FOREST","Выветренный лес"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Выветренные гравийные холмы"}, {"BIOME_WINDSWEPT_SAVANNA","Выветренная саванна"}, {"BIOME_SAVANNA_PLATEAU","Саванна (плато)"}, {"BIOME_SPARSE_JUNGLE","Редкие джунгли"}, {"BIOME_SNOWY_TAIGA","Заснеженная тайга"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Реликтовая сосновая тайга"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Реликтовая еловая тайга"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Реликтовый березняк"}, {"BIOME_FLOWER_FOREST","Цветочный лес"}, {"BIOME_SUNFLOWER_PLAINS","Подсолнуховые поля"}, {"BIOME_ERODED_BADLANDS","Выветренные бесплодные земли"}, {"BIOME_WOODED_BADLANDS","Лесистые бесплодные земли"}, {"BIOME_LUSH_CAVES","Заросшие пещеры"}, {"BIOME_DRIPSTONE_CAVES","Карстовые пещеры"} }}},
            {"fr", {{ {"BIOME_PLAINS","Plaines"}, {"BIOME_DESERT","Désert"}, {"BIOME_EXTREME_HILLS","Collines venteuses"}, {"BIOME_FOREST","Forêt"}, {"BIOME_TAIGA","Taïga"}, {"BIOME_MANGROVE_SWAMP","Marais à mangroves"}, {"BIOME_SWAMP","Marais"}, {"BIOME_RIVER","Rivière"}, {"BIOME_HELL","Terres désolées du Nether"}, {"BIOME_THE_END","L'End"}, {"BIOME_FROZEN_OCEAN","Océan gelé"}, {"BIOME_WARM_OCEAN","Océan chaud"}, {"BIOME_COLD_OCEAN","Océan froid"}, {"BIOME_OCEAN","Océan"}, {"BIOME_SNOWY_PLAINS","Plaines enneigées"}, {"BIOME_ICE_SPIKES","Stalagmites de glace"}, {"BIOME_MUSHROOM","Champs de champignons"}, {"BIOME_BEACH","Plage"}, {"BIOME_BAMBOO_JUNGLE","Jungle de bambous"}, {"BIOME_JUNGLE","Jungle"}, {"BIOME_BIRCH_FOREST","Forêt de bouleaux"}, {"BIOME_DARK_FOREST","Forêt sombre"}, {"BIOME_SAVANNA","Savane"}, {"BIOME_MESA","Badlands"}, {"BIOME_CHERRY","Bosquet de cerisiers"}, {"BIOME_CRIMSON_FOREST","Forêt carmin"}, {"BIOME_WARPED_FOREST","Forêt biscornue"}, {"BIOME_SOUL_SAND_VALLEY","Vallée des âmes"}, {"BIOME_BASALT_DELTAS","Deltas de basalte"}, {"BIOME_MEADOW","Prairie"}, {"BIOME_GROVE","Bosquet"}, {"BIOME_SNOWY_SLOPES","Pentes enneigées"}, {"BIOME_JAGGED_PEAKS","Pics dentelés"}, {"BIOME_FROZEN_PEAKS","Pics gelés"}, {"BIOME_STONY_PEAKS","Pics rocheux"}, {"BIOME_DEEP_DARK","Abîmes"}, {"BIOME_PALE_GARDEN","Jardin pâle"}, {"BIOME_UNKNOWN","Biome inconnu"}, {"BIOME_LUKEWARM_OCEAN","Océan tiède"}, {"BIOME_DEEP_OCEAN","Océan profond"}, {"BIOME_DEEP_FROZEN_OCEAN","Océan gelé profond"}, {"BIOME_DEEP_COLD_OCEAN","Océan froid profond"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Océan tiède profond"}, {"BIOME_DEEP_WARM_OCEAN","Océan chaud profond"}, {"BIOME_FROZEN_RIVER","Rivière gelée"}, {"BIOME_STONY_SHORE","Côte rocheuse"}, {"BIOME_SNOWY_BEACH","Plage enneigée"}, {"BIOME_WINDSWEPT_FOREST","Forêt venteuse"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Collines graveleuses venteuses"}, {"BIOME_WINDSWEPT_SAVANNA","Savane venteuse"}, {"BIOME_SAVANNA_PLATEAU","Plateau de savane"}, {"BIOME_SPARSE_JUNGLE","Jungle clairsemée"}, {"BIOME_SNOWY_TAIGA","Taïga enneigée"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Taïga ancienne de pins"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Taïga ancienne de sapins"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Forêt ancienne de bouleaux"}, {"BIOME_FLOWER_FOREST","Forêt fleurie"}, {"BIOME_SUNFLOWER_PLAINS","Plaines de tournesols"}, {"BIOME_ERODED_BADLANDS","Badlands érodées"}, {"BIOME_WOODED_BADLANDS","Badlands boisées"}, {"BIOME_LUSH_CAVES","Cavernes luxuriantes"}, {"BIOME_DRIPSTONE_CAVES","Cavernes de spéléothèmes"} }}},
            {"de", {{ {"BIOME_PLAINS","Ebene"}, {"BIOME_DESERT","Wüste"}, {"BIOME_EXTREME_HILLS","Zerzauste Hügel"}, {"BIOME_FOREST","Wald"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Mangrovensumpf"}, {"BIOME_SWAMP","Sumpf"}, {"BIOME_RIVER","Fluss"}, {"BIOME_HELL","Nether-Ödland"}, {"BIOME_THE_END","Das Ende"}, {"BIOME_FROZEN_OCEAN","Vereister Ozean"}, {"BIOME_WARM_OCEAN","Warmer Ozean"}, {"BIOME_COLD_OCEAN","Kalter Ozean"}, {"BIOME_OCEAN","Ozean"}, {"BIOME_SNOWY_PLAINS","Verschneite Ebene"}, {"BIOME_ICE_SPIKES","Eiszapfentundra"}, {"BIOME_MUSHROOM","Pilzland"}, {"BIOME_BEACH","Strand"}, {"BIOME_BAMBOO_JUNGLE","Bambusdschungel"}, {"BIOME_JUNGLE","Dschungel"}, {"BIOME_BIRCH_FOREST","Birkenwald"}, {"BIOME_DARK_FOREST","Dunkler Wald"}, {"BIOME_SAVANNA","Savanne"}, {"BIOME_MESA","Tafelberge"}, {"BIOME_CHERRY","Kirschberghain"}, {"BIOME_CRIMSON_FOREST","Karmesinwald"}, {"BIOME_WARPED_FOREST","Wirrwald"}, {"BIOME_SOUL_SAND_VALLEY","Seelensandtal"}, {"BIOME_BASALT_DELTAS","Basaltdeltas"}, {"BIOME_MEADOW","Alm"}, {"BIOME_GROVE","Berghain"}, {"BIOME_SNOWY_SLOPES","Verschneite Hänge"}, {"BIOME_JAGGED_PEAKS","Zerklüftete Gipfel"}, {"BIOME_FROZEN_PEAKS","Vereiste Gipfel"}, {"BIOME_STONY_PEAKS","Steinige Gipfel"}, {"BIOME_DEEP_DARK","Tiefes Dunkel"}, {"BIOME_PALE_GARDEN","Blasser Garten"}, {"BIOME_UNKNOWN","Unbekanntes Biom"}, {"BIOME_LUKEWARM_OCEAN","Lauwarmer Ozean"}, {"BIOME_DEEP_OCEAN","Tiefsee"}, {"BIOME_DEEP_FROZEN_OCEAN","Vereiste Tiefsee"}, {"BIOME_DEEP_COLD_OCEAN","Kalte Tiefsee"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Lauwarme Tiefsee"}, {"BIOME_DEEP_WARM_OCEAN","Warme Tiefsee"}, {"BIOME_FROZEN_RIVER","Vereister Fluss"}, {"BIOME_STONY_SHORE","Steinige Küste"}, {"BIOME_SNOWY_BEACH","Verschneiter Strand"}, {"BIOME_WINDSWEPT_FOREST","Zerzauster Wald"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Zerzauste Geröllhügel"}, {"BIOME_WINDSWEPT_SAVANNA","Zerzauste Savanne"}, {"BIOME_SAVANNA_PLATEAU","Savannenhochebene"}, {"BIOME_SPARSE_JUNGLE","Lichter Dschungel"}, {"BIOME_SNOWY_TAIGA","Verschneite Taiga"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Kiefern-Urtaiga"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Fichten-Urtaiga"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Birken-Urwald"}, {"BIOME_FLOWER_FOREST","Blumenwald"}, {"BIOME_SUNFLOWER_PLAINS","Sonnenblumenebene"}, {"BIOME_ERODED_BADLANDS","Abgetragene Tafelberge"}, {"BIOME_WOODED_BADLANDS","Bewaldete Tafelberge"}, {"BIOME_LUSH_CAVES","Üppige Höhlen"}, {"BIOME_DRIPSTONE_CAVES","Tropfsteinhöhlen"} }}},
            {"pt_BR", {{ {"BIOME_PLAINS","Planícies"}, {"BIOME_DESERT","Deserto"}, {"BIOME_EXTREME_HILLS","Colinas das ventanias"}, {"BIOME_FOREST","Floresta"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Manguezal"}, {"BIOME_SWAMP","Pântano"}, {"BIOME_RIVER","Rio"}, {"BIOME_HELL","Ruínas do Nether"}, {"BIOME_THE_END","O End"}, {"BIOME_FROZEN_OCEAN","Oceano congelado"}, {"BIOME_WARM_OCEAN","Oceano quente"}, {"BIOME_COLD_OCEAN","Oceano frio"}, {"BIOME_OCEAN","Oceano"}, {"BIOME_SNOWY_PLAINS","Planícies nevadas"}, {"BIOME_ICE_SPIKES","Picos de gelo"}, {"BIOME_MUSHROOM","Campos de cogumelos"}, {"BIOME_BEACH","Praia"}, {"BIOME_BAMBOO_JUNGLE","Selva de bambu"}, {"BIOME_JUNGLE","Selva"}, {"BIOME_BIRCH_FOREST","Floresta de bétulas"}, {"BIOME_DARK_FOREST","Floresta escura"}, {"BIOME_SAVANNA","Savana"}, {"BIOME_MESA","Terras áridas"}, {"BIOME_CHERRY","Cerejal"}, {"BIOME_CRIMSON_FOREST","Floresta carmesim"}, {"BIOME_WARPED_FOREST","Floresta distorcida"}, {"BIOME_SOUL_SAND_VALLEY","Vale das almas"}, {"BIOME_BASALT_DELTAS","Deltas de basalto"}, {"BIOME_MEADOW","Pradaria"}, {"BIOME_GROVE","Bosque"}, {"BIOME_SNOWY_SLOPES","Encostas nevadas"}, {"BIOME_JAGGED_PEAKS","Picos pontiagudos"}, {"BIOME_FROZEN_PEAKS","Picos congelados"}, {"BIOME_STONY_PEAKS","Picos rochosos"}, {"BIOME_DEEP_DARK","Profundezas sombrias"}, {"BIOME_PALE_GARDEN","Jardim pálido"}, {"BIOME_UNKNOWN","Bioma Desconhecido"}, {"BIOME_LUKEWARM_OCEAN","Oceano morno"}, {"BIOME_DEEP_OCEAN","Oceano profundo"}, {"BIOME_DEEP_FROZEN_OCEAN","Oceano congelado profundo"}, {"BIOME_DEEP_COLD_OCEAN","Oceano frio profundo"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Oceano morno profundo"}, {"BIOME_DEEP_WARM_OCEAN","Oceano quente profundo"}, {"BIOME_FROZEN_RIVER","Rio congelado"}, {"BIOME_STONY_SHORE","Costa rochosa"}, {"BIOME_SNOWY_BEACH","Praia nevada"}, {"BIOME_WINDSWEPT_FOREST","Floresta das ventanias"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Colinas de cascalho das ventanias"}, {"BIOME_WINDSWEPT_SAVANNA","Savana das ventanias"}, {"BIOME_SAVANNA_PLATEAU","Planalto de savana"}, {"BIOME_SPARSE_JUNGLE","Margem de selva"}, {"BIOME_SNOWY_TAIGA","Taiga nevada"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Taiga de árvores antigas"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Taiga de pinheiros antigos"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Floresta de bétulas antigas"}, {"BIOME_FLOWER_FOREST","Floresta de flores"}, {"BIOME_SUNFLOWER_PLAINS","Planícies de girassóis"}, {"BIOME_ERODED_BADLANDS","Terras áridas erodidas"}, {"BIOME_WOODED_BADLANDS","Terras áridas florestadas"}, {"BIOME_LUSH_CAVES","Cavernas verdejantes"}, {"BIOME_DRIPSTONE_CAVES","Cavernas de espeleotemas"} }}},
            {"it", {{ {"BIOME_PLAINS","Pianura"}, {"BIOME_DESERT","Deserto"}, {"BIOME_EXTREME_HILLS","Colline ventose"}, {"BIOME_FOREST","Foresta"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Palude di mangrovie"}, {"BIOME_SWAMP","Palude"}, {"BIOME_RIVER","Fiume"}, {"BIOME_HELL","Distese del Nether"}, {"BIOME_THE_END","L'End"}, {"BIOME_FROZEN_OCEAN","Oceano ghiacciato"}, {"BIOME_WARM_OCEAN","Oceano caldo"}, {"BIOME_COLD_OCEAN","Oceano freddo"}, {"BIOME_OCEAN","Oceano"}, {"BIOME_SNOWY_PLAINS","Pianura innevata"}, {"BIOME_ICE_SPIKES","Spuntoni di ghiaccio"}, {"BIOME_MUSHROOM","Campi fungosi"}, {"BIOME_BEACH","Spiaggia"}, {"BIOME_BAMBOO_JUNGLE","Giungla di bambù"}, {"BIOME_JUNGLE","Giungla"}, {"BIOME_BIRCH_FOREST","Foresta di betulle"}, {"BIOME_DARK_FOREST","Selva oscura"}, {"BIOME_SAVANNA","Savana"}, {"BIOME_MESA","Calanchi"}, {"BIOME_CHERRY","Boschetto di ciliegi"}, {"BIOME_CRIMSON_FOREST","Foresta cremisi"}, {"BIOME_WARPED_FOREST","Foresta distorta"}, {"BIOME_SOUL_SAND_VALLEY","Valle delle anime"}, {"BIOME_BASALT_DELTAS","Delta di basalto"}, {"BIOME_MEADOW","Prateria"}, {"BIOME_GROVE","Boschetto"}, {"BIOME_SNOWY_SLOPES","Pendii innevati"}, {"BIOME_JAGGED_PEAKS","Vette frastagliate"}, {"BIOME_FROZEN_PEAKS","Vette ghiacciate"}, {"BIOME_STONY_PEAKS","Vette rocciose"}, {"BIOME_DEEP_DARK","Abisso oscuro"}, {"BIOME_PALE_GARDEN","Giardino pallido"}, {"BIOME_UNKNOWN","Bioma sconosciuto"}, {"BIOME_LUKEWARM_OCEAN","Oceano tiepido"}, {"BIOME_DEEP_OCEAN","Oceano profondo"}, {"BIOME_DEEP_FROZEN_OCEAN","Oceano ghiacciato profondo"}, {"BIOME_DEEP_COLD_OCEAN","Oceano freddo profondo"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Oceano tiepido profondo"}, {"BIOME_DEEP_WARM_OCEAN","Oceano caldo profondo"}, {"BIOME_FROZEN_RIVER","Fiume ghiacciato"}, {"BIOME_STONY_SHORE","Costa rocciosa"}, {"BIOME_SNOWY_BEACH","Spiaggia innevata"}, {"BIOME_WINDSWEPT_FOREST","Foresta ventosa"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Colline di ghiaia ventose"}, {"BIOME_WINDSWEPT_SAVANNA","Savana ventosa"}, {"BIOME_SAVANNA_PLATEAU","Altopiano della savana"}, {"BIOME_SPARSE_JUNGLE","Giungla rada"}, {"BIOME_SNOWY_TAIGA","Taiga innevata"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Taiga di pini secolari"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Taiga di abeti secolari"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Foresta di betulle secolari"}, {"BIOME_FLOWER_FOREST","Foresta floreale"}, {"BIOME_SUNFLOWER_PLAINS","Pianura di girasoli"}, {"BIOME_ERODED_BADLANDS","Calanchi erosi"}, {"BIOME_WOODED_BADLANDS","Calanchi boscosi"}, {"BIOME_LUSH_CAVES","Caverne rigogliose"}, {"BIOME_DRIPSTONE_CAVES","Caverne di speleotemi"} }}},
            {"es", {{ {"BIOME_PLAINS","Llanura"}, {"BIOME_DESERT","Desierto"}, {"BIOME_EXTREME_HILLS","Colinas ventiscosas"}, {"BIOME_FOREST","Bosque"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Manglar"}, {"BIOME_SWAMP","Pantano"}, {"BIOME_RIVER","Río"}, {"BIOME_HELL","Desiertos del Nether"}, {"BIOME_THE_END","El End"}, {"BIOME_FROZEN_OCEAN","Océano helado"}, {"BIOME_WARM_OCEAN","Océano cálido"}, {"BIOME_COLD_OCEAN","Océano frío"}, {"BIOME_OCEAN","Océano"}, {"BIOME_SNOWY_PLAINS","Llanura nevada"}, {"BIOME_ICE_SPIKES","Picos de hielo"}, {"BIOME_MUSHROOM","Campo de champiñones"}, {"BIOME_BEACH","Playa"}, {"BIOME_BAMBOO_JUNGLE","Jungla de bambú"}, {"BIOME_JUNGLE","Jungla"}, {"BIOME_BIRCH_FOREST","Abedular"}, {"BIOME_DARK_FOREST","Bosque oscuro"}, {"BIOME_SAVANNA","Sabana"}, {"BIOME_MESA","Tierras baldías"}, {"BIOME_CHERRY","Cerezal"}, {"BIOME_CRIMSON_FOREST","Bosque carmesí"}, {"BIOME_WARPED_FOREST","Bosque distorsionado"}, {"BIOME_SOUL_SAND_VALLEY","Valle de almas"}, {"BIOME_BASALT_DELTAS","Deltas de basalto"}, {"BIOME_MEADOW","Prado"}, {"BIOME_GROVE","Abetal"}, {"BIOME_SNOWY_SLOPES","Ladera nevada"}, {"BIOME_JAGGED_PEAKS","Cumbres escarpadas"}, {"BIOME_FROZEN_PEAKS","Cumbres heladas"}, {"BIOME_STONY_PEAKS","Cumbres rocosas"}, {"BIOME_DEEP_DARK","Oscuridad profunda"}, {"BIOME_PALE_GARDEN","Jardín pálido"}, {"BIOME_UNKNOWN","Bioma desconocido"}, {"BIOME_LUKEWARM_OCEAN","Océano tibio"}, {"BIOME_DEEP_OCEAN","Océano profundo"}, {"BIOME_DEEP_FROZEN_OCEAN","Océano helado profundo"}, {"BIOME_DEEP_COLD_OCEAN","Océano frío profundo"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Océano tibio profundo"}, {"BIOME_DEEP_WARM_OCEAN","Océano cálido profundo"}, {"BIOME_FROZEN_RIVER","Río helado"}, {"BIOME_STONY_SHORE","Costa rocosa"}, {"BIOME_SNOWY_BEACH","Playa nevada"}, {"BIOME_WINDSWEPT_FOREST","Bosque ventiscoso"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Colinas pedregosas ventiscosas"}, {"BIOME_WINDSWEPT_SAVANNA","Sabana ventiscosa"}, {"BIOME_SAVANNA_PLATEAU","Meseta de sabana"}, {"BIOME_SPARSE_JUNGLE","Jungla dispersa"}, {"BIOME_SNOWY_TAIGA","Taiga nevada"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Taiga de pinos ancestral"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Taiga de abetos ancestral"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Abedular ancestral"}, {"BIOME_FLOWER_FOREST","Bosque floral"}, {"BIOME_SUNFLOWER_PLAINS","Llanura de girasoles"}, {"BIOME_ERODED_BADLANDS","Tierras baldías erosionadas"}, {"BIOME_WOODED_BADLANDS","Tierras baldías frondosas"}, {"BIOME_LUSH_CAVES","Cuevas frondosas"}, {"BIOME_DRIPSTONE_CAVES","Cuevas kársticas"} }}},
            {"id", {{ {"BIOME_PLAINS","Tanah Datar"}, {"BIOME_DESERT","Gurun"}, {"BIOME_EXTREME_HILLS","Bukit Berangin"}, {"BIOME_FOREST","Hutan"}, {"BIOME_TAIGA","Taiga"}, {"BIOME_MANGROVE_SWAMP","Rawa Bakau"}, {"BIOME_SWAMP","Rawa"}, {"BIOME_RIVER","Sungai"}, {"BIOME_HELL","Tanah Kosong Nether"}, {"BIOME_THE_END","End"}, {"BIOME_FROZEN_OCEAN","Lautan Beku"}, {"BIOME_WARM_OCEAN","Lautan Hangat"}, {"BIOME_COLD_OCEAN","Lautan Dingin"}, {"BIOME_OCEAN","Lautan"}, {"BIOME_SNOWY_PLAINS","Dataran Bersalju"}, {"BIOME_ICE_SPIKES","Paku Es"}, {"BIOME_MUSHROOM","Ladang Jamur"}, {"BIOME_BEACH","Pantai"}, {"BIOME_BAMBOO_JUNGLE","Rimba Bambu"}, {"BIOME_JUNGLE","Rimba"}, {"BIOME_BIRCH_FOREST","Hutan Betula"}, {"BIOME_DARK_FOREST","Hutan Gelap"}, {"BIOME_SAVANNA","Sabana"}, {"BIOME_MESA","Tanah Tandus"}, {"BIOME_CHERRY","Hutan Ceri"}, {"BIOME_CRIMSON_FOREST","Hutan Kirmizi"}, {"BIOME_WARPED_FOREST","Hutan Kerukut"}, {"BIOME_SOUL_SAND_VALLEY","Lembah Pasir Jiwa"}, {"BIOME_BASALT_DELTAS","Delta Basal"}, {"BIOME_MEADOW","Padang Rumput"}, {"BIOME_GROVE","Hutan Kecil"}, {"BIOME_SNOWY_SLOPES","Lereng Bersalju"}, {"BIOME_JAGGED_PEAKS","Puncak Bergigi"}, {"BIOME_FROZEN_PEAKS","Puncak Beku"}, {"BIOME_STONY_PEAKS","Puncak Berbatu"}, {"BIOME_DEEP_DARK","Gelap Dalam"}, {"BIOME_PALE_GARDEN","Kebun Pucat"}, {"BIOME_UNKNOWN","Bioma Tidak Diketahui"}, {"BIOME_LUKEWARM_OCEAN","Lautan Suam"}, {"BIOME_DEEP_OCEAN","Lautan Dalam"}, {"BIOME_DEEP_FROZEN_OCEAN","Lautan Beku Dalam"}, {"BIOME_DEEP_COLD_OCEAN","Lautan Dingin Dalam"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Lautan Suam Dalam"}, {"BIOME_DEEP_WARM_OCEAN","Lautan Hangat Dalam"}, {"BIOME_FROZEN_RIVER","Sungai Beku"}, {"BIOME_STONY_SHORE","Pesisir Berbatu"}, {"BIOME_SNOWY_BEACH","Pantai Bersalju"}, {"BIOME_WINDSWEPT_FOREST","Hutan Bukit Berangin"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Bukit Kerikil Berangin"}, {"BIOME_WINDSWEPT_SAVANNA","Sabana Berangin"}, {"BIOME_SAVANNA_PLATEAU","Plato Sabana"}, {"BIOME_SPARSE_JUNGLE","Rimba Jarang"}, {"BIOME_SNOWY_TAIGA","Taiga Bersalju"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Taiga Pinus Tua"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Taiga Cemara Tua"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Hutan Betula Tua"}, {"BIOME_FLOWER_FOREST","Hutan Bunga"}, {"BIOME_SUNFLOWER_PLAINS","Tanah Datar Kanigara"}, {"BIOME_ERODED_BADLANDS","Tanah Tandus Kikis"}, {"BIOME_WOODED_BADLANDS","Hutan Tanah Tandus"}, {"BIOME_LUSH_CAVES","Gua Rimbun"}, {"BIOME_DRIPSTONE_CAVES","Gua Batu Tetes"} }}},
            {"th", {{ {"BIOME_PLAINS","ที่ราบ"}, {"BIOME_DESERT","ทะเลทราย"}, {"BIOME_EXTREME_HILLS","เนินเขาลมแรง"}, {"BIOME_FOREST","ป่า"}, {"BIOME_TAIGA","ไทกา"}, {"BIOME_MANGROVE_SWAMP","ป่าชายเลน"}, {"BIOME_SWAMP","ที่ลุ่มน้ำขัง"}, {"BIOME_RIVER","แม่น้ำ"}, {"BIOME_HELL","เนเธอร์โล้น"}, {"BIOME_THE_END","ดิเอนด์"}, {"BIOME_FROZEN_OCEAN","มหาสมุทรแช่แข็ง"}, {"BIOME_WARM_OCEAN","มหาสมุทรอุ่น"}, {"BIOME_COLD_OCEAN","มหาสมุทรเย็น"}, {"BIOME_OCEAN","มหาสมุทร"}, {"BIOME_SNOWY_PLAINS","ที่ราบหิมะ"}, {"BIOME_ICE_SPIKES","ทุ่งน้ำแข็ง"}, {"BIOME_MUSHROOM","ทุ่งเห็ด"}, {"BIOME_BEACH","หาด"}, {"BIOME_BAMBOO_JUNGLE","ป่าไผ่"}, {"BIOME_JUNGLE","ป่าดงดิบ"}, {"BIOME_BIRCH_FOREST","ป่าเบิร์ช"}, {"BIOME_DARK_FOREST","ป่ามืด"}, {"BIOME_SAVANNA","สะวันนา"}, {"BIOME_MESA","แบดแลนด์"}, {"BIOME_CHERRY","ป่าซากุระ"}, {"BIOME_CRIMSON_FOREST","ป่าสีเลือด"}, {"BIOME_WARPED_FOREST","ป่าวิปริต"}, {"BIOME_SOUL_SAND_VALLEY","หุบเขาทรายวิญญาณ"}, {"BIOME_BASALT_DELTAS","สันดอนหินบะซอลต์"}, {"BIOME_MEADOW","ทุ่งหญ้า"}, {"BIOME_GROVE","ป่าหิมะ"}, {"BIOME_SNOWY_SLOPES","เนินลาดหิมะ"}, {"BIOME_JAGGED_PEAKS","ยอดเขาขรุขระ"}, {"BIOME_FROZEN_PEAKS","ยอดเขาแช่แข็ง"}, {"BIOME_STONY_PEAKS","ยอดเขาหิน"}, {"BIOME_DEEP_DARK","มืดสงัด"}, {"BIOME_PALE_GARDEN","สวนซีดจาง"}, {"BIOME_UNKNOWN","ไบโอมที่ไม่รู้จัก"}, {"BIOME_LUKEWARM_OCEAN","มหาสมุทรกึ่งอุ่น"}, {"BIOME_DEEP_OCEAN","มหาสมุทรลึก"}, {"BIOME_DEEP_FROZEN_OCEAN","มหาสมุทรแช่แข็งลึก"}, {"BIOME_DEEP_COLD_OCEAN","มหาสมุทรเย็นลึก"}, {"BIOME_DEEP_LUKEWARM_OCEAN","มหาสมุทรกึ่งอุ่นลึก"}, {"BIOME_DEEP_WARM_OCEAN","มหาสมุทรอุ่นลึก"}, {"BIOME_FROZEN_RIVER","แม่น้ำแช่แข็ง"}, {"BIOME_STONY_SHORE","ชายฝั่งหิน"}, {"BIOME_SNOWY_BEACH","หาดหิมะ"}, {"BIOME_WINDSWEPT_FOREST","ป่าลมแรง"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","เนินเขากรวดลมแรง"}, {"BIOME_WINDSWEPT_SAVANNA","สะวันนาลมแรง"}, {"BIOME_SAVANNA_PLATEAU","ที่ราบสูงสะวันนา"}, {"BIOME_SPARSE_JUNGLE","ป่าดงดิบเบาบาง"}, {"BIOME_SNOWY_TAIGA","ไทกาหิมะ"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","ไทกาสนเก่าแก่"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","ไทกาสนสปรูซเก่าแก่"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","ป่าเบิร์ชเก่าแก่"}, {"BIOME_FLOWER_FOREST","ป่าดอกไม้"}, {"BIOME_SUNFLOWER_PLAINS","ที่ราบทานตะวัน"}, {"BIOME_ERODED_BADLANDS","แบดแลนด์กัดกร่อน"}, {"BIOME_WOODED_BADLANDS","แบดแลนด์ป่า"}, {"BIOME_LUSH_CAVES","ถ้ำเขียวชอุ่ม"}, {"BIOME_DRIPSTONE_CAVES","ถ้ำหินหยด"} }}},
            {"tr", {{ {"BIOME_PLAINS","Ova"}, {"BIOME_DESERT","Çöl"}, {"BIOME_EXTREME_HILLS","Esintili Tepeler"}, {"BIOME_FOREST","Orman"}, {"BIOME_TAIGA","Tayga"}, {"BIOME_MANGROVE_SWAMP","Mangrov Bataklığı"}, {"BIOME_SWAMP","Bataklık"}, {"BIOME_RIVER","Nehir"}, {"BIOME_HELL","Nether Atıkları"}, {"BIOME_THE_END","End"}, {"BIOME_FROZEN_OCEAN","Donmuş Okyanus"}, {"BIOME_WARM_OCEAN","Sıcak Okyanus"}, {"BIOME_COLD_OCEAN","Soğuk Okyanus"}, {"BIOME_OCEAN","Okyanus"}, {"BIOME_SNOWY_PLAINS","Karlı Ovalar"}, {"BIOME_ICE_SPIKES","Buz Dikitleri"}, {"BIOME_MUSHROOM","Mantar Arazileri"}, {"BIOME_BEACH","Sahil"}, {"BIOME_BAMBOO_JUNGLE","Bambu Ormanı"}, {"BIOME_JUNGLE","Yağmur Ormanı"}, {"BIOME_BIRCH_FOREST","Huş Ormanı"}, {"BIOME_DARK_FOREST","Kara Orman"}, {"BIOME_SAVANNA","Savan"}, {"BIOME_MESA","Kırgıbayır"}, {"BIOME_CHERRY","Kiraz Ağacı Korusu"}, {"BIOME_CRIMSON_FOREST","Kızıl Orman"}, {"BIOME_WARPED_FOREST","Çarpık Orman"}, {"BIOME_SOUL_SAND_VALLEY","Ruh Kumu Vadisi"}, {"BIOME_BASALT_DELTAS","Bazalt Deltaları"}, {"BIOME_MEADOW","Çayır"}, {"BIOME_GROVE","Koru"}, {"BIOME_SNOWY_SLOPES","Karlı Yamaçlar"}, {"BIOME_JAGGED_PEAKS","Engebeli Tepeler"}, {"BIOME_FROZEN_PEAKS","Donmuş Tepeler"}, {"BIOME_STONY_PEAKS","Taşlı Tepeler"}, {"BIOME_DEEP_DARK","Karanlık Derinlikler"}, {"BIOME_PALE_GARDEN","Solgun Bahçe"}, {"BIOME_UNKNOWN","Bilinmeyen Biyom"}, {"BIOME_LUKEWARM_OCEAN","Ilık Okyanus"}, {"BIOME_DEEP_OCEAN","Derin Okyanus"}, {"BIOME_DEEP_FROZEN_OCEAN","Derin Donmuş Okyanus"}, {"BIOME_DEEP_COLD_OCEAN","Derin Soğuk Okyanus"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Derin Ilık Okyanus"}, {"BIOME_DEEP_WARM_OCEAN","Derin Sıcak Okyanus"}, {"BIOME_FROZEN_RIVER","Donmuş Nehir"}, {"BIOME_STONY_SHORE","Taşlı Kıyı"}, {"BIOME_SNOWY_BEACH","Karlı Sahil"}, {"BIOME_WINDSWEPT_FOREST","Esintili Orman"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Esintili Çakıllı Tepeler"}, {"BIOME_WINDSWEPT_SAVANNA","Esintili Savan"}, {"BIOME_SAVANNA_PLATEAU","Savan Yaylası"}, {"BIOME_SPARSE_JUNGLE","Seyrek Yağmur Ormanı"}, {"BIOME_SNOWY_TAIGA","Karlı Tayga"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","El Değmemiş Çam Ormanı"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","El Değmemiş Ladin Ormanı"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","El Değmemiş Huş Ormanı"}, {"BIOME_FLOWER_FOREST","Çiçek Ormanı"}, {"BIOME_SUNFLOWER_PLAINS","Ayçiçeği Ovaları"}, {"BIOME_ERODED_BADLANDS","Aşınmış Kırgıbayır"}, {"BIOME_WOODED_BADLANDS","Ormanlık Kırgıbayır"}, {"BIOME_LUSH_CAVES","Yemyeşil Mağaralar"}, {"BIOME_DRIPSTONE_CAVES","Damla Taş Mağaraları"} }}},
            {"uk", {{ {"BIOME_PLAINS","Рівнини"}, {"BIOME_DESERT","Пустеля"}, {"BIOME_EXTREME_HILLS","Вітряні пагорби"}, {"BIOME_FOREST","Ліс"}, {"BIOME_TAIGA","Тайга"}, {"BIOME_MANGROVE_SWAMP","Мангрове болото"}, {"BIOME_SWAMP","Болото"}, {"BIOME_RIVER","Річка"}, {"BIOME_HELL","Незерська пустка"}, {"BIOME_THE_END","Енд"}, {"BIOME_FROZEN_OCEAN","Замерзлий океан"}, {"BIOME_WARM_OCEAN","Теплий океан"}, {"BIOME_COLD_OCEAN","Холодний океан"}, {"BIOME_OCEAN","Океан"}, {"BIOME_SNOWY_PLAINS","Засніжені рівнини"}, {"BIOME_ICE_SPIKES","Льодяні шипи"}, {"BIOME_MUSHROOM","Грибні поля"}, {"BIOME_BEACH","Пляж"}, {"BIOME_BAMBOO_JUNGLE","Бамбукові джунглі"}, {"BIOME_JUNGLE","Джунглі"}, {"BIOME_BIRCH_FOREST","Березовий ліс"}, {"BIOME_DARK_FOREST","Темний ліс"}, {"BIOME_SAVANNA","Савана"}, {"BIOME_MESA","Безплідні землі"}, {"BIOME_CHERRY","Вишневий гай"}, {"BIOME_CRIMSON_FOREST","Багряний ліс"}, {"BIOME_WARPED_FOREST","Химерний ліс"}, {"BIOME_SOUL_SAND_VALLEY","Долина піску душ"}, {"BIOME_BASALT_DELTAS","Базальтові дельти"}, {"BIOME_MEADOW","Лука"}, {"BIOME_GROVE","Гай"}, {"BIOME_SNOWY_SLOPES","Засніжені схили"}, {"BIOME_JAGGED_PEAKS","Гострі вершини"}, {"BIOME_FROZEN_PEAKS","Крижані вершини"}, {"BIOME_STONY_PEAKS","Кам’яні вершини"}, {"BIOME_DEEP_DARK","Темні глибини"}, {"BIOME_PALE_GARDEN","Блідий сад"}, {"BIOME_UNKNOWN","Невідомий біом"}, {"BIOME_LUKEWARM_OCEAN","Помірний океан"}, {"BIOME_DEEP_OCEAN","Глибокий океан"}, {"BIOME_DEEP_FROZEN_OCEAN","Глибокий замерзлий океан"}, {"BIOME_DEEP_COLD_OCEAN","Глибокий холодний океан"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Глибокий помірний океан"}, {"BIOME_DEEP_WARM_OCEAN","Глибокий теплий океан"}, {"BIOME_FROZEN_RIVER","Замерзла річка"}, {"BIOME_STONY_SHORE","Скелястий берег"}, {"BIOME_SNOWY_BEACH","Засніжений пляж"}, {"BIOME_WINDSWEPT_FOREST","Вітряний ліс"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Вітряні гравійні пагорби"}, {"BIOME_WINDSWEPT_SAVANNA","Вітряна савана"}, {"BIOME_SAVANNA_PLATEAU","Саванне плато"}, {"BIOME_SPARSE_JUNGLE","Рідкі джунглі"}, {"BIOME_SNOWY_TAIGA","Засніжена тайга"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Одвічна соснова тайга"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Одвічна смерекова тайга"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Одвічний березовий ліс"}, {"BIOME_FLOWER_FOREST","Квітковий ліс"}, {"BIOME_SUNFLOWER_PLAINS","Соняшникові рівнини"}, {"BIOME_ERODED_BADLANDS","Вивітрені безплідні землі"}, {"BIOME_WOODED_BADLANDS","Лісисті безплідні землі"}, {"BIOME_LUSH_CAVES","Зарослі печери"}, {"BIOME_DRIPSTONE_CAVES","Сталактитові печери"} }}},
            {"vi", {{ {"BIOME_PLAINS","Đồng bằng"}, {"BIOME_DESERT","Sa mạc"}, {"BIOME_EXTREME_HILLS","Đồi lộng gió"}, {"BIOME_FOREST","Rừng"}, {"BIOME_TAIGA","Rừng Taiga"}, {"BIOME_MANGROVE_SWAMP","Đầm lầy ngập mặn"}, {"BIOME_SWAMP","Đầm lầy"}, {"BIOME_RIVER","Sông"}, {"BIOME_HELL","Vùng Nether hoang vu"}, {"BIOME_THE_END","The End"}, {"BIOME_FROZEN_OCEAN","Đại dương băng giá"}, {"BIOME_WARM_OCEAN","Đại dương ấm áp"}, {"BIOME_COLD_OCEAN","Đại dương lạnh giá"}, {"BIOME_OCEAN","Đại dương"}, {"BIOME_SNOWY_PLAINS","Đồng bằng băng tuyết"}, {"BIOME_ICE_SPIKES","Mũi băng"}, {"BIOME_MUSHROOM","Đồng bằng nấm"}, {"BIOME_BEACH","Bãi biển"}, {"BIOME_BAMBOO_JUNGLE","Rừng tre"}, {"BIOME_JUNGLE","Rừng nhiệt đới"}, {"BIOME_BIRCH_FOREST","Rừng gỗ bạch dương"}, {"BIOME_DARK_FOREST","Rừng tối"}, {"BIOME_SAVANNA","Xa-van"}, {"BIOME_MESA","Vùng đất cằn cỗi"}, {"BIOME_CHERRY","Rừng núi anh đào"}, {"BIOME_CRIMSON_FOREST","Rừng đỏ thẫm"}, {"BIOME_WARPED_FOREST","Rừng kì dị"}, {"BIOME_SOUL_SAND_VALLEY","Thung lũng cát linh hồn"}, {"BIOME_BASALT_DELTAS","Châu thổ đá bazan"}, {"BIOME_MEADOW","Thảo điền"}, {"BIOME_GROVE","Rừng núi phủ tuyết"}, {"BIOME_SNOWY_SLOPES","Dốc tuyết"}, {"BIOME_JAGGED_PEAKS","Đỉnh núi lởm chởm"}, {"BIOME_FROZEN_PEAKS","Đỉnh núi lạnh"}, {"BIOME_STONY_PEAKS","Đỉnh núi đá"}, {"BIOME_DEEP_DARK","Bóng tối sâu thẳm"}, {"BIOME_PALE_GARDEN","Vườn nhợt nhạt"}, {"BIOME_UNKNOWN","Quần xã chưa biết"}, {"BIOME_LUKEWARM_OCEAN","Đại dương âm ấm"}, {"BIOME_DEEP_OCEAN","Đại dương sâu thẳm"}, {"BIOME_DEEP_FROZEN_OCEAN","Đại dương băng giá sâu thẳm"}, {"BIOME_DEEP_COLD_OCEAN","Đại dương lạnh giá sâu thẳm"}, {"BIOME_DEEP_LUKEWARM_OCEAN","Đại dương âm ấm sâu thẳm"}, {"BIOME_DEEP_WARM_OCEAN","Đại dương ấm áp sâu thẳm"}, {"BIOME_FROZEN_RIVER","Sông băng giá"}, {"BIOME_STONY_SHORE","Bờ đá"}, {"BIOME_SNOWY_BEACH","Biển băng tuyết"}, {"BIOME_WINDSWEPT_FOREST","Rừng lộng gió"}, {"BIOME_WINDSWEPT_GRAVELLY_HILLS","Đồi sỏi lộng gió"}, {"BIOME_WINDSWEPT_SAVANNA","Xa-van lộng gió"}, {"BIOME_SAVANNA_PLATEAU","Cao nguyên Xa-van"}, {"BIOME_SPARSE_JUNGLE","Rừng nhiệt đới thưa thớt"}, {"BIOME_SNOWY_TAIGA","Rừng Taiga băng tuyết"}, {"BIOME_OLD_GROWTH_PINE_TAIGA","Rừng Taiga nguyên sinh"}, {"BIOME_OLD_GROWTH_SPRUCE_TAIGA","Rừng Taiga vân sam nguyên sinh"}, {"BIOME_OLD_GROWTH_BIRCH_FOREST","Rừng bạch dương nguyên sinh"}, {"BIOME_FLOWER_FOREST","Rừng hoa"}, {"BIOME_SUNFLOWER_PLAINS","Đồng bằng hướng dương"}, {"BIOME_ERODED_BADLANDS","Vùng đất cằn cỗi bị biến đổi"}, {"BIOME_WOODED_BADLANDS","Rừng cằn cỗi"}, {"BIOME_LUSH_CAVES","Hang động tươi tốt"}, {"BIOME_DRIPSTONE_CAVES","Hang động thạch nhũ"} }}},
            };

            auto langIt = biomeDict.find(g_currentLanguage);
            if (langIt != biomeDict.end()) {
                auto wordIt = langIt->second.find(key);
                if (wordIt != langIt->second.end()) return wordIt->second.c_str();
            }
            auto enIt = biomeDict["en"].find(key);
            if (enIt != biomeDict["en"].end()) return enIt->second.c_str();
        }

        if (key == "TEXT_SCALE") {
            if (g_currentLanguage == "zh_CN") return "文本缩放比例";
            if (g_currentLanguage == "zh_TW") return "文字縮放比例";
            if (g_currentLanguage == "de") return "Textskalierung";
            if (g_currentLanguage == "fr") return "Échelle du texte";
            if (g_currentLanguage == "id") return "Skala Teks";
            if (g_currentLanguage == "it") return "Scala del testo";
            if (g_currentLanguage == "ja") return "テキストの倍率";
            if (g_currentLanguage == "ko") return "텍스트 크기";
            if (g_currentLanguage == "pt_BR") return "Escala de Texto";
            if (g_currentLanguage == "ru") return "Масштаб текста";
            if (g_currentLanguage == "th") return "ขนาดข้อความ";
            if (g_currentLanguage == "tr") return "Metin Ölçeği";
            if (g_currentLanguage == "uk") return "Масштаб тексту";
            if (g_currentLanguage == "vi") return "Tỷ lệ Văn bản";
            if (g_currentLanguage == "es") return "Escala de texto";
            return "Text Scale";
        }
        if (key == "MINIMAP_SCALE") {
            if (g_currentLanguage == "zh_CN") return "缩放比例";
            if (g_currentLanguage == "zh_TW") return "縮放比例";
            if (g_currentLanguage == "de") return "Skalierung";
            if (g_currentLanguage == "fr") return "Échelle";
            if (g_currentLanguage == "id") return "Skala";
            if (g_currentLanguage == "it") return "Scala";
            if (g_currentLanguage == "ja") return "倍率";
            if (g_currentLanguage == "ko") return "배율";
            if (g_currentLanguage == "pt_BR") return "Escala";
            if (g_currentLanguage == "ru") return "Масштаб";
            if (g_currentLanguage == "th") return "ขนาด";
            if (g_currentLanguage == "tr") return "Ölçek";
            if (g_currentLanguage == "uk") return "Масштаб";
            if (g_currentLanguage == "vi") return "Tỷ lệ";
            if (g_currentLanguage == "es") return "Escala";
            return "Scale";
        }
        if (key == "MINIMAP_POS_SETTINGS") {
            if (g_currentLanguage == "zh_CN") return "小地图布局设置";
            if (g_currentLanguage == "zh_TW") return "小地圖佈局設置";
            if (g_currentLanguage == "de") return "Minimap-Layout-Einstellungen";
            if (g_currentLanguage == "fr") return "Paramètres de la disposition de la minimap";
            if (g_currentLanguage == "id") return "Pengaturan Tata Letak Minimap";
            if (g_currentLanguage == "it") return "Impostazioni layout minimappa";
            if (g_currentLanguage == "ja") return "ミニマップレイアウト設定";
            if (g_currentLanguage == "ko") return "미니맵 레이아웃 설정";
            if (g_currentLanguage == "pt_BR") return "Configurações de Layout do Minimapa";
            if (g_currentLanguage == "ru") return "Настройки расположения мини-карты";
            if (g_currentLanguage == "th") return "ตั้งค่าการจัดวางแผนที่ย่อ";
            if (g_currentLanguage == "tr") return "Mini Harita Düzen Ayarları";
            if (g_currentLanguage == "uk") return "Налаштування розташування мінікарти";
            if (g_currentLanguage == "vi") return "Cài đặt bố cục bản đồ nhỏ";
            if (g_currentLanguage == "es") return "Configuración del diseño del minimapa";
            return "Minimap Layout Settings";
        }
        if (key == "X_OFFSET") {
            if (g_currentLanguage == "zh_CN") return "X 偏移";
            if (g_currentLanguage == "zh_TW") return "X 偏移";
            if (g_currentLanguage == "de") return "X-Versatz";
            if (g_currentLanguage == "fr") return "Décalage X";
            if (g_currentLanguage == "id") return "Offset X";
            if (g_currentLanguage == "it") return "Offset X";
            if (g_currentLanguage == "ja") return "X オフセット";
            if (g_currentLanguage == "ko") return "X 오프셋";
            if (g_currentLanguage == "pt_BR") return "Deslocamento X";
            if (g_currentLanguage == "ru") return "Смещение X";
            if (g_currentLanguage == "th") return "ระยะห่าง X";
            if (g_currentLanguage == "tr") return "X Kayması";
            if (g_currentLanguage == "uk") return "Зміщення X";
            if (g_currentLanguage == "vi") return "Dịch X";
            if (g_currentLanguage == "es") return "Desplazamiento X";
            return "X Offset";
        }
        if (key == "Y_OFFSET") {
            if (g_currentLanguage == "zh_CN") return "Y 偏移";
            if (g_currentLanguage == "zh_TW") return "Y 偏移";
            if (g_currentLanguage == "de") return "Y-Versatz";
            if (g_currentLanguage == "fr") return "Décalage Y";
            if (g_currentLanguage == "id") return "Offset Y";
            if (g_currentLanguage == "it") return "Offset Y";
            if (g_currentLanguage == "ja") return "Y オフセット";
            if (g_currentLanguage == "ko") return "Y 오프셋";
            if (g_currentLanguage == "pt_BR") return "Deslocamento Y";
            if (g_currentLanguage == "ru") return "Смещение Y";
            if (g_currentLanguage == "th") return "ระยะห่าง Y";
            if (g_currentLanguage == "tr") return "Y Kayması";
            if (g_currentLanguage == "uk") return "Зміщення Y";
            if (g_currentLanguage == "vi") return "Dịch Y";
            if (g_currentLanguage == "es") return "Desplazamiento Y";
            return "Y Offset";
        }
        if (key == "SAVE_AND_EXIT") {
            if (g_currentLanguage == "zh_CN") return "保存并退出";
            if (g_currentLanguage == "zh_TW") return "保存並退出";
            if (g_currentLanguage == "de") return "Speichern und Beenden";
            if (g_currentLanguage == "fr") return "Enregistrer et quitter";
            if (g_currentLanguage == "id") return "Simpan dan Keluar";
            if (g_currentLanguage == "it") return "Salva ed esci";
            if (g_currentLanguage == "ja") return "保存して終了";
            if (g_currentLanguage == "ko") return "저장 후 종료";
            if (g_currentLanguage == "pt_BR") return "Salvar e Sair";
            if (g_currentLanguage == "ru") return "Сохранить и выйти";
            if (g_currentLanguage == "th") return "บันทึกและออก";
            if (g_currentLanguage == "tr") return "Kaydet ve Çık";
            if (g_currentLanguage == "uk") return "Зберегти і вийти";
            if (g_currentLanguage == "vi") return "Lưu và Thoát";
            if (g_currentLanguage == "es") return "Guardar y Salir";
            return "Save and Exit";
        }
        if (key == "DONT_SAVE") {
            if (g_currentLanguage == "zh_CN") return "不保存";
            if (g_currentLanguage == "zh_TW") return "不保存";
            if (g_currentLanguage == "de") return "Nicht speichern";
            if (g_currentLanguage == "fr") return "Ne pas enregistrer";
            if (g_currentLanguage == "id") return "Jangan Simpan";
            if (g_currentLanguage == "it") return "Non salvare";
            if (g_currentLanguage == "ja") return "保存しない";
            if (g_currentLanguage == "ko") return "저장하지 않음";
            if (g_currentLanguage == "pt_BR") return "Não Salvar";
            if (g_currentLanguage == "ru") return "Не сохранять";
            if (g_currentLanguage == "th") return "ไม่บันทึก";
            if (g_currentLanguage == "tr") return "Kaydetme";
            if (g_currentLanguage == "uk") return "Не зберігати";
            if (g_currentLanguage == "vi") return "Không Lưu";
            if (g_currentLanguage == "es") return "No Guardar";
            return "Don't Save";
        }
        if (key == "EDIT_MINIMAP_POS") {
            if (g_currentLanguage == "zh_CN") return "调整小地图布局";
            if (g_currentLanguage == "zh_TW") return "調整小地圖佈局";
            if (g_currentLanguage == "de") return "Minimap-Layout bearbeiten";
            if (g_currentLanguage == "fr") return "Modifier la disposition de la minimap";
            if (g_currentLanguage == "id") return "Edit Tata Letak Minimap";
            if (g_currentLanguage == "it") return "Modifica layout minimappa";
            if (g_currentLanguage == "ja") return "ミニマップのレイアウトを編集";
            if (g_currentLanguage == "ko") return "미니맵 레이아웃 편집";
            if (g_currentLanguage == "pt_BR") return "Editar Layout do Minimapa";
            if (g_currentLanguage == "ru") return "Изменить расположение мини-карты";
            if (g_currentLanguage == "th") return "แก้ไขการจัดวางแผนที่ย่อ";
            if (g_currentLanguage == "tr") return "Mini Harita Düzenini Düzenle";
            if (g_currentLanguage == "uk") return "Редагувати розташування мінікарти";
            if (g_currentLanguage == "vi") return "Chỉnh sửa bố cục bản đồ nhỏ";
            if (g_currentLanguage == "es") return "Editar diseño del minimapa";
            return "Edit Minimap Layout";
        }
        if (key == "NATIVE_IME_TOOLTIP") {
            if (g_currentLanguage == "zh_CN") return "点击打开原生输入框以输入中文等字符";
            if (g_currentLanguage == "zh_TW") return "點擊打開原生輸入框以輸入中文等字元";
            if (g_currentLanguage == "ja") return "クリックしてネイティブ入力ボックスを開き、日本語などを入力します";
            if (g_currentLanguage == "ko") return "클릭하여 기본 입력란을 열고 한국어 등을 입력합니다";
            if (g_currentLanguage == "ru") return "Нажмите, чтобы открыть системное поле ввода для ввода текста";
            if (g_currentLanguage == "fr") return "Cliquez pour ouvrir la zone de saisie native (chinois, etc.)";
            if (g_currentLanguage == "de") return "Klicken, um das native Eingabefeld für Text zu öffnen";
            if (g_currentLanguage == "pt_BR") return "Clique para abrir a caixa de entrada nativa e digitar texto";
            if (g_currentLanguage == "it") return "Fai clic per aprire la casella di input nativa per digitare";
            if (g_currentLanguage == "es") return "Haga clic para abrir el cuadro de entrada nativo para escribir";
            if (g_currentLanguage == "id") return "Klik untuk membuka kotak masukan bawaan untuk mengetik";
            if (g_currentLanguage == "th") return "คลิกเพื่อเปิดกล่องป้อนข้อความพื้นฐานสำหรับพิมพ์";
            if (g_currentLanguage == "tr") return "Metin yazmak için yerel giriş kutusunu açın";
            if (g_currentLanguage == "uk") return "Натисніть, щоб відкрити системне поле введення";
            if (g_currentLanguage == "vi") return "Nhấn để mở hộp nhập văn bản gốc để gõ văn bản";
            return "Click to open native input box to type Chinese/etc.";
        }
        if (key == "DEFAULT_POS") {
            if (g_currentLanguage == "zh_CN") return "恢复默认";
            if (g_currentLanguage == "zh_TW") return "恢復預設";
            if (g_currentLanguage == "de") return "Standard wiederherstellen";
            if (g_currentLanguage == "fr") return "Restaurer par défaut";
            if (g_currentLanguage == "id") return "Kembalikan Bawaan";
            if (g_currentLanguage == "it") return "Ripristina predefinito";
            if (g_currentLanguage == "ja") return "既定値に戻す";
            if (g_currentLanguage == "ko") return "기본값 복원";
            if (g_currentLanguage == "pt_BR") return "Restaurar Padrão";
            if (g_currentLanguage == "ru") return "Восстановить по умолчанию";
            if (g_currentLanguage == "th") return "คืนค่าเริ่มต้น";
            if (g_currentLanguage == "tr") return "Varsayılanı Geri Yükle";
            if (g_currentLanguage == "uk") return "Відновити типове";
            if (g_currentLanguage == "vi") return "Khôi phục Mặc định";
            if (g_currentLanguage == "es") return "Restaurar predeterminado";
            return "Restore Default";
        }
        if (key == "TOP_LEFT_POS") {
            if (g_currentLanguage == "zh_CN") return "移至左上";
            if (g_currentLanguage == "zh_TW") return "移至左上";
            if (g_currentLanguage == "de") return "Nach oben links verschieben";
            if (g_currentLanguage == "fr") return "Déplacer en haut à gauche";
            if (g_currentLanguage == "id") return "Pindah ke Kiri Atas";
            if (g_currentLanguage == "it") return "Sposta in alto a sinistra";
            if (g_currentLanguage == "ja") return "左上に移動";
            if (g_currentLanguage == "ko") return "왼쪽 위로 이동";
            if (g_currentLanguage == "pt_BR") return "Mover para o Canto Superior Esquerdo";
            if (g_currentLanguage == "ru") return "Переместить вверх-влево";
            if (g_currentLanguage == "th") return "ย้ายไปซ้ายบน";
            if (g_currentLanguage == "tr") return "Sol Üst Köşeye Taşı";
            if (g_currentLanguage == "uk") return "Перемістити ліворуч-вгору";
            if (g_currentLanguage == "vi") return "Di chuyển đến Trên Trái";
            if (g_currentLanguage == "es") return "Mover a la esquina superior izquierda";
            return "Move to Top-Left";
        }
        if (key == "ROTATE_MINIMAP") {
            if (g_currentLanguage == "zh_CN") return "小地图跟随视角旋转";
            if (g_currentLanguage == "zh_TW") return "小地圖跟隨視角旋轉";
            if (g_currentLanguage == "ja") return "ミニマップを回転";
            if (g_currentLanguage == "ko") return "미니맵 회전";
            if (g_currentLanguage == "ru") return "Вращать мини-карту";
            if (g_currentLanguage == "fr") return "Faire pivoter la minimap";
            if (g_currentLanguage == "de") return "Minimap drehen";
            if (g_currentLanguage == "pt_BR") return "Girar Minimapa";
            if (g_currentLanguage == "it") return "Ruota la minimappa";
            if (g_currentLanguage == "es") return "Rotar minimapa";
            if (g_currentLanguage == "id") return "Putar Minimap";
            if (g_currentLanguage == "th") return "หมุนแผนที่ย่อ";
            if (g_currentLanguage == "tr") return "Mini Haritayı Döndür";
            if (g_currentLanguage == "uk") return "Обертати мінікарту";
            if (g_currentLanguage == "vi") return "Xoay bản đồ nhỏ";
            return "Rotate Minimap";
        }
        if (key == "SETTINGS_TOOLTIP") {
            if (g_currentLanguage == "zh_CN") return "打开设置面板";
            if (g_currentLanguage == "zh_TW") return "打開設定面板";
            if (g_currentLanguage == "ja") return "設定を開く";
            if (g_currentLanguage == "ko") return "설정 열기";
            if (g_currentLanguage == "ru") return "Открыть настройки";
            if (g_currentLanguage == "fr") return "Ouvrir les paramètres";
            if (g_currentLanguage == "de") return "Einstellungen öffnen";
            if (g_currentLanguage == "pt_BR") return "Abrir Configurações";
            if (g_currentLanguage == "it") return "Apri Impostazioni";
            if (g_currentLanguage == "es") return "Abrir Ajustes";
            if (g_currentLanguage == "id") return "Buka Pengaturan";
            if (g_currentLanguage == "th") return "เปิดการตั้งค่า";
            if (g_currentLanguage == "tr") return "Ayarları Aç";
            if (g_currentLanguage == "uk") return "Відкрити налаштування";
            if (g_currentLanguage == "vi") return "Mở Cài đặt";
            return "Open Settings";
        }
        if (key == "LOG_VERSION_MISMATCH") {
            if (g_currentLanguage == "zh_CN") return "【严重警告】游戏版本不适配！当前客户端版本为";
            if (g_currentLanguage == "zh_TW") return "【嚴重警告】遊戲版本不符合！當前客戶端版本為";
            if (g_currentLanguage == "de") return "[KRITISCH] Spielversion stimmt nicht überein! Aktuelle Client-Version:";
            if (g_currentLanguage == "fr") return "[CRITIQUE] Incompatibilité de version du jeu ! Version du client actuelle :";
            if (g_currentLanguage == "id") return "[KRITIS] Ketidakcocokan versi game! Versi klien saat ini:";
            if (g_currentLanguage == "it") return "[CRITICO] Conflitto di versione del gioco! Versione client attuale:";
            if (g_currentLanguage == "ja") return "[重大] ゲームのバージョンが一致しません！現在のクライアントバージョン：";
            if (g_currentLanguage == "ko") return "[심각] 게임 버전이 일치하지 않습니다! 현재 클라이언트 버전:";
            if (g_currentLanguage == "pt_BR") return "[CRÍTICO] Incompatibilidade de versão do jogo! Versão atual do cliente:";
            if (g_currentLanguage == "ru") return "[КРИТИЧНО] Несоответствие версии игры! Текущая версия клиента:";
            if (g_currentLanguage == "th") return "[วิกฤต] เวอร์ชันเกมไม่ตรงกัน! เวอร์ชันไคลเอนต์ปัจจุบัน:";
            if (g_currentLanguage == "tr") return "[KRİTİK] Oyun sürümü uyuşmuyor! Mevcut istemci sürümü:";
            if (g_currentLanguage == "uk") return "[КРИТИЧНО] Невідповідність версії гри! Поточна версія клієнта:";
            if (g_currentLanguage == "vi") return "[QUAN TRỌNG] Phiên bản game không khớp! Phiên bản client hiện tại:";
            if (g_currentLanguage == "es") return "[CRÍTICO] ¡Incompatibilidad de versión del juego! Versión actual del cliente:";
            return "[CRITICAL] Game version mismatch! Current client version";
        }
        if (key == "LOG_VERSION_STRICT") {
            if (g_currentLanguage == "zh_CN") return "【严重警告】赤焰地图 (ChiyanMap) 底层拦截器当前严格限定仅兼容 1.26.20.04 版本！";
            if (g_currentLanguage == "zh_TW") return "【嚴重警告】赤焰地圖 (ChiyanMap) 底層攔截器當前嚴格限定僅相容 1.26.20.04 版本！";
            if (g_currentLanguage == "de") return "[KRITISCH] ChiyanMap unterstützt streng genommen nur Version 1.26.20.04!";
            if (g_currentLanguage == "fr") return "[CRITIQUE] ChiyanMap ne prend en charge que la version 1.26.20.04 !";
            if (g_currentLanguage == "id") return "[KRITIS] ChiyanMap hanya mendukung versi 1.26.20.04!";
            if (g_currentLanguage == "it") return "[CRITICO] ChiyanMap supporta rigorosamente solo la versione 1.26.20.04!";
            if (g_currentLanguage == "ja") return "[重大] ChiyanMap は厳密にバージョン 1.26.20.04 のみをサポートしています！";
            if (g_currentLanguage == "ko") return "[심각] ChiyanMap은 엄격히 버전 1.26.20.04만 지원합니다!";
            if (g_currentLanguage == "pt_BR") return "[CRÍTICO] O ChiyanMap é compatível estritamente apenas com a versão 1.26.20.04!";
            if (g_currentLanguage == "ru") return "[КРИТИЧНО] ChiyanMap строго поддерживает только версию 1.26.20.04!";
            if (g_currentLanguage == "th") return "[วิกฤต] ChiyanMap รองรับเฉพาะเวอร์ชัน 1.26.20.04 เท่านั้น!";
            if (g_currentLanguage == "tr") return "[KRİTİK] ChiyanMap yalnızca sürüm 1.26.20.04'ü destekler!";
            if (g_currentLanguage == "uk") return "[КРИТИЧНО] ChiyanMap суворо підтримує лише версію 1.26.20.04!";
            if (g_currentLanguage == "vi") return "[QUAN TRỌNG] ChiyanMap chỉ hỗ trợ phiên bản 1.26.20.04!";
            if (g_currentLanguage == "es") return "[CRÍTICO] ¡ChiyanMap solo es compatible estrictamente con la versión 1.26.20.04!";
            return "[CRITICAL] ChiyanMap strictly supports version 1.26.20.04 only!";
        }
        if (key == "LOG_VERSION_ABORT") {
            if (g_currentLanguage == "zh_CN") return "【严重警告】为防止加载进入世界时发生 Access Violation 崩溃，模组已主动中止加载。";
            if (g_currentLanguage == "zh_TW") return "【嚴重警告】為防止載入進入世界時發生 Access Violation 崩潰，模組已主動終止載入。";
            if (g_currentLanguage == "de") return "[KRITISCH] Mod-Laden abgebrochen, um Access-Violation-Abstürze zu verhindern.";
            if (g_currentLanguage == "fr") return "[CRITIQUE] Chargement du mod abandonné pour éviter les plantages par violation d'accès.";
            if (g_currentLanguage == "id") return "[KRITIS] Pemuanan mod dibatalkan untuk mencegah crash Access Violation.";
            if (g_currentLanguage == "it") return "[CRITICO] Caricamento della mod interrotto per evitare crash da violazione di accesso.";
            if (g_currentLanguage == "ja") return "[重大] Access Violation クラッシュを防ぐため、Mod の読み込みを中止しました。";
            if (g_currentLanguage == "ko") return "[심각] Access Violation 충돌을 방지하기 위해 모드 로딩을 중단했습니다.";
            if (g_currentLanguage == "pt_BR") return "[CRÍTICO] Carregamento do mod abortado para evitar crashes de Violação de Acesso.";
            if (g_currentLanguage == "ru") return "[КРИТИЧНО] Загрузка мода прервана во избежание сбоев Access Violation.";
            if (g_currentLanguage == "th") return "[วิกฤต] ยกเลิกการโหลดมอดเพื่อป้องกันการพังจาก Access Violation";
            if (g_currentLanguage == "tr") return "[KRİTİK] Access Violation çökmelerini önlemek için mod yüklemesi durduruldu.";
            if (g_currentLanguage == "uk") return "[КРИТИЧНО] Завантаження моду перервано, щоб запобігти збоям Access Violation.";
            if (g_currentLanguage == "vi") return "[QUAN TRỌNG] Đã hủy tải mod để ngăn chặn sự cố Access Violation.";
            if (g_currentLanguage == "es") return "[CRÍTICO] Carga del mod abortada para prevenir fallos de Violación de Acceso.";
            return "[CRITICAL] Mod loading aborted to prevent Access Violation crashes.";
        }
        if (key == "LOG_VERSION_PASS") {
            if (g_currentLanguage == "zh_CN") return "游戏客户端版本验证通过";
            if (g_currentLanguage == "zh_TW") return "遊戲客戶端版本驗證通過";
            if (g_currentLanguage == "de") return "Spiel-Client-Version-Verifizierung bestanden";
            if (g_currentLanguage == "fr") return "Vérification de la version du client de jeu réussie";
            if (g_currentLanguage == "id") return "Verifikasi versi klien game berhasil";
            if (g_currentLanguage == "it") return "Verifica della versione del client di gioco superata";
            if (g_currentLanguage == "ja") return "ゲームクライアントのバージョン確認に成功しました";
            if (g_currentLanguage == "ko") return "게임 클라이언트 버전 확인을 통과했습니다";
            if (g_currentLanguage == "pt_BR") return "Verificação da versão do cliente do jogo concluída";
            if (g_currentLanguage == "ru") return "Проверка версии игрового клиента пройдена";
            if (g_currentLanguage == "th") return "ตรวจสอบเวอร์ชันไคลเอนต์เกมผ่านแล้ว";
            if (g_currentLanguage == "tr") return "Oyun istemci sürümü doğrulaması başarılı";
            if (g_currentLanguage == "uk") return "Перевірку версії ігрового клієнта пройдено";
            if (g_currentLanguage == "vi") return "Xác minh phiên bản client game đã qua";
            if (g_currentLanguage == "es") return "Verificación de la versión del cliente del juego superada";
            return "Game client version verification passed";
        }
        if (key == "LOG_VERSION_UNKNOWN") {
            if (g_currentLanguage == "zh_CN") return "无法识别当前游戏可执行文件的版本信息，模组将尝试强行加载...";
            if (g_currentLanguage == "zh_TW") return "無法識別當前遊戲執行檔的版本資訊，模組將嘗試強行載入...";
            if (g_currentLanguage == "de") return "Spiel-Executable-Version kann nicht identifiziert werden, erzwungenes Laden wird versucht...";
            if (g_currentLanguage == "fr") return "Impossible d'identifier la version de l'exécutable du jeu, tentative de chargement forcé...";
            if (g_currentLanguage == "id") return "Tidak dapat mengidentifikasi versi executable game, mencoba memaksa muat...";
            if (g_currentLanguage == "it") return "Impossibile identificare la versione dell'eseguibile di gioco, tentativo di caricamento forzato...";
            if (g_currentLanguage == "ja") return "ゲーム実行ファイルのバージョンを特定できません。強制的に読み込みを試みます...";
            if (g_currentLanguage == "ko") return "게임 실행 파일 버전을 식별할 수 없어 강제 로드를 시도합니다...";
            if (g_currentLanguage == "pt_BR") return "Não foi possível identificar a versão do executável do jogo, tentando carregar à força...";
            if (g_currentLanguage == "ru") return "Не удалось определить версию исполняемого файла игры, попытка принудительной загрузки...";
            if (g_currentLanguage == "th") return "ไม่สามารถระบุเวอร์ชันของไฟล์ปฏิบัติการเกมได้ กำลังพยายามโหลดแบบบังคับ...";
            if (g_currentLanguage == "tr") return "Oyun yürütülebilir dosya sürümü tanımlanamadı, zorla yükleme deneniyor...";
            if (g_currentLanguage == "uk") return "Не вдалося визначити версію виконуваного файлу гри, спроба примусового завантаження...";
            if (g_currentLanguage == "vi") return "Không thể xác định phiên bản tệp thực thi game, đang thử tải bắt buộc...";
            if (g_currentLanguage == "es") return "No se pudo identificar la versión del ejecutable del juego, intentando carga forzada...";
            return "Unable to identify game executable version, attempting to force load...";
        }

        auto it = g_translationCache.find(key);
        if (it != g_translationCache.end()) {
            return it->second.c_str();
        }
        std::string_view sv = ll::i18n::getInstance().get(key, g_currentLanguage);
        g_translationCache[key] = std::string(sv);
        return g_translationCache[key].c_str();
    }
}
