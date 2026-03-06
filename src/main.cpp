#include <iostream>
#include <glad/glad.h> // GLAD必须在GLFW之前被包含
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>

// 窗口尺寸
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

void processInput(GLFWwindow *window);

int main()
{
    // 1. 初始化GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. 创建窗口
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "My First OpenGL Window", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 3. 初始化GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 渲染循环 (Render Loop)
    while (!glfwWindowShouldClose(window))
    {
        // 输入处理
        processInput(window);

        // 渲染指令
        glClearColor(0.2f, 0.3f, 0.8f, 1.0f); // 设置清屏颜色 (蓝色)
        glClear(GL_COLOR_BUFFER_BIT);

        // 交换缓冲区并检查事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 释放资源
    glfwTerminate();
    return 0;
}

// 处理输入: 如果按下ESC键，则关闭窗口
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
