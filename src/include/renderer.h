/*
 * renderer.h (adapted)
 *
 * Declares the top level renderer routines.
 * (GUIrenderer is subordinated to it)
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"

namespace framework
{
  class Renderer
  {
  public:
      Renderer();
      virtual ~Renderer();

      // Pure virtual render method
      virtual void render() = 0;

//      void setCamera(Camera *c);
//      const Camera* getCamera();

  protected:
//      Camera *mCamera;
  }; // end class Renderer

} // end namespace framework

#endif // RENDERER_H
