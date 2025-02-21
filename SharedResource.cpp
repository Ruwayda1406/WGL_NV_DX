
#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <assert.h>
#include "glew.h"
#include "wglew.h"
#include <iostream>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 1000
#define CUSTOMFVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)
#define KEYDOWN(vk) (GetAsyncKeyState(vk) & 0x8000)

// Must choose a sharing mechanism below
#define SHARE_TEXTURE 1 // Shared surface is created as a texture (IDirect3DTexture9)
#define SHARE_OFFSCREEN_PLAIN 0 // Shared surface is created as an off-screen plain (IDirect3DSurface9)
#define SHARE_RENDER_TARGET 0 // Shared surface is created as an off-screen plain then set to current RT.

// By default, do not perform a copy
#define DO_CPU_COPY 0 // Intermediate copy to system memory, for CPU utilization comparison. Don't actually do this.
#define DO_GPU_COPY 1 // Intermediate copy to video memory, useful for surface format conversions. Should be avoided.

unsigned int fbo = 0;

// globals
LPDIRECT3D9EX g_pD3d = NULL;
LPDIRECT3DDEVICE9EX g_pDevice = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;

struct CUSTOMVERTEX {float x, y, z;
                     DWORD color;
                     float u, v;};

IDirect3DSurface9*  g_pSurfaceRenderTarget = NULL;
IDirect3DSurface9*  g_pSharedSurface = NULL;
HANDLE              g_hSharedSurface = NULL;
IDirect3DTexture9*  dxSharedTexture = NULL;
HANDLE              g_hDXSharedTexture = NULL;
IDirect3DSurface9*  g_pSysmemSurface = NULL;

HDC                 g_hDCGL = NULL;
HANDLE              g_hDX9Device = NULL;
HANDLE              g_hGLSharedTexture = NULL;
GLuint              glSharedTexture = NULL;

unsigned int triangleProgram;
unsigned int screenProgram;

