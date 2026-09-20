#ifndef MENUBAR_H
#define MENUBAR_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>

// Forward declaration
class TextRenderer;

// Callback type for menu actions
using MenuCallback = std::function<void()>;

class Menu;

// ---------------- MenuItem ----------------
class MenuItem {
public:
    MenuItem(const std::string& label,
             MenuCallback callback = nullptr,
             std::unique_ptr<class Menu> subMenu = nullptr);

    const std::string& GetLabel() const { return label; }
    void Trigger() const;
    bool HasSubMenu() const { return subMenu != nullptr; }
    Menu* GetSubMenu() const { return subMenu.get(); }

    void Render(TextRenderer* renderer, float x, float y,
                float width, float height, bool hovered,
                int screenW, int screenH);
    bool HandleClick(float mouseX, float mouseY,
                     float x, float y, float width, float height);

private:
    std::string label;
    MenuCallback callback;
    std::unique_ptr<Menu> subMenu;
};

// ---------------- Menu ----------------
class Menu {

public:
    explicit Menu(const std::string& title);

    void AddItem(const std::string& label,
                 MenuCallback callback = nullptr,
                 std::unique_ptr<Menu> subMenu = nullptr);

    const std::string& GetTitle() const { return title; }
    const std::vector<std::unique_ptr<MenuItem>>& GetItems() const { return items; }

    void Render(TextRenderer* renderer, float x, float y,
                float width, bool open, class MenuBar* menuBar = nullptr);
    void HandleMouse(double mouseX, double mouseY,
                     int windowWidth, int windowHeight, bool isPress);
    void Close();

private:
    std::string title;
    std::vector<std::unique_ptr<MenuItem>> items;
    int hoveredItem = -1;
};

// ---------------- MenuBar ----------------
class MenuBar {
public:
    MenuBar(TextRenderer* renderer, float windowWidth, float windowHeight);
    ~MenuBar();

    void AddMenu(std::unique_ptr<Menu> menu);
    void Render();
    void HandleMouse(double mouseX, double mouseY,
                     int windowWidth, int windowHeight, bool isPress);

    void Resize(float newWidth, float newHeight);

    float GetWidth() const { return width; }
    float GetHeight() const { return height; }

    void RenderQuad(float x, float y, float w, float h, glm::vec4 color);
    bool IsMenuOpen();
    static constexpr float TITLE_BAR_OFFSET = 15.0f;

private:
    TextRenderer* textRenderer;
    std::vector<std::unique_ptr<Menu>> menus;
    float width, height;
    float barHeight = 30.0f;
    int activeMenu = -1;

    GLuint shaderProgram;
    GLuint VAO, VBO;

    void InitShaders();
};

#endif // MENUBAR_H
