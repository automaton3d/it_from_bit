/*
 * logo.h
 */

#ifndef LOGO_H_
#define LOGO_H_

#include <glad/glad.h>
#include <string>


extern int textureMvpLoc;
extern unsigned int textureProgram2D;

namespace framework
{

  class Logo
  {
  public:
    explicit Logo(const std::string& path = "logo.png");
    ~Logo();

    Logo(const Logo&)            = delete;
    Logo& operator=(const Logo&) = delete;
    // Move is deleted as well: the members are raw GL handles and ~Logo()
    // releases them, so moving would leave two objects owning the same
    // texture/VAO/VBO.  Nothing moves a Logo (it is always heap-allocated by
    // the splash and by the HUD and reached through a pointer).
    Logo(Logo&&)                 = delete;
    Logo& operator=(Logo&&)      = delete;

    // Draw in pixel coordinates (top-origin)
    void draw(int x, int y, float scale = 1.0f) const;

    int width()  const noexcept { return mWidth_;  }
    int height() const noexcept { return mHeight_; }
    bool valid() const noexcept { return mTexture_ != 0; }

  private:
    GLuint mTexture_ = 0;
    int    mWidth_   = 0;
    int    mHeight_  = 0;

    // Quad geometry, created by the first draw() and reused afterwards.  The
    // draw path used to create and delete a VAO+VBO pair on every call, once
    // per frame on the splash screen and once per frame in the HUD.  Mutable
    // because draw() is const.
    mutable GLuint mVao_ = 0;
    mutable GLuint mVbo_ = 0;
  };
} // namespace framework

#endif /* LOGO_H_ */