unsigned int triangleVAO;
unsigned int rectVAO;
// prototypes
void InitDX(HWND hWndDX);
void InitDX2(HWND hWndDX);
void InitGL(HWND hWndGL);
void RenderDX(void);
void RenderDX2(void);
void RenderGL(void);
void Destroy(void);
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// functions
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
{
    HWND hWndDX;
    HWND hWndGL;
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";

    RegisterClassEx(&wc);

    hWndDX = CreateWindowEx(NULL, L"WindowClass", L"DX - Shared Resource",
                          WS_OVERLAPPEDWINDOW, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                          NULL, NULL, hInstance, NULL);
    hWndGL = CreateWindowEx(NULL, L"WindowClass", L"GL - Shared Resource",
                          WS_OVERLAPPEDWINDOW, SCREEN_WIDTH+20, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                          NULL, NULL, hInstance, NULL);
    int mode, argc;
    char * argv;
    argv = (char *)CommandLineToArgvW((LPCWSTR)lpCmdLine, &argc);
    mode = atoi((const char*)lpCmdLine);

    ShowWindow(hWndDX, nCmdShow);
    ShowWindow(hWndGL, nCmdShow);

   // InitDX(hWndDX);
    InitDX2(hWndDX);
    InitGL(hWndGL);

    MSG msg;

    while(TRUE)
    {
        if (KEYDOWN(VK_ESCAPE))
        {
            SendMessage(hWndDX, WM_CLOSE, 0, 0);
            SendMessage(hWndGL, WM_CLOSE, 0, 0);
            PostQuitMessage(0);
        }

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
        {
            break;
        }
        RenderGL();
        RenderDX2();
        //RenderDX();
       
    }

    Destroy();

    return msg.wParam;
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc (hWnd, message, wParam, lParam);
}
void createTriangleProgram()
{
    unsigned int vertexShader;
    unsigned int fragmentShader;

    unsigned int VBO;

    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aColor;\n"
        "out vec3 oColor;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "   oColor = aColor;\n"
        "}\0";
    const char* fragmentShaderSource = "#version 330 core\n"
        "in vec3 oColor;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = vec4(oColor, 1.0f);\n"
        "}\n\0";

    //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int  success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::fragment:ATION_FAILED\n" << infoLog << std::endl;
    }

    triangleProgram = glCreateProgram();

    glAttachShader(triangleProgram, vertexShader);
    glAttachShader(triangleProgram, fragmentShader);
    glLinkProgram(triangleProgram);


    glGetProgramiv(triangleProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(triangleProgram, 512, NULL, infoLog);
        std::cout << "ERROR::programm::COMPILATION_FAILED\n" << infoLog << std::endl;

    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    float vertices[] = {
-0.5f, -0.5f, 0.0f,     1.0f, 0.0f, 0.0f,
 0.5f, -0.5f, 0.0f,     0.0f, 1.0f, 0.0f,
 0.0f,  0.5f, 0.0f,     0.0f, 0.0f, 1.0f
    };


    glGenVertexArrays(1, &triangleVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(triangleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // You can unbind the triangleVAO afterwards so other triangleVAO calls won't accidentally modify this triangleVAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);
}
void createScreenProgram()
{
    unsigned int vertexShader;
    unsigned int fragmentShader;

    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "   TexCoord = aTexCoord;\n"
        "}\0";
    const char* fragmentShaderSource = "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D texture0;\n"
        "void main()\n"
        "{\n"
        "   vec3 texColor = vec3(texture2D(texture0, TexCoord));\n"
        "   FragColor = vec4(texColor, 1.0f);\n"
        "}\n\0";

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int  success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::fragment:ATION_FAILED\n" << infoLog << std::endl;
    }

    screenProgram = glCreateProgram();

    glAttachShader(screenProgram, vertexShader);
    glAttachShader(screenProgram, fragmentShader);
    glLinkProgram(screenProgram);


    glGetProgramiv(screenProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(screenProgram, 512, NULL, infoLog);
        std::cout << "ERROR::programm::COMPILATION_FAILED\n" << infoLog << std::endl;

    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    float vertices[] = {
        // positions          // texture coords
         1.0f, 1.0f, 0.0f,     1.0f, 1.0f, // top right
        1.0f, -1.0f, 0.0f,    1.0f, 0.0f, // bottom right
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f, // bottom left
        -1.0f,  1.0f, 0.0f,    0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    unsigned int VBO, EBO;
    glGenVertexArrays(1, &rectVAO);
    glBindVertexArray(rectVAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}
void InitDX(HWND hWndDX)
{

    D3DPRESENT_PARAMETERS d3dpp;

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWndDX;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

    HRESULT hr = S_OK;

    // A D3D9EX device is required to create the g_hSharedSurface 
    Direct3DCreate9Ex(D3D_SDK_VERSION, &g_pD3d);

    // The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor
    hr = g_pD3d->CreateDeviceEx(D3DADAPTER_DEFAULT,
                      D3DDEVTYPE_HAL,
                      hWndDX,
                      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_MULTITHREADED,
                      &d3dpp,
                      NULL,
                      &g_pDevice);

    hr = g_pDevice->GetRenderTarget(0, &g_pSurfaceRenderTarget);
    D3DSURFACE_DESC rtDesc;
    g_pSurfaceRenderTarget->GetDesc(&rtDesc);

#if SHARE_TEXTURE
    // dxSharedTexture should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateTexture(rtDesc.Width, 
                                  rtDesc.Height,
                                  1,
                                  0,
                                  rtDesc.Format,
                                  D3DPOOL_DEFAULT,
                                  &dxSharedTexture,
                                  &g_hDXSharedTexture);

    // We want access to the underlying surface of this texture
    if (dxSharedTexture)
    {
        hr = dxSharedTexture->GetSurfaceLevel(0, &g_pSharedSurface);
    }
#elif SHARE_OFFSCREEN_PLAIN
    // g_pSharedSurface should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateOffscreenPlainSurface(rtDesc.Width, 
                                                rtDesc.Height, 
                                                rtDesc.Format, 
                                                D3DPOOL_DEFAULT, 
                                                &g_pSharedSurface, 
                                                &g_hSharedSurface);
#elif SHARE_RENDER_TARGET
    // g_pSharedSurface should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateRenderTarget(rtDesc.Width,
                                       rtDesc.Height,
                                       rtDesc.Format,
                                       rtDesc.MultiSampleType,
                                       rtDesc.MultiSampleQuality,
                                       FALSE,
                                       &g_pSharedSurface,
                                       &g_hSharedSurface);

    // Replace the default render target with the new render target backed by a shared surface
    if (g_pSurfaceRenderTarget)
        g_pSurfaceRenderTarget->Release();

    hr = g_pDevice->SetRenderTarget(0, g_pSharedSurface);
    hr = g_pDevice->GetRenderTarget(0, &g_pSurfaceRenderTarget);
#else
    assert(!"Must choose at least one sharing mechanism!");
#endif

#if DO_CPU_COPY
    // create an intermediate copy in system memory, for performance comparison
    hr = g_pDevice->CreateOffscreenPlainSurface(rtDesc.Width, 
                                                rtDesc.Height, 
                                                rtDesc.Format, 
                                                D3DPOOL_SYSTEMMEM, 
                                                &g_pSysmemSurface, 
                                                NULL);
#endif

    hr = g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);


    CUSTOMVERTEX vertices[] = 
    {
        {  2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(0, 0, 255)},
        {  0.0f,  2.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 0)},
        { -2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0)},
    };

    hr = g_pDevice->CreateVertexBuffer(9*sizeof(CUSTOMVERTEX),
                                       0,
                                       CUSTOMFVF,
                                       D3DPOOL_DEFAULT,
                                       &g_pVB,
                                       NULL);

    VOID* pVoid;
    hr = g_pVB->Lock(0, 0, (void**)&pVoid, 0);
    memcpy(pVoid, vertices, sizeof(vertices));
    hr = g_pVB->Unlock();
}


