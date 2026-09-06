#include "PitchEngine.h"
#include <chrono>
#include <iostream>
#include <random>
#include <stdexcept>
#include <fstream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
double estimate(const std::vector<float>& y,double sr){
    // Independent normalized autocorrelation, interpolated first strong peak.
    int start=(int)(sr*0.6),n=std::min(8192,(int)y.size()-start-1500);
    std::vector<double> corr(1500);
    for(int lag=1;lag<1400;++lag){double xy=0,xx=0,yy=0;for(int i=0;i<n;++i){double a=y[(size_t)(start+i)],b=y[(size_t)(start+i+lag)];xy+=a*b;xx+=a*a;yy+=b*b;}corr[(size_t)lag]=xy/std::sqrt(std::max(1e-30,xx*yy));}
    for(int lag=20;lag<1398;++lag)if(corr[(size_t)lag]>0.9 && corr[(size_t)lag]>corr[(size_t)(lag-1)] && corr[(size_t)lag]>corr[(size_t)(lag+1)]){
        double a=corr[(size_t)(lag-1)],b=corr[(size_t)lag],c=corr[(size_t)(lag+1)];
        return sr/(lag+0.5*(a-c)/(a-2*b+c));
    }
    return 0;
}
int main(){try{
    for(int k=0;k<12;++k)for(int s=0;s<5;++s)for(float n=40;n<85;n+=0.137f)check(prem::allowedNote((int)prem::nearestNote(n,k,s),k,s),"Scale returned disallowed note");
    check(prem::allowedNote(64,0,1)&&!prem::allowedNote(63,0,1),"C major mask");
    check(prem::allowedNote(63,0,2)&&!prem::allowedNote(64,0,2),"C minor mask");
    check(!prem::allowedNote(68,7,1),"G major must not contain G sharp");
    check(prem::allowedNote(68,7,1,prem::chordMask(4,0)),"E major borrowed chord must add G sharp");
    check((prem::chordMask(4,0)&~prem::scaleMask(7,1))==(1<<8),"G major plus E major should add only G sharp");
    check(prem::nearestNote(68.05f,7,1,-1,prem::chordMask(4,0))==68,"Borrowed chord target selection");
    for(double sr:{44100.0,48000.0,96000.0}){
        prem::PitchEngine e;e.prepare(sr);prem::Settings s;s.retuneMs=0;s.humanize=0;
        std::vector<float> impulse(4096);impulse[0]=1;float* ip[]={impulse.data()};e.process(ip,1,(int)impulse.size(),s);
        int peak=(int)(std::max_element(impulse.begin(),impulse.end())-impulse.begin());
        check(peak==e.latencySamples(),"Impulse latency mismatch");check(impulse[(size_t)peak]==1,"Impulse changed");
        std::cout<<"latency "<<sr<<" Hz: "<<peak<<" samples / "<<peak*1000/sr<<" ms\n";
        for(double f:{95.0,147.0,225.0,435.0,880.0}){
            e.prepare(sr);int length=(int)(sr*1.2);std::vector<float> y((size_t)length);
            for(int i=0;i<length;++i)y[(size_t)i]=(float)(0.3*std::sin(2*prem::pi*f*i/sr)+0.09*std::sin(4*prem::pi*f*i/sr));
            for(int i=0;i<length;i+=64){float* p[]={y.data()+i};e.process(p,1,std::min(64,length-i),s);}
            double wanted=440*std::pow(2.0,(std::round(69+12*std::log2(f/440))-69)/12);
            double out=estimate(y,sr),error=1200*std::log2(out/wanted);
            std::cout<<"pitch "<<f<<" -> "<<out<<" expected "<<wanted<<" error "<<error<<" cents; detected "<<e.frequency<<"\n";
            check(std::abs(1200*std::log2(e.frequency/f))<8,"Pitch detector error >8 cents");
            check(out>0&&std::abs(error)<12,"Correction error >12 cents");
            for(float v:y)check(std::isfinite(v)&&std::abs(v)<1,"Invalid audio");
        }
    }
    prem::PitchEngine e;e.prepare(48000);prem::Settings s;
    std::array<float,64> l{},r{};float* p[]={l.data(),r.data()};
    std::mt19937 rng(42);std::uniform_real_distribution<float>d(-0.5f,0.5f);
    s.mix=0;for(int block=0;block<100;++block){for(int i=0;i<64;++i)l[i]=r[i]=d(rng);e.process(p,2,64,s);for(int i=0;i<64;++i)check(l[i]==r[i],"Stereo alignment");}
    e.reset();for(int block=0;block<100;++block){l.fill(0);r.fill(0);e.process(p,2,64,s);for(float v:l)check(v==0,"Silence must remain silent");}
    int fiveChordMask=0;
    for(int i=0;i<5;++i) fiveChordMask|=prem::chordMask(i*2,i%3);
    check((fiveChordMask&prem::chordMask(8,1))!=0,"Five borrowed chord masks must combine");
    s.mix=100;s.scale=2;s.key=9;
    auto begin=std::chrono::steady_clock::now();
    for(int block=0;block<7500;++block){for(int i=0;i<64;++i)l[i]=r[i]=(float)(0.3*std::sin(2*prem::pi*223*(block*64+i)/48000));e.process(p,2,64,s);}
    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"CPU: "<<elapsed<<" s for 10 s stereo audio ("<<elapsed*10<<"% of one core)\n";
    check(elapsed<5,"Realtime budget exceeded");
    std::cout<<"PASS\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
