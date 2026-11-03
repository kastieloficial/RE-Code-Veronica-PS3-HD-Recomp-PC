#include "cv_appearance_preview.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cmath>
namespace cv_appearance {void render_preview_reference(const PreviewModel&,float,std::vector<rsx_ui_quad>&,float,float,float,float);}
int main(int argc,char** argv){
 using namespace cv_appearance;assert(argc==3);PlayerAsset asset;PreviewModel model;std::string error;
 if(!load_player_asset(argv[1],asset,error)||!make_preview(asset,model,error)){fprintf(stderr,"%s\n",error.c_str());return 1;}
 std::vector<rsx_ui_quad> high,low;std::vector<double> ht,lt;
 for(unsigned i=0;i<10;i++){
  auto t=std::chrono::steady_clock::now();high.clear();render_preview_reference(model,float(i)*.2f,high,0,0,256,296);
  auto m=std::chrono::steady_clock::now();low.clear();render_preview(model,float(i)*.2f,low,0,0,256,296);
  auto n=std::chrono::steady_clock::now();if(i){ht.push_back(std::chrono::duration<double,std::milli>(m-t).count());lt.push_back(std::chrono::duration<double,std::milli>(n-m).count());}
  assert(!high.empty()&&!low.empty()&&low.size()<=256u*296u);
  for(auto& q:low)assert(q.x>=0&&q.y>=0&&q.x+q.w<=256&&q.y+q.h<=296&&q.w>0&&q.h>0);
 }
 std::sort(ht.begin(),ht.end());std::sort(lt.begin(),lt.end());
 printf("Preview raster: 512x592 -> 256x296 (75%% fewer pixels); median reference=%.3f ms reduced=%.3f ms; last quads=%zu -> %zu; triangles=%zu\n",ht[ht.size()/2],lt[lt.size()/2],high.size(),low.size(),model.triangles.size());
 // Returning to the same model/angle must produce the same image even though
 // the scratch buffers are reused; no stale depth/colors across selections.
 auto expected=low;std::vector<rsx_ui_quad> again;render_preview(model,0,again,0,0,256,296);again.clear();render_preview(model,float(9)*.2f,again,0,0,256,296);
 assert(again.size()==expected.size());
 for(size_t i=0;i<again.size();i++)assert(again[i].x==expected[i].x&&again[i].y==expected[i].y&&again[i].argb==expected[i].argb);
 std::vector<uint32_t> pixels(256*296,0xFF161413);
 for(auto& q:low)for(int y=int(q.y);y<int(q.y+q.h);y++)for(int x=int(q.x);x<int(q.x+q.w);x++)pixels[y*256+x]=q.argb;
 std::ofstream f(argv[2],std::ios::binary);f<<"P6\n256 296\n255\n";for(auto c:pixels){char b[]={char(c>>16),char(c>>8),char(c)};f.write(b,3);}
 puts("PASS real local Claire geometry, finite bounded quads, rotation and exact scratch-buffer reuse; no game execution or original assets modified");
}