void InitGL(HWND hWndGL)
{
    static	PIXELFORMATDESCRIPTOR pfd=
    {
        sizeof(PIXELFORMATDESCRIPTOR),              // Size Of This Pixel Format Descriptor
        1,                                          // Version Number
        PFD_DRAW_TO_WINDOW |                        // Format Must Support Window
        PFD_SUPPORT_OPENGL |                        // Format Must Support OpenGL
        PFD_DOUBLEBUFFER,                           // Must Support Double Buffering
        PFD_TYPE_RGBA,                              // Request An RGBA Format
        32,                                         // Select Our Color Depth
        0, 0, 0, 0, 0, 0,                           // Color Bits Ignored
        0,                                          // No Alpha Buffer
        0,                                          // Shift Bit Ignored
        0,                                          // No Accumulation Buffer
        0, 0, 0, 0,                                 // Accumulation Bits Ignored
        16,                                         // 16Bit Z-Buffer (Depth Buffer)  
        0,                                          // No Stencil Buffer
        0,                                          // No Auxiliary Buffer
        PFD_MAIN_PLANE,                             // Main Drawing Layer
        0,                                          // Reserved
        0, 0, 0                                     // Layer Masks Ignored
    };
    
    g_hDCGL = GetDC(hWndGL);
    GLuint PixelFormat = ChoosePixelFormat(g_hDCGL, &pfd);
    SetPixelFormat(g_hDCGL, PixelFormat, &pfd);
    HGLRC hRC = wglCreateContext(g_hDCGL);
    wglMakeCurrent(g_hDCGL, hRC);

    GLenum x = glewInit();

    // Register the shared DX texture with OGL
    if (WGLEW_NV_DX_interop)
    {
        // Acquire a handle to the D3D device for use in OGL
        g_hDX9Device = wglDXOpenDeviceNV(g_pDevice);

        if (g_hDX9Device)
        {
            glGenTextures(1, &glSharedTexture);

#if SHARE_TEXTURE
            // This registers a resource that was created as shared in DX with its shared handle
            bool success = wglDXSetResourceShareHandleNV(dxSharedTexture, g_hDXSharedTexture);

            // g_hGLSharedTexture is the shared texture data, now identified by the glSharedTexture name
            g_hGLSharedTexture = wglDXRegisterObjectNV(g_hDX9Device, dxSharedTexture,
                                                       glSharedTexture,
                                                       GL_TEXTURE_2D,
                                                       WGL_ACCESS_READ_WRITE_NV);
            // attach the Direct3D buffers to an FBO
            if (fbo > 0)
            {
                glDeleteFramebuffers(1, &fbo);
            }

            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glBindTexture(GL_TEXTURE_2D, glSharedTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glSharedTexture, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
            // This registers a resource that was created as shared in DX with its shared handle
            bool success = wglDXSetResourceShareHandleNV(g_pSharedSurface, g_hSharedSurface);

            // g_hGLSharedTexture is the shared texture data, now identified by the glSharedTexture name
            g_hGLSharedTexture = wglDXRegisterObjectNV(g_hDX9Device,
                                                       g_pSharedSurface,
                                                       g_GLTexture,
                                                       GL_TEXTURE_2D,
                                                       WGL_ACCESS_READ_ONLY_NV);
#endif
        }
    }
    createTriangleProgram();
    createScreenProgram();
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT,0, -1, 1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


void RenderDX(void)
{    
    // Set up transformations
    D3DXMATRIX matView;
    D3DXMatrixLookAtLH(&matView,
                       &D3DXVECTOR3 (0.0f, 0.0f, -10.0f),
                       &D3DXVECTOR3 (0.0f, 0.0f, 0.0f),
                       &D3DXVECTOR3 (0.0f, 1.0f, 0.0f));
    g_pDevice->SetTransform(D3DTS_VIEW, &matView);

    D3DXMATRIX matProjection;
    D3DXMatrixPerspectiveFovLH(&matProjection,
                               D3DXToRadian(45),
                               (FLOAT)SCREEN_WIDTH / (FLOAT)SCREEN_HEIGHT,
                               1.0f,
                               25.0f);
    g_pDevice->SetTransform(D3DTS_PROJECTION, &matProjection);

    D3DXMATRIX matTranslate;
    D3DXMatrixTranslation(&matTranslate, 0.0f, 0.0f, 0.0f);

    D3DXMATRIX matRotate;
    static float rot = 0; 
    rot+=0.01;
    D3DXMatrixRotationZ(&matRotate, rot);

    D3DXMATRIX matTransform = matRotate * matTranslate;

    HRESULT hr = S_OK;
    hr = g_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 60), 1.0f, 0);

    // Draw a spinning triangle
    hr = g_pDevice->BeginScene();

    hr = g_pDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
    hr = g_pDevice->SetFVF(CUSTOMFVF);
    hr = g_pDevice->SetTransform(D3DTS_WORLD, &matTransform);
    hr = g_pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

    // Copy the render target to the shared surface
#if DO_CPU_COPY
    // GPU to CPU copy
    hr = g_pDevice->GetRenderTargetData(g_pSurfaceRenderTarget, g_pSysmemSurface);
    // CPU to GPU copy
    hr = g_pDevice->UpdateSurface(g_pSysmemSurface, NULL, g_pSharedSurface, NULL);
#elif DO_GPU_COPY && (SHARE_TEXTURE || SHARE_OFFSCREEN_PLAIN)
    // StretchRect between two D3DPOOL_DEFAULT surfaces will be a GPU Blt.
    // Note that GetRenderTargetData() cannot be used because it is intended to copy from GPU to CPU.
    hr = g_pDevice->StretchRect(g_pSurfaceRenderTarget, NULL, g_pSharedSurface, NULL, D3DTEXF_NONE);
#elif DO_GPU_COPY && (SHARE_RENDER_TARGET)
    assert(!"Can't do GPU copy to/from same surface (if sharing RT)");
#elif (SHARE_TEXTURE || SHARE_OFFSCREEN_PLAIN)
    assert(!"Must perform a copy if not sharing render target directly");
#endif
    hr = g_pDevice->EndScene();

    hr = g_pDevice->Present(NULL, NULL, NULL, NULL);
}


