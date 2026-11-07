#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QElapsedTimer>
#include <QTime>
#include <QRectF>
#include "weatherscene.h"
class QTimer;
class HomeBackgroundShader:public QOpenGLWidget,protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit HomeBackgroundShader(QWidget *parent=nullptr);
    ~HomeBackgroundShader() override;
    void setWeather(const QString &condition);
    void setLocalTime(const QTime &time);
    void setTextBounds(const QRectF &bounds);
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
    WeatherScene::Parameters scene() const;
    QOpenGLShaderProgram program;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QTimer *timer;
    QElapsedTimer elapsed;
    QElapsedTimer wallElapsed;
    QTime localTime=QTime::currentTime();
    QRectF textBounds;
    WeatherScene::Parameters sourceScene,targetScene;
    qreal transitionStart=-10;
    qreal seconds=32;
    bool ready=false,shaderFailed=false;
};
