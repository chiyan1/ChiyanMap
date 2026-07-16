#pragma once
#include <windows.h>
// 定义这些宏，避免与 LeviLamina 中的重复定义冲突
#define D3D12_FEATURE_DATA_D3D12_OPTIONS  D3D12_FEATURE_DATA_D3D12_OPTIONS_LEGACY
#define D3D12_FEATURE_DATA_ARCHITECTURE  D3D12_FEATURE_DATA_ARCHITECTURE_LEGACY
#define D3D12_RAYTRACING_GEOMETRY_DESC  D3D12_RAYTRACING_GEOMETRY_DESC_LEGACY
// 包含我们需要的 DX 头文件
#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
// 取消这些宏定义，避免后续问题
#undef D3D12_FEATURE_DATA_D3D12_OPTIONS
#undef D3D12_FEATURE_DATA_ARCHITECTURE
#undef D3D12_RAYTRACING_GEOMETRY_DESC
// 现在包含其他头文件
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include <ll/api/memory/Hook.h>
#include <MinHook.h>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include "hooks/PlayerHook.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"
#include "state/LanguageManager.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX11Hook {
    
    // 【极致平滑引擎】供全局调用的亚像素平滑坐标
    inline float g_smoothPX = 0.0f;
    inline float g_smoothPZ = 0.0f;

    inline void DbgLog(const char* msg, HRESULT hr = 0) {
        std::ofstream out("MapMod_Debug.txt", std::ios::app);
        if (hr != 0) {
            char hexBuf[32]; snprintf(hexBuf, sizeof(hexBuf), " (HR: 0x%08X)", hr);
            out << msg << hexBuf << "\n";
        } else {
            out << msg << "\n";
        }
    }

    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(__stdcall* Present1_t)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    typedef void(__stdcall* ExecuteCommandLists_t)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    inline Present_t oPresent = nullptr;
    inline Present1_t oPresent1 = nullptr;
    inline ResizeBuffers_t oResizeBuffers = nullptr;
    inline ExecuteCommandLists_t oExecuteCommandLists = nullptr;

    inline ID3D11Device* g_pd3dDevice = nullptr;
    inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    inline ID3D11On12Device* g_d3d11On12Device = nullptr;
    inline ID3D12CommandQueue* g_pGameCommandQueue = nullptr; 
    
    inline HWND g_hWnd = nullptr;
    inline WNDPROC oWndProc = nullptr;
    inline bool g_imguiInitialized = false;

    inline ID3D11Texture2D* g_mapTexture = nullptr;
    inline ID3D11ShaderResourceView* g_mapTextureView = nullptr;
    inline uint8_t g_textureData[MAP_DATA_SIZE * MAP_DATA_SIZE * 4];

    inline std::unordered_map<uint64_t, ID3D11Texture2D*> g_regionTextures;
    inline std::unordered_map<uint64_t, ID3D11ShaderResourceView*> g_regionSRVs;

    // 记录上一次真实推送到 DX11 纹理的坐标中心
    inline float g_textureCenterX = 0.0f;
    inline float g_textureCenterZ = 0.0f;

    // ==========================================
    // [另辟蹊径] Windows Raw Input 硬件级欺骗器
    // ==========================================
    typedef UINT(WINAPI* PGETRAWINPUTDATA_HOOK)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
    inline PGETRAWINPUTDATA_HOOK oGetRawInputData = nullptr;

    inline UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
        if (MapRenderState::IsUIActive()) {
            return (UINT)-1;
        }
        if (oGetRawInputData) return oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);
        return 0;
    }

    // ==========================================
    // [终极防御] 拦截 GetRawInputBuffer 与异步按键状态 (杀灭侧键)
    // ==========================================
    typedef UINT(WINAPI* PGETRAWINPUTBUFFER_HOOK)(PRAWINPUT, PUINT, UINT);
    inline PGETRAWINPUTBUFFER_HOOK oGetRawInputBuffer = nullptr;
    inline UINT WINAPI hkGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader) {
        if (MapRenderState::IsUIActive()) return (UINT)-1;
        if (oGetRawInputBuffer) return oGetRawInputBuffer(pData, pcbSize, cbSizeHeader);
        return 0;
    }

    typedef SHORT(WINAPI* PGETASYNCKEYSTATE_HOOK)(int);
    inline PGETASYNCKEYSTATE_HOOK oGetAsyncKeyState = nullptr;
    inline SHORT WINAPI hkGetAsyncKeyState(int vKey) {
        if (MapRenderState::IsUIActive() && vKey != VK_F11) {
            return 0;
        }
        if (oGetAsyncKeyState) return oGetAsyncKeyState(vKey);
        return 0;
    }

    typedef SHORT(WINAPI* PGETKEYSTATE_HOOK)(int);
    inline PGETKEYSTATE_HOOK oGetKeyState = nullptr;
    inline SHORT WINAPI hkGetKeyState(int vKey) {
        if (MapRenderState::IsUIActive() && vKey != VK_F11) {
            return 0;
        }
        if (oGetKeyState) return oGetKeyState(vKey);
        return 0;
    }

    typedef BOOL(WINAPI* PGETCURSORPOS_HOOK)(LPPOINT);
    inline PGETCURSORPOS_HOOK oGetCursorPos = nullptr;
    inline BOOL WINAPI hkGetCursorPos(LPPOINT lpPoint) {
        if (oGetCursorPos) return oGetCursorPos(lpPoint);
        return FALSE;
    }

    typedef BOOL(WINAPI* PSETCURSORPOS_HOOK)(int, int);
    inline PSETCURSORPOS_HOOK oSetCursorPos = nullptr;
    inline BOOL WINAPI hkSetCursorPos(int X, int Y) {
        if (oSetCursorPos) return oSetCursorPos(X, Y);
        return FALSE;
    }

    inline void ShutdownImGuiAndBuffers() {
        for(auto& p : g_regionSRVs) if(p.second) p.second->Release();
        g_regionSRVs.clear();
        for(auto& p : g_regionTextures) if(p.second) p.second->Release();
        g_regionTextures.clear();
        if (g_imguiInitialized) {
            if (g_mapTextureView) { g_mapTextureView->Release(); g_mapTextureView = nullptr; }
            if (g_mapTexture) { g_mapTexture->Release(); g_mapTexture = nullptr; }
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_imguiInitialized = false;
        }
        if (g_pd3dDeviceContext) {
            ID3D11RenderTargetView* nullRTV = nullptr;
            g_pd3dDeviceContext->OMSetRenderTargets(1, &nullRTV, NULL);
            g_pd3dDeviceContext->ClearState();
            g_pd3dDeviceContext->Flush();
        }
        if (g_d3d11On12Device) { g_d3d11On12Device->Release(); g_d3d11On12Device = nullptr; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
        if (g_pGameCommandQueue) { g_pGameCommandQueue->Release(); g_pGameCommandQueue = nullptr; }
    }

    inline HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
        if (g_imguiInitialized) {
            ImGui_ImplDX11_InvalidateDeviceObjects();
        }

        HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr) && g_imguiInitialized) {
            ImGui_ImplDX11_CreateDeviceObjects();
        }
        return hr;
    }

    inline void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* pQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
        if (!g_pGameCommandQueue) {
            D3D12_COMMAND_QUEUE_DESC desc = pQueue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                g_pGameCommandQueue = pQueue;
                g_pGameCommandQueue->AddRef();
            }
        }
        oExecuteCommandLists(pQueue, NumCommandLists, ppCommandLists);
    }

    inline LRESULT __stdcall WndProcHook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (g_imguiInitialized && g_hasPlayer) {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            
            bool isTyping = false;
            if (ImGui::GetCurrentContext()) {
                isTyping = ImGui::GetIO().WantCaptureKeyboard;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x4D && !isTyping) {
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                
                MapRenderState::showBigMap = !MapRenderState::showBigMap;

                if (MapRenderState::showBigMap) {
                    MapRenderState::bigMapOffsetX = 0.0f;
                    MapRenderState::bigMapOffsetZ = 0.0f;
                }
                return 1;
            }
            if (uMsg == WM_KEYDOWN && wParam == 0x55 && !isTyping) {
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    // 当处于游戏原生的聊天、物品栏等界面时（鼠标显示），不拦截按键
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                MapRenderState::showWaypointUI = !MapRenderState::showWaypointUI;
                return 1;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x4E && !isTyping) {
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                MapRenderState::showMiniMap = !MapRenderState::showMiniMap;
                LanguageManager::SaveConfig();
                return 1;
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x59 && !isTyping) {
                if (!MapRenderState::showMiniMap) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                
                // 仅在小地图显示时，才允许切换地图形状并拦截按键
                if (MapRenderState::showMiniMap) {
                    MapRenderState::isSquareMap = !MapRenderState::isSquareMap;
                    LanguageManager::SaveConfig();
                    return 1;
                }
                
                // 如果小地图隐藏，则将按键透传给游戏处理
                return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
            }

            if (uMsg == WM_KEYDOWN && wParam == 0x4A && !isTyping) {
                if (!MapRenderState::showMiniMap) {
                    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                }
                CURSORINFO ci = {}; ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == CURSOR_SHOWING && !MapRenderState::IsUIActive()) {
                        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
                    }
                }
                
                if (MapRenderState::showMiniMap) {
                    MapRenderState::rotateMiniMap = !MapRenderState::rotateMiniMap;
                    LanguageManager::SaveConfig();
                    return 1;
                }
                
                return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
            }

            if (MapRenderState::IsUIActive()) {
                ClipCursor(NULL);
                if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                    if (MapRenderState::showMiniMapPosSettings) {
                        MapRenderState::showMiniMapPosSettings = false; // 触发回退操作
                        MapRenderState::showBigMap = true; // 返回大地图
                        return 1;
                    }
                    MapRenderState::showBigMap = false;
                    MapRenderState::showWaypointUI = false; 
                    return 1;
                }
                if (uMsg == WM_INPUT || uMsg == WM_INPUT_DEVICE_CHANGE) return 1;
                if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return 1;
                // 当处于 UI 状态时拦截所有按键（除F11外），且不再为快捷键开特例
                // 但放行 WM_CHAR 等字符消息，以支持 IME 中文输入
                if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST && uMsg != WM_CHAR && uMsg != WM_SYSCHAR && uMsg != WM_DEADCHAR && uMsg != WM_SYSDEADCHAR && wParam != VK_F11) return 1;
                if (uMsg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
            }
        }
        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
    }

    inline void InitImGuiFonts(ImGuiIO& io) {
        io.Fonts->Clear();

        ImFontConfig config;
        config.OversampleH = 1;
        config.OversampleV = 1;

        std::string baseFont = "c:\\Windows\\Fonts\\segoeui.ttf";
        if (!std::filesystem::exists(baseFont)) baseFont = "c:\\Windows\\Fonts\\arial.ttf";

        if (std::filesystem::exists(baseFont)) {
            io.Fonts->AddFontFromFileTTF(baseFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesCyrillic());
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(baseFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesVietnamese());
        } else {
            io.Fonts->AddFontDefault();
            config.MergeMode = true;
        }

        std::string cnHan = "c:\\Windows\\Fonts\\msyh.ttc";
        if (std::filesystem::exists(cnHan)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(cnHan.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesChineseFull());
        }

        std::string jpFont = "c:\\Windows\\Fonts\\meiryo.ttc";
        if (!std::filesystem::exists(jpFont)) jpFont = "c:\\Windows\\Fonts\\msgothic.ttc";
        if (std::filesystem::exists(jpFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(jpFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesJapanese());
        }

        std::string koFont = "c:\\Windows\\Fonts\\malgun.ttf";
        if (std::filesystem::exists(koFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(koFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesKorean());
        }

        std::string thFont = "c:\\Windows\\Fonts\\leelawdb.ttf";
        if (std::filesystem::exists(thFont)) {
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(thFont.c_str(), 18.0f, &config, io.Fonts->GetGlyphRangesThai());
        }

        std::string symbolFont = "c:\\Windows\\Fonts\\seguisym.ttf";
        if (std::filesystem::exists(symbolFont)) {
            static const ImWchar symbolRanges[] = { 0x2600, 0x26FF, 0 };
            config.MergeMode = true;
            io.Fonts->AddFontFromFileTTF(symbolFont.c_str(), 18.0f, &config, symbolRanges);
        }
    }

    inline void InitMapTexture() {
        if (!g_pd3dDevice) return;
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = MAP_DATA_SIZE; desc.Height = MAP_DATA_SIZE;
        desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; desc.CPUAccessFlags = 0;

        if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, NULL, &g_mapTexture))) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = desc.Format; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            if (FAILED(g_pd3dDevice->CreateShaderResourceView(g_mapTexture, &srvDesc, &g_mapTextureView))) {
                g_mapTexture->Release(); g_mapTexture = nullptr;
            }
        }
    }

    inline std::atomic<bool> g_textureBaking{false};
    inline std::atomic<bool> g_textureReadyToUpload{false};

    inline void UpdateMapTexture() {
        if (!g_pd3dDeviceContext || !g_mapTexture) return;

        if (g_mapDataUpdated.load() && !g_textureBaking.load()) {
            g_textureBaking.store(true);
            g_mapDataUpdated.store(false);

            std::thread([]() {
                static mce::Color localColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
                static float localHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];
                float centerX, centerZ;
                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(localColors, g_mapColors, sizeof(localColors));
                    std::memcpy(localHeights, g_mapHeights, sizeof(localHeights));
                    centerX = g_lastRenderX;
                    centerZ = g_lastRenderZ;
                }

                static uint8_t bakedData[MAP_DATA_SIZE * MAP_DATA_SIZE * 4];
                for (int x = 0; x < MAP_DATA_SIZE; x++) {
                    for (int z = 0; z < MAP_DATA_SIZE; z++) {
                        int index = (z * MAP_DATA_SIZE + x) * 4;
                        mce::Color col = localColors[x][z];

                        if (col.a > 0.01f) {
                            float currentY = localHeights[x][z];
                            float northY = currentY, westY = currentY;
                            if (z > 0 && localColors[x][z - 1].a > 0.01f && std::abs(currentY - localHeights[x][z - 1]) < 64.0f) northY = localHeights[x][z - 1];
                            if (x > 0 && localColors[x - 1][z].a > 0.01f && std::abs(currentY - localHeights[x - 1][z]) < 64.0f) westY = localHeights[x - 1][z];

                            float shade = std::clamp(1.0f + (currentY - northY) * 0.15f + (currentY - westY) * 0.15f, 0.65f, 1.25f);
                            bakedData[index]     = (uint8_t)(std::clamp(col.r * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 1] = (uint8_t)(std::clamp(col.g * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 2] = (uint8_t)(std::clamp(col.b * shade, 0.0f, 1.0f) * 255.0f);
                            bakedData[index + 3] = (uint8_t)(col.a * 255.0f);
                        } else {
                            bakedData[index] = bakedData[index+1] = bakedData[index+2] = bakedData[index+3] = 0;
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(g_mapDataMutex);
                    std::memcpy(g_textureData, bakedData, sizeof(bakedData));
                    g_textureCenterX = centerX;
                    g_textureCenterZ = centerZ;
                    g_textureReadyToUpload.store(true);
                }
                g_textureBaking.store(false);
            }).detach();
        }

        if (g_textureReadyToUpload.load()) {
            std::lock_guard<std::mutex> lock(g_mapDataMutex);
            g_pd3dDeviceContext->UpdateSubresource(g_mapTexture, 0, NULL, g_textureData, MAP_DATA_SIZE * 4, 0);
            g_textureReadyToUpload.store(false);
        }
    }

    inline void DrawWaypointIcon(ImDrawList* draw_list, ImVec2 center, mce::Color color, const std::string& name, bool isEdge = false) {
        float size = isEdge ? 5.5f : 8.0f; 
        ImU32 col32 = IM_COL32(color.r * 255.0f, color.g * 255.0f, color.b * 255.0f, 255);
        ImU32 outline = IM_COL32(0, 0, 0, 255); 
        
        ImVec2 pts[4] = {
            ImVec2(center.x, center.y - size),
            ImVec2(center.x + size, center.y),
            ImVec2(center.x, center.y + size),
            ImVec2(center.x - size, center.y)
        };
        
        draw_list->AddConvexPolyFilled(pts, 4, col32);
        draw_list->AddPolyline(pts, 4, outline, ImDrawFlags_Closed, 1.5f);
        
        if (!isEdge && !name.empty()) {
            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize() * MapRenderState::uiTextScale;
            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name.c_str());
            ImVec2 textPos(center.x - textSize.x / 2.0f, center.y + size + 3.0f);
            draw_list->AddText(font, fontSize, ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), name.c_str(), NULL, 0.0f, NULL); 
            draw_list->AddText(font, fontSize, textPos, IM_COL32(255, 255, 255, 255), name.c_str(), NULL, 0.0f, NULL); 
        }
    }

    inline void PointSamplerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        if (!g_pd3dDeviceContext) return;
        static ID3D11SamplerState* pPointSampler = nullptr;
        if (!pPointSampler) {
            D3D11_SAMPLER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.MipLODBias = 0.0f;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            g_pd3dDevice->CreateSamplerState(&desc, &pPointSampler);
        }
        g_pd3dDeviceContext->PSSetSamplers(0, 1, &pPointSampler);
    }

    inline void LinearSamplerCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
        if (!g_pd3dDeviceContext) return;
        static ID3D11SamplerState* pLinearSampler = nullptr;
        if (!pLinearSampler) {
            D3D11_SAMPLER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.MipLODBias = 0.0f;
            desc.MaxAnisotropy = 1;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MinLOD = 0.0f;
            desc.MaxLOD = D3D11_FLOAT32_MAX;
            g_pd3dDevice->CreateSamplerState(&desc, &pLinearSampler);
        }
        g_pd3dDeviceContext->PSSetSamplers(0, 1, &pLinearSampler);
    }

    // ==========================================
    // 【极致平滑引擎】更新核心航位推测坐标
    // ==========================================
    inline void UpdateSmoothCamera() {
        static float s_lastSeenX = g_playerX;
        static float s_lastSeenZ = g_playerZ;
        static float s_velX = 0.0f;
        static float s_velZ = 0.0f;
        static auto s_lastUpdateTime = std::chrono::steady_clock::now();

        // 当底层逻辑坐标更新时，计算真实物理速度
        if (std::abs(g_playerX - s_lastSeenX) > 0.001f || std::abs(g_playerZ - s_lastSeenZ) > 0.001f) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - s_lastUpdateTime).count();
            if (dt > 0.005f && dt < 1.0f) { // 过滤异常时间跳变
                s_velX = (g_playerX - s_lastSeenX) / dt;
                s_velZ = (g_playerZ - s_lastSeenZ) / dt;
            }
            s_lastSeenX = g_playerX;
            s_lastSeenZ = g_playerZ;
            s_lastUpdateTime = now;
        }

        // 超过 150ms 没收到坐标更新，判定玩家已彻底停下，强制阻断速度消除滑步
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::duration<float>>(now - s_lastUpdateTime).count() > 0.15f) {
            s_velX = 0.0f;
            s_velZ = 0.0f;
        }

        float frameDt = ImGui::GetIO().DeltaTime;
        if (frameDt > 0.1f) frameDt = 0.1f;

        // 初始化兜底
        if (g_smoothPX == 0.0f && g_smoothPZ == 0.0f) {
            g_smoothPX = g_playerX;
            g_smoothPZ = g_playerZ;
        }

        // 1. 根据物理速度连续推测坐标 (完全脱离 20Hz 阶梯感)
        g_smoothPX += s_velX * frameDt;
        g_smoothPZ += s_velZ * frameDt;

        // 2. 软弹簧纠偏：微弱拉向真实坐标，防止漂移误差累积
        g_smoothPX += (g_playerX - g_smoothPX) * 12.0f * frameDt;
        g_smoothPZ += (g_playerZ - g_smoothPZ) * 12.0f * frameDt;

        // 3. 传送/死亡瞬间断层强行复位
        if (std::abs(g_playerX - g_smoothPX) > 10.0f || std::abs(g_playerZ - g_smoothPZ) > 10.0f) {
            g_smoothPX = g_playerX;
            g_smoothPZ = g_playerZ;
            s_velX = 0.0f;
            s_velZ = 0.0f;
        }
    }

    // ==========================================
    // [小地图 UI 渲染引擎]
    // ==========================================
    inline void RenderImGuiXaeroMap() {
        if (!MapRenderState::showMiniMap) return; 

        // 【原生界面避让系统】如果我们的模组界面未开启，但系统鼠标却处于显示状态
        // 意味着玩家正处于聊天栏、命令输入、背包或暂停菜单中，此时主动隐藏小地图以避免阻碍视线。
        if (!MapRenderState::IsUIActive()) {
            CURSORINFO ci = {};
            ci.cbSize = sizeof(CURSORINFO);
            if (GetCursorInfo(&ci)) {
                if (ci.flags == CURSOR_SHOWING) {
                    return;
                }
            }
        }

        if (!g_mapTextureView) return;
        UpdateMapTexture();

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        // 乘以小地图大小缩放因子，动态调整地图尺寸
        float IM_MAP_R = std::floor(135.0f * MapRenderState::miniMapScale); 
        float IM_MAP_MARGIN = 20.0f;
        
        // 计算原生基础中心点
        float base_cx = std::floor(ImGui::GetIO().DisplaySize.x - IM_MAP_MARGIN - IM_MAP_R);
        float base_cy = std::floor(IM_MAP_MARGIN + IM_MAP_R);
        
        // 应用偏移量
        float cx = base_cx + MapRenderState::miniMapOffsetX;
        float cy = base_cy + MapRenderState::miniMapOffsetY;

        // 【屏幕越界保护系统】计算文本边界并强行将地图锁死在屏幕内
        float fontSizeBottom = ImGui::GetFontSize() * MapRenderState::uiTextScale;
        float bottomTextSpace = 15.0f + fontSizeBottom * 2.5f + fontSizeBottom; // 底层文本需要的最小冗余高度
        
        float min_cx = IM_MAP_MARGIN + IM_MAP_R;
        float max_cx = ImGui::GetIO().DisplaySize.x - IM_MAP_MARGIN - IM_MAP_R;
        float min_cy = IM_MAP_MARGIN + IM_MAP_R;
        float max_cy = ImGui::GetIO().DisplaySize.y - IM_MAP_MARGIN - IM_MAP_R - bottomTextSpace;

        if (cx < min_cx) cx = min_cx;
        if (cx > max_cx) cx = max_cx;
        if (cy < min_cy) cy = min_cy;
        if (cy > max_cy) cy = max_cy;

        // 反写回真实限制过的偏移值，确保拖动条数值与实际物理边界完美同步
        MapRenderState::miniMapOffsetX = cx - base_cx;
        MapRenderState::miniMapOffsetY = cy - base_cy;
        
        // 强制向下取整，防止 ImGui 渲染到亚像素网格导致 DX11 采样边缘发毛
        cx = std::floor(cx);
        cy = std::floor(cy);

        float pX = g_smoothPX;
        float pZ = g_smoothPZ;

        // 【极致防撕裂核心】UV 偏移只以最新的同步 Texture 中心为基准运算！
        float dx = pX - g_textureCenterX;
        float dz = pZ - g_textureCenterZ;

        float playerYaw = g_localPlayer ? g_localPlayer->getRotation().y : 0.0f;
        // 算出地图需要旋转的弧度：如果要将玩家朝向置于正北，则地图需反向旋转其 yaw 角
        float mapRotateRad = MapRenderState::rotateMiniMap ? -(playerYaw + 180.0f) * (3.14159265f / 180.0f) : 0.0f;
        float c_rot = std::cos(mapRotateRad);
        float s_rot = std::sin(mapRotateRad);

        float ZOOM_RADIUS = 50.0f; 
        float u = 0.5f + (dx / MAP_DATA_SIZE);
        float v = 0.5f + (dz / MAP_DATA_SIZE);
        float uvR = ZOOM_RADIUS / MAP_DATA_SIZE;

        float drawRadius = IM_MAP_R;
        float uvDrawR = uvR;
        // 对于方形地图，如果开启旋转，为了避免边角露馅，渲染内容必须扩大 1.415 倍（正方形对角线长度）
        if (MapRenderState::isSquareMap && MapRenderState::rotateMiniMap) {
            drawRadius = IM_MAP_R * 1.415f;
            uvDrawR = uvR * 1.415f;
        }

        ImVec2 uv0(u - uvDrawR, v - uvDrawR);
        ImVec2 uv1(u + uvDrawR, v + uvDrawR);
        ImVec2 drawMin(cx - drawRadius, cy - drawRadius);
        ImVec2 drawMax(cx + drawRadius, cy + drawRadius);
        ImVec2 mapMin(cx - IM_MAP_R, cy - IM_MAP_R);
        ImVec2 mapMax(cx + IM_MAP_R, cy + IM_MAP_R);

        // 如果是方形地图，直接利用 ImGui 的底层 DrawList 屏幕空间裁剪实现 Mask 遮罩
        if (MapRenderState::isSquareMap) draw_list->PushClipRect(mapMin, mapMax, true);
        
        int vtxStart = draw_list->VtxBuffer.Size;

        if (MapRenderState::isSquareMap) {
            draw_list->AddRectFilled(drawMin, drawMax, IM_COL32(20, 20, 20, 255));
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            draw_list->AddImage((void*)g_mapTextureView, drawMin, drawMax, uv0, uv1, IM_COL32_WHITE);
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
        } else {
            draw_list->AddCircleFilled(ImVec2(cx, cy), drawRadius, IM_COL32(20, 20, 20, 255), 64);
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            draw_list->AddImageRounded((void*)g_mapTextureView, drawMin, drawMax, uv0, uv1, IM_COL32_WHITE, drawRadius);
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
        }

        // 强行在提交前对绘制顶点进行矩阵旋转
        if (MapRenderState::rotateMiniMap) {
            for (int i = vtxStart; i < draw_list->VtxBuffer.Size; i++) {
                ImVec2& p = draw_list->VtxBuffer[i].pos;
                float p_dx = p.x - cx;
                float p_dy = p.y - cy;
                p.x = cx + p_dx * c_rot - p_dy * s_rot;
                p.y = cy + p_dx * s_rot + p_dy * c_rot;
            }
        }

        if (MapRenderState::isSquareMap) draw_list->PopClipRect();

        // 绘制物理外边框
        if (MapRenderState::isSquareMap) {
            draw_list->AddRect(mapMin, mapMax, IM_COL32(30, 30, 30, 255), 0.0f, 0, 2.0f);
        } else {
            draw_list->AddCircle(ImVec2(cx, cy), IM_MAP_R, IM_COL32(30, 30, 30, 255), 64, 2.0f);
        }

        ImFont* font = ImGui::GetFont();
        float fontSize = ImGui::GetFontSize() * MapRenderState::uiTextScale;
        
        char coordBuf[64];
        snprintf(coordBuf, sizeof(coordBuf), "%d, %d, %d", g_playerBlockX, (int)std::floor(g_playerY), g_playerBlockZ);
        ImVec2 coordSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, coordBuf);
        ImVec2 coordPos(cx - coordSize.x / 2, cy + IM_MAP_R + 15 + fontSize); 
        draw_list->AddText(font, fontSize, ImVec2(coordPos.x + 1, coordPos.y + 1), IM_COL32(0,0,0,200), coordBuf, NULL, 0.0f, NULL);
        draw_list->AddText(font, fontSize, coordPos, IM_COL32(255, 255, 255, 255), coordBuf, NULL, 0.0f, NULL);

        std::string biomeStr = MapRenderState::translatedBiomeName;
        ImVec2 biomeSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, biomeStr.c_str());
        ImVec2 biomePos(cx - biomeSize.x / 2, cy + IM_MAP_R + 15 + fontSize * 2.2f);
        draw_list->AddText(font, fontSize, ImVec2(biomePos.x + 1, biomePos.y + 1), IM_COL32(0,0,0,200), biomeStr.c_str(), NULL, 0.0f, NULL);
        draw_list->AddText(font, fontSize, biomePos, IM_COL32(220, 220, 220, 255), biomeStr.c_str(), NULL, 0.0f, NULL);

        // 绘制正向的指南针东南西北（根据地图旋转角度推算正确的位置）
        auto drawRotatedText = [&](const char* text, float offX, float offY) {
            float rotX = offX * c_rot - offY * s_rot;
            float rotY = offX * s_rot + offY * c_rot;
            
            // 如果是方形地图且在旋转，文字会因为距离固定而跑到地图框内部。
            // 这里将其动态投影到方形外边框上，确保方向字母始终在方形外部平移。
            if (MapRenderState::isSquareMap) {
                float maxAxis = std::max(std::abs(rotX), std::abs(rotY));
                if (maxAxis > 0.001f) {
                    float textDist = IM_MAP_R + 4.0f + fontSize / 2.0f;
                    rotX = (rotX / maxAxis) * textDist;
                    rotY = (rotY / maxAxis) * textDist;
                }
            }
            
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            ImVec2 pos(cx + rotX - ts.x / 2.0f, cy + rotY - ts.y / 2.0f);
            draw_list->AddText(font, fontSize, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0,0,0,200), text, NULL, 0.0f, NULL);
            draw_list->AddText(font, fontSize, pos, IM_COL32(220, 220, 255, 255), text, NULL, 0.0f, NULL);
        };
        float textDist = IM_MAP_R + 4.0f + fontSize / 2.0f;
        drawRotatedText(LanguageManager::GetText("COMPASS_N"), 0.0f, -textDist);
        drawRotatedText(LanguageManager::GetText("COMPASS_S"), 0.0f, textDist);
        drawRotatedText(LanguageManager::GetText("COMPASS_E"), textDist, 0.0f);
        drawRotatedText(LanguageManager::GetText("COMPASS_W"), -textDist, 0.0f);

        static std::vector<RadarEntity> s_cachedEntities;
        if (g_radarUpdated.load()) {
            s_cachedEntities = g_radarEntities;
            g_radarUpdated.store(false);
        }

        float scale = IM_MAP_R / ZOOM_RADIUS; 
        for (const auto& ent : s_cachedEntities) {
            float edx = ent.x - pX;
            float edz = ent.z - pZ;
            
            // 应用相对雷达坐标的矩阵旋转
            float rotDx = edx * c_rot - edz * s_rot;
            float rotDz = edx * s_rot + edz * c_rot;
            
            float ex = cx + rotDx * scale;
            float ez = cy + rotDz * scale;
            
            bool inBounds = false;
            if (MapRenderState::isSquareMap) {
                inBounds = (std::abs(rotDx * scale) <= IM_MAP_R && std::abs(rotDz * scale) <= IM_MAP_R);
            } else {
                float distSq = (ex - cx) * (ex - cx) + (ez - cy) * (ez - cy);
                inBounds = (distSq <= IM_MAP_R * IM_MAP_R);
            }

            if (inBounds) {
                ImU32 col;
                if (ent.type == 0) col = IM_COL32(255, 255, 255, 255);
                else if (ent.type == 1) col = IM_COL32(255, 50, 50, 255);
                else if (ent.type == 2) col = IM_COL32(50, 255, 50, 255);
                else col = IM_COL32(255, 255, 50, 255);
                draw_list->AddRectFilled(ImVec2(ex - 2, ez - 2), ImVec2(ex + 2, ez + 2), col);
            }
        }

        {
            std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
            for (const auto& wp : WaypointManager::g_waypoints) {
                if (!wp.enabled) continue;
                
                float wDx = wp.x - pX;
                float wDz = wp.z - pZ;
                float physicalDist = std::sqrt(wDx * wDx + wDz * wDz);
                if (physicalDist < 0.001f) physicalDist = 0.001f;

                float rotDx = wDx * c_rot - wDz * s_rot;
                float rotDz = wDx * s_rot + wDz * c_rot;

                float ex = cx + rotDx * scale;
                float ez = cy + rotDz * scale;
                
                bool inMap = false;
                float edgeX = cx, edgeZ = cy;

                if (MapRenderState::isSquareMap) {
                    if (std::abs(rotDx * scale) <= IM_MAP_R && std::abs(rotDz * scale) <= IM_MAP_R) {
                        inMap = true;
                    } else {
                        float maxDist = std::max(std::abs(rotDx), std::abs(rotDz));
                        edgeX = cx + (rotDx / maxDist) * IM_MAP_R;
                        edgeZ = cy + (rotDz / maxDist) * IM_MAP_R;
                    }
                } else {
                    if (physicalDist <= ZOOM_RADIUS) {
                        inMap = true;
                    } else {
                        edgeX = cx + (rotDx / physicalDist) * IM_MAP_R;
                        edgeZ = cy + (rotDz / physicalDist) * IM_MAP_R;
                    }
                }

                if (inMap) {
                    DrawWaypointIcon(draw_list, ImVec2(ex, ez), mce::Color(wp.r, wp.g, wp.b, 1.0f), wp.name, false);
                } else {
                    DrawWaypointIcon(draw_list, ImVec2(edgeX, edgeZ), mce::Color(wp.r, wp.g, wp.b, 1.0f), "", true);
                }
            }
        }

        // 绘制玩家朝向指示器。如果地图旋转开启，指示器永远朝上 (角度 0)。
        float yawRad = MapRenderState::rotateMiniMap ? 0.0f : (playerYaw + 180.0f) * (3.14159265f / 180.0f); 
        float cosY = std::cos(yawRad);
        float sinY = std::sin(yawRad);
        auto rotate = [&](float x, float y) -> ImVec2 { return ImVec2(cx + (x * cosY - y * sinY), cy + (x * sinY + y * cosY)); };

        draw_list->AddTriangleFilled(rotate(0, -10.0f), rotate(-7.0f, 10.0f), rotate(7.0f, 10.0f), IM_COL32(0, 0, 0, 255));
        draw_list->AddTriangleFilled(rotate(0, -8.0f), rotate(-5.0f, 8.0f), rotate(5.0f, 8.0f), IM_COL32(220, 20, 20, 255));
    }

    inline void UpdateRegionTexture(uint64_t hash, int& texCount) {
        if (!g_pd3dDevice || !g_pd3dDeviceContext) return;
        
        bool isKnown = (g_regionTextures.find(hash) != g_regionTextures.end());
        
        // 1. 【极速状态探测】直接窥探底层 IO 状态，如果缺失直接排队，绝不阻塞主线程，完美修复加载断层
        bool isLoadedAndDirty = false;
        {
            std::lock_guard<std::mutex> lock(MapCacheManager::g_cacheMutex);
            auto it = MapCacheManager::g_loadedRegions.find(hash);
            if (it == MapCacheManager::g_loadedRegions.end()) {
                MapCacheManager::g_loadedRegions[hash] = nullptr;
                MapCacheManager::g_loadQueue.push_back(hash);
                return; // 刚排队，直接闪退
            } else if (it->second != nullptr) {
                isLoadedAndDirty = it->second->textureDirty;
            }
        }

        // 数据没脏就不需要更新
        if (!isLoadedAndDirty) return;

        // 需要建图但本帧建图配额（提升至4以加快大后期渲染）已满，直接返回（保留 Dirty 给下帧）
        if (!isKnown && texCount >= 4) return;

        // 内存对齐以支持极速 64 位空域探测
        alignas(8) static uint8_t tempBuffer[256 * 256 * 4];
        if (MapCacheManager::FetchRegionTextureData(hash, tempBuffer)) {
            
            // 【DrawCall级优化核心】透明区块免疫技术：如果这片区域完全没探索过（纯透明），彻底免渲染！
            bool isEmpty = true;
            uint64_t* ptr64 = (uint64_t*)tempBuffer;
            for (int i = 0; i < (256 * 256 * 4) / 8; ++i) {
                if (ptr64[i] != 0) { isEmpty = false; break; }
            }

            // 若原本未知，或原本是空贴图但现在有了数据
            if (!isKnown || (isKnown && g_regionTextures[hash] == nullptr && !isEmpty)) {
                if (isEmpty) {
                    // 标记为空贴图，不占用显存，极大幅度缩减 ImGui DrawCall 数量
                    g_regionTextures[hash] = nullptr;
                    g_regionSRVs[hash] = nullptr;
                } else {
                    texCount++;
                    D3D11_TEXTURE2D_DESC desc = {};
                    desc.Width = 256; desc.Height = 256;
                    desc.MipLevels = 1; desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    
                    D3D11_SUBRESOURCE_DATA initData = {};
                    initData.pSysMem = tempBuffer;
                    initData.SysMemPitch = 256 * 4;
                    
                    ID3D11Texture2D* tex = nullptr;
                    ID3D11ShaderResourceView* srv = nullptr;
                    
                    if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex))) {
                        g_pd3dDevice->CreateShaderResourceView(tex, NULL, &srv);
                        g_regionTextures[hash] = tex;
                        g_regionSRVs[hash] = srv;
                    }
                }
            } else if (g_regionTextures[hash] != nullptr) {
                g_pd3dDeviceContext->UpdateSubresource(g_regionTextures[hash], 0, NULL, tempBuffer, 256 * 4, 0);
            }
        }
    }

    // ==========================================
    // 提取公共重命名模态弹窗组件
    // ==========================================
    inline void RenderMiniMapPosSettings() {
        static float origX = 0.0f;
        static float origY = 0.0f;
        static float origScale = 1.0f;
        static bool initialized = false;

        if (!MapRenderState::showMiniMapPosSettings) {
            if (initialized) {
                // 窗口异常关闭时的状态还原
                MapRenderState::miniMapOffsetX = origX;
                MapRenderState::miniMapOffsetY = origY;
                MapRenderState::miniMapScale = origScale;
                initialized = false;
            }
            return;
        }

        if (!initialized) {
            origX = MapRenderState::miniMapOffsetX;
            origY = MapRenderState::miniMapOffsetY;
            origScale = MapRenderState::miniMapScale;
            initialized = true;
        }

        // 窗口正居中显示，宽度适度加宽以容纳文字和三个滑块
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing); 

        if (ImGui::Begin(LanguageManager::GetText("MINIMAP_POS_SETTINGS"), &MapRenderState::showMiniMapPosSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
            
            float displayX = ImGui::GetIO().DisplaySize.x;
            float displayY = ImGui::GetIO().DisplaySize.y;
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnSize = ImGui::GetFrameHeight();
            float inputWidth = 55.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float labelWidth = std::max(ImGui::CalcTextSize(LanguageManager::GetText("X_OFFSET")).x, ImGui::CalcTextSize(LanguageManager::GetText("MINIMAP_SCALE")).x);
            float sliderWidth = availWidth - labelWidth - (btnSize * 2.0f) - inputWidth - (spacing * 4.0f) - 15.0f;

            float IM_MAP_R = std::floor(135.0f * MapRenderState::miniMapScale);
            float IM_MAP_MARGIN = 20.0f;
            float fontSize = ImGui::GetFontSize() * MapRenderState::uiTextScale;
            float bottomTextSpace = 15.0f + fontSize * 2.5f + fontSize;

            // 动态计算绝对边界范围（剔除了无意义的多余滑动区域）
            float minOffsetX = 2.0f * (IM_MAP_MARGIN + IM_MAP_R) - displayX;
            float maxOffsetX = 0.0f;
            float minOffsetY = 0.0f;
            float maxOffsetY = displayY - 2.0f * (IM_MAP_MARGIN + IM_MAP_R) - bottomTextSpace;

            auto drawRow = [&](const char* label, const char* idSlider, const char* idSub, const char* idInput, const char* idAdd, float& value, float minVal, float maxVal, float step, const char* format) {
                ImGui::Text("%s", label);
                ImGui::SameLine(labelWidth + 15.0f);
                
                ImGui::PushItemWidth(sliderWidth);
                ImGui::SliderFloat(idSlider, &value, minVal, maxVal, format);
                ImGui::PopItemWidth();
                
                ImGui::SameLine();
                if (ImGui::ArrowButton(idSub, ImGuiDir_Left)) value -= step; 
                
                ImGui::SameLine();
                ImGui::PushItemWidth(inputWidth);
                ImGui::InputFloat(idInput, &value, 0.0f, 0.0f, format);
                ImGui::PopItemWidth();
                
                ImGui::SameLine();
                if (ImGui::ArrowButton(idAdd, ImGuiDir_Right)) value += step; 
            };

            drawRow(LanguageManager::GetText("X_OFFSET"), "##XSlider", "##XSub", "##XInput", "##XAdd", MapRenderState::miniMapOffsetX, minOffsetX, maxOffsetX, 5.0f, "%.0f");
            drawRow(LanguageManager::GetText("Y_OFFSET"), "##YSlider", "##YSub", "##YInput", "##YAdd", MapRenderState::miniMapOffsetY, minOffsetY, maxOffsetY, 5.0f, "%.0f");
            drawRow(LanguageManager::GetText("MINIMAP_SCALE"), "##ScaleSlider", "##ScaleSub", "##ScaleInput", "##ScaleAdd", MapRenderState::miniMapScale, 0.2f, 2.5f, 0.1f, "%.2f");

            if (MapRenderState::miniMapScale < 0.2f) MapRenderState::miniMapScale = 0.2f;

            ImGui::Spacing();
            float btnWidth = (availWidth - spacing) / 2.0f;
            
            if (ImGui::Button(LanguageManager::GetText("DEFAULT_POS"), ImVec2(btnWidth, 0))) {
                MapRenderState::miniMapOffsetX = 0.0f;
                MapRenderState::miniMapOffsetY = 0.0f;
                MapRenderState::miniMapScale = 1.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("TOP_LEFT_POS"), ImVec2(btnWidth, 0))) {
                float current_R = std::floor(135.0f * MapRenderState::miniMapScale);
                MapRenderState::miniMapOffsetX = - (displayX - (IM_MAP_MARGIN + current_R) * 2.0f);
                MapRenderState::miniMapOffsetY = 0.0f; 
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(LanguageManager::GetText("SAVE_AND_EXIT"), ImVec2(btnWidth, 0))) {
                LanguageManager::SaveConfig();
                MapRenderState::showMiniMapPosSettings = false;
                MapRenderState::showBigMap = true;
                initialized = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("DONT_SAVE"), ImVec2(btnWidth, 0))) {
                MapRenderState::miniMapOffsetX = origX;
                MapRenderState::miniMapOffsetY = origY;
                MapRenderState::miniMapScale = origScale;
                MapRenderState::showMiniMapPosSettings = false;
                MapRenderState::showBigMap = true;
                initialized = false;
            }
        }
        ImGui::End();
    }

    inline void RenderRenameModal(const char* modalId, std::string& wpId, bool& trigger) {
        if (trigger) {
            ImGui::OpenPopup(modalId);
            trigger = false;
        }
        if (ImGui::BeginPopupModal(modalId, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char renameBuf[256] = "";
            static bool initialized = false;
            
            Waypoint targetWp;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for(auto& w : WaypointManager::g_waypoints) {
                    if(w.id == wpId) { targetWp = w; found = true; break; }
                }
            }
            
            if (!found) {
                ImGui::CloseCurrentPopup();
            } else {
                if (!initialized) {
                    snprintf(renameBuf, sizeof(renameBuf), "%s", targetWp.name.c_str());
                    initialized = true;
                }
                
                ImGui::InputText(LanguageManager::GetText("WP_NAME"), renameBuf, sizeof(renameBuf));
                ImGui::Spacing();
                
                if (ImGui::Button(LanguageManager::GetText("WP_SAVE"), ImVec2(120, 0))) {
                    {
                        std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                        for(auto& w : WaypointManager::g_waypoints) {
                            if(w.id == wpId) { 
                                w.name = renameBuf; 
                                break; 
                            }
                        }
                    }
                    WaypointManager::SaveWaypoints();
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("WP_CANCEL"), ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    initialized = false;
                }
            }
            ImGui::EndPopup();
        }
    }

    inline void RenderImGuiBigMap() {
        if (MapRenderState::clearGPUCache.load()) {
            for(auto& p : g_regionSRVs) if(p.second) p.second->Release();
            g_regionSRVs.clear();
            for(auto& p : g_regionTextures) if(p.second) p.second->Release();
            g_regionTextures.clear();
            MapRenderState::clearGPUCache.store(false);
        }

        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | 
                                        ImGuiWindowFlags_NoBackground;
        
        ImGui::Begin("BigMapCanvas", nullptr, window_flags);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        bool isHoveringCanvas = ImGui::IsWindowHovered();

        static bool s_isDraggingMap = false;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isHoveringCanvas) {
            s_isDraggingMap = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            s_isDraggingMap = false;
        }

        if (s_isDraggingMap) {
            MapRenderState::bigMapOffsetX += io.MouseDelta.x;
            MapRenderState::bigMapOffsetZ += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f && isHoveringCanvas) {
            float oldZoom = MapRenderState::bigMapZoom;
            float zoomSpeed = 0.15f * oldZoom;
            MapRenderState::bigMapZoom += io.MouseWheel * zoomSpeed;
            if (MapRenderState::bigMapZoom < 0.2f) MapRenderState::bigMapZoom = 0.2f;
            if (MapRenderState::bigMapZoom > 40.0f) MapRenderState::bigMapZoom = 40.0f;
            
            float k = MapRenderState::bigMapZoom / oldZoom;
            float cx = io.DisplaySize.x * 0.5f + MapRenderState::bigMapOffsetX;
            float cy = io.DisplaySize.y * 0.5f + MapRenderState::bigMapOffsetZ;
            float dx = io.MousePos.x - cx;
            float dy = io.MousePos.y - cy;
            
            MapRenderState::bigMapOffsetX -= dx * (k - 1.0f);
            MapRenderState::bigMapOffsetZ -= dy * (k - 1.0f);
        }

        draw_list->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(20, 20, 20, 255));

        float cx = io.DisplaySize.x * 0.5f;
        float cy = io.DisplaySize.y * 0.5f;
        
        float minWx = g_smoothPX - (cx + MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float maxWx = g_smoothPX + (cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float minWz = g_smoothPZ - (cy + MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;
        float maxWz = g_smoothPZ + (cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        int startRx = (int)std::floor(minWx / 256.0f);
        int endRx   = (int)std::floor(maxWx / 256.0f);
        int startRz = (int)std::floor(minWz / 256.0f);
        int endRz   = (int)std::floor(maxWz / 256.0f);

        int texturesCreatedThisFrame = 0;
        if (MapRenderState::currentDimensionId != 1) {
            static int s_vramGcTimer = 0;
            if (++s_vramGcTimer > 300) {
                s_vramGcTimer = 0;
                std::vector<uint64_t> keysToErase;
                for (auto& p : g_regionSRVs) {
                    int rx = (int)(p.first >> 32);
                    int rz = (int)(p.first & 0xFFFFFFFF);
                    if (rx < startRx - 5 || rx > endRx + 5 || rz < startRz - 5 || rz > endRz + 5) {
                        keysToErase.push_back(p.first);
                    }
                }
                for (uint64_t k : keysToErase) {
                    if (g_regionSRVs[k]) g_regionSRVs[k]->Release();
                    g_regionSRVs.erase(k);
                    if (g_regionTextures[k]) g_regionTextures[k]->Release();
                    g_regionTextures.erase(k);
                    MapCacheManager::MarkTextureDirty(k);
                }
            }

            draw_list->AddCallback(PointSamplerCallback, nullptr);
            
            for (int rx = startRx; rx <= endRx; rx++) {
                for (int rz = startRz; rz <= endRz; rz++) {
                    uint64_t hash = MapCacheManager::GetRegionHash(rx, rz);
                    UpdateRegionTexture(hash, texturesCreatedThisFrame);

                    if (g_regionSRVs.find(hash) != g_regionSRVs.end()) {
                        // 【渲染管线减负】只有包含实际像素的非空贴图才会被加入 ImGui 的 DrawCall 绘制队列！
                        // 极大减负显卡在微缩大地图时的渲染压力，实现绝对满帧体验。
                        if (g_regionSRVs[hash] != nullptr) {
                            float sx_min = std::floor(cx + (rx * 256.0f - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                            float sy_min = std::floor(cy + (rz * 256.0f - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                            float sx_max = std::floor(cx + ((rx + 1) * 256.0f - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                            float sy_max = std::floor(cy + ((rz + 1) * 256.0f - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                            
                            draw_list->AddImage((void*)g_regionSRVs[hash], ImVec2(sx_min, sy_min), ImVec2(sx_max, sy_max));
                        }
                    }
                }
            }
            
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
        } else {
            ImFont* font = ImGui::GetFont();
            float fontSize = ImGui::GetFontSize() * MapRenderState::uiTextScale;
            const char* netherMsg = LanguageManager::GetText("NETHER_WARNING");
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, netherMsg);
            draw_list->AddText(font, fontSize, ImVec2(cx - ts.x / 2, cy - ts.y / 2 - 50.0f), IM_COL32(255, 80, 80, 200), netherMsg, NULL, 0.0f, NULL);
        }
        
        float px = cx + MapRenderState::bigMapOffsetX;
        float py = cy + MapRenderState::bigMapOffsetZ;
        
        float yawRad = (g_playerYaw + 180.0f) * (3.14159265f / 180.0f); 
        float cosY = std::cos(yawRad);
        float sinY = std::sin(yawRad);
        auto rotate = [&](float x, float y) -> ImVec2 { 
            return ImVec2(px + (x * cosY - y * sinY), py + (x * sinY + y * cosY)); 
        };

        draw_list->AddTriangleFilled(rotate(0, -10.0f), rotate(-7.0f, 10.0f), rotate(7.0f, 10.0f), IM_COL32(0, 0, 0, 255));
        draw_list->AddTriangleFilled(rotate(0, -8.0f), rotate(-5.0f, 8.0f), rotate(5.0f, 8.0f), IM_COL32(220, 20, 20, 255));

        static std::string selectedWpId = "";
        static bool triggerWpMenu = false;
        {
            std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
            for (const auto& wp : WaypointManager::g_waypoints) {
                if (!wp.enabled) continue;
                
                float wx = cx + (wp.x - g_smoothPX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX;
                float wz = cy + (wp.z - g_smoothPZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ;
                
                if (wx > -50.0f && wx < io.DisplaySize.x + 50.0f && wz > -50.0f && wz < io.DisplaySize.y + 50.0f) {
                    DrawWaypointIcon(draw_list, ImVec2(wx, wz), mce::Color(wp.r, wp.g, wp.b, 1.0f), wp.name, false);
                    
                    float distSq = (io.MousePos.x - wx) * (io.MousePos.x - wx) + (io.MousePos.y - wz) * (io.MousePos.y - wz);
                    if (distSq <= 144.0f && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        selectedWpId = wp.id;
                        triggerWpMenu = true;
                    }
                }
            }
        }

        float hoverWx = g_smoothPX + (io.MousePos.x - cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float hoverWz = g_smoothPZ + (io.MousePos.y - cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        ImFont* mFont = ImGui::GetFont();
        float mFontSize = ImGui::GetFontSize() * MapRenderState::uiTextScale;
        
        char infoBuf[256];
        snprintf(infoBuf, sizeof(infoBuf), LanguageManager::GetText("BIGMAP_TITLE"), MapRenderState::bigMapZoom);
        draw_list->AddText(mFont, mFontSize, ImVec2(20, 20), IM_COL32(255, 200, 50, 255), infoBuf, NULL, 0.0f, NULL);
        draw_list->AddText(mFont, mFontSize, ImVec2(20, 20 + mFontSize + 5), IM_COL32(200, 200, 200, 255), LanguageManager::GetText("BIGMAP_HELP"), NULL, 0.0f, NULL);
        
        snprintf(infoBuf, sizeof(infoBuf), LanguageManager::GetText("CURSOR_POS"), (int)std::floor(hoverWx), (int)std::floor(hoverWz));
        ImVec2 textSize = mFont->CalcTextSizeA(mFontSize, FLT_MAX, 0.0f, infoBuf);
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - textSize.x / 2 - 15, io.DisplaySize.y - textSize.y - 25), 
                                 ImVec2(io.DisplaySize.x / 2 + textSize.x / 2 + 15, io.DisplaySize.y - 10), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(mFont, mFontSize, ImVec2(io.DisplaySize.x / 2 - textSize.x / 2, io.DisplaySize.y - textSize.y - 17), IM_COL32(255, 255, 255, 255), infoBuf, NULL, 0.0f, NULL);

        char biomeBuf[512];
        std::string combinedBiome = MapRenderState::rawBiomeName + " (" + MapRenderState::translatedBiomeName + ")";
        snprintf(biomeBuf, sizeof(biomeBuf), LanguageManager::GetText("BIOME_LABEL"), combinedBiome.c_str());
        ImVec2 biomeTextSize = mFont->CalcTextSizeA(mFontSize, FLT_MAX, 0.0f, biomeBuf);
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2 - 20, 15), 
                                 ImVec2(io.DisplaySize.x / 2 + biomeTextSize.x / 2 + 20, 15 + biomeTextSize.y + 15), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(mFont, mFontSize, ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2, 22), IM_COL32(180, 255, 180, 255), biomeBuf, NULL, 0.0f, NULL);

        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 240, 20));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
        ImGui::BeginChild("MapSidebar", ImVec2(220, 150), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), LanguageManager::GetText("SIDEBAR_PLAYER_STATUS"));
        ImGui::Separator();
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_X"), g_playerBlockX);
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_Y"), (int)std::floor(g_playerY));
        ImGui::Text(LanguageManager::GetText("PLAYER_POS_Z"), g_playerBlockZ);
        
        ImGui::Spacing(); ImGui::Spacing();
        
        // 居中并排摆放 [⛶ 视角回中] 与 [⚙ 齿轮设置] 按钮
        float btnWidth = 35.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - (btnWidth * 2.0f + spacing)) * 0.5f);
        
        if (ImGui::Button("\xe2\x9b\xb6", ImVec2(btnWidth, btnWidth))) { // U+26F6 (Square with crosshairs)
            MapRenderState::bigMapOffsetX = 0.0f;
            MapRenderState::bigMapOffsetZ = 0.0f;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", LanguageManager::GetText("CENTER_CAMERA"));
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("\xe2\x9a\x99", ImVec2(btnWidth, btnWidth))) { // U+2699 (Gear)
            ImGui::OpenPopup("SettingsPopup");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", LanguageManager::GetText("SETTINGS_TOOLTIP"));
        }
        
        ImGui::SetNextWindowSize(ImVec2(290, 270));
        if (ImGui::BeginPopup("SettingsPopup")) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), LanguageManager::GetText("SIDEBAR_OPS"));
            ImGui::Separator();
            
            if (ImGui::Checkbox(LanguageManager::GetText("SHOW_MINIMAP"), &MapRenderState::showMiniMap)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::Checkbox(LanguageManager::GetText("SQUARE_MINIMAP"), &MapRenderState::isSquareMap)) {
                LanguageManager::SaveConfig();
            }
            if (ImGui::Checkbox(LanguageManager::GetText("ROTATE_MINIMAP"), &MapRenderState::rotateMiniMap)) {
                LanguageManager::SaveConfig();
            }
            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LanguageManager::GetText("TEXT_SCALE"));
            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnSize = ImGui::GetFrameHeight();
            float inputWidth = 45.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float sliderWidth = availWidth - (btnSize * 2.0f) - inputWidth - (spacing * 3.0f);
            
            bool tScaleChanged = false;
            
            ImGui::PushItemWidth(sliderWidth);
            tScaleChanged |= ImGui::SliderFloat("##TextScaleSlider", &MapRenderState::uiTextScale, 0.5f, 2.5f, "%.2f x");
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::ArrowButton("##TextScaleSub", ImGuiDir_Left)) {
                MapRenderState::uiTextScale -= 0.1f;
                tScaleChanged = true;
            }
            
            ImGui::SameLine();
            ImGui::PushItemWidth(inputWidth);
            tScaleChanged |= ImGui::InputFloat("##TextInput", &MapRenderState::uiTextScale, 0.0f, 0.0f, "%.2f");
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::ArrowButton("##TextScaleAdd", ImGuiDir_Right)) {
                MapRenderState::uiTextScale += 0.1f;
                tScaleChanged = true;
            }
            
            if (tScaleChanged) {
                if (MapRenderState::uiTextScale < 0.1f) MapRenderState::uiTextScale = 0.1f;
                LanguageManager::SaveConfig();
            }
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            if (ImGui::Button(LanguageManager::GetText("EDIT_MINIMAP_POS"), ImVec2(-1, 0))) {
                MapRenderState::showMiniMapPosSettings = true;
                MapRenderState::showBigMap = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::Spacing();

            std::string previewName = LanguageManager::g_currentLanguage;
            for (const auto& p : LanguageManager::g_availableLanguages) {
                if (p.first == LanguageManager::g_currentLanguage) {
                    previewName = p.second;
                    break;
                }
            }
            
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LanguageManager::GetText("LANG_SELECT"));
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##LangSelectCombo", previewName.c_str())) {
                for (const auto& p : LanguageManager::g_availableLanguages) {
                    bool isSelected = (LanguageManager::g_currentLanguage == p.first);
                    if (ImGui::Selectable(p.second.c_str(), isSelected)) {
                        LanguageManager::g_currentLanguage = p.first;
                        LanguageManager::LoadLanguage(p.first);
                        LanguageManager::SaveConfig();
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            
            ImGui::EndPopup();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();

        static float rcWorldX = 0.0f;
        static float rcWorldZ = 0.0f;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            rcWorldX = hoverWx;
            rcWorldZ = hoverWz;
            ImGui::OpenPopup("BigMapContextMenu");
        }

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        if (ImGui::BeginPopup("BigMapContextMenu")) {
            int bx = (int)std::floor(rcWorldX);
            int bz = (int)std::floor(rcWorldZ);
            
            // 【完成需求2】全屏大地图添加地标时，瞬间获取真正的底层地形表面高度
            int by = 320; 
            if (g_clientInstance) {
                BlockSource* region = g_clientInstance->getRegion();
                if (region) {
                    short topY = region->getAboveTopSolidBlock(bx, bz, true, true);
                    if (topY > -60 && topY < 319) {
                        by = (int)topY + 1; // 地形如果已经加载过，立刻抓取地表最高点！
                    }
                }
            }

            ImVec2 titleSize = ImGui::CalcTextSize(LanguageManager::GetText("CONTEXT_TITLE"));
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
            ImGui::Text("%s", LanguageManager::GetText("CONTEXT_TITLE"));
            ImGui::Separator();

            char chunkBuf[64]; snprintf(chunkBuf, sizeof(chunkBuf), LanguageManager::GetText("CHUNK_POS"), bx >> 4, bz >> 4);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(chunkBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", chunkBuf);

            char blockBuf[64]; snprintf(blockBuf, sizeof(blockBuf), LanguageManager::GetText("BLOCK_POS"), bx, by, bz);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(blockBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", blockBuf);
            ImGui::Separator();

            if (ImGui::Selectable(LanguageManager::GetText("COPY_COORDS"))) {
                char buf[128]; snprintf(buf, sizeof(buf), "%d %d %d", bx, by, bz);
                ImGui::SetClipboardText(buf);
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable(LanguageManager::GetText("CREATE_WAYPOINT"))) {
                MapRenderState::addWaypointX = bx;
                MapRenderState::addWaypointY = by;
                MapRenderState::addWaypointZ = bz;
                MapRenderState::triggerAddWaypoint = true;
                MapRenderState::showWaypointUI = true;
            }
            
            if (ImGui::Selectable(LanguageManager::GetText("TELEPORT_HERE"))) {
                MapRenderState::tpTargetX = (float)bx + 0.5f;
                MapRenderState::tpTargetY = (float)by; 
                MapRenderState::tpTargetZ = (float)bz + 0.5f;
                MapRenderState::triggerTeleport.store(true);
                MapRenderState::showBigMap = false; 
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable(LanguageManager::GetText("OPEN_WP_MENU"))) {
                MapRenderState::showWaypointUI = true;
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);

        if (triggerWpMenu) {
            ImGui::OpenPopup("WaypointContextMenu");
            triggerWpMenu = false;
        }

        static std::string bigMapRenameId = "";
        static bool bigMapTriggerRename = false;

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        if (ImGui::BeginPopup("WaypointContextMenu")) {
            Waypoint targetWp;
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for(auto& w : WaypointManager::g_waypoints) {
                    if(w.id == selectedWpId) {
                        targetWp = w;
                        found = true; 
                        break;
                    }
                }
            }

            if (found) {
                ImVec2 titleSize = ImGui::CalcTextSize(targetWp.name.c_str());
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
                ImGui::TextColored(ImVec4(targetWp.r, targetWp.g, targetWp.b, 1.0f), "%s", targetWp.name.c_str());
                ImGui::Separator();
                
                char coordBuf[64]; snprintf(coordBuf, sizeof(coordBuf), "X: %d, Y: %d, Z: %d", targetWp.x, targetWp.y, targetWp.z);
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(coordBuf).x) * 0.5f);
                ImGui::TextDisabled("%s", coordBuf);
                ImGui::Separator();
                
                if (ImGui::Selectable(LanguageManager::GetText("TELEPORT_WP"))) {
                    MapRenderState::tpTargetX = (float)targetWp.x + 0.5f;
                    MapRenderState::tpTargetY = (float)targetWp.y; 
                    MapRenderState::tpTargetZ = (float)targetWp.z + 0.5f;
                    MapRenderState::triggerTeleport.store(true);
                    MapRenderState::showBigMap = false;
                }
                
                if (ImGui::Selectable(LanguageManager::GetText("RENAME_WP"))) {
                    bigMapRenameId = selectedWpId;
                    bigMapTriggerRename = true;
                }
                
                ImGui::Separator();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Selectable(LanguageManager::GetText("DELETE_WP"))) {
                    WaypointManager::RemoveWaypoint(selectedWpId);
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor(2);

        // 调用大地图右键地标重命名弹窗模块
        RenderRenameModal((std::string(LanguageManager::GetText("RENAME_WP")) + "##ModalBigMap").c_str(), bigMapRenameId, bigMapTriggerRename);

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    // ==========================================
    // 路径点 ImGui 管理控制台 (添加搜索、重命名与传送)
    // ==========================================
    inline void RenderImGuiWaypointUI() {
        ImGui::SetNextWindowSize(ImVec2(750, 480), ImGuiCond_FirstUseEver); 
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2 - 375, ImGui::GetIO().DisplaySize.y / 2 - 240), ImGuiCond_FirstUseEver);
        
        ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin(LanguageManager::GetText("WP_MANAGER_TITLE"), &MapRenderState::showWaypointUI, winFlags)) {
            
            static bool showAddPopup = false;
            static char searchBuf[256] = "";
            
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
            ImGui::InputTextWithHint("##WPSearch", LanguageManager::GetText("SEARCH_HINT"), searchBuf, sizeof(searchBuf));
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button(LanguageManager::GetText("NEW_WP_BUTTON"), ImVec2(140, 0))) {
                showAddPopup = true;
            }
            ImGui::Separator();

            ImGui::BeginChild("WPList", ImVec2(0, 0), true);
            std::string toDelete = "";
            bool toggled = false;
            bool triggerTp = false; 

            static std::string uiRenameId = "";
            static bool uiTriggerRename = false;

            std::string query = searchBuf;
            for (char& c : query) { if (c >= 'A' && c <= 'Z') c += 32; }

            {
                std::lock_guard<std::mutex> lock(WaypointManager::g_wpMutex);
                for (auto& wp : WaypointManager::g_waypoints) {
                    
                    if (!query.empty()) {
                        std::string lowerName = wp.name;
                        for (char& c : lowerName) { if (c >= 'A' && c <= 'Z') c += 32; }
                        if (lowerName.find(query) == std::string::npos) {
                            continue; 
                        }
                    }

                    ImGui::PushID(wp.id.c_str());
                    
                    ImGui::ColorButton("##color", ImVec4(wp.r, wp.g, wp.b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24));
                    ImGui::SameLine();
                    
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                    ImGui::Text("%s", wp.name.c_str());
                    
                    float winWidth = ImGui::GetWindowWidth();
                    
                    ImGui::SameLine(winWidth > 750 ? winWidth - 490 : 200);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4);
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "X: %d  Y: %d  Z: %d", wp.x, wp.y, wp.z);
                    
                    ImGui::SameLine(winWidth - 270);
                    bool enabled = wp.enabled;
                    if (ImGui::Checkbox(LanguageManager::GetText("WP_LIST_SHOW"), &enabled)) {
                        wp.enabled = enabled;
                        toggled = true;
                    }
                    
                    ImGui::SameLine(winWidth - 195);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_RENAME"), ImVec2(55, 0))) {
                        uiRenameId = wp.id;
                        uiTriggerRename = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(winWidth - 135);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_TELEPORT"), ImVec2(45, 0))) {
                        MapRenderState::tpTargetX = (float)wp.x + 0.5f;
                        MapRenderState::tpTargetY = (float)wp.y; 
                        MapRenderState::tpTargetZ = (float)wp.z + 0.5f;
                        MapRenderState::triggerTeleport.store(true);
                        triggerTp = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(winWidth - 75);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                    if (ImGui::Button(LanguageManager::GetText("WP_LIST_DELETE"), ImVec2(45, 0))) {
                        toDelete = wp.id;
                    }
                    ImGui::PopStyleColor(3);
                    
                    ImGui::PopID();
                    ImGui::Separator();
                }
            } 

            if (!toDelete.empty()) {
                WaypointManager::RemoveWaypoint(toDelete);
                WaypointManager::SaveWaypoints();
            } else if (toggled) {
                WaypointManager::SaveWaypoints();
            }
            
            if (triggerTp) {
                MapRenderState::showWaypointUI = false;
                MapRenderState::showBigMap = false;
            }
            
            ImGui::EndChild();

            // 调用 UI 列表专属重命名弹窗模块
            RenderRenameModal((std::string(LanguageManager::GetText("RENAME_WP")) + "##ModalUI").c_str(), uiRenameId, uiTriggerRename);

            if (MapRenderState::triggerAddWaypoint) {
                showAddPopup = true;
                MapRenderState::triggerAddWaypoint = false;
            }

            if (showAddPopup) ImGui::OpenPopup(LanguageManager::GetText("NEW_WP_TITLE"));
            
            if (ImGui::BeginPopupModal(LanguageManager::GetText("NEW_WP_TITLE"), &showAddPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                static char nameBuf[256] = "";
                static int pos[3] = {0, 0, 0};
                static float col[3] = {1.0f, 0.3f, 0.3f};
                
                if (ImGui::IsWindowAppearing()) {
                    nameBuf[0] = '\0'; 
                    pos[0] = (MapRenderState::addWaypointX != -999999) ? MapRenderState::addWaypointX : g_playerBlockX;
                    pos[1] = (MapRenderState::addWaypointY != -999999) ? MapRenderState::addWaypointY : (int)std::floor(g_playerY);
                    pos[2] = (MapRenderState::addWaypointZ != -999999) ? MapRenderState::addWaypointZ : g_playerBlockZ;
                    
                    MapRenderState::addWaypointX = -999999;
                    MapRenderState::addWaypointY = -999999;
                    MapRenderState::addWaypointZ = -999999;
                }

                ImGui::InputText(LanguageManager::GetText("WP_NAME"), nameBuf, sizeof(nameBuf));
                ImGui::InputInt3("X / Y / Z", pos);
                ImGui::ColorEdit3(LanguageManager::GetText("WP_COLOR"), col);
                
                ImGui::Spacing();
                if (ImGui::Button(LanguageManager::GetText("WP_SAVE"), ImVec2(120, 0))) {
                    WaypointManager::AddWaypoint(nameBuf, pos[0], pos[1], pos[2], col[0], col[1], col[2]);
                    showAddPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(LanguageManager::GetText("WP_CANCEL"), ImVec2(120, 0))) {
                    showAddPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    inline void RenderImGui(IDXGISwapChain* pSwapChain) {
        static std::atomic<bool> isRendering{false};
        if (isRendering.exchange(true)) return;

        if (g_clientInstance) {
            __try {
                if (g_clientInstance->isShowingLoadingScreen() ||
                    g_clientInstance->isShowingProgressScreen() ||
                    g_clientInstance->isShowingWorldProgressScreen() ||
                    g_clientInstance->isShowingDeathScreen() ||
                    (g_clientInstance->isShowingPauseScreen() && !MapRenderState::IsUIActive())) {
                    isRendering = false;
                    return;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                isRendering = false;
                return;
            }
        }

        static bool initAttempted = false; 
        if (!g_imguiInitialized && !initAttempted) {
            HRESULT hr11 = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
            if (SUCCEEDED(hr11)) {
                initAttempted = true;
                g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);
            } else {
                if (!g_pGameCommandQueue) { isRendering = false; return; }
                initAttempted = true;
                ID3D12Device* pD3D12Device = nullptr;
                if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pD3D12Device))) {
                    if (SUCCEEDED(D3D11On12CreateDevice(pD3D12Device, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, (IUnknown**)&g_pGameCommandQueue, 1, 0, &g_pd3dDevice, &g_pd3dDeviceContext, nullptr))) {
                        g_pd3dDevice->QueryInterface(__uuidof(ID3D11On12Device), (void**)&g_d3d11On12Device);
                    }
                    pD3D12Device->Release();
                }
            }

            if (g_pd3dDevice) {
                DXGI_SWAP_CHAIN_DESC sd;
                pSwapChain->GetDesc(&sd);
                g_hWnd = sd.OutputWindow;
                if (!g_hWnd) g_hWnd = FindWindowW(L"Minecraft", NULL);

                oWndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();

                InitImGuiFonts(io);
                ImGui_ImplWin32_Init(g_hWnd);
                ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
                
                InitMapTexture(); 
                g_imguiInitialized = true;
            }
        }

        if (g_imguiInitialized && g_hasPlayer) {
            auto renderImGuiFrame = [&](ID3D11RenderTargetView* rtv) {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &rtv, NULL);
                ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
                ImGui::GetIO().MouseDrawCursor = MapRenderState::IsUIActive();

                UpdateSmoothCamera(); // 更新底层的无极平滑推测坐标！

                if (MapRenderState::showBigMap) {
                    RenderImGuiBigMap();
                } else {
                    RenderImGuiXaeroMap();
                }

                if (MapRenderState::showWaypointUI) {
                    RenderImGuiWaypointUI();
                }

                // 无条件调用，内含状态检测，处理还原逻辑
                RenderMiniMapPosSettings();

                ImGui::Render();
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                ID3D11RenderTargetView* nullRTV = nullptr;
                g_pd3dDeviceContext->OMSetRenderTargets(1, &nullRTV, NULL);
                rtv->Release();
            };

            if (g_d3d11On12Device) {
                UINT bufferIndex = 0;
                IDXGISwapChain3* pSwapChain3 = nullptr;
                if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pSwapChain3))) {
                    bufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
                    pSwapChain3->Release();
                }

                ID3D12Resource* d3d12BackBuffer = nullptr;
                if (SUCCEEDED(pSwapChain->GetBuffer(bufferIndex, __uuidof(ID3D12Resource), (void**)&d3d12BackBuffer))) {
                    ID3D11Resource* wrappedBackBuffer = nullptr;
                    D3D11_RESOURCE_FLAGS d3d11Flags = {D3D11_BIND_RENDER_TARGET};

                    if (SUCCEEDED(g_d3d11On12Device->CreateWrappedResource(
                        d3d12BackBuffer, &d3d11Flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT, __uuidof(ID3D11Resource), (void**)&wrappedBackBuffer)))
                    {
                        ID3D11RenderTargetView* rtv = nullptr;
                        g_pd3dDevice->CreateRenderTargetView(wrappedBackBuffer, NULL, &rtv);
                        g_d3d11On12Device->AcquireWrappedResources(&wrappedBackBuffer, 1);

                        if (rtv) renderImGuiFrame(rtv);

                        g_d3d11On12Device->ReleaseWrappedResources(&wrappedBackBuffer, 1);
                        wrappedBackBuffer->Release();

                        g_pd3dDeviceContext->ClearState();
                        g_pd3dDeviceContext->Flush();
                    }
                    d3d12BackBuffer->Release();
                }
            } else {
                ID3D11Texture2D* pBackBuffer = nullptr;
                if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer))) {
                    ID3D11RenderTargetView* rtv = nullptr;
                    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &rtv);
                    pBackBuffer->Release();
                    if (rtv) {
                        renderImGuiFrame(rtv);
                        g_pd3dDeviceContext->ClearState();
                        g_pd3dDeviceContext->Flush();
                    }
                }
            }
        }
        isRendering = false;
    }

    inline HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        RenderImGui(pSwapChain);
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    inline HRESULT __stdcall hkPresent1(IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pParams) {
        RenderImGui(pSwapChain);
        return oPresent1(pSwapChain, SyncInterval, Flags, pParams);
    }

    inline bool init() {
        HWND hwnd = FindWindowW(L"Minecraft", NULL);
        if (!hwnd) hwnd = GetForegroundWindow();
        if (!hwnd) return false;
        
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* dummyDevice = nullptr;
        IDXGISwapChain* dummySwapChain = nullptr;
        ID3D11DeviceContext* dummyContext = nullptr;

        MH_STATUS status = MH_Initialize();

        if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1, D3D11_SDK_VERSION, &sd, &dummySwapChain, &dummyDevice, NULL, &dummyContext))) {
            void** pVTable = *reinterpret_cast<void***>(dummySwapChain);
            if (status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED) {
                MH_CreateHook(pVTable[8], (LPVOID)hkPresent, (void**)&oPresent);
                MH_EnableHook(pVTable[8]);

                MH_CreateHook(pVTable[13], (LPVOID)hkResizeBuffers, (void**)&oResizeBuffers);
                MH_EnableHook(pVTable[13]);

                IDXGISwapChain1* dummySwapChain1 = nullptr;
                if (SUCCEEDED(dummySwapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&dummySwapChain1))) {
                    void** pVTable1 = *reinterpret_cast<void***>(dummySwapChain1);
                    MH_CreateHook(pVTable1[22], (LPVOID)hkPresent1, (void**)&oPresent1);
                    MH_EnableHook(pVTable1[22]);
                    dummySwapChain1->Release();
                }

                HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
                if (hUser32) {
                    void* pGetRawInputData = (void*)GetProcAddress(hUser32, "GetRawInputData");
                    if (pGetRawInputData && MH_CreateHook(pGetRawInputData, (LPVOID)hkGetRawInputData, (void**)&oGetRawInputData) == MH_OK) {
                        MH_EnableHook(pGetRawInputData);
                    }
                    void* pGetRawInputBuffer = (void*)GetProcAddress(hUser32, "GetRawInputBuffer");
                    if (pGetRawInputBuffer && MH_CreateHook(pGetRawInputBuffer, (LPVOID)hkGetRawInputBuffer, (void**)&oGetRawInputBuffer) == MH_OK) {
                        MH_EnableHook(pGetRawInputBuffer);
                    }
                    void* pGetAsyncKeyState = (void*)GetProcAddress(hUser32, "GetAsyncKeyState");
                    if (pGetAsyncKeyState && MH_CreateHook(pGetAsyncKeyState, (LPVOID)hkGetAsyncKeyState, (void**)&oGetAsyncKeyState) == MH_OK) {
                        MH_EnableHook(pGetAsyncKeyState);
                    }
                    void* pGetKeyState = (void*)GetProcAddress(hUser32, "GetKeyState");
                    if (pGetKeyState && MH_CreateHook(pGetKeyState, (LPVOID)hkGetKeyState, (void**)&oGetKeyState) == MH_OK) {
                        MH_EnableHook(pGetKeyState);
                    }
                    void* pGetCursorPos = (void*)GetProcAddress(hUser32, "GetCursorPos");
                    if (pGetCursorPos && MH_CreateHook(pGetCursorPos, (LPVOID)hkGetCursorPos, (void**)&oGetCursorPos) == MH_OK) {
                        MH_EnableHook(pGetCursorPos);
                    }
                    void* pSetCursorPos = (void*)GetProcAddress(hUser32, "SetCursorPos");
                    if (pSetCursorPos && MH_CreateHook(pSetCursorPos, (LPVOID)hkSetCursorPos, (void**)&oSetCursorPos) == MH_OK) {
                        MH_EnableHook(pSetCursorPos);
                    }
                }
            }
            dummySwapChain->Release(); dummyDevice->Release(); dummyContext->Release();
        }

        ID3D12Device* pDummyD12Device = nullptr;
        if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDummyD12Device))) {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            ID3D12CommandQueue* pDummyQueue = nullptr;
            if (SUCCEEDED(pDummyD12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pDummyQueue))) {
                void** pVTable12 = *reinterpret_cast<void***>(pDummyQueue);
                if (MH_CreateHook(pVTable12[10], (LPVOID)hkExecuteCommandLists, (void**)&oExecuteCommandLists) == MH_OK) {
                    MH_EnableHook(pVTable12[10]);
                }
                pDummyQueue->Release();
            }
            pDummyD12Device->Release();
        }
        return true;
    }
}