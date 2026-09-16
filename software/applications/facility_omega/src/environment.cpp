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
            art({((x/256+3*y/256)%7==0)?"floor_material_cool":
                 ((x/256+3*y/256)%7==3)?"floor_material_worn":"floor_material",x,y,true});
    // Authored room-local decoration. Everything below is paint/recessed floor;
    // none of it participates in position_clear or changes the R3 footprints.
    auto r=[&](i32 x,i32 y,u32 w,u32 h,Color c) {rect(1536+x,1536+y,w,h,c);};
    const auto dark=Color::rgb(19,28,34), edge=Color::rgb(43,53,59);
    const auto amber=Color::rgb(111,93,50), cyan=Color::rgb(37,93,107);
    auto line=[&](i32 x,i32 y,i32 dx,i32 dy,Color c) {
        r(x,y,dx?static_cast<u32>(dx):2,dy?static_cast<u32>(dy):2,c);
    };
    auto panel=[&](i32 x,i32 y,i32 w,i32 h,bool technical=false) {
        r(x,y,w,h,dark);r(x+2,y+2,w-4,h-4,Color::rgb(35,44,49));
        r(x+3,y+3,w-6,1,edge);r(x+3,y+h-3,w-6,1,Color::rgb(27,35,41));
        // Short corner brackets and sunk fasteners, not a bright rectangular frame.
        for(i32 xx:{x+6,x+w-15}) for(i32 yy:{y+6,y+h-7}) {
            r(xx,yy,9,1,Color::rgb(63,69,68));r(xx+3,yy+2,2,2,dark);
        }
        r(x+w/2-8,y+h/2,16,2,dark);
        if(technical) {r(x+w-24,y+h-14,12,2,cyan);r(x+w-24,y+h-10,6,1,edge);}
    };
    auto drain=[&](i32 x,i32 y,i32 w) {
        r(x,y,w,14,dark);r(x,y,w,1,edge);r(x,y+13,w,1,edge);
        for(i32 k=4;k<w-3;k+=7) r(x+k,y+3,2,8,Color::rgb(44,52,57));
    };
    auto cable=[&](i32 x,i32 y,i32 dx,i32 dy,Color c) {
        const u32 w=dx?static_cast<u32>(dx):7,h=dy?static_cast<u32>(dy):7;
        r(x,y,w,h,dark);r(x+2,y+2,dx?w-2:2,dy?h-2:2,c);
        if(dx) for(i32 k=12;k<dx-6;k+=40) r(x+k,y,3,7,edge);
        else for(i32 k=12;k<dy-6;k+=40) r(x,y+k,7,3,edge);
    };
    auto arrow=[&](i32 x,i32 y,Color c) {
        r(x,y+6,24,4,c);
        for(i32 k=0;k<8;++k) r(x+20+k,y+k,2,16-k*2,c);
    };
    auto wear=[&](i32 x,i32 y,i32 seed) {
        // Small clusters of scuffs around service routes, never a world-wide scatter.
        for(i32 k=0;k<18;++k) {
            const i32 dx=(k*37+seed*11)%96,dy=(k*13+seed*7)%42;
            r(x+dx,y+dy,3+(k%5)*3,1+(k%2),k%3?Color::rgb(29,37,41):edge);
        }
    };
    // Offset joints end at maintenance panels or service channels; no full-room cross.
    line(40,266,226,0,dark);line(266,266,0,72,edge);
    line(330,84,0,126,dark);line(330,210,146,0,dark);
    line(572,252,178,0,dark);line(750,252,0,104,dark);
    line(36,650,216,0,dark);line(252,610,0,40,dark);
    line(470,666,230,0,dark);line(700,666,0,106,edge);
    line(800,882,194,0,dark);line(584,850,0,132,dark);
    // Maintenance: a connected pair of feed pipes, repair covers and yellow route marks.
    cable(40,70,72,0,Color::rgb(75,78,61));cable(105,70,0,94,Color::rgb(75,78,61));
    cable(40,82,84,0,Color::rgb(54,71,78));cable(117,82,0,82,Color::rgb(54,71,78));
    panel(158,52,118,42);drain(50,310,124);panel(192,288,88,56);
    for(i32 k=0;k<5;++k) r(94+k*24,214,14,3,amber);
    arrow(280,224,amber);wear(65,238,2);wear(214,350,7);
    // Storage: loading footprints are painted L corners, clear and freely traversable.
    for(i32 y:{690,870}) for(i32 x:{58,194}) {
        r(x,y,25,2,amber);r(x,y,2,48,amber);
        r(x+80,y+46,25,2,amber);r(x+103,y,2,48,amber);
    }
    drain(42,947,206);cable(47,612,0,151,Color::rgb(73,64,44));
    cable(47,756,88,0,Color::rgb(73,64,44));
    wear(186,846,5);wear(84,696,9);arrow(300,890,amber);
    // Power lab: connected conduits, circuit traces and segmented light strips.
    cable(851,30,0,99,cyan);cable(851,122,142,0,cyan);
    cable(898,199,0,205,cyan);cable(849,397,56,0,cyan);
    panel(756,252,168,76,true);panel(666,92,106,48,true);
    for(i32 k=0;k<4;++k) {
        r(770+k*36,344,22,2,cyan);r(943,240+k*29,2,15,cyan);
    }
    drain(762,468,162);wear(850,491,3);
    // Central hall: asymmetric service panels and test lanes add scale, not obstacles.
    panel(352,300,116,56,true);panel(525,470,160,76,true);
    panel(308,574,105,70);panel(747,620,162,58,true);
    panel(445,830,94,48);panel(772,934,146,42);
    drain(418,742,164);drain(575,146,108);
    cable(678,380,0,187,Color::rgb(33,67,76));cable(678,560,110,0,Color::rgb(33,67,76));
    line(322,392,0,116,amber);line(332,396,0,52,Color::rgb(72,69,46));
    line(364,716,0,112,amber);line(364,826,53,0,amber);
    arrow(400,224,cyan);arrow(734,718,cyan);arrow(605,914,amber);
    wear(338,476,4);wear(556,612,11);wear(755,768,1);wear(381,915,6);
    // Recessed service numbers and ticks: flat, low-contrast stencils.
    for(const auto p: {Bounds{486,286,0,0},Bounds{923,574,0,0},Bounds{174,607,0,0}}) {
        r(p.x,p.y,12,2,edge);r(p.x,p.y+2,2,20,edge);r(p.x+10,p.y+2,2,20,edge);
        r(p.x,p.y+22,12,2,edge);r(p.x+19,p.y,3,24,edge);
        for(i32 k=0;k<3;++k) r(p.x+30+k*5,p.y+18,2,6,edge);
    }
    for(const auto& a:e.decals) art(a);
    // Local pools use the existing alpha sprite; no new blend or lighting semantics.
    if(const auto* glow=tex.find("glow_large")) {
        for(const auto p:{Bounds{2350,1738,110,46},Bounds{2382,2010,100,42},Bounds{1660,1740,72,32}}) {
            gpu2d::SpriteParams light;
            light.tex=glow->id;light.src_x=glow->sx;light.src_y=glow->sy;
            light.w=glow->w;light.h=glow->h;light.scale_w=p.w;light.scale_h=p.h;
            light.dst_x=p.x-cx-p.w/2;light.dst_y=p.y-cy-p.h/2;
            light.blend=gpu2d::BlendMode::StraightAlpha;light.global_alpha=42;
            api.draw_sprite(light);
        }
    }
    for(const auto& a:e.effects) art(a);
    for(const auto& st:e.structures) {
        const auto& b=st.bounds;
        const bool wall=b.w<=32 || b.h<=32;
        rect(b.x-2,b.y,b.w+4,b.h+5,Color::rgb(12,20,26));
        rect(b.x,b.y,b.w,b.h,Color::rgb(34,45,53));
        rect(b.x+2,b.y+2,b.w-4,2,Color::rgb(wall?64:43,wall?77:54,wall?84:60));
        rect(b.x,b.y+b.h-4,b.w,4,Color::rgb(18,27,33));
        if(wall) {
            // Edge rail, recessed pipe strip and support straps stay inside the AABB.
            const bool vertical=b.w<=32;
            if(vertical) {
                rect(b.x+5,b.y+5,3,b.h-10,Color::rgb(46,61,70));
                rect(b.x+11,b.y+5,2,b.h-10,Color::rgb(18,28,35));
                for(i32 y=b.y+18;y<b.y+b.h-16;y+=86) {
                    rect(b.x+2,y,b.w-4,7,Color::rgb(55,64,67));
                    rect(b.x+5,y+2,2,2,Color::rgb(103,107,96));
                }
                for(i32 y:{b.y+4,b.y+b.h-13}) {
                    rect(b.x+2,y,b.w-4,8,dark);
                    rect(b.x+b.w-7,y+1,3,5,Color::rgb(86,156,165));
                }
            } else {
                rect(b.x+4,b.y+8,b.w-8,3,Color::rgb(48,62,70));
                for(i32 x=b.x+18;x<b.x+b.w-16;x+=94) {
                    rect(x,b.y+2,7,b.h-4,Color::rgb(55,64,67));
                    rect(x+2,b.y+5,2,2,Color::rgb(103,107,96));
                }
                for(i32 x:{b.x+4,b.x+b.w-14}) {
                    rect(x,b.y+2,10,b.h-4,dark);
                    rect(x+2,b.y+b.h-8,6,3,Color::rgb(143,115,58));
                }
            }
        } else {
            // Low base plates with broken corner marks, grounded by incoming cables.
            for(i32 x:{b.x+5,b.x+b.w-18}) {
                rect(x,b.y+b.h-8,12,2,st.power?cyan:amber);
                rect(x,b.y+6,3,3,Color::rgb(67,74,72));
            }
            rect(b.x+8,b.y+b.h-15,b.w-16,1,Color::rgb(25,34,40));
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
