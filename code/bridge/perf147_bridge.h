#pragma once
// Included after Windows/D3D/WRL/STL. No ABI change. All calls occur under the
// existing bridge lock AND engine context ownership; no new Flush or wait.
#include "perf147_policy.h"
namespace native_xr {
struct Perf147 {
    struct Region { const char* name; int eye,layer; };
    struct Slot {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint,ts[gx_perf147::MaxRegions*2];
        Region region[gx_perf147::MaxRegions]; unsigned n; bool overflow;
        uint64_t frame,generation;
        Slot():n(0),overflow(false),frame(0),generation(0){}
    };
    Slot slots[gx_perf147::RingSize]; gx_perf147::Ring ring;
    unsigned active; bool created,failed,enabled;
    long long frequency; gx_perf147::Steps steps;
    std::function<void(const std::string&)> output;
    Perf147():active(gx_perf147::RingSize),created(false),failed(false),enabled(false),frequency(0){}
    static long long now() { LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart; }
    double ms(long long ticks)const { return frequency>0?double(ticks)*1000.0/double(frequency):0; }
    // Logging failures must not alter XR success/fault behaviour.
    void emit(const std::string&s) { try{if(enabled&&output)output(s);}catch(...){} }
    void init(bool on,const std::function<void(const std::string&)>&log) {
        enabled=on;output=log;LARGE_INTEGER f;QueryPerformanceFrequency(&f);frequency=f.QuadPart;
        emit("event=PERF147_BRIDGE_CONTRACT version=1 min_readback_age=2 ring=8 max_regions=64 gpu_units=command_span_ms");
    }
    template<class F> auto cpu(const char*name,F f)->decltype(f()) {
        if(!enabled)return f();
        struct Guard {Perf147*p;Guard(Perf147*v,const char*n):p(v){p->steps.push(n,now());}~Guard(){p->steps.pop(now());}} guard(this,name);
        return f();
    }
    bool create(ID3D11Device*dev) {
        if(created||failed)return created;
        D3D11_QUERY_DESC d={};bool ok=true;
        for(unsigned i=0;i<gx_perf147::RingSize&&ok;++i){Slot&s=slots[i];d.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
            ok=SUCCEEDED(dev->CreateQuery(&d,s.disjoint.GetAddressOf()));d.Query=D3D11_QUERY_TIMESTAMP;
            for(unsigned k=0;k<gx_perf147::MaxRegions*2&&ok;++k)ok=SUCCEEDED(dev->CreateQuery(&d,s.ts[k].GetAddressOf()));}
        created=ok;failed=!ok;
        if(!ok){for(auto&s:slots){s.disjoint.Reset();for(auto&t:s.ts)t.Reset();}emit("event=PERF147_BRIDGE_GPU_STATUS status=query_create_failed");}
        return ok;
    }
    void status(const Slot&s,const char*why) {
        try{emit("event=PERF147_BRIDGE_GPU_STATUS frame="+std::to_string(s.frame)+" generation="+std::to_string(s.generation)+" status="+why);}catch(...){}
    }
    void collect(ID3D11DeviceContext*ctx,uint64_t current) {
        for(unsigned i=0;i<gx_perf147::RingSize;++i){if(!ring.eligible(i,current))continue;Slot&s=slots[i];
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d={};HRESULT h=ctx->GetData(s.disjoint.Get(),&d,sizeof(d),D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if(h==S_FALSE)continue;
            if(FAILED(h)||d.Disjoint||!d.Frequency||s.overflow){status(s,s.overflow?"overflow":d.Disjoint?"disjoint":"query_error");ring.retire(i);continue;}
            UINT64 ticks[gx_perf147::MaxRegions*2];bool ready=true,bad=false;
            for(unsigned k=0;k<s.n*2;++k){h=ctx->GetData(s.ts[k].Get(),&ticks[k],sizeof(ticks[k]),D3D11_ASYNC_GETDATA_DONOTFLUSH);if(h==S_FALSE){ready=false;break;}if(FAILED(h)){bad=true;break;}}
            if(!ready)continue; // timestamps may lag disjoint readiness; retain the ENTIRE slot
            for(unsigned k=0;k<s.n&&!bad;++k){double v=0;bad=!gx_perf147::gpuMs(ticks[2*k],ticks[2*k+1],d.Frequency,v);}
            if(bad){status(s,"timestamp_error");ring.retire(i);continue;}
            try{double total=0;std::ostringstream line;
            line<<"event=PERF147_BRIDGE_GPU frame="<<s.frame<<" generation="<<s.generation<<" age="<<(current-s.frame)<<" regions="<<s.n;
            for(unsigned k=0;k<s.n;++k){double v=0;gx_perf147::gpuMs(ticks[2*k],ticks[2*k+1],d.Frequency,v);total+=v;const Region&r=s.region[k];
                line<<" r"<<k<<'='<<r.name<<'/'<<r.eye<<'/'<<r.layer<<'/'<<std::fixed<<std::setprecision(6)<<v;}
            line<<" total_ms="<<std::fixed<<std::setprecision(6)<<total;emit(line.str());}catch(...){}
            ring.retire(i);
        }
    }
    void open(ID3D11Device*dev,ID3D11DeviceContext*ctx,uint64_t frame,uint64_t generation) {
        if(!enabled)return;
        collect(ctx,frame);
        if(!create(dev))return;
        active=ring.acquire(frame);
        if(active==gx_perf147::RingSize){Slot missing;missing.frame=frame;missing.generation=generation;status(missing,"ring_full");return;}
        Slot&s=slots[active];s.frame=frame;s.generation=generation;s.n=0;s.overflow=false;ctx->Begin(s.disjoint.Get());
    }
    void seal(ID3D11DeviceContext*ctx) {
        if(active==gx_perf147::RingSize)return;
        ctx->End(slots[active].disjoint.Get());ring.seal(active);active=gx_perf147::RingSize;
    }
    template<class F> auto gpu(ID3D11DeviceContext*ctx,const char*name,int eye,int layer,F f)->decltype(f()) {
        if(active==gx_perf147::RingSize)return f();Slot&s=slots[active];
        if(s.n==gx_perf147::MaxRegions){s.overflow=true;return f();}
        const unsigned k=s.n++;s.region[k].name=name;s.region[k].eye=eye;s.region[k].layer=layer;
        struct Guard{ID3D11DeviceContext*c;ID3D11Query*q;Guard(ID3D11DeviceContext*ct,ID3D11Query*qt):c(ct),q(qt){}~Guard(){c->End(q);}} guard(ctx,s.ts[2*k+1].Get());
        ctx->End(s.ts[2*k].Get());return f();
    }
    void cpuRecord(uint64_t frame,uint64_t generation,const char*phase,long long display,long long period,bool shouldRender,long long begin,long long end) {
        if(!enabled)return;try{std::ostringstream s;
        s<<"event=PERF147_BRIDGE_CPU frame="<<frame<<" generation="<<generation<<" phase="<<phase<<" display_ns="<<display<<" period_ns="<<period
         <<" should_render="<<(shouldRender?1:0)<<" qpc_begin="<<begin<<" qpc_end="<<end<<" qpc_frequency="<<frequency<<" overflow="<<steps.overflow;
        long long measured=0;for(unsigned i=0;i<steps.n;++i){measured+=steps.ticks[i];s<<" cpu_"<<steps.name[i]<<'='<<std::fixed<<std::setprecision(6)<<ms(steps.ticks[i]);}
        s<<" cpu_other="<<std::fixed<<std::setprecision(6)<<ms(end>begin+measured?end-begin-measured:0);emit(s.str());}catch(...){}
    }
    void close(ID3D11DeviceContext*ctx) {
        if(!enabled)return;seal(ctx);
        for(unsigned i=0;i<gx_perf147::RingSize;++i){if(ring.state[i]!=gx_perf147::Free)status(slots[i],"abandoned_shutdown");ring.retire(i);}
        for(auto&s:slots){s.disjoint.Reset();for(auto&t:s.ts)t.Reset();}created=false;
    }
};
}
