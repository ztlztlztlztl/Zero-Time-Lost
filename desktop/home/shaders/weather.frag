#version 330 core
out vec4 FragColor;
uniform vec2 u_resolution;
uniform float u_dpr,u_time,u_motion,u_storm;
uniform vec4 u_weather; // cloud, rain, snow, fog; all independent, blended by the client
uniform vec3 u_celestial; // sun arc, moon arc, sun opacity
uniform vec4 u_textRect; // normalized left, bottom, right, top of the real clock layout
const float PI=3.14159265;
const float FLOW_DIRECTION=-1.; // Cloud and wind travel share one prevailing direction.
const vec3 BASE=vec3(16,18,22)/255.;
const vec3 BLUE=vec3(88,169,214)/255.;
const vec3 FOAM=vec3(159,209,232)/255.;
const vec3 SUN=vec3(240,181,109)/255.;
const vec3 MOON=vec3(173,160,232)/255.;
const vec3 CLOUD=vec3(231,155,174)/255.;
const vec3 WIND=vec3(169,201,132)/255.;

float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
// Quintic interpolation gives continuous motion, never reseeded per frame.
float smoothNoise(vec2 p){
    vec2 cell=floor(p),f=fract(p);
    vec2 u=f*f*f*(f*(f*6.-15.)+10.);
    float a=hash(cell),b=hash(cell+vec2(1,0));
    float c=hash(cell+vec2(0,1)),d=hash(cell+vec2(1,1));
    return mix(mix(a,b,u.x),mix(c,d,u.x),u.y)*2.-1.;
}
float ink(float distance,float halfWidth,float aa){return 1.-smoothstep(halfWidth-aa,halfWidth+aa,abs(distance));}
float segment(vec2 p,vec2 a,vec2 b){vec2 v=b-a;return length(p-a-v*clamp(dot(p-a,v)/dot(v,v),0.,1.));}
float smoothMin(float a,float b,float k){float h=max(k-abs(a-b),0.)/k;return min(a,b)-h*h*k*.25;}
float cloudShape(vec2 q,int kind){
    // Rounded bases and distinct dome arrangements, rather than copies of one blob.
    if(kind==0){ // Low, broad bank, behind the taller clouds.
        float d=segment(q,vec2(-1.18,-.10),vec2(1.14,-.10))-.23;
        d=smoothMin(d,length(q-vec2(-.89,.03))-.30,.08);
        d=smoothMin(d,length(q-vec2(-.36,.14))-.40,.08);
        d=smoothMin(d,length(q-vec2(.23,.09))-.34,.09);
        return smoothMin(d,length(q-vec2(.77,.00))-.32,.08);
    }
    if(kind==1){ // Asymmetric, high cumulus with a clear central dome.
        float d=segment(q,vec2(-.79,-.12),vec2(.78,-.12))-.25;
        d=smoothMin(d,length(q-vec2(-.70,.02))-.35,.05);
        d=smoothMin(d,length(q-vec2(-.32,.25))-.34,.06);
        d=smoothMin(d,length(q-vec2(.16,.32))-.51,.05);
        return smoothMin(d,length(q-vec2(.71,.00))-.36,.06);
    }
    // Wide, twin-domed cloud; the low left shoulder breaks the symmetry.
    float d=segment(q,vec2(-.99,-.12),vec2(.94,-.12))-.24;
    d=smoothMin(d,length(q-vec2(-.86,.02))-.32,.07);
    d=smoothMin(d,length(q-vec2(-.40,.21))-.44,.06);
    d=smoothMin(d,length(q-vec2(.25,.11))-.39,.07);
    return smoothMin(d,length(q-vec2(.82,.00))-.33,.06);
}
float arcDistance(vec2 q,float radius,float start,float sweep){
    float angle=mod(atan(q.y,q.x)-start+2.*PI,2.*PI);
    if(angle<=sweep)return abs(length(q)-radius);
    return min(length(q-radius*vec2(cos(start),sin(start))),
               length(q-radius*vec2(cos(start+sweep),sin(start+sweep))));
}
float quietMask(vec2 p,vec2 size){
    vec2 lo=u_textRect.xy*size-vec2(14,12),hi=u_textRect.zw*size+vec2(14,12);
    vec2 outside=max(lo-p,p-hi);
    float avoid=smoothstep(-5.,14.,max(outside.x,outside.y));
    return avoid*smoothstep(size.y*.075,size.y*.15,p.y);
}
float flash(float t){
    // One soft pulse per 22 seconds; no full-screen flash or rapid strobe.
    float phase=mod(t+12.,22.);
    return u_motion*u_storm*smoothstep(0.,.30,phase)*(1.-smoothstep(.34,.90,phase));
}
void drawBody(inout vec3 color,vec2 p,vec2 size,float phase,float visibility,bool moon,float ceiling,float aa){
    if(visibility<.001)return;
    float radius=clamp(min(size.x,size.y)*.060,28.75,53.75);
    float horizon=size.y*.365;
    float rise=clamp(ceiling-horizon-radius-16.,size.y*.08,size.y*.23);
    vec2 center=vec2(size.x*(.5+.35*cos(PI*phase)),horizon+rise*sin(PI*phase));
    vec2 q=p-center;
    float d=length(q)-radius;
    if(moon)d=max(d,-(length(q-vec2(radius*.49,radius*.22))-radius*.85));
    float shape=1.-smoothstep(-aa,aa,d);
    vec3 body=moon?MOON:SUN;
    float relief=clamp(.60+.28*q.y/radius-.13*q.x/radius,.35,1.);
    vec3 shade=mix(body*.70,mix(body,vec3(1),.20),relief);
    if(!moon)shade=mix(shade,vec3(.97,.58,.28),.18*(1.-sin(PI*phase)));
    visibility*=1.-u_weather.x*.42;
    color=mix(color,shade,shape*visibility*.95);
    color+=body*exp(-max(d,0.)/3.)*(1.-shape)*visibility*.055;
}
void drawClouds(inout vec3 color,vec2 p,vec2 size,float ceiling,float aa,float lightning){
    if(u_weather.x<.001)return;
    for(int i=0;i<3;++i){
        float n=float(i);
        float presence=i==0?smoothstep(.48,.82,u_weather.x)
                     :(i==1?smoothstep(.02,.22,u_weather.x):smoothstep(.20,.47,u_weather.x));
        if(presence<.001)continue;
        float drift=smoothNoise(vec2(u_time*.075+n*9.,n*17.3));
        float bob=smoothNoise(vec2(u_time*.11+n*13.,n*5.7+31.));
        float scale=.90*(i==0?113.:(i==1?89.:103.))*clamp(size.x/1000.,.67,1.22);
        scale*=mix(.77,1.18,smoothstep(.15,.90,u_weather.x));
        scale*=1.+bob*.025;
        float travel=fract((i==0?.93:(i==1?.39:.75))+FLOW_DIRECTION*u_time*(.0014+n*.00018));
        vec2 center=vec2(travel*(size.x+scale*3.2)-scale*1.6,
                         min(size.y*(i==0?.54:(i==1?.49:.51)),ceiling-scale*(i==1?.86:.59)));
        center+=vec2(drift*5.,bob*5.+sin(u_time*.12+n*2.)*1.5);
        vec2 q=(p-center)/scale;
        if(abs(q.x)>1.6||q.y<-.43||q.y>.89)continue;
        float d=cloudShape(q,i)*scale;
        float mask=1.-smoothstep(-aa,aa,d);
        float depth=i==0?.64:(i==1?1.:.85);
        vec3 top=mix(CLOUD,MOON,.09+n*.045)*depth*(1.-u_storm*.24);
        vec3 bottom=mix(CLOUD*.40,BLUE*.34,u_storm*.48)*depth;
        vec3 shade=mix(bottom,top,smoothstep(-.37,.60,q.y));
        shade+=vec3(.17,.18,.24)*lightning;
        color=mix(color,shade,mask*presence*.96);
        color+=CLOUD*ink(d,.85,aa)*presence*.13*depth;
    }
}
void drawWind(inout vec3 color,vec2 p,vec2 size,float aa){
    for(int i=0;i<3;++i){
        float n=float(i),period=38.+n*7.;
        float phase=fract((i==0?.19:(i==1?.57:.88))+FLOW_DIRECTION*u_time/period);
        float envelope=smoothstep(.01,.13,phase)*(1.-smoothstep(.86,.99,phase));
        vec2 center=vec2(mix(-125.,size.x+125.,phase),size.y*(.44+.052*n));
        center+=vec2(smoothNoise(vec2(u_time*.12+n*3.,27.))*6.,
                     smoothNoise(vec2(u_time*.17+n*11.,9.))*7.);
        float scale=.80*(1.+smoothNoise(vec2(u_time*.10+n*7.,53.))*.035);
        vec2 q=(p-center)/scale;
        if(abs(q.x)>113.||abs(q.y)>65.)continue;
        float localAA=aa/scale;
        float r=i==0?17.:(i==1?13.:21.);
        vec2 curl=vec2(-39.,r);
        float mainLine=min(segment(q,vec2(-39.,0),vec2(73.-n*8.,0)),
                           arcDistance(q-curl,r,0.,1.5*PI));
        float secondary=segment(q,vec2(-63.+n*6.,-12.),vec2(35.,-12.));
        if(i!=1){
            float lowerRadius=i==0?11.:15.;
            secondary=min(secondary,segment(q,vec2(-15.,-25.),vec2(58.,-25.)));
            secondary=min(secondary,arcDistance(q-vec2(-15.,-25.-lowerRadius),lowerRadius,PI*.5,1.5*PI));
        }else{
            secondary=min(secondary,segment(q,vec2(-4.,15.),vec2(50.,15.)));
        }
        float accent=ink(segment(q,vec2(47.,-12.),vec2(76.+n*4.,-12.)),1.5,localAA);
        float shape=max(ink(mainLine,2.25,localAA),ink(secondary,1.5,localAA)*.72);
        color=mix(color,WIND,max(shape,accent*.65)*envelope*(.47+n*.055));
    }
}
void drawPrecipitation(inout vec3 color,vec2 p,vec2 size,float aa){
    float region=smoothstep(size.y*.23,size.y*.31,p.y)*(1.-smoothstep(size.y*.57,size.y*.66,p.y));
    if(region<.001)return;
    if(u_weather.y>.001)for(int i=0;i<3;++i){
        float n=float(i),speed=105.+n*43.;
        vec2 cell=vec2(40.+n*15.,108.+n*28.);
        vec2 q=vec2(p.x+FLOW_DIRECTION*.17*p.y+n*137.,p.y+u_time*speed);
        vec2 id=floor(q/cell),local=mod(q,cell)-cell*.5;
        float r=hash(id+n),enabled=smoothstep(r-.08,r+.08,u_weather.y);
        vec2 offset=vec2((hash(id+4.)-.5)*cell.x*.6,(hash(id+9.)-.5)*cell.y*.5);
        float drop=ink(local.x-offset.x,1.05+n*.23,aa)
                   *(1.-smoothstep(9.+n*3.,12.+n*3.,abs(local.y-offset.y)));
        color=mix(color,mix(BLUE,FOAM,.24),drop*enabled*region*(.24+n*.10));
    }
    if(u_weather.z>.001)for(int i=0;i<3;++i){
        float n=float(i),speed=19.+n*14.;
        vec2 cell=vec2(61.+n*22.,83.+n*14.);
        vec2 q=vec2(p.x-FLOW_DIRECTION*u_time*7.+n*197.+12.*sin(p.y*.012+u_time*.30+n),p.y+u_time*speed);
        vec2 id=floor(q/cell),local=mod(q,cell)-cell*.5;
        float r=hash(id+n),enabled=smoothstep(r-.08,r+.08,u_weather.z);
        vec2 offset=vec2((hash(id+8.)-.5)*cell.x*.65,(hash(id+3.)-.5)*cell.y*.60);
        vec2 flake=local-offset;
        float radius=1.6+n*.85;
        float shape=1.-smoothstep(radius-aa,radius+aa,length(flake));
        if(i==2){
            float branch=1e3;
            for(int arm=0;arm<3;++arm){float a=float(arm)*PI/3.;vec2 v=vec2(cos(a),sin(a))*4.5;branch=min(branch,segment(flake,-v,v));}
            shape=max(shape,1.-smoothstep(.45,.45+aa,branch));
        }
        color=mix(color,mix(FOAM,vec3(.90,.91,.96),.50),shape*enabled*region*(.40+n*.14));
    }
}
void drawLightning(inout vec3 color,vec2 p,vec2 size,float amount,float aa){
    if(amount<.001)return;
    float cycle=floor((u_time+12.)/22.);
    vec2 origin=vec2(size.x*(.26+.48*hash(vec2(cycle,5))),size.y*.54);
    vec2 q=p-origin;
    float d=min(segment(q,vec2(0,0),vec2(-12,-24)),segment(q,vec2(-12,-24),vec2(5,-21)));
    d=min(d,segment(q,vec2(5,-21),vec2(-15,-59)));
    vec3 inkColor=mix(MOON,FOAM,.55);
    color+=inkColor*(exp(-d/5.)*.13+ink(d,1.6,aa)*.38)*amount;
}
void drawSea(inout vec3 color,vec2 p,vec2 size,float aa){
    float heavy=max(u_weather.y,u_weather.z)*.65+u_storm*.35;
    float edge=smoothstep(12.,58.,p.x)*smoothstep(12.,58.,size.x-p.x);
    float night=mix(.76,1.,u_celestial.z);
    for(int i=0;i<5;++i){
        float n=float(i),seed=hash(vec2(n,41.2));
        float k=(.0068+n*.0018+seed*.0007)*clamp(1000./size.x,.72,1.35);
        // Noise depends only on layer and time, so a whole band moves together.
        float bob=smoothNoise(vec2(u_time*.055+n*5.3,n*7.1+63.));
        // Specify logical pixels/second, not phase speed: increasing frequency
        // must not cancel the visible speed differences between the layers.
        float baseSpeed=9.+n*8.+n*n*.75; // Rear 9, front 53; unmistakable parallax.
        // This bounded time offset varies speed by at most 33.75%, preserving
        // leftward travel and the clear speed hierarchy over long sessions.
        float flowTime=u_time+2.*smoothNoise(vec2(u_time*.045+n*11.7,n*4.3+29.));
        float phase=(p.x-FLOW_DIRECTION*baseSpeed*flowTime)*k+n*2.13+seed*.9;
        float heightScale=clamp(size.y/760.,.70,1.15)*(1.+heavy*.40);
        float swell=(26.+n*.5)*heightScale;
        float crest=size.y*(.400-n*.048)+swell*(sin(phase)+.24*sin(phase*1.73+n))
                    +heightScale*((18.+n*.5)*bob+(seed-.5)*12.);
        float slope=swell*k*(cos(phase)+.24*1.73*cos(phase*1.73+n));
        float normal=sqrt(1.+slope*slope);
        // Opaque water below each crest hides rear contours at crossings.
        // This gives the nested lines the shape of overlapping wave bands.
        vec3 water=mix(BASE,BLUE,.018+n*.009)*night;
        float fill=1.-smoothstep(-aa,aa,p.y-crest);
        color=mix(color,water,fill*edge);
        for(int line=0;line<5;++line){
            float j=float(line),spacing=9.5+n*.45;
            float contour=(p.y-crest+j*spacing)/normal;
            float halfWidth=line==0?3.2+n*.13:(line==1?1.65:1.05);
            vec3 rim=mix(BLUE,FOAM,.14+n*.065);
            vec3 deep=mix(BLUE*.43,BLUE*.69,n/4.);
            vec3 paint=line==0?mix(deep,rim,smoothstep(-halfWidth,halfWidth,contour))
                              :mix(BLUE,FOAM,.40+j*.08);
            float opacity=line==0?.94:(.58-j*.065)*(.62+n*.095);
            color=mix(color,paint*night*mix(.68,1.,n/4.),ink(contour,halfWidth,aa)*edge*opacity);
        }
    }
}
void main(){
    vec2 size=u_resolution/u_dpr,p=gl_FragCoord.xy/u_dpr;
    float aa=.85/u_dpr;
    float ceiling=clamp(u_textRect.y*size.y-24.,size.y*.49,size.y*.72);
    vec3 color=BASE;
    float lightning=flash(u_time);
    drawBody(color,p,size,u_celestial.x,u_celestial.z,false,ceiling,aa);
    drawBody(color,p,size,u_celestial.y,1.-u_celestial.z,true,ceiling,aa);
    drawClouds(color,p,size,ceiling,aa,lightning);
    drawWind(color,p,size,aa);
    drawPrecipitation(color,p,size,aa);
    drawLightning(color,p,size,lightning,aa);
    drawSea(color,p,size,aa);
    if(u_weather.w>.001){
        float fog=exp(-pow((p.y-size.y*.40-8.*sin(p.x*.005+u_time*.06))/(size.y*.10),2.));
        color=mix(color,mix(BASE,vec3(.36,.43,.48),.25),fog*u_weather.w*.52);
    }
    color=mix(BASE,color,quietMask(p,size));
    FragColor=vec4(clamp(color,0.,1.),1.);
}
