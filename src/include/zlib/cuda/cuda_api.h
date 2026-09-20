#pragma once

bool isCudaAvailable();
bool initializeCudaSimulation();
void cudaSimulationStepWrapper();
void updateBufferCuda();
bool tryEnableCuda();