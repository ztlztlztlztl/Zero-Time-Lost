#include "homebackgroundshader.h"
#include "uimotion.h"
#include <QtWidgets>
#include <QResource>

HomeBackgroundShader::HomeBackgroundShader(QWidget *parent):QOpenGLWidget(parent){
    static const bool resourceReady=[] {Q_INIT_RESOURCE(home_shaders);return true;}();Q_UNUSED(resourceReady)
    setObjectName("homeFlow");setAttribute(Qt::WA_TransparentForMouseEvents);
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);setFormat(format);
    timer=new QTimer(this);timer->setObjectName("homeAnimationTimer");timer->setInterval(33);timer->setTimerType(Qt::PreciseTimer);
    connect(timer,&QTimer::timeout,this,&HomeBackgroundShader::advance);
    wallElapsed.start();
    qApp->installEventFilter(this);connect(qApp,&QGuiApplication::applicationStateChanged,this,[this]{syncPlayback();});
}
HomeBackgroundShader::~HomeBackgroundShader(){
    qApp->removeEventFilter(this);
    if(context()&&context()->isValid()){makeCurrent();vao.destroy();vbo.destroy();program.removeAllShaders();doneCurrent();}
}
WeatherScene::Parameters HomeBackgroundShader::scene() const {
    if(!UiMotion::enabled())return targetScene;
    auto t=qBound(0.f,float((seconds-transitionStart)/2.6),1.f);t=t*t*(3-2*t);
    return WeatherScene::blend(sourceScene,targetScene,t);
}
void HomeBackgroundShader::setWeather(const QString &condition){
    const auto next=WeatherScene::fromCondition(condition);
    if(next.clouds==targetScene.clouds&&next.rain==targetScene.rain&&next.snow==targetScene.snow&&next.fog==targetScene.fog&&next.storm==targetScene.storm)return;
    sourceScene=scene();targetScene=next;transitionStart=seconds;
    if(!canAnimate())sourceScene=targetScene;
    update();
}
void HomeBackgroundShader::setLocalTime(const QTime &time){
    if(!time.isValid())return;
    const auto next=UiMotion::enabled()?time:QTime(time.hour(),time.minute());
    if(next==localTime)return;
    localTime=next;wallElapsed.restart();if(isVisible())update();
}
void HomeBackgroundShader::setTextBounds(const QRectF &bounds){if(textBounds!=bounds){textBounds=bounds;update();}}
void HomeBackgroundShader::initializeGL(){
    initializeOpenGLFunctions();glClearColor(16.f/255,18.f/255,22.f/255,1);
    ready=program.addShaderFromSourceFile(QOpenGLShader::Vertex,":/home/shaders/fullscreen.vert")
        &&program.addShaderFromSourceFile(QOpenGLShader::Fragment,":/home/shaders/weather.frag")&&program.link();
    setProperty("shaderReady",ready);
    if(!ready){shaderFailed=true;qWarning()<<"Home shader unavailable:"<<program.log();timer->stop();return;}
    const float vertices[]{-1,-1,1,-1,-1,1,1,1};vao.create();vao.bind();vbo.create();vbo.bind();vbo.allocate(vertices,sizeof(vertices));
}
void HomeBackgroundShader::resizeGL(int w,int h){glViewport(0,0,qRound(w*devicePixelRatioF()),qRound(h*devicePixelRatioF()));}
void HomeBackgroundShader::paintGL(){
    glClear(GL_COLOR_BUFFER_BIT);if(!ready||width()==0||height()==0)return;
    program.bind();
    program.setUniformValue("u_resolution",QVector2D(width()*devicePixelRatioF(),height()*devicePixelRatioF()));
    program.setUniformValue("u_dpr",float(devicePixelRatioF()));program.setUniformValue("u_time",float(seconds));
    program.setUniformValue("u_motion",UiMotion::enabled()?1.f:0.f);
    const auto weather=scene();
    program.setUniformValue("u_weather",QVector4D(weather.clouds,weather.rain,weather.snow,weather.fog));
    program.setUniformValue("u_storm",weather.storm);
    const auto hours=localTime.msecsSinceStartOfDay()/3600000.f+(UiMotion::enabled()?qMin<qint64>(wallElapsed.elapsed(),1500)/3600000.f:0.f);
    const auto sky=WeatherScene::celestial(hours);
    program.setUniformValue("u_celestial",QVector3D(sky.sunProgress,sky.moonProgress,sky.sunOpacity));
    program.setUniformValue("u_textRect",textBounds.isValid()
        ?QVector4D(textBounds.left()/width(),1-textBounds.bottom()/height(),textBounds.right()/width(),1-textBounds.top()/height())
        :QVector4D(.1f,.70f,.9f,.98f));
    vao.bind();vbo.bind();program.enableAttributeArray(0);program.setAttributeBuffer(0,GL_FLOAT,0,2,2*sizeof(float));
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);program.release();
}
bool HomeBackgroundShader::canAnimate() const {
    return !shaderFailed&&isVisible()&&!window()->isMinimized()&&UiMotion::enabled()&&qApp->applicationState()!=Qt::ApplicationSuspended;
}
void HomeBackgroundShader::syncPlayback(){
    if(canAnimate()){if(!timer->isActive()){elapsed.restart();timer->start();}}
    else timer->stop();
    if(!UiMotion::enabled()){
        sourceScene=targetScene;transitionStart=seconds-3;
        localTime=QTime(localTime.hour(),localTime.minute());
    }
    if(isVisible())update();
}
void HomeBackgroundShader::advance(){
    if(!canAnimate()){syncPlayback();return;}
    seconds+=qMin(elapsed.restart()/1000.0,.25);emit flowTimeChanged(seconds);update();
}
void HomeBackgroundShader::hideEvent(QHideEvent *event){QOpenGLWidget::hideEvent(event);timer->stop();}
void HomeBackgroundShader::showEvent(QShowEvent *event){QOpenGLWidget::showEvent(event);syncPlayback();}
bool HomeBackgroundShader::eventFilter(QObject *object,QEvent *event){
    if((object==qApp&&event->type()==QEvent::DynamicPropertyChange&&static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="ztlAnimation")
       ||(object==window()&&event->type()==QEvent::WindowStateChange))syncPlayback();
    return QOpenGLWidget::eventFilter(object,event);
}
