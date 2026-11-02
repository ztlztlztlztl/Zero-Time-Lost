#include "homebackgroundshader.h"
#include "homepalette.h"
#include <QtWidgets>

namespace {
const char *vertex=R"glsl(#version 330 core
layout(location=0) in vec2 position;
void main(){gl_Position=vec4(position,0.0,1.0);}
)glsl";
const char *fragment=R"glsl(#version 330 core
out vec4 FragColor;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_a,u_b,u_c;
float band(float distance,float width){return exp(-distance*distance/(width*width));}
void main(){
    vec2 uv=gl_FragCoord.xy/u_resolution;
    float x=(uv.x-.5)*u_resolution.x/u_resolution.y;
    float t=u_time;
    float a=.44+.12*sin(x*2.1+t*.075)+.035*sin(x*4.2-t*.04);
    float b=.28+.14*sin(x*1.7-t*.055+1.8)+.028*cos(x*3.6+t*.045);
    float c=.60+.11*cos(x*1.8+t*.06+2.5)+.03*sin(x*3.1-t*.05);
    float wa=.085+.022*sin(x*1.5+t*.035);
    float wb=.10+.024*cos(x*1.8-t*.04);
    float wc=.075+.022*sin(x*1.6+t*.03+1.0);
    vec3 glow=u_a*(band(uv.y-a,wa)*.64+band(uv.y-a,.24)*.17)
             +u_b*(band(uv.y-b,wb)*.58+band(uv.y-b,.28)*.14)
             +u_c*(band(uv.y-c,wc)*.56+band(uv.y-c,.22)*.14);
    // A faint filament gives the soft layers a direction without a hard edge.
    glow+=mix(u_a,u_b,.5)*band(uv.y-a+wa*.45,.007)*.15;
    vec2 calm=(uv-vec2(.5,.75))*vec2(1.7,2.2);
    glow*=1.0-.82*exp(-dot(calm,calm)*7.0);
    glow*=smoothstep(.04,.19,uv.y)*(1.0-.16*abs(uv.x-.5));
    vec3 base=vec3(16,18,22)/255.0;
    float grain=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)-.5;
    FragColor=vec4(base+glow+grain/900.0,1.0);
}
)glsl";
}
HomeBackgroundShader::HomeBackgroundShader(QWidget *parent):QOpenGLWidget(parent){
    setObjectName("homeFlow");setAttribute(Qt::WA_TransparentForMouseEvents);
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);setFormat(format);
    timer=new QTimer(this);timer->setObjectName("homeAnimationTimer");timer->setInterval(33);timer->setTimerType(Qt::PreciseTimer);connect(timer,&QTimer::timeout,this,&HomeBackgroundShader::advance);
    qApp->installEventFilter(this);connect(qApp,&QGuiApplication::applicationStateChanged,this,[this]{syncPlayback();});
}
HomeBackgroundShader::~HomeBackgroundShader(){
    qApp->removeEventFilter(this);
    if(context()&&context()->isValid()){makeCurrent();vao.destroy();vbo.destroy();program.removeAllShaders();doneCurrent();}
}
void HomeBackgroundShader::initializeGL(){
    initializeOpenGLFunctions();glClearColor(16.f/255,18.f/255,22.f/255,1);
    ready=program.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex)&&program.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment)&&program.link();
    setProperty("shaderReady",ready);
    if(!ready){shaderFailed=true;qWarning()<<"Home shader unavailable:"<<program.log();timer->stop();return;}
    const float vertices[]{-1,-1,1,-1,-1,1,1,1};vao.create();vao.bind();vbo.create();vbo.bind();vbo.allocate(vertices,sizeof(vertices));
}
void HomeBackgroundShader::resizeGL(int w,int h){glViewport(0,0,qRound(w*devicePixelRatioF()),qRound(h*devicePixelRatioF()));}
void HomeBackgroundShader::paintGL(){
    glClear(GL_COLOR_BUFFER_BIT);if(!ready||width()==0||height()==0)return;
    program.bind();program.setUniformValue("u_resolution",QVector2D(width()*devicePixelRatioF(),height()*devicePixelRatioF()));program.setUniformValue("u_time",float(seconds));
    const auto colors=HomePalette::colors(seconds);const char *names[]{"u_a","u_b","u_c"};
    for(int i=0;i<3;++i)program.setUniformValue(names[i],QVector3D(colors[i].redF(),colors[i].greenF(),colors[i].blueF()));
    vao.bind();vbo.bind();program.enableAttributeArray(0);program.setAttributeBuffer(0,GL_FLOAT,0,2,2*sizeof(float));glDrawArrays(GL_TRIANGLE_STRIP,0,4);program.release();
}
bool HomeBackgroundShader::canAnimate() const {
    const auto animation=qApp->property("ztlAnimation");
    return !shaderFailed&&isVisible()&&!window()->isMinimized()&&(!animation.isValid()||animation.toBool())&&qApp->applicationState()!=Qt::ApplicationSuspended;
}
void HomeBackgroundShader::syncPlayback(){
    if(canAnimate()){if(!timer->isActive()){elapsed.restart();timer->start();}}
    else timer->stop();
    if(isVisible())update();
}
void HomeBackgroundShader::advance(){
    if(!canAnimate()){syncPlayback();return;}
    seconds+=qMin(elapsed.restart()/1000.0,.25);emit flowTimeChanged(seconds);update();
}
void HomeBackgroundShader::hideEvent(QHideEvent *event){QOpenGLWidget::hideEvent(event);timer->stop();}
void HomeBackgroundShader::showEvent(QShowEvent *event){QOpenGLWidget::showEvent(event);syncPlayback();}
bool HomeBackgroundShader::eventFilter(QObject *object,QEvent *event){
    if((object==qApp&&event->type()==QEvent::DynamicPropertyChange&&static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName()=="ztlAnimation")||(object==window()&&event->type()==QEvent::WindowStateChange))syncPlayback();
    return QOpenGLWidget::eventFilter(object,event);
}
