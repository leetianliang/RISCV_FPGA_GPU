#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace facility {
// CPU broad-phase. GameplayGridCell is NOT a GPU RenderTile or a map art cell.
class GameplaySpatialGrid {
public:
    static constexpr int cell_size=128, side=32, capacity=2048;
    struct Result { std::array<int,capacity> ids{}; int count=0; };
    GameplaySpatialGrid() {clear();}
    void clear() {heads.fill(-1);count=0;}
    bool insert(int id,int x,int y) {
        if(count==capacity || x<0 || y<0 || x>=4096 || y>=4096) return false;
        const int cell=(y/cell_size)*side+x/cell_size;
        nodes[count]={id,x,y,heads[cell]};heads[cell]=count++;return true;
    }
    void query(int x0,int y0,int x1,int y1,Result& out) const {
        out.count=0;
        if(x1<0 || y1<0 || x0>=4096 || y0>=4096 || x1<x0 || y1<y0)return;
        for(int cy=std::max(0,y0)/cell_size;cy<=std::min(4095,y1)/cell_size;++cy)
            for(int cx=std::max(0,x0)/cell_size;cx<=std::min(4095,x1)/cell_size;++cx)
                for(int k=heads[cy*side+cx];k>=0;k=nodes[k].next) {
                    const auto& n=nodes[k];
                    if(n.x>=x0 && n.x<=x1 && n.y>=y0 && n.y<=y1)out.ids[out.count++]=n.id;
                }
        std::sort(out.ids.begin(),out.ids.begin()+out.count);
    }
    int nearest(int x,int y,int radius=8192) const {
        std::int64_t best=static_cast<std::int64_t>(radius)*radius;
        int id=-1;
        for(int cy=0;cy<side;++cy)for(int cx=0;cx<side;++cx) {
            const int cell=cy*side+cx;if(heads[cell]<0)continue;
            const auto dx=x-std::clamp(x,cx*cell_size,(cx+1)*cell_size-1);
            const auto dy=y-std::clamp(y,cy*cell_size,(cy+1)*cell_size-1);
            if(static_cast<std::int64_t>(dx)*dx+static_cast<std::int64_t>(dy)*dy>best)continue;
            for(int k=heads[cell];k>=0;k=nodes[k].next) {
                const auto& n=nodes[k];const std::int64_t ex=n.x-x,ey=n.y-y,d=ex*ex+ey*ey;
                if(d<best || (d==best && (id<0 || n.id<id))) {best=d;id=n.id;}
            }
        }
        return id;
    }
private:
    struct Node {int id,x,y,next;};
    std::array<int,side*side> heads{};
    std::array<Node,capacity> nodes{};
    int count=0;
};
}