void RenderGL(void)
{
    //glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
   // glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
   
    //++++++++++++++++++ Pass 1 ++++++++++++++++++++++++++++++
     // Lock the shared surface
    wglDXLockObjectsNV(g_hDX9Device, 1, &g_hGLSharedTexture);
    glUseProgram(triangleProgram);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);  
    glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, glSharedTexture);
    
    glBindVertexArray(triangleVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
   
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    //++++++++++++++++++ Pass 2 ++++++++++++++++++++++++++++++
    glUseProgram(screenProgram);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    // bind Texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glSharedTexture);
    // render container
    glBindVertexArray(rectVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    // Unlock the shared surface
    wglDXUnlockObjectsNV(g_hDX9Device, 1, &g_hGLSharedTexture);
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    SwapBuffers(g_hDCGL);

   
}


void Destroy(void)
{
    if (WGLEW_NV_DX_interop)
    {
        if (g_hGLSharedTexture)
            wglDXUnregisterObjectNV(g_hDX9Device, g_hGLSharedTexture);
        if (g_hDX9Device)
            wglDXCloseDeviceNV(g_hDX9Device);
    }
    glDeleteFramebuffers(1, &fbo);

    if (g_pSysmemSurface)
        g_pSysmemSurface->Release();
    if (g_pSharedSurface)
        g_pSharedSurface->Release();
    if (dxSharedTexture)
        dxSharedTexture->Release();
    if (g_pSurfaceRenderTarget)
        g_pSurfaceRenderTarget->Release();
    if (g_pVB)
        g_pVB->Release();
    if (g_pDevice)
        g_pDevice->Release();
    if (g_pD3d)
        g_pD3d->Release();
}
void RenderDX2(void)
{
    D3DXMATRIX matRotate;
    static float rot = 0;
    rot += 0.01;
    D3DXMatrixRotationZ(&matRotate, rot);

    D3DXMATRIX matTransform = matRotate; // * matTranslate;

    HRESULT hr = S_OK;
    hr = g_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 60), 1.0f, 0);

    // Draw a spinning triangle
    hr = g_pDevice->BeginScene();

    hr = g_pDevice->SetStreamSource(0, g_pVB, 0, sizeof(CUSTOMVERTEX));
    hr = g_pDevice->SetFVF(CUSTOMFVF);

    // S: I commented this so simplyfy 
   //hr = g_pDevice->SetTransform(D3DTS_WORLD, &matTransform);
    //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    hr = g_pDevice->SetTexture(0, dxSharedTexture);

    hr = g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
    hr = g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    hr = g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);


    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    hr = g_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

    // Copy the render target to the shared surface
