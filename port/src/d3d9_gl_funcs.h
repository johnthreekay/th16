// The OpenGL functions the renderer calls, loaded through
// SDL_GL_GetProcAddress once a context exists (d3d9_gl.cpp). Nothing links
// against libGL directly, so the same binary runs on GLX, EGL and macOS.
// Call them as gl::Name (gl::Clear for glClear).
#pragma once

// GL 1.0/1.1 entry points: SDL_opengl.h declares them, decltype gives the
// pointer type without referencing the symbol.
#define PORT_GL_FUNCS_11(X)                                                                                         \
    X(GetString) X(GetIntegerv) X(GetError) X(Enable) X(Disable) X(Viewport) X(DepthRange) X(ClearColor)           \
        X(ClearDepth) X(Clear) X(Scissor) X(ColorMask) X(DepthMask) X(DepthFunc) X(BlendFunc) X(CullFace)            \
            X(FrontFace) X(GenTextures) X(DeleteTextures) X(BindTexture) X(TexImage2D) X(TexSubImage2D)              \
                X(TexParameteri) X(PixelStorei) X(ReadPixels) X(Finish) X(Flush) X(ReadBuffer) X(DrawBuffer) X(DrawArrays)

// Later entry points, with SDL_opengl_glext.h's pointer types.
#define PORT_GL_FUNCS_EXT(X)                                                                                        \
    X(BlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC)                                                                \
    X(BlendEquation, PFNGLBLENDEQUATIONPROC)                                                                        \
    X(BlendEquationSeparate, PFNGLBLENDEQUATIONSEPARATEPROC)                                                        \
    X(BlendColor, PFNGLBLENDCOLORPROC)                                                                              \
    X(ActiveTexture, PFNGLACTIVETEXTUREPROC)                                                                        \
    X(GenFramebuffers, PFNGLGENFRAMEBUFFERSPROC)                                                                    \
    X(DeleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC)                                                              \
    X(BindFramebuffer, PFNGLBINDFRAMEBUFFERPROC)                                                                    \
    X(FramebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC)                                                          \
    X(FramebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC)                                                    \
    X(CheckFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC)                                                      \
    X(BlitFramebuffer, PFNGLBLITFRAMEBUFFERPROC)                                                                    \
    X(GenRenderbuffers, PFNGLGENRENDERBUFFERSPROC)                                                                  \
    X(DeleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC)                                                            \
    X(BindRenderbuffer, PFNGLBINDRENDERBUFFERPROC)                                                                  \
    X(RenderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC)                                                            \
    X(GenBuffers, PFNGLGENBUFFERSPROC)                                                                              \
    X(DeleteBuffers, PFNGLDELETEBUFFERSPROC)                                                                        \
    X(BindBuffer, PFNGLBINDBUFFERPROC)                                                                              \
    X(BufferData, PFNGLBUFFERDATAPROC)                                                                              \
    X(MapBufferRange, PFNGLMAPBUFFERRANGEPROC)                                                                      \
    X(UnmapBuffer, PFNGLUNMAPBUFFERPROC)                                                                            \
    X(GenVertexArrays, PFNGLGENVERTEXARRAYSPROC)                                                                    \
    X(DeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC)                                                              \
    X(BindVertexArray, PFNGLBINDVERTEXARRAYPROC)                                                                    \
    X(EnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC)                                                    \
    X(DisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC)                                                  \
    X(VertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC)                                                            \
    X(VertexAttrib4f, PFNGLVERTEXATTRIB4FPROC)                                                                      \
    X(CreateShader, PFNGLCREATESHADERPROC)                                                                          \
    X(ShaderSource, PFNGLSHADERSOURCEPROC)                                                                          \
    X(CompileShader, PFNGLCOMPILESHADERPROC)                                                                        \
    X(GetShaderiv, PFNGLGETSHADERIVPROC)                                                                            \
    X(GetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC)                                                                  \
    X(DeleteShader, PFNGLDELETESHADERPROC)                                                                          \
    X(CreateProgram, PFNGLCREATEPROGRAMPROC)                                                                        \
    X(AttachShader, PFNGLATTACHSHADERPROC)                                                                          \
    X(BindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC)                                                              \
    X(BindFragDataLocation, PFNGLBINDFRAGDATALOCATIONPROC)                                                          \
    X(LinkProgram, PFNGLLINKPROGRAMPROC)                                                                            \
    X(GetProgramiv, PFNGLGETPROGRAMIVPROC)                                                                          \
    X(GetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC)                                                                \
    X(DeleteProgram, PFNGLDELETEPROGRAMPROC)                                                                        \
    X(UseProgram, PFNGLUSEPROGRAMPROC)                                                                              \
    X(GetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC)                                                              \
    X(Uniform1i, PFNGLUNIFORM1IPROC)                                                                                \
    X(Uniform4iv, PFNGLUNIFORM4IVPROC)                                                                              \
    X(Uniform4fv, PFNGLUNIFORM4FVPROC)                                                                              \
    X(UniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC)                                                                  \
    X(GenSamplers, PFNGLGENSAMPLERSPROC)                                                                            \
    X(DeleteSamplers, PFNGLDELETESAMPLERSPROC)                                                                      \
    X(BindSampler, PFNGLBINDSAMPLERPROC)                                                                            \
    X(SamplerParameteri, PFNGLSAMPLERPARAMETERIPROC)                                                                \
    X(SamplerParameterfv, PFNGLSAMPLERPARAMETERFVPROC)
