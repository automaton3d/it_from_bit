/*
 * callbacks.h
 *
 *  Created on: 4 de dez. de 2025
 *      Author: Alexandre
 */

#ifndef INCLUDE_CALLBACKS_H_
#define INCLUDE_CALLBACKS_H_

namespace framework {

    void buttonCallback(GLFWwindow *window, int button, int action, int mods);
    void moveCallback(GLFWwindow* window, double xpos, double ypos);
    void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void setViewFromRadio(OrbitCamera& cam, int viewIndex);

}

#endif /* INCLUDE_CALLBACKS_H_ */
