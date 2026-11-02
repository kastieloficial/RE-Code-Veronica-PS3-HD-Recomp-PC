#include "../cv_combat_hud_model.h"
#include <cassert>
int main(){using namespace cv_combat_hud;
 cv_srt_state state{};state.equipped=1;
 state.inventory[0]={9,15,0,1,0};state.inventory[1]={12,112,0,1,0};
 auto gun=weapon(state);assert(gun.loaded==15&&gun.reserve==112);assert(ammo_label(gun)==L"15/112");
 state.inventory[2]={12,10,0,1,0};state.inventory[3]={9,15,0,1,0};state.inventory[4]={14,7,0,1,0};
 assert(ammo_label(weapon(state))==L"15/122"); // sum matching stacks, not other guns or ammo
 state.inventory[0].quantity=14;assert(ammo_label(weapon(state))==L"14/122");
 state.inventory[0].quantity=0;assert(ammo_label(weapon(state))==L"0/122");
 state.inventory[1]={};state.inventory[2]={};assert(ammo_label(weapon(state))==L"0");
 state.inventory[0].infinite=1;assert(ammo_label(weapon(state))==L"∞");
 state.inventory[0]={0x22,94,0,1,0};assert(ammo_label(weapon(state))==L"94%");
 state.inventory[1]={0x26,100,0,1,0};assert(ammo_label(weapon(state))==L"94%/100%");
 state.inventory[0]={6,64+5,0,1,0};state.inventory[1]={0x10,8,0,1,0};state.inventory[2]={0xf,9,0,1,0};
 assert(ammo_label(weapon(state))==L"5/8"); // grenade ammunition variant
 state.inventory[0]={8,1,0,1,0};assert(ammo_label(weapon(state)).empty());
 state.inventory[0]={0x37,1,0,1,0};assert(ammo_label(weapon(state)).empty());
 state.equipped=0;assert(ammo_label(weapon(state)).empty());
}
