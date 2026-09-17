#include "facility/spatial.hpp"
#include <cstdio>
#include <vector>
#include <random>
int main() {
    facility::GameplaySpatialGrid grid;
    facility::GameplaySpatialGrid::Result result;
    std::mt19937 rng(1234);int fail=0;
    struct Point {int x,y;};
    for(int fixture=0;fixture<128;++fixture) {
        grid.clear();std::vector<Point> points;
        int count=fixture==0?0:fixture==127?2048:128;
        for(int i=0;i<count;++i) {
            Point p{int(rng()%4096),int(rng()%4096)};
            if(fixture%4==0)p={2048+int(rng()%8),2048+int(rng()%8)};
            if(i<4)p={i*128,i*128};
            points.push_back(p);if(!grid.insert(i,p.x,p.y))++fail;
        }
        if(count==2048 && grid.insert(2048,0,0))++fail;
        for(int query=0;query<32;++query) {
            int x=query<4?query*128:int(rng()%4096),y=query<4?query*128:int(rng()%4096);
            int radius=query%2?18:128;
            grid.query(x-radius,y-radius,x+radius,y+radius,result);
            std::vector<int> expected;
            std::int64_t best=INT64_MAX;int nearest=-1;
            for(int i=0;i<count;++i) {
                const auto p=points[i];std::int64_t dx=p.x-x,dy=p.y-y,d=dx*dx+dy*dy;
                if(d<best){best=d;nearest=i;}
                if(p.x>=x-radius && p.x<=x+radius && p.y>=y-radius && p.y<=y+radius)expected.push_back(i);
            }
            if(grid.nearest(x,y)!=nearest || result.count!=int(expected.size()))++fail;
            for(int i=0;i<result.count && i<int(expected.size());++i)if(result.ids[i]!=expected[i])++fail;
        }
    }
    grid.clear();grid.insert(9,127,128);grid.insert(3,129,128);
    if(grid.nearest(128,128)!=3)++fail;
    grid.query(126,126,130,130,result);if(result.count!=2)++fail;
    std::printf("spatial oracle 128 fixtures / 4096 queries / boundaries and ties: %s\n",fail?"FAIL":"PASS");
    return fail?1:0;
}
