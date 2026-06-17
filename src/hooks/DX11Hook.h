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
#include <fstream>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include "hooks/PlayerHook.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX11Hook {
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
        if (MapRenderState::IsUIActive() && vKey != 0x4D && vKey != VK_F11) {
            return 0;
        }
        if (oGetAsyncKeyState) return oGetAsyncKeyState(vKey);
        return 0;
    }

    typedef SHORT(WINAPI* PGETKEYSTATE_HOOK)(int);
    inline PGETKEYSTATE_HOOK oGetKeyState = nullptr;
    inline SHORT WINAPI hkGetKeyState(int vKey) {
        if (MapRenderState::IsUIActive() && vKey != 0x4D && vKey != VK_F11) {
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
        ShutdownImGuiAndBuffers();
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
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
            
            if (uMsg == WM_KILLFOCUS || (uMsg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE)) {
                MapRenderState::showBigMap = false;
                MapRenderState::showWaypointUI = false;
            }
            if (uMsg == WM_KEYDOWN && wParam == 0x4D) {
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
            if (uMsg == WM_KEYDOWN && wParam == 0x55) {
                MapRenderState::showWaypointUI = !MapRenderState::showWaypointUI;
                return 1;
            }

            if (MapRenderState::IsUIActive()) {
                ClipCursor(NULL);
                if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                    MapRenderState::showBigMap = false;
                    MapRenderState::showWaypointUI = false; 
                    return 1;
                }
                if (uMsg == WM_INPUT || uMsg == WM_INPUT_DEVICE_CHANGE) return 1;
                if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) return 1;
                if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST && wParam != 0x4D && wParam != 0x55 && wParam != VK_F11) return 1;
                if (uMsg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
            }
        }
        return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
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

    inline void UpdateMapTexture() {
        if (!g_mapDataUpdated.load() || !g_pd3dDeviceContext || !g_mapTexture) return;

        for (int x = 0; x < MAP_DATA_SIZE; x++) {
            for (int z = 0; z < MAP_DATA_SIZE; z++) {
                int index = (z * MAP_DATA_SIZE + x) * 4;
                mce::Color col = g_mapColors[x][z];

                if (col.a > 0.01f) {
                    float currentY = g_mapHeights[x][z];

                    float northY = currentY;
                    if (z > 0 && g_mapColors[x][z - 1].a > 0.01f) {
                        float h = g_mapHeights[x][z - 1];
                        if (std::abs(currentY - h) < 64.0f) northY = h;
                    }
                    float westY = currentY;
                    if (x > 0 && g_mapColors[x - 1][z].a > 0.01f) {
                        float h = g_mapHeights[x - 1][z];
                        if (std::abs(currentY - h) < 64.0f) westY = h;
                    }

                    float diff = (currentY - northY) * 0.15f + (currentY - westY) * 0.15f;
                    float shade = std::clamp(1.0f + diff, 0.65f, 1.25f);

                    g_textureData[index]     = (uint8_t)(std::clamp(col.r * shade, 0.0f, 1.0f) * 255.0f);
                    g_textureData[index + 1] = (uint8_t)(std::clamp(col.g * shade, 0.0f, 1.0f) * 255.0f);
                    g_textureData[index + 2] = (uint8_t)(std::clamp(col.b * shade, 0.0f, 1.0f) * 255.0f);
                    g_textureData[index + 3] = (uint8_t)(col.a * 255.0f);
                } else {
                    g_textureData[index] = 0; g_textureData[index+1] = 0; g_textureData[index+2] = 0; g_textureData[index+3] = 0;
                }
            }
        }
        g_pd3dDeviceContext->UpdateSubresource(g_mapTexture, 0, NULL, g_textureData, MAP_DATA_SIZE * 4, 0);
        g_mapDataUpdated.store(false);
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
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(center.x - textSize.x / 2.0f, center.y + size + 3.0f);
            draw_list->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), name.c_str()); 
            draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), name.c_str()); 
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

    inline void RenderImGuiXaeroMap() {
        if (!MapRenderState::showMiniMap) return; 
        if (!g_mapTextureView) return;
        UpdateMapTexture();

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        float IM_MAP_R = 135.0f; 
        float IM_MAP_MARGIN = 20.0f;
        float cx = ImGui::GetIO().DisplaySize.x - IM_MAP_MARGIN - IM_MAP_R;
        float cy = IM_MAP_MARGIN + IM_MAP_R;

        auto now = std::chrono::steady_clock::now();
        float alpha = std::chrono::duration_cast<std::chrono::duration<float>>(now - g_lastPhysicsTime).count() / 0.05f;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        float pX = g_prevPhysicsPos.x + (g_currPhysicsPos.x - g_prevPhysicsPos.x) * alpha;
        float pZ = g_prevPhysicsPos.z + (g_currPhysicsPos.z - g_prevPhysicsPos.z) * alpha;

        float dx = pX - g_lastRenderX;
        float dz = pZ - g_lastRenderZ;

        float ZOOM_RADIUS = 50.0f; 
        float u = 0.5f + (dx / MAP_DATA_SIZE);
        float v = 0.5f + (dz / MAP_DATA_SIZE);
        float uvR = ZOOM_RADIUS / MAP_DATA_SIZE;

        ImVec2 uv0(u - uvR, v - uvR);
        ImVec2 uv1(u + uvR, v + uvR);
        ImVec2 mapMin(cx - IM_MAP_R, cy - IM_MAP_R);
        ImVec2 mapMax(cx + IM_MAP_R, cy + IM_MAP_R);

        if (MapRenderState::isSquareMap) {
            draw_list->AddRectFilled(mapMin, mapMax, IM_COL32(20, 20, 20, 255));
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            draw_list->AddImage((void*)g_mapTextureView, mapMin, mapMax, uv0, uv1, IM_COL32_WHITE);
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
            draw_list->AddRect(mapMin, mapMax, IM_COL32(30, 30, 30, 255), 0.0f, 0, 2.0f);
        } else {
            draw_list->AddCircleFilled(ImVec2(cx, cy), IM_MAP_R, IM_COL32(20, 20, 20, 255), 64);
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            draw_list->AddImageRounded((void*)g_mapTextureView, mapMin, mapMax, uv0, uv1, IM_COL32_WHITE, IM_MAP_R);
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
            draw_list->AddCircle(ImVec2(cx, cy), IM_MAP_R, IM_COL32(30, 30, 30, 255), 64, 2.0f);
        }

        char coordBuf[64];
        snprintf(coordBuf, sizeof(coordBuf), "%d, %d, %d", g_playerBlockX, (int)g_playerY, g_playerBlockZ);
        ImVec2 coordSize = ImGui::CalcTextSize(coordBuf);
        ImVec2 coordPos(cx - coordSize.x / 2, cy + IM_MAP_R + 22); 
        draw_list->AddText(ImVec2(coordPos.x + 1, coordPos.y + 1), IM_COL32(0,0,0,200), coordBuf);
        draw_list->AddText(coordPos, IM_COL32(255, 255, 255, 255), coordBuf);

        std::string biomeStr = MapRenderState::currentBiomeName;
        size_t startPos = biomeStr.find("(");
        size_t endPos = biomeStr.find(")");
        if (startPos != std::string::npos && endPos != std::string::npos) {
            biomeStr = biomeStr.substr(startPos + 1, endPos - startPos - 1);
        }
        ImVec2 biomeSize = ImGui::CalcTextSize(biomeStr.c_str());
        ImVec2 biomePos(cx - biomeSize.x / 2, cy + IM_MAP_R + 46);
        draw_list->AddText(ImVec2(biomePos.x + 1, biomePos.y + 1), IM_COL32(0,0,0,200), biomeStr.c_str());
        draw_list->AddText(biomePos, IM_COL32(220, 220, 220, 255), biomeStr.c_str());

        draw_list->AddText(ImVec2(cx - 8, cy - IM_MAP_R - 20), IM_COL32(220, 220, 255, 255), "北");
        draw_list->AddText(ImVec2(cx - 8, cy + IM_MAP_R - 2), IM_COL32(220, 220, 255, 255), "南");
        draw_list->AddText(ImVec2(cx + IM_MAP_R + 4, cy - 9), IM_COL32(220, 220, 255, 255), "东");
        draw_list->AddText(ImVec2(cx - IM_MAP_R - 20, cy - 9), IM_COL32(220, 220, 255, 255), "西");

        static std::vector<RadarEntity> s_cachedEntities;
        if (g_radarUpdated.load()) {
            s_cachedEntities = g_radarEntities;
            g_radarUpdated.store(false);
        }

        float scale = IM_MAP_R / ZOOM_RADIUS; 
        for (const auto& ent : s_cachedEntities) {
            float edx = ent.x - pX;
            float edz = ent.z - pZ;
            float ex = cx + edx * scale;
            float ez = cy + edz * scale;
            
            bool inBounds = false;
            if (MapRenderState::isSquareMap) {
                inBounds = (std::abs(ex - cx) <= IM_MAP_R && std::abs(ez - cy) <= IM_MAP_R);
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

                float ex = cx + wDx * scale;
                float ez = cy + wDz * scale;
                
                bool inMap = false;
                float edgeX = cx, edgeZ = cy;

                if (MapRenderState::isSquareMap) {
                    if (std::abs(wDx * scale) <= IM_MAP_R && std::abs(wDz * scale) <= IM_MAP_R) {
                        inMap = true;
                    } else {
                        float maxDist = std::max(std::abs(wDx), std::abs(wDz));
                        edgeX = cx + (wDx / maxDist) * IM_MAP_R;
                        edgeZ = cy + (wDz / maxDist) * IM_MAP_R;
                    }
                } else {
                    if (physicalDist <= ZOOM_RADIUS) {
                        inMap = true;
                    } else {
                        edgeX = cx + (wDx / physicalDist) * IM_MAP_R;
                        edgeZ = cy + (wDz / physicalDist) * IM_MAP_R;
                    }
                }

                if (inMap) {
                    DrawWaypointIcon(draw_list, ImVec2(ex, ez), mce::Color(wp.r, wp.g, wp.b, 1.0f), wp.name, false);
                } else {
                    DrawWaypointIcon(draw_list, ImVec2(edgeX, edgeZ), mce::Color(wp.r, wp.g, wp.b, 1.0f), "", true);
                }
            }
        }

        float playerYaw = g_localPlayer ? g_localPlayer->getRotation().y : 0.0f;
        float yawRad = (playerYaw + 180.0f) * (3.14159265f / 180.0f); 
        float cosY = std::cos(yawRad);
        float sinY = std::sin(yawRad);
        auto rotate = [&](float x, float y) -> ImVec2 { return ImVec2(cx + (x * cosY - y * sinY), cy + (x * sinY + y * cosY)); };

        draw_list->AddTriangleFilled(rotate(0, -10.0f), rotate(-7.0f, 10.0f), rotate(7.0f, 10.0f), IM_COL32(0, 0, 0, 255));
        draw_list->AddTriangleFilled(rotate(0, -8.0f), rotate(-5.0f, 8.0f), rotate(5.0f, 8.0f), IM_COL32(220, 20, 20, 255));
    }

    inline void UpdateRegionTexture(uint64_t hash, int& texCount) {
        if (!g_pd3dDevice || !g_pd3dDeviceContext) return;
        static uint8_t tempBuffer[256 * 256 * 4];
        
        if (MapCacheManager::FetchRegionTextureData(hash, tempBuffer)) {
            if (g_regionTextures.find(hash) == g_regionTextures.end()) {
                if (texCount >= 2) {
                    MapCacheManager::MarkTextureDirty(hash);
                    return;
                }
                texCount++;
                
                D3D11_TEXTURE2D_DESC desc;
                ZeroMemory(&desc, sizeof(desc));
                desc.Width = 256; desc.Height = 256;
                desc.MipLevels = 1; desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                
                D3D11_SUBRESOURCE_DATA initData;
                initData.pSysMem = tempBuffer;
                initData.SysMemPitch = 256 * 4;
                initData.SysMemSlicePitch = 0;
                
                ID3D11Texture2D* tex = nullptr;
                ID3D11ShaderResourceView* srv = nullptr;
                
                if (SUCCEEDED(g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex))) {
                    g_pd3dDevice->CreateShaderResourceView(tex, NULL, &srv);
                    g_regionTextures[hash] = tex;
                    g_regionSRVs[hash] = srv;
                }
            } else {
                g_pd3dDeviceContext->UpdateSubresource(g_regionTextures[hash], 0, NULL, tempBuffer, 256 * 4, 0);
            }
        }
    }

    // ==========================================
    // 提取公共重命名模态弹窗组件
    // ==========================================
    inline void RenderRenameModal(const char* modalId, std::string& wpId, bool& trigger) {
        if (trigger) {
            ImGui::OpenPopup(modalId);
            trigger = false;
        }
        if (ImGui::BeginPopupModal(modalId, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char renameBuf[64] = "";
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
                
                ImGui::InputText("新名称", renameBuf, sizeof(renameBuf));
                ImGui::Spacing();
                
                if (ImGui::Button("保存", ImVec2(120, 0))) {
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
                if (ImGui::Button("取消", ImVec2(120, 0))) {
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
        
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsPopupOpen("BigMapContextMenu") && !ImGui::IsPopupOpen("WaypointContextMenu") && !ImGui::IsPopupOpen("重命名路径点##ModalBigMap") && !ImGui::IsPopupOpen("新建路径点##Popup")) {
            MapRenderState::bigMapOffsetX += io.MouseDelta.x;
            MapRenderState::bigMapOffsetZ += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f) {
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

        draw_list->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(20, 20, 20, 255));

        float cx = io.DisplaySize.x * 0.5f;
        float cy = io.DisplaySize.y * 0.5f;
        
        float minWx = g_playerX - (cx + MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float maxWx = g_playerX + (cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float minWz = g_playerZ - (cy + MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;
        float maxWz = g_playerZ + (cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        int startRx = (int)std::floor(minWx / 256.0f);
        int endRx   = (int)std::floor(maxWx / 256.0f);
        int startRz = (int)std::floor(minWz / 256.0f);
        int endRz   = (int)std::floor(maxWz / 256.0f);

        int texturesCreatedThisFrame = 0;
        if (MapRenderState::currentDimensionId != 1) {
            draw_list->AddCallback(PointSamplerCallback, nullptr);
            
            for (int rx = startRx; rx <= endRx; rx++) {
                for (int rz = startRz; rz <= endRz; rz++) {
                    uint64_t hash = MapCacheManager::GetRegionHash(rx, rz);
                    UpdateRegionTexture(hash, texturesCreatedThisFrame);

                    if (g_regionSRVs.find(hash) != g_regionSRVs.end()) {
                        float sx_min = std::floor(cx + (rx * 256.0f - g_playerX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                        float sy_min = std::floor(cy + (rz * 256.0f - g_playerZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                        float sx_max = std::floor(cx + ((rx + 1) * 256.0f - g_playerX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX);
                        float sy_max = std::floor(cy + ((rz + 1) * 256.0f - g_playerZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ);
                        
                        draw_list->AddImage((void*)g_regionSRVs[hash], ImVec2(sx_min, sy_min), ImVec2(sx_max, sy_max));
                    }
                }
            }
            
            draw_list->AddCallback(LinearSamplerCallback, nullptr);
        } else {
            const char* netherMsg = "【 下界磁场干扰过强，无法绘制地图 】";
            ImVec2 ts = ImGui::CalcTextSize(netherMsg);
            draw_list->AddText(ImVec2(cx - ts.x / 2, cy - ts.y / 2 - 50.0f), IM_COL32(255, 80, 80, 200), netherMsg);
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
                
                float wx = cx + (wp.x - g_playerX) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetX;
                float wz = cy + (wp.z - g_playerZ) * MapRenderState::bigMapZoom + MapRenderState::bigMapOffsetZ;
                
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

        float hoverWx = g_playerX + (io.MousePos.x - cx - MapRenderState::bigMapOffsetX) / MapRenderState::bigMapZoom;
        float hoverWz = g_playerZ + (io.MousePos.y - cy - MapRenderState::bigMapOffsetZ) / MapRenderState::bigMapZoom;

        char infoBuf[256];
        snprintf(infoBuf, sizeof(infoBuf), "赤焰全局大地图 | 缩放: %.1fx", MapRenderState::bigMapZoom);
        draw_list->AddText(ImVec2(20, 20), IM_COL32(255, 200, 50, 255), infoBuf);
        draw_list->AddText(ImVec2(20, 45), IM_COL32(200, 200, 200, 255), "[拖拽] 平移    [滚轮] 缩放    [Esc] 关闭地图");
        
        snprintf(infoBuf, sizeof(infoBuf), "光标位置: X: %d  Z: %d", (int)std::floor(hoverWx), (int)std::floor(hoverWz));
        ImVec2 textSize = ImGui::CalcTextSize(infoBuf);
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - textSize.x / 2 - 15, io.DisplaySize.y - 45), 
                                 ImVec2(io.DisplaySize.x / 2 + textSize.x / 2 + 15, io.DisplaySize.y - 10), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(ImVec2(io.DisplaySize.x / 2 - textSize.x / 2, io.DisplaySize.y - 35), IM_COL32(255, 255, 255, 255), infoBuf);

        std::string biomeDisplay = "生物群系: " + MapRenderState::currentBiomeName;
        ImVec2 biomeTextSize = ImGui::CalcTextSize(biomeDisplay.c_str());
        draw_list->AddRectFilled(ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2 - 20, 15), 
                                 ImVec2(io.DisplaySize.x / 2 + biomeTextSize.x / 2 + 20, 50), 
                                 IM_COL32(0, 0, 0, 180), 5.0f);
        draw_list->AddText(ImVec2(io.DisplaySize.x / 2 - biomeTextSize.x / 2, 25), IM_COL32(180, 255, 180, 255), biomeDisplay.c_str());

        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - 240, 20));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));
        ImGui::BeginChild("MapSidebar", ImVec2(220, 250), true, ImGuiWindowFlags_NoScrollbar);
        
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ 玩家状态 ]");
        ImGui::Separator();
        ImGui::Text("玩家 X: %d", g_playerBlockX);
        ImGui::Text("玩家 Y: %d", (int)g_playerY);
        ImGui::Text("玩家 Z: %d", g_playerBlockZ);
        
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[ 操作面板 ]");
        ImGui::Separator();
        
        ImGui::Checkbox("显示右上角小地图", &MapRenderState::showMiniMap);
        ImGui::Checkbox("使用方形小地图", &MapRenderState::isSquareMap);
        ImGui::Spacing();
        
        if (ImGui::Button("视角回中至玩家", ImVec2(-1, 35))) {
            MapRenderState::bigMapOffsetX = 0.0f;
            MapRenderState::bigMapOffsetZ = 0.0f;
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
            
            // 【核心2修复】动态探测大地图指定坐标的地表高度 (Y轴精准计算，未加载才给320)
            int by = 320; // 默认防窒息高度
            if (g_clientInstance) {
                BlockSource* region = g_clientInstance->getRegion();
                if (region) {
                    short topY = region->getAboveTopSolidBlock(bx, bz, true, true);
                    if (topY > -60 && topY < 319) {
                        by = (int)topY + 1; // 提取出真实地表
                    }
                }
            }

            ImVec2 titleSize = ImGui::CalcTextSize("选择操作");
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
            ImGui::Text("选择操作");
            ImGui::Separator();

            char chunkBuf[64]; snprintf(chunkBuf, sizeof(chunkBuf), "区块: (%d; %d)", bx >> 4, bz >> 4);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(chunkBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", chunkBuf);

            char blockBuf[64]; snprintf(blockBuf, sizeof(blockBuf), "X: %d, Y: %d, Z: %d", bx, by, bz);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(blockBuf).x) * 0.5f);
            ImGui::TextDisabled("%s", blockBuf);
            ImGui::Separator();

            if (ImGui::Selectable("[XP] 复制坐标")) {
                char buf[128]; snprintf(buf, sizeof(buf), "%d %d %d", bx, by, bz);
                ImGui::SetClipboardText(buf);
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable("U 创建路径点")) {
                MapRenderState::addWaypointX = bx;
                MapRenderState::addWaypointY = by; // 传入真实计算出的表面高度
                MapRenderState::addWaypointZ = bz;
                MapRenderState::triggerAddWaypoint = true;
                MapRenderState::showWaypointUI = true;
            }
            
            if (ImGui::Selectable("传送到此地")) {
                MapRenderState::tpTargetX = (float)bx + 0.5f;
                MapRenderState::tpTargetY = (float)by; // 传送至真实安全高度
                MapRenderState::tpTargetZ = (float)bz + 0.5f;
                MapRenderState::triggerTeleport.store(true);
                MapRenderState::showBigMap = false; 
            }
            
            ImGui::Separator();
            
            if (ImGui::Selectable("U 打开路径点菜单")) {
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
                
                if (ImGui::Selectable("传送到此路径点")) {
                    MapRenderState::tpTargetX = (float)targetWp.x + 0.5f;
                    MapRenderState::tpTargetY = (float)targetWp.y; 
                    MapRenderState::tpTargetZ = (float)targetWp.z + 0.5f;
                    MapRenderState::triggerTeleport.store(true);
                    MapRenderState::showBigMap = false;
                }
                
                if (ImGui::Selectable("重命名路径点")) {
                    bigMapRenameId = selectedWpId;
                    bigMapTriggerRename = true;
                }
                
                ImGui::Separator();
                
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Selectable("删除此路径点")) {
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
        RenderRenameModal("重命名路径点##ModalBigMap", bigMapRenameId, bigMapTriggerRename);

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
        if (ImGui::Begin("路径点管理器 (按 'U' 或 'Esc' 关闭)##WP", &MapRenderState::showWaypointUI, winFlags)) {
            
            static bool showAddPopup = false;
            static char searchBuf[128] = "";
            
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
            ImGui::InputTextWithHint("##WPSearch", "在此输入名称以搜索路径点...", searchBuf, sizeof(searchBuf));
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button(" + 新建路径点", ImVec2(140, 0))) {
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
                    if (ImGui::Checkbox("显示", &enabled)) {
                        wp.enabled = enabled;
                        toggled = true;
                    }
                    
                    // 【核心1修复】主列表中追加独立设计的重命名按钮
                    ImGui::SameLine(winWidth - 195);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                    if (ImGui::Button("重命名", ImVec2(55, 0))) {
                        uiRenameId = wp.id;
                        uiTriggerRename = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(winWidth - 135);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));
                    if (ImGui::Button("传送", ImVec2(45, 0))) {
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
                    if (ImGui::Button("删除", ImVec2(45, 0))) {
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
            RenderRenameModal("重命名路径点##ModalUI", uiRenameId, uiTriggerRename);

            if (MapRenderState::triggerAddWaypoint) {
                showAddPopup = true;
                MapRenderState::triggerAddWaypoint = false;
            }

            if (showAddPopup) ImGui::OpenPopup("新建路径点##Popup");
            
            if (ImGui::BeginPopupModal("新建路径点##Popup", &showAddPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
                static char nameBuf[64] = "新地标";
                static int pos[3] = {0, 0, 0};
                static float col[3] = {1.0f, 0.3f, 0.3f};
                
                if (ImGui::IsWindowAppearing()) {
                    snprintf(nameBuf, sizeof(nameBuf), "新地标"); 
                    pos[0] = (MapRenderState::addWaypointX != -999999) ? MapRenderState::addWaypointX : g_playerBlockX;
                    pos[1] = (MapRenderState::addWaypointY != -999999) ? MapRenderState::addWaypointY : (int)g_playerY;
                    pos[2] = (MapRenderState::addWaypointZ != -999999) ? MapRenderState::addWaypointZ : g_playerBlockZ;
                    
                    MapRenderState::addWaypointX = -999999;
                    MapRenderState::addWaypointY = -999999;
                    MapRenderState::addWaypointZ = -999999;
                }

                ImGui::InputText("名称", nameBuf, 64);
                ImGui::InputInt3("X / Y / Z", pos);
                ImGui::ColorEdit3("颜色", col);
                
                ImGui::Spacing();
                if (ImGui::Button("保存", ImVec2(120, 0))) {
                    WaypointManager::AddWaypoint(nameBuf, pos[0], pos[1], pos[2], col[0], col[1], col[2]);
                    showAddPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("取消", ImVec2(120, 0))) {
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
                    g_clientInstance->isShowingPauseScreen()) {
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
                io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 20.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

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

                if (MapRenderState::showBigMap) {
                    RenderImGuiBigMap();
                } else {
                    RenderImGuiXaeroMap();
                }

                if (MapRenderState::showWaypointUI) {
                    RenderImGuiWaypointUI();
                }

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