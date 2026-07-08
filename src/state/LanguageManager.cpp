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
    "LANG_SELECT": "Language"
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
    "WP_NAME": "\u540d\u79f0",
    "WP_COLOR": "\u989c\u8272",
    "WP_DEFAULT_NAME": "\u65b0\u5730\u6807",
    "LANG_SELECT": "\u8bed\u8a00"
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
    "WP_NAME": "\u540d\u7a31",
    "WP_COLOR": "\u984f\u8272",
    "WP_DEFAULT_NAME": "\u65b0\u5730\u6a19",
    "LANG_SELECT": "\u8a9e\u8a00"
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
    "LANG_SELECT": "Sprache"
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
    "COMPASS_E": "E",
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
    "LANG_SELECT": "Langue"
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
    "LANG_SELECT": "Bahasa"
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
    "COMPASS_E": "E",
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
    "LANG_SELECT": "Lingua"
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
    "WP_NAME": "\u540d\u524d",
    "WP_COLOR": "\u8272",
    "WP_DEFAULT_NAME": "\u65b0\u898f\u30dd\u30a4\u30f3\u30c8",
    "LANG_SELECT": "\u8a00\u8a9e"
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
    "WP_NAME": "\uc774\ub984",
    "WP_COLOR": "\uc0c9\uc0c1",
    "WP_DEFAULT_NAME": "\uc0c8 \uc704\uce58",
    "LANG_SELECT": "\uc5b8\uc5b4"
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
    "LANG_SELECT": "Idioma"
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
    "WP_NAME": "\u0418\u043c\u044f",
    "WP_COLOR": "\u0426\u0432\u0435\u0442",
    "WP_DEFAULT_NAME": "\u041d\u043e\u0432\u0430\u044f \u0442\u043e\u0447\u043a\u0430",
    "LANG_SELECT": "\u042f\u0437\u044b\u043a"
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
    "WP_NAME": "\u0e0a\u0e37\u0e48\u0e2d",
    "WP_COLOR": "\u0e2a\u0e35",
    "WP_DEFAULT_NAME": "\u0e08\u0e38\u0e14\u0e40\u0e2a\u0e49\u0e19\u0e17\u0e32\u0e07\u0e43\u0e2b\u0e21\u0e48",
    "LANG_SELECT": "\u0e20\u0e32\u0e29\u0e32"
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
    "WP_NAME": "Isim",
    "WP_COLOR": "Renk",
    "WP_DEFAULT_NAME": "Yeni Isaret",
    "LANG_SELECT": "Dil"
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
    "WP_NAME": "\u0406\u043c\u02bc\u044f",
    "WP_COLOR": "\u041a\u043e\u043b\u0456\u0440",
    "WP_DEFAULT_NAME": "\u041d\u043e\u0432\u0430 \u0442\u043e\u0447\u043a\u0430",
    "LANG_SELECT": "\u041c\u043e\u0432\u0430"
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
    "WP_NAME": "T\u00ean",
    "WP_COLOR": "M\u00e0u s\u1eafc",
    "WP_DEFAULT_NAME": "M\u1ed1c m\u1edbi",
    "LANG_SELECT": "Ng\u00f4n ng\u1eef"
})json"}
    };

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/lang");

        for (const auto& [langCode, jsonContent] : g_defaultJsonFiles) {
            std::string filePath = "mods/ChiyanMap/lang/" + langCode + ".json";
            if (!std::filesystem::exists(filePath)) {
                std::ofstream out(filePath);
                if (out.is_open()) {
                    out << jsonContent;
                    out.close();
                }
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
                MapRenderState::uiTextScale = j.value("uiTextScale", 1.0f);
                MapRenderState::miniMapScale = j.value("miniMapScale", 1.0f);
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
        j["uiTextScale"] = MapRenderState::uiTextScale;
        j["miniMapScale"] = MapRenderState::miniMapScale;

        std::ofstream out(filePath);
        if (out.is_open()) {
            out << j.dump(4);
            out.close();
        }
    }

    const char* GetText(const std::string& key) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (key == "TEXT_SCALE") {
            if (g_currentLanguage == "zh_CN") return "文本缩放比例";
            if (g_currentLanguage == "zh_TW") return "文字縮放比例";
            return "Text Scale";
        }
        if (key == "MINIMAP_SCALE") {
            if (g_currentLanguage == "zh_CN") return "小地图缩放";
            if (g_currentLanguage == "zh_TW") return "小地圖縮放";
            return "Minimap Scale";
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
