#ifndef HOMEBACKGROUNDSHADER_H
#define HOMEBACKGROUNDSHADER_H

#include "constants.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QTimer>

// 同时继承 QOpenGLWidget 和 QOpenGLFunctions
class HomeBackgroundShader : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit HomeBackgroundShader(QWidget *parent = nullptr);
    ~HomeBackgroundShader();

protected:
    void initializeGL() override; // 编译 Shader，准备顶点数据，初次运行时激活
    void resizeGL(int w, int h) override; // 适配大小，窗口改变和初始化时激活
    void paintGL() override; // 激活 Shader，绘制全屏矩形

    // 看不到的时候停止绘制
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onTimeout(); // 计时器每次超时激活的槽函数

private:
    QOpenGLShaderProgram m_shaderProgram; // Qt 内置的着色器程序
    QOpenGLBuffer m_vbo; // 顶点缓冲区，在GPU里存东西
    QTimer *m_timer; // 驱动时间流逝的马达
    float m_time = 0.0f; // 传给 Shader 的时间变量 t
};

#endif // HOMEBACKGROUNDSHADER_H