#if DO_CPU_COPY
    // GPU to CPU copy
    hr = g_pDevice->GetRenderTargetData(g_pSurfaceRenderTarget, g_pSysmemSurface);
    // CPU to GPU copy
    hr = g_pDevice->UpdateSurface(g_pSysmemSurface, NULL, g_pSharedSurface, NULL);
#elif DO_GPU_COPY && (SHARE_TEXTURE || SHARE_OFFSCREEN_PLAIN)
    // StretchRect between two D3DPOOL_DEFAULT surfaces will be a GPU Blt.
    // Note that GetRenderTargetData() cannot be used because it is intended to copy from GPU to CPU.
    hr = g_pDevice->StretchRect(g_pSurfaceRenderTarget, NULL, g_pSharedSurface, NULL, D3DTEXF_NONE);
#elif DO_GPU_COPY && (SHARE_RENDER_TARGET)
    assert(!"Can't do GPU copy to/from same surface (if sharing RT)");
#elif (SHARE_TEXTURE || SHARE_OFFSCREEN_PLAIN)
    assert(!"Must perform a copy if not sharing render target directly");
#endif
    hr = g_pDevice->EndScene();

    hr = g_pDevice->Present(NULL, NULL, NULL, NULL);
}
void InitDX2(HWND hWndDX)
{

    D3DPRESENT_PARAMETERS d3dpp;

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hWndDX;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

    HRESULT hr = S_OK;

    // A D3D9EX device is required to create the g_hSharedSurface 
    Direct3DCreate9Ex(D3D_SDK_VERSION, &g_pD3d);

    // The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor
    hr = g_pD3d->CreateDeviceEx(D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWndDX,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_MULTITHREADED,
        &d3dpp,
        NULL,
        &g_pDevice);

    hr = g_pDevice->GetRenderTarget(0, &g_pSurfaceRenderTarget);
    D3DSURFACE_DESC rtDesc;
    g_pSurfaceRenderTarget->GetDesc(&rtDesc);

#if SHARE_TEXTURE
    // dxSharedTexture should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateTexture(rtDesc.Width,
        rtDesc.Height,
        1,
        0,
        rtDesc.Format,
        D3DPOOL_DEFAULT,
        &dxSharedTexture,
        &g_hDXSharedTexture);

    // We want access to the underlying surface of this texture
    if (dxSharedTexture)
    {
        hr = dxSharedTexture->GetSurfaceLevel(0, &g_pSharedSurface);
    }
