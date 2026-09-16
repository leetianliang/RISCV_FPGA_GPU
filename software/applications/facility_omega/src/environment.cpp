#include "facility/environment.hpp"
#include "facility/app.hpp"
#include <algorithm>
#include <cstdlib>

namespace facility {
Environment make_hero_environment() {
    Environment e;
    e.zones = {{"CENTRAL POWER TEST HALL",{1776,1800,680,552}},
               {"MAINTENANCE",{1568,1584,300,280}},
               {"STORAGE",{1568,2216,288,280}},
               {"POWER LAB",{2224,1576,296,272}}};
    auto solid = [&](Bounds b, bool power=false) {
        e.structures.push_back({b,power}); e.collision.push_back(b);
    };
    // Deliberately asymmetric boundaries with wide entrances, never a repeated bay.
    solid({1544,1544,348,24}); solid({2132,1544,428,24});
    solid({1544,1568,24,312}); solid({1544,2128,24,432});
    solid({1568,2536,288,24}); solid({2144,2536,416,24});
    solid({2528,1568,32,304}); solid({2528,2176,32,360});
    // Four equipment groups, explicit simple footprints; room between each group.
    solid({1624,1668,176,68});
    solid({1624,2300,128,80});
    solid({2320,1656,128,80},true);
    solid({2288,1940,128,68},true);
    e.props = {{"pipe_elbow",1654,1698},{"power_cabinet",1720,1708},{"console",1770,1710},
               {"stacked_crates",1664,2344},{"crate",1722,2344},{"barrel",1730,2370},
               {"canister",2350,1708},{"canister",2418,1708},
               {"console",2316,1982},{"canister",2382,1982}};
    // Every prop above sits on a collidable equipment plinth. The following are flat.
    e.decals = {{"mark_service",1656,1776,true},{"mark_a3",1984,1920,true}};
    e.effects = {{"glow_small",2350,1738,false,36},{"glow_small",2382,2010,false,36}};
    return e;
}

bool position_clear(const Environment& e, i32 x, i32 y, i32 r) {
    if (x<r || y<r || x>4096-r || y>4096-r) return false;
    for (const auto& b:e.collision)
        if (x+r>b.x && x-r<b.x+b.w && y+r>b.y && y-r<b.y+b.h) return false;
    return true;
}

void move_in_environment(const Environment& e,i32& x,i32& y,i32 dx,i32 dy,i32 r) {
    // Unit substeps prevent tunnelling through thin walls even for large input steps.
    const i32 steps=std::max(std::abs(dx),std::abs(dy));
    if (!steps) return;
    i32 prev_x=0,prev_y=0;
    for (i32 n=1;n<=steps;++n) {
        const i32 next_x=static_cast<i32>(static_cast<long long>(dx)*n/steps);
        const i32 next_y=static_cast<i32>(static_cast<long long>(dy)*n/steps);
        if (position_clear(e,x+next_x-prev_x,y,r)) x+=next_x-prev_x;
        if (position_clear(e,x,y+next_y-prev_y,r)) y+=next_y-prev_y;
        prev_x=next_x; prev_y=next_y;
    }
}

void move_enemy_in_environment(const Environment& e,i32& x,i32& y,i32 dx,i32 dy,
                               i32 r,i32 tx,i32 ty) {
    const i32 ox=x,oy=y;
    move_in_environment(e,x,y,dx,dy,r);
    const bool blocked_x=dx && x==ox, blocked_y=dy && y==oy;
    if (!blocked_x && !blocked_y) return;
    // Tangent around the blocking rectangle. Pick the nearer edge including target leg.
    for (const auto& b:e.collision) {
        if (blocked_x && y+r>b.y && y-r<b.y+b.h &&
            ((dx>0 && x+r==b.x)||(dx<0 && x-r==b.x+b.w))) {
            const i32 top=b.y-r-1,bottom=b.y+b.h+r+1;
            const i32 dir=(std::abs(y-top)+std::abs(ty-top)<=std::abs(y-bottom)+std::abs(ty-bottom)) ? -1:1;
            y=oy;
            move_in_environment(e,x,y,0,dir,r); break;
        }
        if (blocked_y && x+r>b.x && x-r<b.x+b.w &&
            ((dy>0 && y+r==b.y)||(dy<0 && y-r==b.y+b.h))) {
            const i32 left=b.x-r-1,right=b.x+b.w+r+1;
            const i32 dir=(std::abs(x-left)+std::abs(tx-left)<=std::abs(x-right)+std::abs(tx-right)) ? -1:1;
            x=ox;
            move_in_environment(e,x,y,dir,0,r); break;
        }
    }
}

void render_environment(gpu2d::GraphicsApi& api,const Environment& e,const TexBank& tex,
                        i32 cx,i32 cy,u32 width,u32 height,bool debug) {
    using gpu2d::Color;
    auto rect=[&](i32 x,i32 y,u32 w,u32 h,Color c) {
        // Explicit viewport clip also handles rectangles crossing world-view edges.
        const i32 x0=std::max(0,x-cx),y0=std::max(0,y-cy);
        const i32 x1=std::min(static_cast<i32>(width),x-cx+static_cast<i32>(w));
        const i32 y1=std::min(static_cast<i32>(height),y-cy+static_cast<i32>(h));
        if(x1>x0 && y1>y0) api.fill_rect(x0,y0,x1-x0,y1-y0,c);
    };
    auto art=[&](const EnvironmentArt& a) {
        const auto* t=tex.find(a.sprite); if(!t) return;
        i32 x=a.x-cx-static_cast<i32>(t->ax),y=a.y-cy-static_cast<i32>(t->ay);
        if(x+static_cast<i32>(t->w)<=0 || y+static_cast<i32>(t->h)<=0 || x>=static_cast<i32>(width) || y>=static_cast<i32>(height)) return;
        gpu2d::SpriteParams sp;
        sp.tex=t->id; sp.src_x=t->sx; sp.src_y=t->sy; sp.w=t->w; sp.h=t->h;
        sp.dst_x=x;sp.dst_y=y;
        sp.global_alpha=a.alpha;
        sp.blend=a.opaque?gpu2d::BlendMode::Copy:gpu2d::BlendMode::StraightAlpha;
        api.draw_sprite(sp);
    };
    api.fill_rect(0,0,width,height,Color::rgb(28,39,48));
    // 256px material sheets have no visible frame/rivets; 32px map cells are not drawn.
    for(i32 y=cy/256*256;y<cy+static_cast<i32>(height);y+=256)
        for(i32 x=cx/256*256;x<cx+static_cast<i32>(width);x+=256)
            art({"floor_material",x,y,true});
    // Wide service recesses and nonperiodic expansion joints follow the authored hall.
    rect(1576,1584,248,204,Color::rgb(23,34,42));
    rect(1576,2240,216,200,Color::rgb(33,37,40));
    rect(2288,1592,208,176,Color::rgb(21,38,46));
    for(i32 y:{1800,2208,2464}) { rect(1576,y,936,1,Color::rgb(18,29,36)); rect(1576,y+1,936,1,Color::rgb(40,50,57)); }
    rect(1872,1584,1,928,Color::rgb(22,32,39));
    rect(2224,1784,1,728,Color::rgb(22,32,39));
    // Long lanes remain floor markings: no collision is inferred from paint.
    rect(1878,1816,5,580,Color::rgb(103,90,48));
    rect(1889,1816,1,580,Color::rgb(75,72,49));
    rect(1878,2396,340,5,Color::rgb(103,90,48));
    rect(2190,1856,230,3,Color::rgb(38,95,112));
    for(const auto& a:e.decals) art(a);
    for(const auto& a:e.effects) art(a);
    for(const auto& st:e.structures) {
        const auto& b=st.bounds;
        rect(b.x-3,b.y-3,b.w+6,b.h+8,Color::rgb(10,17,23));
        rect(b.x,b.y,b.w,b.h,Color::rgb(38,51,61));
        rect(b.x,b.y,b.w,3,Color::rgb(70,85,96));
        rect(b.x,b.y+b.h-6,b.w,6,Color::rgb(16,25,33));
        if(b.h>40) {
            for(i32 x=b.x+8;x<b.x+b.w-8;x+=24)
                rect(x,b.y+b.h-5,12,3,st.power?Color::rgb(38,118,140):Color::rgb(141,109,48));
        }
    }
    for(const auto& a:e.props) art(a);
    if(debug) for(const auto& b:e.collision) {
        const auto c=Color::rgb(250,110,70);
        rect(b.x,b.y,b.w,1,c);rect(b.x,b.y+b.h-1,b.w,1,c);
        rect(b.x,b.y,1,b.h,c);rect(b.x+b.w-1,b.y,1,b.h,c);
    }
}
}
