#include "../cv_native_language_overlay.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(){
 namespace fs=std::filesystem;using cv_native_language::resolve;
 const auto root=fs::temp_directory_path()/"cv-native-language-overlay-test";
 fs::create_directories(root/"system");std::ofstream(root/"system/sysmes.msb")<<"fixture";
 const auto value=root.string();
 const auto original="D:/Games/NPUB30467/USRDIR/BHCV/nativePS3/biocv_disc/usa/system/sysmes.msb";
 assert(resolve("NPUB30467",original,value.c_str())==root/"system/sysmes.msb");
 assert(resolve("NPEB00553",original,value.c_str()).empty());
 assert(resolve("NPJB00135",original,value.c_str()).empty());
 assert(resolve("NPUB30467",original,"").empty());
 assert(resolve("NPUB30467","D:/biocv_disc/usa/../system/sysmes.msb",value.c_str()).empty());
 assert(resolve("NPUB30467","D:/biocv_disc/usa/system/missing.msb",value.c_str()).empty());
 assert(resolve("NPUB30467","D:/biocv_disc/jpn/system/sysmes.msb",value.c_str()).empty());
 fs::remove(root/"system/sysmes.msb");fs::remove(root/"system");fs::remove(root);
 std::cout<<"PASS: US-only read-only locale routing, missing files, traversal and JP exclusion\n";
}
