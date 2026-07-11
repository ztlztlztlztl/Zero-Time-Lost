#include "homebackgroundshader.h"

// 顶点着色器源码，把全屏矩形的 4 个坐标传给显卡
static const char* vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
    "}\n";

// 片段着色器源码
// 2. 片段着色器源码：极致极简 —— 凌厉硬边流动色带
static const char* fragmentShaderSource = R"glsl(
    #version 330 core

    out vec4 FragColor;
    uniform vec2 u_resolution;
    uniform float u_time;

    #define COL_BLACK vec3(16, 18, 22) / 255.0

    void main()
    {
        vec2 uv = gl_FragCoord.xy / u_resolution.xy;
        float t = u_time * 0.2;
        float wave = sin(uv.x * 2.5 + t) * 0.12
                   + cos(uv.x * 5.5 - t * 1.3) * 0.04;
        float totalWave = wave + 0.35;
        float distanceToWave = abs(totalWave - uv.y);
        float edgeWidth = 0.025;
        float aaBuffer = 1.5 / u_resolution.y;

        float ribbonMask = smoothstep(edgeWidth + aaBuffer, edgeWidth - aaBuffer, distanceToWave);

        vec3 waveCol = 0.5 + 0.5 * cos(totalWave * 5.0 + t * 0.6 + vec3(0.0, 2.0, 4.0));

        waveCol *= 0.9;

        vec3 finalCol = mix(COL_BLACK, waveCol, ribbonMask);

        FragColor = vec4(finalCol, 1.0);
    }
)glsl";

HomeBackgroundShader::HomeBackgroundShader(QWidget *parent)
    : QOpenGLWidget(parent)
{
    qInfo() << "[HomeBackgroundShader] 启动。";
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

void HomeBackgroundShader::hideEvent(QHideEvent *event){
    QOpenGLWidget::hideEvent(event);

    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
        qDebug() << "[HomeBackgroundShader] 背景暂停。";
    }
}

void HomeBackgroundShader::showEvent(QShowEvent *event){
    QOpenGLWidget::showEvent(event);

    if (m_timer && !m_timer->isActive()) {
        m_timer->start(ZTL::Home::Background::timeInterval);
        qDebug() << "[HomeBackgroundShader] 背景继续。";
    }
}



void HomeBackgroundShader::onTimeout()
{
    m_time += ZTL::Home::Background::timeInterval; // 时间递增
    this->update();   // 重绘
}