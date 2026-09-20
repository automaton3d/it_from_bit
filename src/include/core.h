// core.h
#ifndef CORE_H
#define CORE_H

#include <GLFW/glfw3.h>

// Function declarations
void StartSimulationThread();
void mainSimulationLoop(GLFWwindow* window);
int runSimulation();
int runReplay(); 

#endif // CORE_H
