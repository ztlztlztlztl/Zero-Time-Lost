#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
class QTimer;
class HomeBackgroundShader:public QOpenGLWidget,protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit HomeBackgroundShader(QWidget *parent=nullptr);
    ~HomeBackgroundShader() override;
signals:
    void flowTimeChanged(qreal seconds);
protected:
    void initializeGL() override;
    void resizeGL(int w,int h) override;
    void paintGL() override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void syncPlayback();
    void advance();
    bool canAnimate() const;
    QOpenGLShaderProgram program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QTimer *timer;
    QElapsedTimer elapsed;
    qreal seconds=32;
    bool ready=false,shaderFailed=false;
};
