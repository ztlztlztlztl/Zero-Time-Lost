#include "homebackgroundshader.h"

// 顶点着色器源码，把全屏矩形的 4 个坐标传给显卡
static const char* vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
    "}\n";

// 片段着色器源码
static const char* fragmentShaderSource = R"glsl(
    #version 330 core

    out vec4 FragColor;
    uniform vec2 u_resolution;
    uniform float u_time;

    #define COUNT 20.
    #define COL_BLACK vec3(16,18,22) / 255.0

    #define SF 1./min(u_resolution.x, u_resolution.y)
    #define SS(l,s) smoothstep(SF,-SF,l-s)
    #define hue(h) clamp( abs( fract(h + vec4(3,2,1,0)/3.) * 6. - 3.) -1. , 0., 1.)

    #define MOD3 vec3(.1031,.11369,.13787)

    vec3 hash33(vec3 p3)
    {
        p3 = fract(p3 * MOD3);
        p3 += dot(p3, p3.yxz+19.19);
        return -1.0 + 2.0 * fract(vec3((p3.x + p3.y)*p3.z, (p3.x+p3.z)*p3.y, (p3.y+p3.z)*p3.x));
    }

    float simplex_noise(vec3 p)
    {
        const float K1 = 0.333333333;
        const float K2 = 0.166666667;

        vec3 i = floor(p + (p.x + p.y + p.z) * K1);
        vec3 d0 = p - (i - (i.x + i.y + i.z) * K2);

        vec3 e = step(vec3(0.0), d0 - d0.yzx);
        vec3 i1 = e * (1.0 - e.zxy);
        vec3 i2 = 1.0 - e.zxy * (1.0 - e);

        vec3 d1 = d0 - (i1 - 1.0 * K2);
        vec3 d2 = d0 - (i2 - 2.0 * K2);
        vec3 d3 = d0 - (1.0 - 3.0 * K2);

        vec4 h = max(0.6 - vec4(dot(d0, d0), dot(d1, d1), dot(d2, d2), dot(d3, d3)), 0.0);
        vec4 n = h * h * h * h * vec4(dot(d0, hash33(i)), dot(d1, hash33(i + i1)), dot(d2, hash33(i + i2)), dot(d3, hash33(i + 1.0)));

        return dot(vec4(31.316), n);
    }

    void main()
    {
        vec2 fragCoord = gl_FragCoord.xy;
        vec2 uv = fragCoord / u_resolution.y;

        float m = 0.;
        float t = u_time * .2;
        vec3 col;

        for(float i=COUNT; i>=0.; i-=1.){
            float edge = simplex_noise(vec3(uv * vec2(2., 0.) + vec2(0, t + i*.15), 1.)) * 0.2 + (0.4 / COUNT) * i + 0.2;
            float mi = SS(edge, uv.y) - SS(edge + .005, uv.y);
            m *= SS(edge, uv.y+.015);
            m += mi;

            if(mi > 0.){
                col = hue(i/COUNT).rgb;
            }
        }

        col = mix(COL_BLACK, col, m);
        FragColor = vec4(col, 1.0);
    }
)glsl";

HomeBackgroundShader::HomeBackgroundShader(QWidget *parent)
    : QOpenGLWidget(parent)
{
     // 让鼠标点击穿透背景
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // 启动计时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HomeBackgroundShader::onTimeout);
    m_timer->start(ZTL::Home::Background::timeInterval);
}

HomeBackgroundShader::~HomeBackgroundShader()
{
    makeCurrent();
    m_vbo.destroy();
    doneCurrent();
}

void HomeBackgroundShader::initializeGL()
{
    // 初始化 OpenGL 的函数指针
    initializeOpenGLFunctions();
    // 编译顶点着色器
    m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    // 编译片段着色器
    m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    // 连接
    m_shaderProgram.link();
    // 铺设画布
    float vertices[] = {
        -1.0f, -1.0f,  // 左下角
        1.0f, -1.0f,  // 右下角
        -1.0f,  1.0f,  // 左上角
        1.0f,  1.0f   // 右上角
    };
    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));
}

void HomeBackgroundShader::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void HomeBackgroundShader::paintGL()
{
    // 清空屏幕
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // 激活 Shader 程序
    m_shaderProgram.bind();
    // 把 C++ 里的变量（宽高和时间）源源不断地送进显卡内部的变量中
    m_shaderProgram.setUniformValue("u_resolution", QVector2D(width(), height()));
    m_shaderProgram.setUniformValue("u_time", m_time);
    // 绑定顶点数据
    m_vbo.bind();
    m_shaderProgram.enableAttributeArray(0);
    m_shaderProgram.setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(float));
    // 以“三角形条带”的方式绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // 解绑，释放控制权
    m_shaderProgram.release();
}

void HomeBackgroundShader::onTimeout()
{
    m_time += ZTL::Home::Background::timeInterval; // 时间递增
    this->update();   // 重绘
}