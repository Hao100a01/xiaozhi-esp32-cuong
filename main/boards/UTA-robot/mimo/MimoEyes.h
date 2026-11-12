// Mimo v0.3 portable core from Hao100a01/mimo-vector-eyes, esp32/src/MimoEyes.h.
// Upstream Git blob: de5384c0c71e558ca9fc93fb665dfab46677f75e
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace mimo {
constexpr float PI=3.14159265358979323846f;
enum class Emotion:uint8_t {Neutral,Happy,Curious,Mischief,Excited,Love,Shy,Surprised,Sleepy,Sleep,Sad,Angry,Confused,Suspicious,Proud,Sparkle,Focus,Dizzy};
enum class Gesture:uint8_t {None,Blink,DoubleBlink,Wink,Scan,Bounce,Peek,Shake,Nod};
struct Settings {float speed=1,intensity=.85f,width=36,height=67,gap=112;bool autonomous=true;};
struct Pose {float x=0,y=0,lw=1,rw=1,lh=1,rh=1,la=0,ra=0,bend=0;uint8_t shape=0;};
inline float clamp(float x,float lo=0,float hi=1){return std::max(lo,std::min(x,hi));}
inline float pulse(float t,float center,float width){return std::max(0.f,1.f-std::fabs(t-center)/width);}
inline Pose expression(Emotion id,float t,float amount){
 Pose p;const float s=std::sin(t);
 switch(id){
 case Emotion::Happy:p.lw=p.rw=1.28;p.lh=p.rh=.23;p.bend=20;p.y=-3;break;
 case Emotion::Curious:p.lw=.94;p.rw=1.12;p.lh=.79;p.rh=1.19;p.la=-9;p.ra=9;p.x=5+s*4;p.y=-3;break;
 case Emotion::Mischief:p.lh=.52;p.rh=.75;p.lw=1.16;p.rw=1.05;p.la=14;p.ra=-12;p.x=s*5;p.bend=3;break;
 case Emotion::Excited:p.lw=p.rw=p.lh=p.rh=1.15;p.y=-std::fabs(std::sin(t*2))*9;break;
 case Emotion::Love:p.shape=1;p.lw=p.rw=1.45;p.lh=p.rh=.88;p.y=s*3;break;
 case Emotion::Shy:p.lw=p.rw=.88;p.lh=p.rh=.7;p.la=-6;p.ra=6;p.x=12;p.y=10;break;
 case Emotion::Surprised:p.lw=p.rw=1.12;p.lh=p.rh=1.3;p.y=-5;break;
 case Emotion::Sleepy:p.lw=p.rw=1.15;p.lh=.37;p.rh=.42;p.la=-5;p.ra=5;p.y=7+s*2;break;
 case Emotion::Sleep:p.lw=p.rw=1.25;p.lh=p.rh=.12;p.bend=-7;p.y=9+s*2;break;
 case Emotion::Sad:p.lw=p.rw=1.05;p.lh=p.rh=.65;p.la=-19;p.ra=19;p.y=9;p.bend=-3;break;
 case Emotion::Angry:p.lw=p.rw=1.2;p.lh=p.rh=.55;p.la=22;p.ra=-22;p.y=2;break;
 case Emotion::Confused:p.lh=.65;p.rh=1.05;p.la=-15;p.ra=8;p.x=s*4;p.y=s*3;break;
 case Emotion::Suspicious:p.lw=p.rw=1.15;p.lh=p.rh=.32;p.x=15;p.la=4;p.ra=-4;break;
 case Emotion::Proud:p.lw=p.rw=1.2;p.lh=p.rh=.38;p.bend=8;p.y=-5;p.la=-7;p.ra=7;break;
 case Emotion::Sparkle:p.shape=2;p.lw=p.rw=1.5;p.lh=p.rh=.95;p.y=s*3;break;
 case Emotion::Focus:p.lw=p.rw=.8;p.lh=p.rh=.88;p.y=-2;break;
 case Emotion::Dizzy:p.shape=3;p.lw=p.rw=1.25;p.lh=p.rh=.8;p.x=s*5;p.y=std::cos(t)*4;break;
 default:break;
 }
 const float a=clamp(amount);p.x*=a;p.y*=a;p.la*=a;p.ra*=a;p.bend*=a;
 p.lw=1+(p.lw-1)*a;p.rw=1+(p.rw-1)*a;p.lh=1+(p.lh-1)*a;p.rh=1+(p.rh-1)*a;return p;
}
inline Pose blend(const Pose&a,const Pose&b,float t){
 Pose p;
 #define MIMO_MIX(k) p.k=a.k+(b.k-a.k)*t
 MIMO_MIX(x);MIMO_MIX(y);MIMO_MIX(lw);MIMO_MIX(rw);MIMO_MIX(lh);MIMO_MIX(rh);MIMO_MIX(la);MIMO_MIX(ra);MIMO_MIX(bend);
 #undef MIMO_MIX
 p.shape=t<.5f?a.shape:b.shape;return p;
}
class EyeEngine {
 float clock_=0,transition_=1000,gestureTime_=0,nextBlink_=2400,blinkStart_=-10000,nextLook_=1700,lookX_=0,lookY_=0,smoothX_=0,smoothY_=0;
 uint32_t seed_=173;Pose from_,last_;Emotion emotion_=Emotion::Neutral;Gesture gesture_=Gesture::None;
 float random(){seed_=seed_*1664525u+1013904223u;return seed_/4294967296.f;}
 public:
 Settings settings;
 void setEmotion(Emotion e){from_=last_;emotion_=e;transition_=0;}
 void playGesture(Gesture g){gesture_=g;gestureTime_=0;}
 Gesture gesture()const{return gesture_;}
 Pose update(float deltaMs){
  const float dt=clamp(deltaMs,0,100)*clamp(settings.speed,.3f,2);clock_+=dt;transition_+=dt;gestureTime_+=dt;
  const float t=clock_/1000;float b=clamp(transition_/420);b=b*b*(3-2*b);
  Pose p=blend(from_,expression(emotion_,t,settings.intensity),b);last_=p;
  if(settings.autonomous){p.y+=std::sin(t*1.7f)*1.2f;if(clock_>=nextLook_){lookX_=(random()-.5f)*17;lookY_=(random()-.5f)*8;nextLook_=clock_+1300+random()*2400;}float follow=1-std::exp(-dt/105);smoothX_+=(lookX_-smoothX_)*follow;smoothY_+=(lookY_-smoothY_)*follow;p.x+=smoothX_;p.y+=smoothY_;if(clock_>=nextBlink_){blinkStart_=clock_;nextBlink_=clock_+2300+random()*3700;}}
  float bl=settings.autonomous?pulse(clock_-blinkStart_,95,95):0,br=bl;
  static const float durations[]={0,650,1000,1300,3200,1800,2400,1900,1800};
  if(gesture_!=Gesture::None){float v=gestureTime_,u=v/durations[static_cast<uint8_t>(gesture_)];
   switch(gesture_){
   case Gesture::Blink:bl=br=std::max(bl,pulse(v,210,120));break;
   case Gesture::DoubleBlink:bl=br=std::max(bl,std::max(pulse(v,210,110),pulse(v,530,110)));break;
   case Gesture::Wink:br=std::max(br,pulse(v,430,320));p.y-=std::sin(PI*clamp(u))*3;break;
   case Gesture::Scan:p.x+=std::sin(u*PI*2)*27;p.y-=std::sin(u*PI)*8;break;
   case Gesture::Bounce:p.y-=std::fabs(std::sin(u*PI*3))*18*(1-u);break;
   case Gesture::Peek:bl=br=std::max(bl,clamp(1-u*2.7f));p.y+=std::max(0.f,1-u*2)*30;break;
   case Gesture::Shake:p.x+=std::sin(u*PI*6)*17*std::sin(u*PI);break;
   case Gesture::Nod:p.y+=std::sin(u*PI*4)*12*std::sin(u*PI);break;
   default:break;
   }if(u>=1)gesture_=Gesture::None;
  }
  p.lh=std::max(.035f,p.lh*(1-bl*.97f));p.rh=std::max(.035f,p.rh*(1-br*.97f));return p;
 }
};
struct Point {float x,y;};
// Drawing adapter: Canvas needs width(), height(), fillTriangle(), drawLine().
// Feed an offscreen sprite to avoid visible clearing between frames.
template<class Canvas> void render(Canvas&canvas,const Pose&p,const Settings&s,uint16_t color){
 const float scale=std::min(canvas.width()/400.f,canvas.height()/240.f),cx=canvas.width()/2.f,cy=canvas.height()/2.f;
 for(int side=0;side<2;side++){
  const float w=s.width*(side?p.rw:p.lw),h=s.height*(side?p.rh:p.lh),angle=(side?p.ra:p.la)*PI/180.f;
  const float ox=(side?1:-1)*s.gap/2+p.x,oy=p.y,co=std::cos(angle),si=std::sin(angle);
  const auto transform=[&](float x,float y)->Point{return {cx+(ox+x*co-y*si)*scale,cy+(oy+x*si+y*co)*scale};};
  if(p.shape==3){for(int i=-1;i<=1;i++){Point a=transform(-w*.36f,-h*.37f+i),b=transform(w*.36f,h*.37f+i),c=transform(w*.36f,-h*.37f+i),d=transform(-w*.36f,h*.37f+i);canvas.drawLine(a.x,a.y,b.x,b.y,color);canvas.drawLine(c.x,c.y,d.x,d.y,color);}continue;}
  Point points[40];int n=32;
  for(int i=0;i<n;i++){const float a=-PI/2+i*2*PI/n;float x,y;
   if(p.shape==1){const float sn=std::sin(a);x=w*.036f*16*sn*sn*sn;y=-h*.032f*(13*std::cos(a)-5*std::cos(2*a)-2*std::cos(3*a)-std::cos(4*a));}
   else if(p.shape==2){n=8;float radius=i%2?.24f:.55f;x=std::cos(-PI/2+i*PI/4)*w*radius;y=std::sin(-PI/2+i*PI/4)*h*radius;}
   else if(std::fabs(p.bend)>1){n=32;const bool lower=i>=16;const float q=lower?(31-i)/15.f:i/15.f;x=-w/2+w*q;y=-p.bend*4*q*(1-q)+(lower?h:0);}
   else {const float r=std::min(w,h)/2;const float sn=std::sin(a),cs=std::cos(a);x=(w/2-r)*(cs>=0?1:-1)+r*cs;y=(h/2-r)*(sn>=0?1:-1)+r*sn;}
   points[i]=transform(x,y);
  }
  // Fan from an interior point; bent eyes use paired strips to preserve concavity.
  if(p.shape==0&&std::fabs(p.bend)>1){for(int i=0;i<15;i++){const Point&a=points[i],&b=points[i+1],&c=points[31-i],&d=points[30-i];canvas.fillTriangle(a.x,a.y,b.x,b.y,c.x,c.y,color);canvas.fillTriangle(b.x,b.y,c.x,c.y,d.x,d.y,color);}}
  else {const Point center=transform(0,0);for(int i=0;i<n;i++){const Point&a=points[i],&b=points[(i+1)%n];canvas.fillTriangle(center.x,center.y,a.x,a.y,b.x,b.y,color);}}
 }
}
} // namespace mimo
