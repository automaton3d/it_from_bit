// MenuSystem.cpp
#include "menubar.h"
#include "text_renderer.h"
#include "projection_manager.h"
#include "globals.h"

// MenuItem Implementation
MenuItem::MenuItem(const std::string& label,
                   MenuCallback callback,
                   std::unique_ptr<Menu> subMenu)
    : label(label), callback(callback), subMenu(std::move(subMenu)) {}

void MenuItem::Trigger() const {
    if (callback) {
        callback();
    }
}

void MenuItem::Render(TextRenderer* r, float x, float y, float w, float h, bool hovered, int screenW, int screenH)
{
    if (!r) return;  // Safety check
    
    // Render text with smaller scale (0.4 instead of 0.8)
    glm::vec3 color = hovered ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 1.0f, 1.0f);
    float textY = y - 8.0f;
    r->RenderText(label, x + 12.0f, textY, 0.35f, color, screenW, screenH);
}

bool MenuItem::HandleClick(float mouseX, float mouseY, float x, float y, float width, float height) {
    if (mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + height) {
        Trigger();
        return true;
    }
    return false;
}

Menu::Menu(const std::string& title) : title(title) {}

void Menu::AddItem(const std::string& label,
                   MenuCallback callback,
                   std::unique_ptr<Menu> subMenu) {
    items.push_back(std::make_unique<MenuItem>(label, callback, std::move(subMenu)));
}

void Menu::Render(TextRenderer* r, float x, float y, float w, bool open, MenuBar* menuBar)
{
    if (!r) {
        std::cerr << "ERROR: Menu textRenderer is NULL!" << std::endl;
        return;
    }

    int screenW = menuBar ? (int)menuBar->GetWidth() : 1920;
    int screenH = menuBar ? (int)menuBar->GetHeight() : 1080;

    // Render the menu title text (y is the top edge of the bar)
    float titleY = y - 7.0f;
    r->RenderText(title, x + 12.0f, titleY, 0.35f,
                  glm::vec3(1.0f, 1.0f, 1.0f), screenW, screenH);

    // Only render dropdown if menu is open
    if (!open) return;

    float ih = 26.0f;
    float dw = 180.0f;
    float dh = items.size() * ih;

    // Dropdown extends downward from the bar
    float dropdownY = MenuBar::TITLE_BAR_OFFSET + 17;

    // Render dropdown background
    if (menuBar) {
        menuBar->RenderQuad(x, dropdownY, dw, dh, glm::vec4(0.2f, 0.2f, 0.2f, 0.95f));
    }

    // Render dropdown items
    for (size_t i = 0; i < items.size(); ++i) {
        bool hov = (int)i == hoveredItem;
        float itemY = y - (i + 1) * ih;

        if (hov && menuBar) {
            menuBar->RenderQuad(x, itemY, dw, ih, glm::vec4(0.3f, 0.3f, 0.4f, 1.0f));
        }

        items[i]->Render(r, x, itemY, dw, ih, hov, screenW, screenH);
    }
}

void Menu::Close() {
    hoveredItem = -1;
}

MenuBar::MenuBar(TextRenderer* renderer, float windowWidth, float windowHeight)
    : textRenderer(renderer), width(windowWidth), height(windowHeight) {
    InitShaders();
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
}

MenuBar::~MenuBar() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
}

void MenuBar::AddMenu(std::unique_ptr<Menu> menu) {
    menus.push_back(std::move(menu));
}

void MenuBar::InitShaders() {
    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec2 aPos;\n"
        "uniform mat4 projection;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n"
        "uniform vec4 color;\n"
        "void main()\n"
        "{\n"
        "    FragColor = color;\n"
        "}\n";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void MenuBar::RenderQuad(float x, float y, float w, float h, glm::vec4 col)
{
    glUseProgram(shaderProgram);

    // Use BOTTOM-left origin to match TextRenderer
    glm::mat4 proj = ProjectionManager::instance().get2DOrtho();

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform4fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(col));

    // y is already in bottom-origin coordinates
    float vtx[] = {
        x,         y,
        x + w,     y,
        x + w,     y + h,
        x,         y,
        x + w,     y + h,
        x,         y + h
    };

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void MenuBar::Render() {
    float screenH = height;
    // Save current OpenGL state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    
    // Set up proper state for 2D rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    if (!textRenderer) {
        std::cerr << "ERROR: MenuBar textRenderer is NULL!" << std::endl;
        return;
    }
    
    // Render bar background at the TOP of the screen
    float barY = screenH - TITLE_BAR_OFFSET;
    float barBottomY = gViewport[3] - (barY + barHeight / 2);     // Bottom edge of the bar
    RenderQuad(0.0f, barBottomY, width, barHeight, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));

    float menuWidth = 100.0f;
    float currentX = 0.0f;

    for (size_t i = 0; i < menus.size(); ++i) {
        bool open = (i == (size_t)activeMenu);
        menus[i]->Render(textRenderer, currentX, barY, menuWidth, open, this);
        currentX += menuWidth;
    }
    
    // Restore previous OpenGL state
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (!blendEnabled) glDisable(GL_BLEND);
}

// Replace the MenuBar::HandleMouse function in menubar.cpp with this:
void MenuBar::HandleMouse(double mouseX, double mouseY, int windowWidth, int windowHeight, bool isPress) {
    // Convert mouse Y from top-origin to bottom-origin
    float bottomMouseY = windowHeight - mouseY;

    float menuWidth = 100.0f;
    float barY = height - barHeight;  // Bar position in bottom-origin

    // Bar bounds
    bool insideBar = (bottomMouseY >= barY && bottomMouseY <= barY + barHeight);

    // ONLY handle clicks on PRESS events, not RELEASE
    if (!isPress) {
        return;
    }

    // If click is inside the bar, handle toggling/opening
    if (insideBar) {
        int clickedMenu = static_cast<int>(mouseX / menuWidth);
        if (clickedMenu >= 0 && clickedMenu < (int)menus.size()) {
            activeMenu = (activeMenu == clickedMenu ? -1 : clickedMenu);
            return;
        }
    }

    // Handle dropdown clicks ONLY if a menu is open
    if (activeMenu != -1 && activeMenu < (int)menus.size()) {
        float currentX = activeMenu * menuWidth;
        float itemHeight = 26.0f;
        const auto& items = menus[activeMenu]->GetItems();

        float dropdownWidth = 180.0f;
        float dropdownHeight = items.size() * itemHeight;
        float dropdownY = barY - dropdownHeight;  // dropdown rendered above the bar

        // Dropdown bounds
        bool insideDropdown =
            (mouseX >= currentX && mouseX <= currentX + dropdownWidth &&
             bottomMouseY >= dropdownY && bottomMouseY <= barY);

        if (insideDropdown) {
            // Check if clicking on a dropdown item
            for (size_t i = 0; i < items.size(); ++i) {
                float itemY = barY - (i + 1) * itemHeight;  // match Render()
                if (mouseX >= currentX && mouseX <= currentX + dropdownWidth &&
                    bottomMouseY >= itemY && bottomMouseY <= itemY + itemHeight) {
                    items[i]->Trigger();   // fire callback
                    activeMenu = -1;       // close menu after selection
                    return;
                }
            }
            return; // click inside dropdown but not on an item
        }

        // Click outside BOTH bar and dropdown: close menu
        activeMenu = -1;
        return;
    }
}

void MenuBar::Resize(float newWidth, float newHeight) {
    width = newWidth;
    height = newHeight;
}

bool MenuBar::IsMenuOpen() {
    return activeMenu != -1;
}
