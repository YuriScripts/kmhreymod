#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cmath>
#include <dlfcn.h>
#include <cstring>
#include <vector>
#include <cstdio>

// ImGui & Dobby
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"
#include "dobby.h"

#define LOG_TAG "KMHREY_MOD"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vector3 operator*(float f) const { return {x * f, y * f, z * f}; }
    float Distance(const Vector3& v) const {
        return sqrtf(powf(x - v.x, 2) + powf(y - v.y, 2) + powf(z - v.z, 2));
    }
};

struct Matrix4x4 {
    float m[4][4];
};

struct Entity {
    uintptr_t address;
    Vector3 position;
    int team;
    float distance;
};

bool g_MenuOpen = false;
bool g_ESP_Player = false;
bool g_ESP_Lines = false;
bool g_ESP_Box = false;
bool g_Aimbot = false;
bool g_AimAssist = false;
bool g_RapidFire = false;
bool g_InfiniteAmmo = false;

float g_AimbotFOV = 150.0f;
float g_Range = 300.0f;
float g_Teleport_X = 0.0f;
float g_Teleport_Y = 0.0f;
float g_Teleport_Z = 0.0f;

uintptr_t g_ClientBase = 0;
size_t g_ClientSize = 0;
uintptr_t g_LocalPlayer = 0;
uintptr_t g_ViewMatrix = 0;
uintptr_t g_EntityList = 0;

void GetModuleInfo(const char* moduleName, uintptr_t& base, size_t& size) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, moduleName)) {
            uintptr_t start, end;
            sscanf(line, "%lx-%lx", &start, &end);
            base = start;
            size = end - start;
            break;
        }
    }
    fclose(fp);
}

uintptr_t FindPattern(uintptr_t start, size_t size, const char* pattern, const char* mask) {
    size_t patternLen = strlen(mask);
    uint8_t* data = (uint8_t*)start;
    for (size_t i = 0; i <= size - patternLen; i++) {
        bool found = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] == 'x' && data[i + j] != (uint8_t)pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return start + i;
    }
    return 0;
}

void InitOffsets() {
    LOGI("Scanning for offsets in libclient.so...");
    
    // FAKE PATTERNS - REPLACE WITH REAL GHIDRA AOBs
    g_ViewMatrix = FindPattern(g_ClientBase, g_ClientSize, "\x00\x00\x80\x3F", "xxxx");
    g_LocalPlayer = FindPattern(g_ClientBase, g_ClientSize, "\x00\x00\xC8\x42", "xxxx");
    g_EntityList = FindPattern(g_ClientBase, g_ClientSize, "\x00\x00\x80\x3F", "xxxx");

    LOGI("View Matrix: %p", (void*)g_ViewMatrix);
    LOGI("Local Player: %p", (void*)g_LocalPlayer);
    LOGI("Entity List: %p", (void*)g_EntityList);
}

bool WorldToScreen(Vector3 pos, Vector3& screen, Matrix4x4 viewMatrix, int width, int height) {
    float clipX = pos.x * viewMatrix.m[0][0] + pos.y * viewMatrix.m[1][0] + pos.z * viewMatrix.m[2][0] + viewMatrix.m[3][0];
    float clipY = pos.x * viewMatrix.m[0][1] + pos.y * viewMatrix.m[1][1] + pos.z * viewMatrix.m[2][1] + viewMatrix.m[3][1];
    float clipW = pos.x * viewMatrix.m[0][3] + pos.y * viewMatrix.m[1][3] + pos.z * viewMatrix.m[2][3] + viewMatrix.m[3][3];
    if (clipW < 0.1f) return false;
    screen.x = (width / 2.0f) * (clipX / clipW + 1.0f);
    screen.y = (height / 2.0f) * (1.0f - clipY / clipW);
    return true;
}

std::vector<Entity> GetEntities() {
    std::vector<Entity> entities;
    if (!g_LocalPlayer || !g_EntityList) return entities;
    Entity dummy;
    dummy.address = g_EntityList;
    dummy.position = {100.0f, 0.0f, 100.0f};
    dummy.team = 2;
    dummy.distance = 50.0f;
    entities.push_back(dummy);
    return entities;
}

void DrawESP() {
    if (!g_ESP_Player && !g_ESP_Lines && !g_ESP_Box) return;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    if (g_Aimbot) draw->AddCircle(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y / 2.0f), g_AimbotFOV, IM_COL32(255, 255, 255, 100), 100, 2.0f);
    if (g_ViewMatrix) {
        Matrix4x4 vm = *(Matrix4x4*)g_ViewMatrix;
        for (auto& ent : GetEntities()) {
            Vector3 screenPos;
            if (WorldToScreen(ent.position, screenPos, vm, io.DisplaySize.x, io.DisplaySize.y)) {
                if (g_ESP_Box) draw->AddRect(ImVec2(screenPos.x - 20, screenPos.y - 40), ImVec2(screenPos.x + 20, screenPos.y + 40), IM_COL32(255, 0, 0, 255), 0, 0, 2.0f);
                if (g_ESP_Lines) draw->AddLine(ImVec2(io.DisplaySize.x / 2.0f, io.DisplaySize.y), ImVec2(screenPos.x, screenPos.y), IM_COL32(255, 0, 0, 255), 1.5f);
                if (g_ESP_Player) draw->AddText(ImVec2(screenPos.x - 10, screenPos.y - 50), IM_COL32(255, 255, 255, 255), "PLAYER");
            }
        }
    }
}

typedef void (*eglSwapBuffers_t)(void* dpy, void* surf);
eglSwapBuffers_t orig_eglSwapBuffers;

void hook_eglSwapBuffers(void* dpy, void* surf) {
    if (g_MenuOpen) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("KMHREY - ROS LEGACY", &g_MenuOpen, ImGuiWindowFlags_NoCollapse);
        if (ImGui::BeginTabBar("Tabs")) {
            if (ImGui::BeginTabItem("Combat")) {
                ImGui::Checkbox("Aimbot", &g_Aimbot);
                if (g_Aimbot) ImGui::SliderFloat("FOV", &g_AimbotFOV, 10.0f, 500.0f);
                ImGui::Checkbox("Aim Assist", &g_AimAssist);
                ImGui::Checkbox("Rapid Fire", &g_RapidFire);
                ImGui::Checkbox("Infinite Ammo", &g_InfiniteAmmo);
                ImGui::SliderFloat("Range", &g_Range, 50.0f, 500.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Visuals")) {
                ImGui::Checkbox("ESP Player", &g_ESP_Player);
                ImGui::Checkbox("ESP Box", &g_ESP_Box);
                ImGui::Checkbox("ESP Lines", &g_ESP_Lines);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Movement")) {
                ImGui::InputFloat("X", &g_Teleport_X);
                ImGui::InputFloat("Y", &g_Teleport_Y);
                ImGui::InputFloat("Z", &g_Teleport_Z);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        DrawESP();
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    orig_eglSwapBuffers(dpy, surf);
}

void* hack_thread(void* arg) {
    while (g_ClientBase == 0) {
        GetModuleInfo("libclient.so", g_ClientBase, g_ClientSize);
        sleep(1);
    }
    InitOffsets();
    return nullptr;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    ImGui::CreateContext();
    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");
    void* target = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    DobbyHook(target, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
    return JNI_VERSION_1_6;
}