#elif SHARE_OFFSCREEN_PLAIN
    // g_pSharedSurface should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateOffscreenPlainSurface(rtDesc.Width,
        rtDesc.Height,
        rtDesc.Format,
        D3DPOOL_DEFAULT,
        &g_pSharedSurface,
        &g_hSharedSurface);
#elif SHARE_RENDER_TARGET
    // g_pSharedSurface should be able to be opened in OGL via the WGL_NV_DX_interop extension
    // Vendor support for various textures/surfaces may vary
    hr = g_pDevice->CreateRenderTarget(rtDesc.Width,
        rtDesc.Height,
        rtDesc.Format,
        rtDesc.MultiSampleType,
        rtDesc.MultiSampleQuality,
        FALSE,
        &g_pSharedSurface,
        &g_hSharedSurface);

    // Replace the default render target with the new render target backed by a shared surface
    if (g_pSurfaceRenderTarget)
        g_pSurfaceRenderTarget->Release();

    hr = g_pDevice->SetRenderTarget(0, g_pSharedSurface);
    hr = g_pDevice->GetRenderTarget(0, &g_pSurfaceRenderTarget);
#else
    assert(!"Must choose at least one sharing mechanism!");
#endif

#if DO_CPU_COPY
    // create an intermediate copy in system memory, for performance comparison
    hr = g_pDevice->CreateOffscreenPlainSurface(rtDesc.Width,
        rtDesc.Height,
        rtDesc.Format,
        D3DPOOL_SYSTEMMEM,
        &g_pSysmemSurface,
        NULL);
#endif

    //hr = g_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
  //  hr = g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

  /*  CUSTOMVERTEX vertices[] =
    {
        {  2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(0, 0, 255)},
        {  0.0f,  2.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 0)},
        { -2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0)},
    };*/
    CUSTOMVERTEX vertices[] =
    {
        { -2.0f, 2.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0), 1.0, 0.0},
        {  2.0f,  2.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0), 0.0, 0.0},
        { -2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(255,0 , 0), 1.0, 1.0},
        {  2.0f, -2.0f, 0.0f, D3DCOLOR_XRGB(0, 0, 0), 0.0 , 1.0},


    };

    hr = g_pDevice->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX),
        0,
        CUSTOMFVF,
        D3DPOOL_DEFAULT,
        &g_pVB,
        NULL);

    VOID* pVoid;
    hr = g_pVB->Lock(0, 0, (void**)&pVoid, 0);
    memcpy(pVoid, vertices, sizeof(vertices));
    hr = g_pVB->Unlock();
}

