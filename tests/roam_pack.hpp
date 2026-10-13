// A pack with several layouts and rules that follow a game's reported state, for the tests:
// on foot, in a vehicle, in an aircraft, and a menu. It is not shipped in the overlay.
#pragma once
#include "../core/config.hpp"
#include "../core/text.hpp"

inline constexpr uint64_t kRoam = 0x0100AAAA00002000ull;

inline const char kRoamPackText[] = R"PACK(
[pack]
name=Open World
author=makaka393
games=0100AAAA00002000
start=foot
[state]
vehicle=0x1
plane=0x2
heli=0x4
boat=0x8
bike=0x10
sub=0x20
entering=0x40
swimming=0x80
freefall=0x100
cover=0x200
phone=0x400
aiming=0x800
ragdoll=0x1000
dead=0x2000
pause=0x10000
cutscene=0x20000
switching=0x40000
faded=0x80000
control=0x100000
wheel=0x200000
alive=0x40000000
stalled=0x80000000
[foot]
name=On foot
when=!vehicle
menu=640,44
stick_l=200,520,105 | zone=0,330,430,720
stick_r=900,330,90 | zone=580,100,1280,720 | hint=0
btn=ZR | 1128,568,68 | icon=fire | drag=rs
btn=B  | 940,648,46 | icon=run | mode=turbo
btn=Y  | 980,540,46 | icon=jump
btn=ZL | 1074,437,46 | icon=aim | mode=toggle
btn=X  | 1201,379,46 | icon=car | goto=car
btn=A  | 838,560,38 | icon=punch
btn=L  | 929,393,38 | icon=gun | drag=rs
btn=R  | 1085,281,38 | icon=cover
btn=DUP   | 60,52,30 | icon=phone | goto=menu
btn=DDOWN | 136,52,30 | icon=people | drag=rs
btn=LS    | 60,250,30 | icon=stealth
btn=NONE  | 716,44,26 | icon=car | goto=car
btn=PLUS  | 1220,52,30 | icon=pause | goto=menu
[car]
name=In vehicle
when=vehicle | entering
menu=640,44
stick_l=200,520,105 | zone=0,330,430,720
stick_r=900,330,90 | zone=580,100,1280,720 | hint=0
btn=ZR | 1128,568,68 | icon=gas
btn=ZL | 940,648,46 | icon=brake
btn=R  | 980,540,46 | icon=handbrake
btn=L  | 1074,437,46 | icon=fire
btn=X  | 1201,379,46 | icon=exit | goto=foot
btn=RS | 929,393,38 | icon=behind
btn=LS | 1085,281,38 | icon=horn
btn=DUP    | 60,52,30 | icon=phone | goto=menu
btn=DLEFT  | 136,52,30 | icon=radio | drag=rs
btn=DRIGHT | 212,52,30 | icon=lights
btn=B      | 288,52,30 | icon=ok
btn=NONE   | 716,44,26 | icon=walk | goto=foot
btn=PLUS   | 1220,52,30 | icon=pause | goto=menu
[air]
name=Aircraft
when=plane | heli
menu=640,44
stick_l=200,520,105 | zone=0,330,430,720
stick_r=900,330,90 | zone=580,100,1280,720 | hint=0
btn=ZR | 1128,568,68 | icon=up
btn=ZL | 940,648,46 | icon=down
btn=L  | 980,540,46 | icon=left
btn=R  | 1074,437,46 | icon=right
btn=X  | 1201,379,46 | icon=exit | goto=foot
btn=B  | 929,393,38 | icon=fire
btn=LS | 1085,281,38 | icon=swap
btn=DUP   | 60,52,30 | icon=phone | goto=menu
btn=MINUS | 136,52,30 | icon=camera
btn=NONE  | 716,44,26 | icon=walk | goto=foot
btn=PLUS  | 1220,52,30 | icon=pause | goto=menu
[menu]
name=Menu
when=pause | stalled
hold=1
menu=640,44
dpad=200,520,120
stick_l=200,250,80
stick_r=800,330,90 | zone=480,120,900,620 | hint=0
btn=B | 1128,568,68 | icon=ok
btn=A | 940,648,46 | icon=back
btn=Y | 980,540,46
btn=X | 1074,437,46
btn=ZL | 92,62,78,40
btn=L  | 278,62,78,40
btn=R  | 1002,62,78,40
btn=ZR | 1188,62,78,40
btn=MINUS | 456,46,30
btn=HOME  | 548,46,30
btn=PLUS  | 824,46,30
btn=NONE  | 640,664,38 | icon=play | goto=back
)PACK";

// Adds the pack to a store (as a file from the card would be) and returns its index.
inline int addRoamPack(tp::PackStore& store) {
    static tp::Pack pack;
    tp::copyText(pack.id, sizeof(pack.id), "roam");
    tp::parsePack(kRoamPackText, pack);
    return store.put(pack);
}
