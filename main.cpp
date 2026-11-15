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

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"
#include "And64InlineHook.hpp" // Fixed include path

#define LOG_TAG "DEFYNN_MOD"
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

struct Matrix4x4 { float m[4][4]; };
struct Entity { uintptr_t address; Vector3 position; int team; float distance; };

// ======================= CHEAT TOGGLES =======================
bool g_MenuOpen = false;
bool g_ESP_Player = false;
bool g_ESP_Lines = false;
bool g_ESP_Box = false;
bool g_Aimbot = false;
bool g_AimAssist = false;
bool g_RapidFire = false;
bool g_InfiniteAmmo = false;
bool g_FistToRPG = false;
bool g_FastShoot = false;
bool g_NoRecoil = false;
bool g_FastHeal = false;
bool g_WideView = false;
bool g_NoGrass = false;
bool g_NoFallDamage = false;
bool g_WallHack = false;

float g_AimbotFOV = 150.0f;
float g_Range = 2000.0f;
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

void InitOffsets() {
    LOGI("Offsets initialized (placeholders).");
    // You will need to replace these 0s with the actual pointers found in Ghidra
    g_ViewMatrix = 0; 
    g_LocalPlayer = 0;
    g_EntityList = 0;
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
    // TODO: Read from memory using g_EntityList
    return entities;
}

// ======================= FEATURE LOGIC =======================
void ApplyFeatures() {
    // All your toggle logic goes here, writing to the memory addresses found in Ghidra
    if (g_FistToRPG) { /* Write RPG weapon ID */ }
    if (g_NoRecoil) { /* Patch recoil float to 0.0f */ }
    if (g_FastHeal) { /* Patch heal timer float */ }
    if (g_NoGrass) { /* Patch grass density to 0 */ }
    if (g_NoFallDamage) { /* Patch fall damage multiplier */ }
    if (g_WideView) { /* Patch camera FOV */ }
    if (g_FastShoot) { /* Patch shoot delay */ }
    if (g_WallHack) { /* Patch wall render depth */ }
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
                ImGui::Checkbox("Fast Shoot", &g_FastShoot);
                ImGui::Checkbox("No Recoil", &g_NoRecoil);
                ImGui::Checkbox("Infinite Ammo", &g_InfiniteAmmo);
                ImGui::Checkbox("Fist to RPG", &g_FistToRPG);
                ImGui::SliderFloat("Range", &g_Range, 50.0f, 2000.0f);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Visuals")) {
                ImGui::Checkbox("ESP Player", &g_ESP_Player);
                ImGui::Checkbox("ESP Box", &g_ESP_Box);
                ImGui::Checkbox("ESP Lines", &g_ESP_Lines);
                ImGui::Checkbox("Wall Hack", &g_WallHack);
                ImGui::Checkbox("No Grass", &g_NoGrass);
                ImGui::Checkbox("Wide View", &g_WideView);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Player")) {
                ImGui::Checkbox("Fast Heal", &g_FastHeal);
                ImGui::Checkbox("No Fall Damage", &g_NoFallDamage);
                ImGui::InputFloat("Teleport X", &g_Teleport_X);
                ImGui::InputFloat("Teleport Y", &g_Teleport_Y);
                ImGui::InputFloat("Teleport Z", &g_Teleport_Z);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        DrawESP();
        ApplyFeatures();
        
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
    ImGui_ImplAndroid_Init(ANativeWindow_fromSurface(dpy, surf));
    ImGui_ImplOpenGL3_Init("#version 300 es");
    
    void* target = dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    A64HookFunction(target, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
    return JNI_VERSION_1_6;
}
