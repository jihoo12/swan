#include "garden.hpp"
#include <cmath>
namespace swan {
Garden makeGarden() {
    Garden garden;
    unsigned count=0;
    auto box = [&](glm::vec3 p,glm::vec3 s,glm::vec3 c,float yaw=0,float emission=0,bool animated=false) {
        Entity entity;
        entity.name="Garden box "+std::to_string(count++);
        entity.transform={p,s,yaw}; entity.material={c,emission}; entity.solid=!animated;
        if(animated) entity.animation=Animation{p.y,0,0.35f,0.5f};
        auto id=garden.scene.create(std::move(entity));
        if(animated) garden.core=id;
    };
    box({0,-0.4f,0},{80,0.8f,80},{0.09f,0.16f,0.18f});
    for (int x=-8;x<=8;++x) for(int z=-8;z<=8;++z) {
        float h=0.12f+0.06f*std::sin(float(x*3+z*7));
        glm::vec3 color=((x+z)%2==0)?glm::vec3(0.22f,0.31f,0.32f):glm::vec3(0.18f,0.26f,0.28f);
        box({x*2.1f,h/2,z*2.1f},{2,h,2},color);
    }
    box({0,0.25f,0},{5,0.5f,5},{0.36f,0.41f,0.42f});
    box({0,0.65f,0},{3.8f,0.3f,3.8f},{0.18f,0.27f,0.3f});
    box({0,3.2f,0},{1.4f,1.4f,1.4f},{0.1f,0.85f,1},0,2.5f,true);
    for(int i=0;i<12;++i) {
        float a=i*6.2831853f/12;
        glm::vec3 p{std::cos(a)*9,0,std::sin(a)*9};
        box(p+glm::vec3(0,0.25f,0),{1.8f,0.5f,1.8f},{0.32f,0.39f,0.4f},a);
        float h=(i%3==0)?4.2f:3.2f;
        box(p+glm::vec3(0,h/2+0.5f,0),{0.85f,h,0.85f},{0.38f,0.46f,0.45f},a);
        box(p+glm::vec3(0,h+0.6f,0),{1.5f,0.35f,1.5f},{0.46f,0.53f,0.49f},a);
        box(p+glm::vec3(0,h+0.9f,0),{0.25f,0.2f,0.25f},{0.25f,1,0.75f},a,2);
    }
    for(int i=0;i<48;++i) {
        float a=i*2.399963f, r=13+float(i%7)*1.1f;
        glm::vec3 p{std::cos(a)*r,0,std::sin(a)*r};
        float h=1.5f+float(i%5)*0.45f;
        box(p+glm::vec3(0,h/2,0),{0.3f,h,0.3f},{0.18f,0.24f,0.23f});
        box(p+glm::vec3(0,h,0),{1.7f,1.8f,1.7f},{0.13f,0.36f+float(i%3)*0.04f,0.28f},a);
    }
    for(int i=0;i<5;++i) {
        float angle=1.5707963f+float(i)*6.2831853f/5;
        Entity shard;
        shard.name="Light shard "+std::to_string(i+1);
        shard.transform={{std::cos(angle)*6,1.2f,std::sin(angle)*6},{0.35f,0.6f,0.35f},angle};
        shard.material={{1,0.65f,0.12f},1.8f}; shard.collectible=true;
        shard.animation=Animation{1.2f,float(i),0.15f,1};
        garden.shards.push_back(garden.scene.create(std::move(shard)));
    }
    return garden;
}
}
