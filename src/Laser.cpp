#include <math.h>
#include <stddef.h>
#include <string.h>

#include "AnmManager.h"
#include "Collision.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Enemy.h"
#include "GameErrorContext.h"
#include "GameThread.h"
#include "Globals.h"
#include "Laser.h"
#include "Player.h"
#include "SoundManager.h"
#include "ZunAsm.h"

static_assert(offsetof(LaserLineInner, ex) == 0x38, "LaserLineInner::ex");
static_assert(offsetof(LaserLineInner, shot_sfx) == 0x350, "LaserLineInner::shot_sfx");

// GLOBAL: TH16 0x4a6ee0
LaserManager *g_LaserManager;

// The bullet types (BulletTypeInfo, BulletManager.h), indexed by the ECL
// bullet type. Each row: bullet.anm script; per color the sprite remaps
// [color][0..3] (colors not listed are zero; -1 keeps the script's own
// sprite); hitbox radius, draw layer, cancel kind, overlay script.
// GLOBAL: TH16 0x49f2e0
BulletTypeInfo g_bullet_types[BULLET_TYPE_COUNT] = {
    {35, // type 0
     {
         {0, 323, 336, 161}, {1, 324, 337, 164}, {2, 324, 338, 167}, {3, 325, 339, 170},
         {4, 325, 340, 173}, {5, 326, 341, 176}, {6, 326, 342, 179}, {7, 327, 343, 182},
         {8, 327, 344, 185}, {9, 328, 345, 188}, {10, 328, 346, 191}, {11, 328, 347, 194},
         {12, 329, 348, 197}, {13, 329, 349, 200}, {14, 329, 350, 203}, {15, 330, 351, 206},
     },
     2.4f, 5, 6, 0},
    {36, // type 1
     {
         {0, 323, 336, 161}, {1, 324, 337, 164}, {2, 324, 338, 167}, {3, 325, 339, 170},
         {4, 325, 340, 173}, {5, 326, 341, 176}, {6, 326, 342, 179}, {7, 327, 343, 182},
         {8, 327, 344, 185}, {9, 328, 345, 188}, {10, 328, 346, 191}, {11, 328, 347, 194},
         {12, 329, 348, 197}, {13, 329, 349, 200}, {14, 329, 350, 203}, {15, 330, 351, 206},
     },
     2.4f, 5, 6, 0},
    {37, // type 2
     {
         {16, 323, 336, 209}, {17, 324, 337, 212}, {18, 324, 338, 215}, {19, 325, 339, 218},
         {20, 325, 340, 221}, {21, 326, 341, 224}, {22, 326, 342, 227}, {23, 327, 343, 230},
         {24, 327, 344, 233}, {25, 328, 345, 236}, {26, 328, 346, 239}, {27, 328, 347, 242},
         {28, 329, 348, 245}, {29, 329, 349, 248}, {30, 329, 350, 251}, {31, 330, 351, 254},
     },
     2.4f, 5, 6, 0},
    {38, // type 3
     {
         {48, 323, 336, 209}, {49, 324, 337, 212}, {50, 324, 338, 215}, {51, 325, 339, 218},
         {52, 325, 340, 221}, {53, 326, 341, 224}, {54, 326, 342, 227}, {55, 327, 343, 230},
         {56, 327, 344, 233}, {57, 328, 345, 236}, {58, 328, 346, 239}, {59, 328, 347, 242},
         {60, 329, 348, 245}, {61, 329, 349, 248}, {62, 329, 350, 251}, {63, 330, 351, 254},
     },
     2.0f, 5, 6, 0},
    {39, // type 4
     {
         {64, 323, 336, 209}, {65, 324, 337, 212}, {66, 324, 338, 215}, {67, 325, 339, 218},
         {68, 325, 340, 221}, {69, 326, 341, 224}, {70, 326, 342, 227}, {71, 327, 343, 230},
         {72, 327, 344, 233}, {73, 328, 345, 236}, {74, 328, 346, 239}, {75, 328, 347, 242},
         {76, 329, 348, 245}, {77, 329, 349, 248}, {78, 329, 350, 251}, {79, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {40, // type 5
     {
         {80, 323, 336, 209}, {81, 324, 337, 212}, {82, 324, 338, 215}, {83, 325, 339, 218},
         {84, 325, 340, 221}, {85, 326, 341, 224}, {86, 326, 342, 227}, {87, 327, 343, 230},
         {88, 327, 344, 233}, {89, 328, 345, 236}, {90, 328, 346, 239}, {91, 328, 347, 242},
         {92, 329, 348, 245}, {93, 329, 349, 248}, {94, 329, 350, 251}, {95, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {41, // type 6
     {
         {96, 323, 336, 209}, {97, 324, 337, 212}, {98, 324, 338, 215}, {99, 325, 339, 218},
         {100, 325, 340, 221}, {101, 326, 341, 224}, {102, 326, 342, 227}, {103, 327, 343, 230},
         {104, 327, 344, 233}, {105, 328, 345, 236}, {106, 328, 346, 239}, {107, 328, 347, 242},
         {108, 329, 348, 245}, {109, 329, 349, 248}, {110, 329, 350, 251}, {111, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {42, // type 7
     {
         {112, 323, 336, 209}, {113, 324, 337, 212}, {114, 324, 338, 215}, {115, 325, 339, 218},
         {116, 325, 340, 221}, {117, 326, 341, 224}, {118, 326, 342, 227}, {119, 327, 343, 230},
         {120, 327, 344, 233}, {121, 328, 345, 236}, {122, 328, 346, 239}, {123, 328, 347, 242},
         {124, 329, 348, 245}, {125, 329, 349, 248}, {126, 329, 350, 251}, {127, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {43, // type 8
     {
         {128, 323, 336, 209}, {129, 324, 337, 212}, {130, 324, 338, 215}, {131, 325, 339, 218},
         {132, 325, 340, 221}, {133, 326, 341, 224}, {134, 326, 342, 227}, {135, 327, 343, 230},
         {136, 327, 344, 233}, {137, 328, 345, 236}, {138, 328, 346, 239}, {139, 328, 347, 242},
         {140, 329, 348, 245}, {141, 329, 349, 248}, {142, 329, 350, 251}, {143, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {44, // type 9
     {
         {160, 323, 336, 209}, {161, 324, 337, 212}, {162, 324, 338, 215}, {163, 325, 339, 218},
         {164, 325, 340, 221}, {165, 326, 341, 224}, {166, 326, 342, 227}, {167, 327, 343, 230},
         {168, 327, 344, 233}, {169, 328, 345, 236}, {170, 328, 346, 239}, {171, 328, 347, 242},
         {172, 329, 348, 245}, {173, 329, 349, 248}, {174, 329, 350, 251}, {175, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {45, // type 10
     {
         {176, 323, 336, 209}, {177, 324, 337, 212}, {178, 324, 338, 215}, {179, 325, 339, 218},
         {180, 325, 340, 221}, {181, 326, 341, 224}, {182, 326, 342, 227}, {183, 327, 343, 230},
         {184, 327, 344, 233}, {185, 328, 345, 236}, {186, 328, 346, 239}, {187, 328, 347, 242},
         {188, 329, 348, 245}, {189, 329, 349, 248}, {190, 329, 350, 251}, {191, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {46, // type 11
     {
         {208, 323, 336, 209}, {209, 324, 337, 212}, {210, 324, 338, 215}, {211, 325, 339, 218},
         {212, 325, 340, 221}, {213, 326, 341, 224}, {214, 326, 342, 227}, {215, 327, 343, 230},
         {216, 327, 344, 233}, {217, 328, 345, 236}, {218, 328, 346, 239}, {219, 328, 347, 242},
         {220, 329, 348, 245}, {221, 329, 349, 248}, {222, 329, 350, 251}, {223, 330, 351, 254},
     },
     2.8f, 3, 6, 0},
    {47, // type 12
     {
         {224, 323, 336, 209}, {225, 324, 337, 212}, {226, 324, 338, 215}, {227, 325, 339, 218},
         {228, 325, 340, 221}, {229, 326, 341, 224}, {230, 326, 342, 227}, {231, 327, 343, 230},
         {232, 327, 344, 233}, {233, 328, 345, 236}, {234, 328, 346, 239}, {235, 328, 347, 242},
         {236, 329, 348, 245}, {237, 329, 349, 248}, {238, 329, 350, 251}, {239, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {48, // type 13
     {
         {240, 323, 336, 209}, {241, 324, 337, 212}, {242, 324, 338, 215}, {243, 325, 339, 218},
         {244, 325, 340, 221}, {245, 326, 341, 224}, {246, 326, 342, 227}, {247, 327, 343, 230},
         {248, 327, 344, 233}, {249, 328, 345, 236}, {250, 328, 346, 239}, {251, 328, 347, 242},
         {252, 329, 348, 245}, {253, 329, 349, 248}, {254, 329, 350, 251}, {255, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {49, // type 14
     {
         {256, 323, 336, 209}, {257, 324, 337, 212}, {258, 324, 338, 215}, {259, 325, 339, 218},
         {260, 325, 340, 221}, {261, 326, 341, 224}, {262, 326, 342, 227}, {263, 327, 343, 230},
         {264, 327, 344, 233}, {265, 328, 345, 236}, {266, 328, 346, 239}, {267, 328, 347, 242},
         {268, 329, 348, 245}, {269, 329, 349, 248}, {270, 329, 350, 251}, {271, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {50, // type 15
     {
         {272, 323, 336, 209}, {273, 324, 337, 212}, {274, 324, 338, 215}, {275, 325, 339, 218},
         {276, 325, 340, 221}, {277, 326, 341, 224}, {278, 326, 342, 227}, {279, 327, 343, 230},
         {280, 327, 344, 233}, {281, 328, 345, 236}, {282, 328, 346, 239}, {283, 328, 347, 242},
         {284, 329, 348, 245}, {285, 329, 349, 248}, {286, 329, 350, 251}, {287, 330, 351, 254},
     },
     2.4f, 4, 6, 0},
    {51, // type 16
     {
         {288, 323, 336, 209}, {289, 324, 337, 212}, {290, 324, 338, 215}, {291, 325, 339, 218},
         {292, 325, 340, 221}, {293, 326, 341, 224}, {294, 326, 342, 227}, {295, 327, 343, 230},
         {296, 327, 344, 233}, {297, 328, 345, 236}, {298, 328, 346, 239}, {299, 328, 347, 242},
         {300, 329, 348, 245}, {301, 329, 349, 248}, {302, 329, 350, 251}, {303, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {52, // type 17
     {
         {320, 329, 340, 221}, {321, 330, 336, 209}, {322, 324, 337, 212},
     },
     4.0f, 3, 0, 0},
    {72, // type 18
     {
         {368, 323, 0, 257}, {369, 324, 0, 260}, {370, 325, 0, 263}, {371, 326, 0, 266},
         {372, 327, 0, 269}, {373, 328, 0, 272}, {374, 329, 0, 275}, {375, 330, 0, 278},
     },
     8.5f, 1, 6, 0},
    {73, // type 19
     {
         {376, 323, 0, 257}, {377, 324, 0, 260}, {378, 325, 0, 263}, {379, 326, 0, 266},
         {380, 327, 0, 269}, {381, 328, 0, 272}, {382, 329, 0, 275}, {383, 330, 0, 278},
     },
     8.5f, 1, 6, 0},
    {74, // type 20
     {
         {392, 323, 0, 257}, {393, 324, 0, 260}, {394, 325, 0, 263}, {395, 326, 0, 266},
         {396, 327, 0, 269}, {397, 328, 0, 272}, {398, 329, 0, 275}, {399, 330, 0, 278},
     },
     7.0f, 2, 6, 0},
    {75, // type 21
     {
         {400, 323, 0, 257}, {401, 324, 0, 260}, {402, 325, 0, 263}, {403, 326, 0, 266},
         {404, 327, 0, 269}, {405, 328, 0, 272}, {406, 329, 0, 275}, {407, 330, 0, 278},
     },
     6.0f, 2, 6, 0},
    {76, // type 22
     {
         {408, 323, 0, 257}, {409, 324, 0, 260}, {410, 325, 0, 263}, {411, 326, 0, 266},
         {412, 327, 0, 269}, {413, 328, 0, 272}, {414, 329, 0, 275}, {415, 330, 0, 278},
     },
     7.0f, 2, 6, 0},
    {77, // type 23
     {
         {416, 323, 0, 257}, {417, 324, 0, 260}, {418, 325, 0, 263}, {419, 326, 0, 266},
         {420, 327, 0, 269}, {421, 328, 0, 272}, {422, 329, 0, 275}, {423, 330, 0, 278},
     },
     7.0f, 1, 6, 0},
    {78, // type 24
     {
         {424, 323, 0, 257}, {425, 324, 0, 260}, {426, 325, 0, 263}, {427, 326, 0, 266},
         {428, 327, 0, 269}, {429, 328, 0, 272}, {430, 329, 0, 275}, {431, 330, 0, 278},
     },
     7.0f, 1, 6, 0},
    {108, // type 25
     {
         {-1, -1, 0, 263}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     6.0f, 2, 7, 0},
    {109, // type 26
     {
         {-1, -1, 0, 260}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     6.0f, 2, 8, 0},
    {110, // type 27
     {
         {-1, -1, 0, 266}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 2, 9, 0},
    {111, // type 28
     {
         {-1, -1, 0, 275}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 2, 10, 0},
    {81, // type 29
     {
         {444, 323, 0, 257}, {445, 324, 0, 260}, {446, 325, 0, 263}, {447, 326, 0, 266},
         {448, 327, 0, 269}, {449, 328, 0, 272}, {450, 329, 0, 275}, {451, 330, 0, 278},
     },
     10.0f, 1, 6, 0},
    {79, // type 30
     {
         {384, 323, 0, 257}, {385, 324, 0, 260}, {386, 325, 0, 263}, {387, 326, 0, 266},
         {388, 327, 0, 269}, {389, 328, 0, 272}, {390, 329, 0, 275}, {391, 330, 0, 278},
     },
     7.0f, 1, 6, 0},
    {82, // type 31
     {
         {452, 323, 0, 257}, {453, 324, 0, 260}, {454, 325, 0, 263}, {455, 326, 0, 266},
         {456, 327, 0, 269}, {457, 328, 0, 272}, {458, 329, 0, 275}, {459, 330, 0, 278},
     },
     4.0f, 3, 6, 0},
    {80, // type 32
     {
         {432, 0, 0, 281}, {433, 0, 0, 284}, {434, 0, 0, 287}, {435, 0, 0, 290},
     },
     14.0f, 0, 6, 0},
    {315, // type 33
     {
         {548, 0, 0, 257}, {549, 0, 0, 260}, {550, 0, 0, 263}, {551, 0, 0, 266},
         {552, 0, 0, 269}, {553, 0, 0, 272}, {554, 0, 0, 275}, {555, 0, 0, 278},
     },
     12.0f, 2, 6, 0},
    {107, // type 34
     {
         {468, 323, 336, 209}, {469, 324, 337, 212}, {470, 324, 338, 215}, {471, 325, 339, 218},
         {472, 325, 340, 221}, {473, 326, 341, 224}, {474, 326, 342, 227}, {475, 327, 343, 230},
         {476, 327, 344, 233}, {477, 328, 345, 236}, {478, 328, 346, 239}, {479, 328, 347, 242},
         {480, 329, 348, 245}, {481, 329, 349, 248}, {482, 329, 350, 251}, {483, 330, 351, 254},
     },
     2.4f, 5, 6, 0},
    {53, // type 35
     {
         {144, 323, 336, 209}, {145, 324, 337, 212}, {146, 324, 338, 215}, {147, 325, 339, 218},
         {148, 325, 340, 221}, {149, 326, 341, 224}, {150, 326, 342, 227}, {151, 327, 343, 230},
         {152, 327, 344, 233}, {153, 328, 345, 236}, {154, 328, 346, 239}, {155, 328, 347, 242},
         {156, 329, 348, 245}, {157, 329, 349, 248}, {158, 329, 350, 251}, {159, 330, 351, 254},
     },
     3.2f, 4, 6, 0},
    {54, // type 36
     {
         {192, 323, 336, 209}, {193, 324, 337, 212}, {194, 324, 338, 215}, {195, 325, 339, 218},
         {196, 325, 340, 221}, {197, 326, 341, 224}, {198, 326, 342, 227}, {199, 327, 343, 230},
         {200, 327, 344, 233}, {201, 328, 345, 236}, {202, 328, 346, 239}, {203, 328, 347, 242},
         {204, 329, 348, 245}, {205, 329, 349, 248}, {206, 329, 350, 251}, {207, 330, 351, 254},
     },
     3.2f, 4, 6, 0},
    {55, // type 37
     {
         {304, 323, 336, 209}, {305, 324, 337, 212}, {306, 324, 338, 215}, {307, 325, 339, 218},
         {308, 325, 340, 221}, {309, 326, 341, 224}, {310, 326, 342, 227}, {311, 327, 343, 230},
         {312, 327, 344, 233}, {313, 328, 345, 236}, {314, 328, 346, 239}, {315, 328, 347, 242},
         {316, 329, 348, 245}, {317, 329, 349, 248}, {318, 329, 350, 251}, {319, 330, 351, 254},
     },
     4.0f, 3, 6, 0},
    {158, // type 38
     {
         {352, -1, 336, 209}, {353, -1, 337, 212}, {354, -1, 338, 215}, {355, -1, 339, 218},
         {356, -1, 340, 221}, {357, -1, 341, 224}, {358, -1, 342, 227}, {359, -1, 343, 230},
         {360, -1, 344, 233}, {361, -1, 345, 236}, {362, -1, 346, 239}, {363, -1, 347, 242},
         {364, -1, 348, 245}, {365, -1, 349, 248}, {366, -1, 350, 251}, {367, -1, 351, 254},
     },
     4.0f, 2, 6, 0},
    {317, // type 39
     {
         {-1, -1, 0, 260}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 3, 6, 0},
    {318, // type 40
     {
         {-1, -1, 0, 266}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 3, 6, 0},
    {319, // type 41
     {
         {-1, -1, 0, 275}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 3, 6, 0},
    {320, // type 42
     {
         {-1, -1, 0, 263}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
         {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0}, {-1, -1, -1, 0},
     },
     4.0f, 3, 6, 0},
    {321, // type 43
     {
         {568, 323, 0, 257}, {569, 324, 0, 260}, {570, 325, 0, 263}, {571, 326, 0, 266},
         {572, 327, 0, 269}, {573, 328, 0, 272}, {574, 329, 0, 275}, {575, 330, 0, 278},
     },
     5.0f, 2, 6, 0},
};

// This file's copy of sincosmul (ZunAsm.h), which TH16 keeps once per
// object file. A static of its own so that it can be annotated. ZUN's laser
// code was one file; the laser methods that call it are kept here so that
// they call this copy (LTCG knows it leaves ecx and edx alone, which it
// would not assume for an external function).
// FUNCTION: TH16 0x43ad00
static void __fastcall laser_sincosmul(Float3 *dst, f32 angle, f32 radius)
{
    ZUN_ASM_SINCOSMUL(dst, angle, radius);
}

// FUNCTION: TH16 0x42cb00
void LaserManager::destroy_all()
{
    LaserDataInf *laser = list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        laser->on_destroy();
        laser->unlink();
        delete laser;
        laser = next;
    }
    list_length = 0;
    list_tail = &list_head;
}

// FUNCTION: TH16 0x430e10
void LaserDataInf::get_point(f32 distance, Float3 *out)
{
}

// FUNCTION: TH16 0x430e20
void LaserDataInf::run_ex()
{
}

// FUNCTION: TH16 0x430e30
void LaserDataInf::method_8(i32 arg)
{
}

// FUNCTION: TH16 0x430e40
i32 LaserDataInf::initialize(void *params)
{
    return 0;
}

// FUNCTION: TH16 0x430e50
i32 LaserDataInf::on_tick()
{
    return 0;
}

// FUNCTION: TH16 0x430e60
i32 LaserDataInf::on_draw()
{
    return 0;
}

// FUNCTION: TH16 0x430e70
i32 LaserDataInf::on_destroy()
{
    unlink();
    return 0;
}

// FUNCTION: TH16 0x430e90
i32 LaserDataInf::sum_rect_damage(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    return 0;
}

// FUNCTION: TH16 0x430ea0
i32 LaserDataInf::cancel_as_bomb_rectangle(Float3 *a, Float3 *b, f32 angle, i32 d, i32 e)
{
    return 0;
}

// FUNCTION: TH16 0x430eb0
i32 LaserDataInf::cancel_as_bomb_circle(Float3 *pos, f32 radius, i32 c, i32 d)
{
    return 0;
}

// FUNCTION: TH16 0x430ec0
i32 LaserDataInf::cancel(i32 mode, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x430ed0
i32 LaserDataInf::method_2c(i32 a, i32 b, i32 c, i32 d)
{
    return 0;
}

// FUNCTION: TH16 0x430ee0
i32 LaserDataInf::touches_circle(Float3 *pos, f32 radius)
{
    return 0;
}

// FUNCTION: TH16 0x430ef0
i32 LaserDataInf::check_graze_or_kill(i32 a)
{
    return 0;
}

// FUNCTION: TH16 0x430f00
i32 LaserDataInf::step_ex_speedup()
{
    return 0;
}

// FUNCTION: TH16 0x430f10
i32 LaserDataInf::step_ex_accel()
{
    return 0;
}

// FUNCTION: TH16 0x430f20
i32 LaserDataInf::step_ex_angle_accel()
{
    return 0;
}

// FUNCTION: TH16 0x430f30
i32 LaserDataInf::step_ex_angle()
{
    return 0;
}

// FUNCTION: TH16 0x430f40
i32 LaserDataInf::step_ex_angle_mode_4()
{
    return 0;
}

// FUNCTION: TH16 0x430f50
i32 LaserDataInf::step_ex_angle_mode_1()
{
    return 0;
}

// FUNCTION: TH16 0x430f60
i32 LaserDataInf::step_ex_bounce()
{
    return 0;
}

// FUNCTION: TH16 0x430f70
i32 LaserDataInf::step_ex_wrap()
{
    return 0;
}

// FUNCTION: TH16 0x430f80
i32 LaserDataInf::method_58()
{
    return 0;
}

// FUNCTION: TH16 0x430f90
i32 LaserDataInf::method_5c()
{
    return 0;
}

// FUNCTION: TH16 0x430fa0
i32 LaserDataInf::step_ex_offscreen()
{
    return 0;
}

// FUNCTION: TH16 0x430fb0
LaserDataInf *LaserDataInf::clone()
{
    return NULL;
}

// FUNCTION: TH16 0x430fc0
LaserDataInf::LaserDataInf()
{
    memset(this, 0, sizeof(*this));
    timer.reset();
}

// FUNCTION: TH16 0x431050
void LaserLineInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x4310b0
LaserDataInf *LaserLineInf::clone()
{
    LaserLineInf *copy = new LaserLineInf(LaserLineInf::InlineCtor());
    memcpy(copy, this, sizeof(LaserLineInf));
    return copy;
}

// Called out of line everywhere but in clone.
// FUNCTION: TH16 0x431130
DECOMP_NOINLINE LaserLineInf::LaserLineInf()
{
}

// FUNCTION: TH16 0x4311f0
void LaserCurveInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x431250
void LaserInfiniteInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x4312b0
void LaserBeamInf::get_point(f32 distance, Float3 *out)
{
    laser_sincosmul(out, angle, distance);
    *out += position;
}

// FUNCTION: TH16 0x431310
void LaserBeamInf::method_8(i32 arg)
{
    inner.flag_38 = arg;
}

// Registers the tick and draw callbacks.
// FUNCTION: TH16 0x431330
i32 LaserManager::initialize()
{
    bullet_anm = AnmManager::preload_anm(ANM_SLOT_BULLET, "bullet.anm");
    if (bullet_anm == NULL)
    {
        // "Enemy bullet data not found. The data is corrupted."
        g_GameErrorContext.log("\x93G\x92" "e\x83" "f\x81[\x83^\x82\xaa\x8c\xa9\x82\xc2\x82\xa9\x82\xe8\x82\xdc\x82"
                               "\xb9\x82\xf1\x81" "B\x83" "f\x81[\x83^\x82\xaa\x89\xf3\x82\xea\x82\xc4\x82\xa2\x82"
                               "\xdc\x82\xb7\r\n");
        return -1;
    }
    UpdateFunc *f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_tick_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, 0x1b);
    on_tick = f;
    f = g_UpdateFuncRegistry->create_func((UpdateFuncCallback)on_draw_callback);
    f->flags &= ~UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, 0x23);
    on_draw = f;
    list_tail = &list_head;
    return 0;
}

LaserManager::LaserManager()
{
    memset(this, 0, sizeof(LaserManager));
    last_id = 0x10000;
    g_LaserManager = this;
}

// FUNCTION: TH16 0x4313b0
LaserManager::~LaserManager()
{
    g_UpdateFuncRegistry->unregister_locked(on_tick);
    g_UpdateFuncRegistry->unregister_locked(on_draw);
    destroy_all();
    g_LaserManager = NULL;
}

// FUNCTION: TH16 0x4314a0
LaserManager *LaserManager::create()
{
    LaserManager *mgr = new LaserManager();
    if (mgr->initialize() != 0)
    {
        delete mgr;
        return NULL;
    }
    return mgr;
}

// Ticks every laser and deletes the cancelled and finished ones (those
// marked pending_delete a frame later); frozen lasers only check grazes.
// FUNCTION: TH16 0x431510
i32 LaserManager::on_tick_body()
{
    LaserDataInf *laser = list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->pending_delete)
        {
            laser->pending_delete++;
            if (laser->pending_delete >= 2)
            {
                destroy(laser);
                laser = next;
                continue;
            }
        }
        if (laser->state == LASER_STATE_CANCELLED)
        {
            destroy(laser);
        }
        else if (laser->frozen)
        {
            laser->check_graze_or_kill(1);
        }
        else if (laser->on_tick())
        {
            destroy(laser);
        }
        else
        {
            laser->timer.tick();
            laser->ticked = 1;
        }
        laser = next;
    }
    return 1;
}

// Runs on_tick_body unless the game is paused (at zero game speed while
// GameThread flag_1 is set).
// FUNCTION: TH16 0x4316b0
i32 __fastcall LaserManager::on_tick_callback(LaserManager *mgr)
{
    if (g_GameThread->flags.flag_0 | g_GameThread->flags.loading)
    {
        return 1;
    }
    if (g_GameThread->flags.flag_10)
    {
        return 1;
    }
    if (g_GameThread->flags.flag_1)
    {
        f32 speed = g_game_speed;
        g_game_speed = 0.0f;
        i32 result = mgr->on_tick_body();
        g_game_speed = speed;
        return result;
    }
    return mgr->on_tick_body();
}

// Draws every laser not cancelled.
// FUNCTION: TH16 0x431720
i32 __fastcall LaserManager::on_draw_callback(LaserManager *mgr)
{
    if (g_GameThread->flags.loading)
    {
        return 1;
    }
    LaserDataInf *laser = mgr->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != LASER_STATE_CANCELLED)
        {
            laser->on_draw();
        }
        laser = next;
    }
    return 1;
}

// Each kind links and initializes its laser itself; the compiler merges the
// four copies.
// TODO: the original reserves an unused stack slot (push ecx).
// FUNCTION: TH16 0x431760
DECOMP_NOINLINE i32 LaserManager::allocate_new_laser(i32 kind, void *params)
{
    LaserManager *mgr = g_LaserManager;
    if (mgr->list_length >= 0x200)
    {
        return 0;
    }
    mgr->last_id++;
    if (mgr->last_id < 0x10000)
    {
        mgr->last_id = 0x10000;
    }
    LaserDataInf *laser;
    switch (kind)
    {
    case LASER_LINE:
        laser = new LaserLineInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_INFINITE:
        laser = new LaserInfiniteInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_BEAM:
        laser = new LaserBeamInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    case LASER_CURVE:
        laser = new LaserCurveInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
        break;
    }
    return mgr->last_id;
}

// FUNCTION: TH16 0x431860
LaserInfiniteInf::LaserInfiniteInf()
{
}

// FUNCTION: TH16 0x4318c0
LaserBeamInf::LaserBeamInf()
{
}

// FUNCTION: TH16 0x431900
LaserCurveInf::LaserCurveInf()
{
}

// TODO: the original has an 8-byte frame (sub esp, 8) where ours has 4.
// FUNCTION: TH16 0x431950
HARNESS_CALLED i32 LaserManager::cancel_in_rectangle(Float3 *a, Float3 *b, f32 angle, i32 mode, i32 skip_invuln)
{
    LaserManager *mgr = g_LaserManager;
    LaserDataInf *laser = mgr->list_head.next;
    i32 count = 0;
    mgr->cancel_pos = *a;
    mgr->cancel_pos_2 = *b;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != LASER_STATE_CANCELLED && laser->ticked)
        {
            count += laser->cancel_as_bomb_rectangle(a, b, angle, mode, skip_invuln);
        }
        laser = next;
    }
    return count;
}

// FUNCTION: TH16 0x4319e0
i32 LaserManager::cancel_all()
{
    LaserDataInf *laser = g_LaserManager->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        laser->offscreen_grace.reset();
        laser->ex_invuln_remaining_frames = 0;
        laser->cancel(1, 0);
        laser = next;
    }
    return 0;
}

// TODO: the original has an 8-byte frame (sub esp, 8) where ours has 4.
// FUNCTION: TH16 0x431a70
HARNESS_CALLED i32 LaserManager::cancel_in_radius(Float3 *pos, f32 radius, i32 mode, i32 skip_invuln)
{
    LaserManager *mgr = g_LaserManager;
    LaserDataInf *laser = mgr->list_head.next;
    LaserDataInf *next;
    i32 count = 0;
    mgr->cancel_pos = *pos;
    for (; laser != NULL; laser = next)
    {
        next = laser->next;
        if (laser->state == LASER_STATE_CANCELLED)
        {
            continue;
        }
        count += laser->cancel_as_bomb_circle(pos, radius, mode, skip_invuln);
    }
    return count;
}

// FUNCTION: TH16 0x431af0
HARNESS_CALLED i32 LaserManager::clear_all(i32 mode, i32 skip_invuln)
{
    LaserDataInf *laser = g_LaserManager->list_head.next;
    while (laser != NULL)
    {
        LaserDataInf *next = laser->next;
        if (laser->state != LASER_STATE_CANCELLED)
        {
            laser->cancel(mode, skip_invuln);
        }
        laser = next;
    }
    return 1;
}

// FUNCTION: TH16 0x411860
LaserInfiniteInner::LaserInfiniteInner()
{
    memset(this, 0, sizeof(LaserInfiniteInner));
    speed = 8.0f;
}

// Draws the laser body, the VM at its tip and, unless unk_7c is set, the
// one at its origin.
// FUNCTION: TH16 0x433720
i32 LaserLineInf::on_draw()
{
    i32 i = 0;
    vm_92c.pos = position;
    f32 rotation = angle + ZUN_PI / 2;
    while (rotation > ZUN_PI)
    {
        rotation -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (rotation < -ZUN_PI)
    {
        rotation += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    AnmVm *vm = &vm_92c;
    vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    vm->rotation.z = rotation;
    g_AnmManager->draw_vm(vm);
    Float3 *tip = &vm_1524.pos;
    laser_sincosmul(tip, angle, hit_length);
    tip->z = 0.0f;
    tip->x += position.x;
    tip->y += position.y;
    g_AnmManager->draw_vm(&vm_1524);
    if (unk_7c == 0.0f)
    {
        vm_f28.pos = position;
        g_AnmManager->draw_vm(&vm_f28);
    }
    return 0;
}

// An et_ex step: moves the curve's origin by ex_state[1]'s velocity (scaled
// by the game speed) and turns it to face its direction of motion, until
// the step's time runs out.
// FUNCTION: TH16 0x4395b0
i32 LaserCurveInf::step_ex_accel()
{
    BulletExState *st = &ex_state[1];
    if (st->timer.current >= st->ints[0])
    {
        ex_flags &= ~BULLET_EX_ACCEL;
        return 1;
    }
    length += st->floats[0] * g_game_speed;
    Float3 v = *(Float3 *)&st->floats[5] * g_game_speed;
    D3DXVec3Add(&tip_offset, &tip_offset, &v);
    if (fabsf(tip_offset.x) > 0.0001f || fabsf(tip_offset.y) > 0.0001f)
    {
        angle = atan2(tip_offset.y, tip_offset.x);
    }
    st->timer.tick();
    return 0;
}

// An et_ex step: turns the curve by ex_state[2]'s angular speed and grows
// it, until the step's time runs out.
// TODO: the original loads floats[0] before storing the new angle (scheduling; wrap_angle or reading floats[0] first do not help).
// FUNCTION: TH16 0x439460
i32 LaserCurveInf::step_ex_angle_accel()
{
    BulletExState *st = &ex_state[2];
    if (st->timer.current >= st->ints[0])
    {
        ex_flags &= ~BULLET_EX_ANGLE_ACCEL;
        return 1;
    }
    i32 i = 0;
    f32 a = st->floats[1] * g_game_speed + angle;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    angle = a;
    length += st->floats[0] * g_game_speed;
    laser_sincosmul(&tip_offset, angle, length);
    st->timer.tick();
    return 0;
}

// allocate_new_laser(LASER_LINE, params) as LTCG inlined it into the wall
// bounce.
static __forceinline void allocate_line_laser_inline(void *params)
{
    LaserManager *mgr = g_LaserManager;
    if (mgr->list_length < 0x200)
    {
        mgr->last_id++;
        if (mgr->last_id < 0x10000)
        {
            mgr->last_id = 0x10000;
        }
        LaserDataInf *laser = new LaserLineInf();
        laser->id = mgr->last_id;
        mgr->append(laser);
        laser->initialize(params);
    }
}

// The wall bounce et_ex step (ex_flags 0x40): once the tip leaves the
// playfield through a wall enabled in ex_state[4].ints[2] (1 top, 2 bottom,
// 4 left, 8 right), a mirrored laser starts where the laser crosses that
// wall, with ex_state[4].floats[0] as its speed (none with bit 0x10), and
// the step ends. 1 if the laser bounced.
// FUNCTION: TH16 0x432620
i32 LaserLineInf::step_ex_bounce()
{
    Float3 tip;
    laser_sincosmul(&tip, angle, hit_length);
    tip += position;
    tip.z = 0.0f;
    if (tip.x + 0.0f <= -192.0f || tip.x - 0.0f >= 192.0f || tip.y + 0.0f <= 0.0f || tip.y - 0.0f >= 448.0f)
    {
        i32 bounced = 0;
        if ((ex_state[4].ints[2] & 1) && tip.y < 0.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -256.0f, 0.0f, 256.0f, 0.0f,
                                               tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = -angle;
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 2) && tip.y > 448.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -256.0f, 448.0f, 256.0f,
                                               448.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = -angle;
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 4) && tip.x < -192.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, -192.0f, -192.0f, -192.0f,
                                               640.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = normalize_angle(-angle - ZUN_PI);
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if ((ex_state[4].ints[2] & 8) && tip.x > 192.0f)
        {
            if (!(ex_state[4].ints[2] & 0x10))
            {
                collision_segment_intersection(&inner.start_pos.x, &inner.start_pos.y, 192.0f, -192.0f, 192.0f,
                                               640.0f, tip.x, tip.y, position.x, position.y);
                inner.start_pos.z = 0.0f;
                inner.ang_aim = normalize_angle(-angle - ZUN_PI);
                inner.speed = ex_state[4].floats[0];
                inner.distance = 0.0f;
                allocate_line_laser_inline(&inner);
            }
            bounced = 1;
        }
        if (bounced)
        {
            ex_flags &= ~BULLET_EX_BOUNCE;
            if (inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            return 1;
        }
    }
    return 0;
}

// The same et_ex step for straight lasers.
// Effective match: as in LaserCurveInf::step_ex_angle, only the ints[2] increment is scheduled differently.
// FUNCTION: TH16 0x432c20
i32 LaserLineInf::step_ex_angle()
{
    f32 len;
    if (ex_state[3].timer.current >= ex_state[3].ints[0])
    {
        if (inner.shot_transform_sfx >= 0)
        {
            g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
        }
        f32 a = ex_state[3].floats[1] + angle;
        ex_state[3].ints[2]++;
        len = ex_state[3].floats[0];
        length = len;
        angle = a;
        ex_state[3].timer.reset();
        if (ex_state[3].ints[2] >= ex_state[3].ints[1])
        {
            laser_sincosmul(&tip_offset, angle, len);
            ex_flags &= ~BULLET_EX_ANGLE;
            return 1;
        }
    }
    else
    {
        len = length - ex_state[3].timer.current_f * length / ex_state[3].ints[0];
    }
    laser_sincosmul(&tip_offset, angle, len);
    ex_state[3].timer.tick_goto();
    return 0;
}

// An et_ex step: retracts the curve over ex_state[3]'s time, then turns it
// and gives it a new length; after ints[1] rounds the step ends.
// The final turn's tip reads angle back from the member (not the local a):
// that keeps a in xmm0 like the original.
// tick_goto gives the timer tick's add of current_f into the speed register.
// Effective match: the original increments ints[2] after loading floats[0]
// (statement order does not move it).
// FUNCTION: TH16 0x4392c0
i32 LaserCurveInf::step_ex_angle()
{
    f32 len;
    if (ex_state[3].timer.current >= ex_state[3].ints[0])
    {
        if (inner.shot_transform_sfx >= 0)
        {
            g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
        }
        f32 a = ex_state[3].floats[1] + angle;
        ex_state[3].ints[2]++;
        len = ex_state[3].floats[0];
        length = len;
        angle = a;
        ex_state[3].timer.reset();
        if (ex_state[3].ints[2] >= ex_state[3].ints[1])
        {
            laser_sincosmul(&tip_offset, angle, len);
            ex_flags &= ~BULLET_EX_ANGLE;
            return 1;
        }
    }
    else
    {
        len = length - ex_state[3].timer.current_f * length / ex_state[3].ints[0];
    }
    laser_sincosmul(&tip_offset, angle, len);
    ex_state[3].timer.tick_goto();
    return 0;
}

// Cancels the laser: a cancel effect and cancel items every 16 units along
// it. Returns the number of points.
// TODO: the original builds the first point as one vector copied to pos and the effect copy, and copies it again at the loop end; ours copies it inside the inlined create_vm.
// FUNCTION: TH16 0x434cd0
i32 LaserLineInf::cancel(i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    // Three dead named locals, as in LaserInfiniteInf::cancel: the count of
    // named variables decides MSVC's choices here (docs/findings.md), and
    // these come closer to the original. Matching only.
    i32 unused_a = 0;
    i32 unused_b = 0;
    i32 unused_c = 0;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 step;
    Float3 pos;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    pos = step + position;
    step.x += step.x;
    step.y += step.y;
    while (hit_length > dist + 8.0f)
    {
        D3DXVECTOR3 effect_pos = pos;
        count++;
        if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
        {
            AnmLoaded *anm = g_BulletManager->bullet_anm;
            anm->create_vm_inline(inner.bullet_color * 2 + 0xd1, &effect_pos, 0.0f, -1);
        }
        else if (bullet_type <= 0x1f || bullet_type == 0x1b)
        {
            g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0);
        }
        else if (bullet_type <= 0x21)
        {
            g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0);
        }
        gen_items_from_cancel(&pos, mode);
        pos += step;
        dist += 16.0f;
    }
    state = LASER_STATE_CANCELLED;
    return count;
}

// Cancels the laser like LaserLineInf::cancel, but only the points on screen
// get an effect and items.
// FUNCTION: TH16 0x436c70
i32 LaserInfiniteInf::cancel(i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    // Three dead named locals: MSVC orders the x component's load by the
    // function's count of named variables (period 8; docs/findings.md), and
    // these give the original's order. Matching only.
    i32 unused_a = 0;
    i32 unused_b = 0;
    i32 unused_c = 0;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 step;
    Float3 pos;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    pos = step + position;
    step.x += step.x;
    step.y += step.y;
    while (hit_length > dist + 8.0f)
    {
        count++;
        if (!(pos.x + 16.0f <= -192.0f || pos.x - 16.0f >= 192.0f || pos.y + 16.0f <= 0.0f || pos.y - 16.0f >= 448.0f))
        {
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x21)
            {
                g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0);
            }
            gen_items_from_cancel(&pos, mode);
        }
        pos += step;
        dist += 16.0f;
    }
    state = LASER_STATE_CANCELLED;
    return count;
}

// Cancels the points (every 16 units) inside a bomb's circle, then cuts the
// laser: a hit head moves its start forward, the first hit run ends it, and
// every later unhit run becomes a new laser. Returns the number of points
// hit.
// As in LaserInfiniteInf::cancel_as_bomb_circle, i is zeroed before the memset
// and step.z is stored before the sincosmul call.
// TODO: register allocation differs (the original keeps center in ebx and count in memory) and the run loops are laid out differently.
// FUNCTION: TH16 0x434730
i32 LaserLineInf::cancel_as_bomb_circle(Float3 *center, f32 radius, i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    // A dead named local: MSVC's register choices here follow the function's
    // count of named variables (docs/findings.md, vector operand order), and
    // one more gives the original's. Matching only.
    i32 unused_a = 0;
    (void)unused_a;
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    i32 i = 0;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    Float3 step;
    step.z = 0.0f;
    laser_sincosmul(&step, angle, 8.0f);
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    radius = radius * radius;
    for (; hit_length >= dist + 8.0f; i++)
    {
        if (!((center->x - pos.x) * (center->x - pos.x) + (center->y - pos.y) * (center->y - pos.y) > radius))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0xd1, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0);
            }
            else if (bullet_type <= 0x21)
            {
                g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0);
            }
        }
        pos += step;
        dist += 16.0f;
    }
    if (count != 0)
    {
        if (count >= i)
        {
            pending_delete = 1;
            return count;
        }
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            position += step * (f32)j;
            hit_length -= (f32)j * 16.0f;
            if (!(hit_length > 24.0f))
            {
                pending_delete = 1;
                return count;
            }
            inner.laser_new_arg_2 = hit_length;
            unk_7c = (f32)j * 16.0f;
        }
        i32 run = 0;
        if (j < i)
        {
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                f32 len = (f32)run * 16.0f;
                inner.laser_new_arg_2 -= hit_length - len;
                hit_length = len;
                if (24.0f > len)
                {
                    pending_delete = 1;
                }
                do
                {
                    if (hit[j])
                    {
                        j++;
                        continue;
                    }
                    i32 start = j;
                    run = 0;
                    while (!hit[j])
                    {
                        j++;
                        run++;
                        if (j >= i)
                        {
                            break;
                        }
                    }
                    LaserLineInner params = inner;
                    params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                    if (params.laser_new_arg_1 > 24.0f)
                    {
                        params.start_pos = origin + step * (f32)start;
                        allocate_line_laser_inline(&params);
                    }
                } while (j < i);
            }
        }
    }
    return count;
}

// collision_test_circle_rect as LTCG inlined it into the sum_rect_damage
// variants.
static __forceinline i32 test_circle_rect_inline(f32 rect_x, f32 rect_y, f32 w, f32 h, f32 angle, f32 circle_x,
                                                 f32 circle_y, f32 radius)
{
    circle_x -= rect_x;
    circle_y -= rect_y;
    angle = -angle;
    f32 s = zun_sinf(angle);
    f32 c = zun_cosf(angle);
    f32 x = circle_x * c - circle_y * s;
    f32 y = circle_x * s + circle_y * c;
    f32 half_w = w * 0.5f;
    f32 abs_x = fabsf(x);
    if (half_w + radius >= abs_x && fabsf(y) <= h * 0.5f)
    {
        return 1;
    }
    if (half_w >= abs_x && fabsf(y) <= h * 0.5f + radius)
    {
        return 1;
    }
    // Then the corners.
    f32 half_h = h * 0.5f;
    f32 radius_sq = radius * radius;
    Float3 d;
    d.x = x - half_w;
    d.y = y - half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x + half_w;
    d.y = y - half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x - half_w;
    d.y = y + half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    d.x = x + half_w;
    d.y = y + half_h;
    if (radius_sq > offset_length_sq(&d))
    {
        return 1;
    }
    return 0;
}

// The first boss, as the sum_rect_damage variants look it up (inlined
// find_enemy_by_id).
static __forceinline EnemyInf *laser_boss()
{
    return g_EnemyManager->find_enemy_by_id(g_EnemyManager->inner.boss_ids[0]);
}

// The size of the sprite of the boss's first VM.
static __forceinline AnmLoadedSprite *laser_boss_sprite()
{
    AnmVm *vm = get_vm_or_clear(laser_boss()->enemy.anm_ids[0]);
    return &g_AnmManager->loaded_anms[vm->anm_loaded_index]->sprites[vm->sprite_id];
}

static_assert(offsetof(EnemyInf, enemy.anm_ids) == 0x1330, "EnemyInf::enemy.anm_ids");

// Never called. LaserInfiniteInf::sum_rect_damage for a straight laser: the boss
// is only tested when it exists, and the damage per point also depends on
// the laser's length, as in LaserCurveInf::sum_rect_damage.
// The seven dead locals are not ZUN's: the count of named variables decides
// MSVC's register choices here (period 8; docs/findings.md), and seven more
// come closest to the original. Matching only.
// TODO: register allocation differs as in LaserInfiniteInf::sum_rect_damage (the original keeps this in edi, size in esi).
// FUNCTION: TH16 0x434010
i32 LaserLineInf::sum_rect_damage(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    i32 unused_a = 0;
    i32 unused_b = 0;
    i32 unused_c = 0;
    i32 unused_d = 0;
    i32 unused_e = 0;
    i32 unused_f = 0;
    i32 unused_g = 0;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;
    (void)unused_d;
    (void)unused_e;
    (void)unused_f;
    (void)unused_g;
    Float3 *pos = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 diff = position - *pos;
    f32 s = zun_sinf(rect_angle);
    f32 cs = zun_cosf(rect_angle);
    f32 rx = diff.x * cs - diff.y * s;
    f32 ry = diff.y * cs + diff.x * s;
    Float3 local_step;
    laser_sincosmul(&local_step, wrap_angle(angle + rect_angle), 8.0f);
    f32 half_w = size->x * 0.5f;
    local_step.z = 0.0f;
    f32 half_h = size->y * 0.5f;
    f32 local_x = local_step.x + rx;
    local_step.x += local_step.x;
    f32 local_y = local_step.y + ry;
    local_step.y += local_step.y;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    f32 world_y = position.y + step.y;
    f32 world_x = position.x + step.x;
    step.y += step.y;
    step.x += step.x;
    step.z = 0.0f;
    u8 hit[0x100];
    u8 *h = hit;
    for (; dist + 8.0f <= hit_length; dist += 16.0f, h++)
    {
        if (*boss_hit == 0 && laser_boss() != NULL)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, world_x, world_y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            *h = 1;
            i32 damage = 15;
            if (length >= 12.0f)
            {
                damage = 45;
            }
            else if (length >= 4.0f && 12.0f > length)
            {
                damage = (i32)(((length - 4.0f) / 8.0f * 2.0f + 1.0f) * 15.0f);
            }
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->rect_damage_sum += damage;
        }
        world_y += step.y;
        world_x += step.x;
        local_x += local_step.x;
        local_y += local_step.y;
    }
    return count;
}

// Never called. Walks the laser in steps of 16 units: while *boss_hit is
// clear, sets it once a point is within 8 units of the boss's sprite (at
// three quarters size). Counts the points inside the rectangle at pos (size,
// turned by rect_angle) and adds a damage value by laser width for each to
// g_LaserManager->rect_damage_sum. Its own points move only 8 units per step in
// the rectangle's frame. The parameters are pos, size, rect_angle, unused,
// e (skip while ex_invuln_remaining_frames runs) and boss_hit.
// TODO: register allocation differs (the original keeps this in edi, size in esi); it multiplies the sprite sizes before zun_sinf.
// FUNCTION: TH16 0x436010
i32 LaserInfiniteInf::sum_rect_damage(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    Float3 *pos = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    f32 dist = 8.0f;
    i32 count = 0;
    Float3 diff = position - *pos;
    f32 s = zun_sinf(rect_angle);
    f32 cs = zun_cosf(rect_angle);
    f32 rx = diff.x * cs - diff.y * s;
    f32 ry = diff.y * cs + diff.x * s;
    Float3 local_step;
    laser_sincosmul(&local_step, wrap_angle(angle + rect_angle), 8.0f);
    f32 local_x = local_step.x + rx;
    f32 local_y = local_step.y + ry;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_step.z = 0.0f;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    f32 world_y = position.y + step.y;
    f32 world_x = position.x + step.x;
    step.y += step.y;
    step.x += step.x;
    step.z = 0.0f;
    u8 hit[0x100];
    u8 *h = hit;
    for (; dist + 8.0f <= hit_length; dist += 16.0f, h++)
    {
        if (*boss_hit == 0)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, world_x, world_y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            *h = 1;
            i32 damage = 18;
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->rect_damage_sum += damage;
        }
        local_x += local_step.x;
        world_x += step.x;
        local_y += local_step.y;
        world_y += step.y;
    }
    return count;
}

// Never called. LaserInfiniteInf::sum_rect_damage for the segments of a curvy
// laser: sets *boss_hit once a segment is within 8 units of the boss's
// sprite, and counts the segments inside the rectangle, adding a damage
// value by laser length and width for each to g_LaserManager->rect_damage_sum.
// TODO: as LaserInfiniteInf::sum_rect_damage; ours also keeps g_EnemyManager in edi across the loop where the original reloads it.
// FUNCTION: TH16 0x439d60
i32 LaserCurveInf::sum_rect_damage(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
    Float3 *center = (Float3 *)a;
    Float3 *size = (Float3 *)b;
    f32 rect_angle = *(f32 *)&c;
    i32 *boss_hit = (i32 *)f;
    if (e != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    i32 count = 0;
    LaserCurveSegment *segment = (LaserCurveSegment *)segments;
    for (i32 i = 0; i < inner.segment_count; i++, segment++)
    {
        Float3 seg_pos = segment->pos;
        if (*boss_hit == 0)
        {
            if (test_circle_rect_inline(laser_boss()->enemy.final_pos.pos.x, laser_boss()->enemy.final_pos.pos.y,
                                        laser_boss_sprite()->sprite_width * 0.75f,
                                        laser_boss_sprite()->sprite_height * 0.75f, 0.0f, seg_pos.x, seg_pos.y, 8.0f))
            {
                *boss_hit = 1;
            }
        }
        f32 half_w = size->x;
        f32 half_h = size->y;
        f32 dx = seg_pos.x - center->x;
        f32 dy = seg_pos.y - center->y;
        if (rect_angle != 0.0f)
        {
            f32 neg_angle = -rect_angle;
            f32 s = zun_sinf(neg_angle);
            f32 cs = zun_cosf(neg_angle);
            f32 rx = dx * cs - dy * s;
            dy = dy * cs + dx * s;
            dx = rx;
        }
        if (half_w * 0.5f >= (f32)fabs(dx) && half_h * 0.5f >= (f32)fabs(dy))
        {
            count++;
            i32 damage = 15;
            if (length >= 12.0f)
            {
                damage = 45;
            }
            else if (length >= 4.0f && 12.0f > length)
            {
                damage = (i32)(((length - 4.0f) / 8.0f * 2.0f + 1.0f) * 15.0f);
            }
            if (width >= 96.0f)
            {
                damage = (i32)(damage + 3.0f + 1.0f);
            }
            else if (width >= 16.0f && 96.0f > width)
            {
                damage = (i32)((width - 16.0f) / 80.0f * 3.0f + damage + 1.0f);
            }
            g_LaserManager->rect_damage_sum += damage;
        }
    }
    return count;
}

// Cancels the points (every 16 units) inside a bomb's circle. A hit head
// shortens the laser to nothing, otherwise it ends at the first hit run;
// every later unhit run that starts on screen becomes a straight laser.
// Returns the number of points hit.
// i is zeroed before the memset, whose 0 then comes from i's register, and step.z
// is stored before the sincosmul call, as in the original.
// TODO: the original lays pos.z's shadow out after step in the frame; the run loops' register use and the params copy differ (g_LaserManager kept in a stack slot).
// FUNCTION: TH16 0x436670
i32 LaserInfiniteInf::cancel_as_bomb_circle(Float3 *center, f32 radius, i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    i32 i = 0;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    Float3 step;
    step.z = 0.0f;
    laser_sincosmul(&step, angle, 8.0f);
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    radius = radius * radius;
    for (; hit_length > dist + 8.0f; i++)
    {
        if (!((center->x - pos.x) * (center->x - pos.x) + (center->y - pos.y) * (center->y - pos.y) > radius))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (!(pos.x + 32.0f <= -192.0f || pos.x - 32.0f >= 192.0f || pos.y + 32.0f <= 0.0f ||
                  pos.y - 32.0f >= 448.0f))
            {
                if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                }
                else if (bullet_type <= 0x1f || bullet_type == 0x1b)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0);
                }
                else if (bullet_type <= 0x21)
                {
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0);
                }
            }
        }
        pos += step;
        dist += 16.0f;
    }
    if (count != 0)
    {
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            hit_length = 0.0f;
        }
        else
        {
            i32 run = 0;
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                hit_length = (f32)run * 16.0f;
            }
        }
        while (j < i)
        {
            if (hit[j])
            {
                j++;
                continue;
            }
            i32 run = 0;
            i32 start = j;
            while (!hit[j])
            {
                j++;
                run++;
                if (j >= i)
                {
                    break;
                }
            }
            f32 start_f = (f32)start;
            pos = origin + step * start_f;
            if (!(pos.x + 32.0f <= -192.0f || pos.x - 32.0f >= 192.0f || pos.y + 32.0f <= 0.0f ||
                  pos.y - 32.0f >= 448.0f))
            {
                LaserLineInner params;
                params.start_pos = pos;
                params.speed = 8.0f;
                params.bullet_type = inner.type;
                params.bullet_color = inner.color;
                params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                params.ang_aim = angle;
                params.laser_new_arg_4 = width;
                params.laser_new_arg_3 = inner.laser_new_arg_2 - start_f * 16.0f;
                allocate_line_laser_inline(&params);
            }
        }
    }
    return count;
}

// FUNCTION: TH16 0x4357a0
i32 LaserInfiniteInf::on_draw()
{
    i32 i = 0;
    vm_950.pos = position;
    f32 rotation = angle + ZUN_PI / 2;
    while (rotation > ZUN_PI)
    {
        rotation -= ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    while (rotation < -ZUN_PI)
    {
        rotation += ZUN_2PI;
        if (i++ > 32)
        {
            break;
        }
    }
    AnmVm *vm = &vm_950;
    vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
    vm->rotation.z = rotation;
    g_AnmManager->draw_vm(vm);
    if (unk_7c == 0.0f)
    {
        vm_f4c.pos = position;
        g_AnmManager->draw_vm(&vm_f4c);
    }
    return 0;
}

// Hits or grazes the player: a hit cancels the laser around the player, a
// graze counts every third frame at the point of the laser nearest the
// player.
// FUNCTION: TH16 0x433510
i32 LaserLineInf::check_graze_or_kill(i32 graze_only)
{
    if (hit_length > 16.0f && width > 3.0f)
    {
        Float3 start;
        if (!(inner.flags & 2))
        {
            laser_sincosmul(&start, angle, hit_length / 10.0f);
            D3DXVec3Add(&start, &start, &position);
        }
        else
        {
            start = position;
        }
        f32 length = hit_length;
        if (!(inner.flags & 2))
        {
            length = length * 4.0f / 5.0f;
        }
        f32 w = width;
        if (32.0f > w)
        {
            w = w * 0.5f;
        }
        else
        {
            w = w - (w + 16.0f) * 0.5f;
        }
        i32 result = g_Player->check_hit_rotated_rect(&start, angle, w, length, graze_only);
        if (result == 1)
        {
            Float3 size(32.0f, 32.0f, 0.0f);
            cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            return 0;
        }
        if (result == 2)
        {
            if (graze_timer.current % 3 == 0)
            {
                f32 x;
                f32 y;
                collision_line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x,
                                            g_Player->inner.pos.y, normalize_angle(angle + ZUN_PI / 2));
                start.x = x;
                start.y = y;
                g_Player->do_graze(&start);
            }
            graze_timer++;
        }
    }
    return 0;
}

// The same for infinite lasers, once they are out (states 2 and 4).
// FUNCTION: TH16 0x435610
i32 LaserInfiniteInf::check_graze_or_kill(i32 graze_only)
{
    if ((state == LASER_STATE_EXPANDING || state == LASER_STATE_ACTIVE) && hit_length > 16.0f)
    {
        Float3 start = position;
        f32 w = width;
        if (32.0f > w)
        {
            w = w * 0.5f;
        }
        else
        {
            w = w - (w + 16.0f) / 3.0f;
        }
        i32 result = g_Player->check_hit_rotated_rect(&start, angle, w, hit_length * 0.9f, graze_only);
        if (result == 1)
        {
            Float3 size(32.0f, 32.0f, 0.0f);
            cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            return 0;
        }
        if (result == 2)
        {
            if (graze_timer.current % 3 == 0)
            {
                f32 x;
                f32 y;
                collision_line_intersection(&x, &y, start.x, start.y, angle, g_Player->inner.pos.x,
                                            g_Player->inner.pos.y, normalize_angle(angle + ZUN_PI / 2));
                start.x = x;
                start.y = y;
                g_Player->do_graze(&start);
            }
            graze_timer++;
        }
    }
    return 0;
}

// The same for curvy lasers, piece by piece past the first 16 units; one
// graze per frame at most.
// TODO: after the hit test the original reloads 0.5 and dist at the loop join, ours at the start of the result == 2 test (nested if or continue forms do not help).
// FUNCTION: TH16 0x437cf0
i32 LaserCurveInf::check_graze_or_kill(i32 graze_only)
{
    i32 grazed = 0;
    f32 dist = 0.0f;
    Float3 graze_pos;
    LaserCurveSegment *segment = (LaserCurveSegment *)segments;
    for (i32 i = 0; i < inner.segment_count - 1; i++, segment++)
    {
        Float3 mid;
        laser_sincosmul(&mid, segment->angle, segment->length * 0.5f);
        mid += segment->pos;
        dist += segment->length;
        if (dist >= 16.0f)
        {
            i32 result = g_Player->check_hit_rotated_rect(&mid, segment->angle, width * 0.5f, segment->length, graze_only);
            if (result == 1)
            {
                Float3 size(32.0f, 32.0f, 0.0f);
                cancel_as_bomb_rectangle(&g_Player->inner.pos, &size, 0.0f, 0, 1);
            }
            else if (result == 2 && !grazed && graze_timer.current % 3 == 0)
            {
                graze_pos = mid;
                grazed = 1;
            }
        }
    }
    if (grazed)
    {
        g_Player->do_graze(&graze_pos);
    }
    graze_timer.tick_split();
    return 0;
}

// cancel_as_bomb_circle for a bomb's rectangle (center, size, rotated by
// rect_angle): the points are tested in the rectangle's frame.
// TODO: register allocation differs throughout (the original keeps this in esi and copies center and size to locals first).
// FUNCTION: TH16 0x433860
i32 LaserLineInf::cancel_as_bomb_rectangle(Float3 *center, Float3 *size, f32 rect_angle, i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    Float3 diff = position - *center;
    f32 neg_angle = -rect_angle;
    f32 s = zun_sinf(neg_angle);
    f32 c = zun_cosf(neg_angle);
    f32 local_y = diff.y * c + diff.x * s;
    f32 local_x = diff.x * c - diff.y * s;
    i32 n = 0;
    f32 local_angle = angle - rect_angle;
    while (local_angle > ZUN_PI)
    {
        local_angle -= ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    while (local_angle < -ZUN_PI)
    {
        local_angle += ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    Float3 local_step;
    laser_sincosmul(&local_step, local_angle, 8.0f);
    local_y += local_step.y;
    local_step.z = 0.0f;
    local_step.y += local_step.y;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_x += local_step.x;
    local_step.x += local_step.x;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    i32 i;
    for (i = 0; hit_length >= dist + 8.0f; i++)
    {
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                AnmId id = g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                g_EffectManager->track_inline(id);
            }
            else if (bullet_type <= 0x1e)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x101, &pos, 0.0f, -1, 0));
            }
            else if (bullet_type <= 0x21)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.bullet_color * 2 + 0x119, &pos, 0.0f, -1, 0));
            }
        }
        pos += step;
        local_x += local_step.x;
        local_y += local_step.y;
        dist += 16.0f;
    }
    if (count != 0)
    {
        if (count >= i)
        {
            pending_delete = 1;
            return count;
        }
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            position += step * (f32)j;
            hit_length -= (f32)j * 16.0f;
            if (!(hit_length > 24.0f))
            {
                pending_delete = 1;
                return count;
            }
            inner.laser_new_arg_2 = hit_length;
            unk_7c = (f32)j * 16.0f;
        }
        i32 run = 0;
        if (j < i)
        {
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                f32 len = (f32)run * 16.0f;
                inner.laser_new_arg_2 -= hit_length - len;
                hit_length = len;
                if (24.0f > len)
                {
                    pending_delete = 1;
                }
                do
                {
                    if (hit[j])
                    {
                        j++;
                        continue;
                    }
                    i32 start = j;
                    run = 0;
                    while (!hit[j])
                    {
                        j++;
                        run++;
                        if (j >= i)
                        {
                            break;
                        }
                    }
                    LaserLineInner params = inner;
                    params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
                    if (params.laser_new_arg_1 > 24.0f)
                    {
                        params.start_pos = origin + step * (f32)start;
                        allocate_line_laser_inline(&params);
                    }
                } while (j < i);
            }
        }
    }
    return count;
}

// cancel_as_bomb_circle for a bomb's rectangle, tested in the rectangle's
// frame. The pieces after the first hit run become straight lasers.
// TODO: register allocation differs throughout, as in LaserLineInf::cancel_as_bomb_rectangle.
// FUNCTION: TH16 0x435880
i32 LaserInfiniteInf::cancel_as_bomb_rectangle(Float3 *center, Float3 *size, f32 rect_angle, i32 mode, i32 skip_invuln)
{
    if (skip_invuln != 0 && ex_invuln_remaining_frames != 0)
    {
        return 0;
    }
    Float3 origin = position;
    i32 count = 0;
    f32 dist = 8.0f;
    u8 hit[0x100];
    memset(hit, 0, sizeof(hit));
    f32 dx = position.x - center->x;
    f32 dy = position.y - center->y;
    f32 neg_angle = -rect_angle;
    f32 s = zun_sinf(neg_angle);
    f32 c = zun_cosf(neg_angle);
    f32 local_x = dx * c - dy * s;
    f32 local_y = dy * c + dx * s;
    i32 n = 0;
    f32 local_angle = angle - rect_angle;
    while (local_angle > ZUN_PI)
    {
        local_angle -= ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    while (local_angle < -ZUN_PI)
    {
        local_angle += ZUN_2PI;
        if (n++ > 32)
        {
            break;
        }
    }
    Float3 local_step;
    laser_sincosmul(&local_step, local_angle, 8.0f);
    local_y += local_step.y;
    local_step.z = 0.0f;
    local_step.y += local_step.y;
    f32 half_w = size->x * 0.5f;
    f32 half_h = size->y * 0.5f;
    local_x += local_step.x;
    local_step.x += local_step.x;
    Float3 step;
    laser_sincosmul(&step, angle, 8.0f);
    step.z = 0.0f;
    Float3 pos;
    pos = position + step;
    pos.z = 0.0f;
    step.x += step.x;
    step.y += step.y;
    step.z += step.z;
    i32 i;
    for (i = 0; hit_length >= dist + 8.0f; i++)
    {
        if (!(-half_w > local_x || local_x > half_w || -half_h > local_y || local_y > half_h))
        {
            count++;
            hit[i] = 1;
            gen_items_from_cancel(&pos, mode);
            if (bullet_type <= 0x11 || bullet_type == 0x22 || bullet_type == 0x26)
            {
                AnmId id = g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0xd1, &pos, 0.0f, -1, 0);
                g_EffectManager->track_inline(id);
            }
            else if (bullet_type <= 0x1f || bullet_type == 0x1b)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x101, &pos, 0.0f, -1, 0));
            }
            else if (bullet_type <= 0x21)
            {
                g_EffectManager->track(
                    g_BulletManager->bullet_anm->create_vm(inner.color * 2 + 0x119, &pos, 0.0f, -1, 0));
            }
        }
        pos += step;
        local_x += local_step.x;
        local_y += local_step.y;
        dist += 16.0f;
    }
    if (count != 0)
    {
        i32 j;
        for (j = 0; j < i; j++)
        {
            if (!hit[j])
            {
                break;
            }
        }
        if (j != 0)
        {
            hit_length = 0.0f;
        }
        else
        {
            i32 run = 0;
            for (; j < i; j++, run++)
            {
                if (hit[j])
                {
                    break;
                }
            }
            if (j < i)
            {
                hit_length = (f32)run * 16.0f;
            }
        }
        while (j < i)
        {
            if (hit[j])
            {
                j++;
                continue;
            }
            i32 run = 0;
            i32 start = j;
            while (!hit[j])
            {
                j++;
                run++;
                if (j >= i)
                {
                    break;
                }
            }
            LaserLineInner params;
            params.speed = 8.0f;
            params.distance = 0.0f;
            params.shot_sfx = -1;
            params.shot_transform_sfx = -1;
            params.laser_new_arg_2 = params.laser_new_arg_1 = (f32)run * 16.0f;
            params.start_pos = origin + step * (f32)start;
            params.ang_aim = angle;
            params.bullet_type = inner.type;
            params.laser_new_arg_4 = width;
            params.bullet_color = inner.color;
            params.laser_new_arg_3 = inner.laser_new_arg_2 - (f32)start * 16.0f;
            params.flags ^= (params.flags ^ (inner.flags >> 1)) & 1;
            allocate_line_laser_inline(&params);
        }
    }
    return count;
}

// Sets the laser up from its parameters: the body and its origin VM, the
// shot sound, and the start offset along the aim.
// The dead double is not ZUN's code: as in LaserLineInf::initialize, it
// makes LTCG realign the frame (and esp, -8) like the original.
// FUNCTION: TH16 0x435050
i32 LaserInfiniteInf::initialize(void *params)
{
    double unused = 0.0;
    (void)unused;
    inner = *(LaserInfiniteInner *)params;
    bullet_type = inner.type;
    state = LASER_STATE_WARNING;
    kind = LASER_INFINITE;
    bullet_color = inner.color;
    AnmVm *vm = &vm_950;
    vm->wipe();
    vm_950.index_of_sprite_mapping_func = ANM_SPRITE_MAPPING_LASER_LINE;
    vm_950.associated_game_entity = this;
    g_LaserManager->bullet_anm->set_vm_script(vm, g_bullet_types[bullet_type].script);
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_950.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_950.flags_hi = vm_950.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f4c;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.color + 0x38);
    vm->parent_vm = NULL;
    vm->root_vm = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f4c.flags_lo)->render_mode = 1;
    vm_f4c.flags_hi = vm_f4c.flags_hi & ~0x80000 | 0x40000;
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        position.x += offset.x;
        position.y += offset.y;
    }
    hit_length = inner.laser_new_arg_1;
    length = inner.speed;
    angle = inner.ang_aim;
    ex_index = *(i32 *)inner.unk_50;
    width = 2.0f;
    id = inner.laser_st_on_arg_1;
    graze_timer.reset();
    unk_94c = 0;
    return 0;
}

// Sets a beam up from its parameters.
// TODO: in the inlined wipe the original schedules the flags_hi and/or one store later (an instruction scheduling difference only).
// FUNCTION: TH16 0x43a860
i32 LaserBeamInf::initialize(void *params)
{
    inner = *(LaserBeamInner *)params;
    position = inner.start_pos;
    hit_length = inner.length;
    angle = inner.ang_aim;
    bullet_color = inner.color;
    state = LASER_STATE_WARNING;
    kind = LASER_BEAM;
    id = inner.id;
    for (i32 i = 0; i < 0x200; i++)
    {
        unk_f28[i] = hit_length;
    }
    width = 1.0f;
    unk_f24 = 0;
    vm_928.wipe();
    AnmVm *vm = &vm_928;
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    return 0;
}

// Sets a curvy laser up from its parameters: the body and origin VMs, the
// segment buffers (every segment at the start), the start offset along the
// aim, and the node list: a copy of the source laser's when a bomb split
// this one off (et_ex then skipped), else one straight node. Then places
// the segments for the starting time.
// Effective match: the original stores inner.distance = 0 between the position's x add and its store.
// FUNCTION: TH16 0x4370a0
i32 LaserCurveInf::initialize(void *params)
{
    inner = *(LaserCurveInner *)params;
    bullet_type = inner.type;
    state = LASER_STATE_ACTIVE;
    kind = LASER_CURVE;
    bullet_color = inner.color;
    AnmVm *vm = &vm_92c;
    vm->wipe();
    if (bullet_type == 1)
    {
        g_LaserManager->bullet_anm->set_vm_script(vm, 0x142);
    }
    else
    {
        vm_92c.index_of_sprite_mapping_func = ANM_SPRITE_MAPPING_LASER_CURVE;
        vm_92c.associated_game_entity = this;
        g_LaserManager->bullet_anm->set_vm_script(vm, bullet_type + 0x8e);
    }
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_92c.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_92c.flags_hi = vm_92c.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f28;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.color + 0x38);
    vm->parent_vm = NULL;
    vm->root_vm = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f28.flags_lo)->render_mode = 1;
    vm_f28.flags_hi = vm_f28.flags_hi & ~0x80000 | 0x40000;
    vertices = malloc(inner.segment_count * 0x38);
    segments = malloc(inner.segment_count * sizeof(LaserCurveSegment));
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        // Through pointers to the components: the adds then load the
        // position and add the offset from memory, as in the original.
        f32 *px = &position.x;
        *px += offset.x;
        f32 *py = &position.y;
        *py += offset.y;
        inner.start_pos = position;
        inner.distance = 0.0f;
    }
    width = inner.laser_new_arg_4;
    angle = inner.ang_aim;
    length = inner.speed;
    laser_sincosmul(&tip_offset, angle, length);
    tip_offset.z = 0.0f;
    for (i32 i = 0; i < inner.segment_count; i++)
    {
        ((LaserCurveSegment *)segments)[i].pos = position;
        *(Float3 *)((LaserCurveSegment *)segments)[i].unk_c = g_zero_vec;
        ((LaserCurveSegment *)segments)[i].angle = inner.ang_aim;
        ((LaserCurveSegment *)segments)[i].length = inner.speed;
    }
    segment_timer.set_f(inner.source_time);
    if (inner.source_nodes != NULL)
    {
        nodes = *inner.source_nodes;
        LaserCurveNode *dst = &nodes;
        for (LaserCurveNode *src = inner.source_nodes; src != NULL; src = src->next)
        {
            if (src->next != NULL)
            {
                dst->next = new LaserCurveNode;
                *dst->next = *src->next;
                dst = dst->next;
            }
        }
        inner.source_nodes = NULL;
        ex_index = 99;
    }
    else
    {
        nodes.next = NULL;
        nodes.speed = length;
        // Stored through a pointer so that angle is loaded again for the
        // velocity below, as in the original.
        f32 *node_angle = &nodes.angle;
        *node_angle = wrap_angle(angle);
        laser_sincosmul(&nodes.velocity, angle, 1.0f);
        nodes.start_pos = position;
        nodes.velocity.z = 0.0f;
        nodes.mode = 0;
        nodes.start_time = 0.0f;
        nodes.end_time = 999999.0f;
        ex_index = *(i32 *)inner.unk_34c;
    }
    *(Float3 *)((LaserCurveSegment *)segments)->unk_c = tip_offset;
    for (i32 i = 0; i < inner.segment_count; i++)
    {
        // prev_pos indexed from segments again (not segment[-1]): segments is
        // then loaded before i is scaled, as in the original.
        LaserCurveSegment *segment = &((LaserCurveSegment *)segments)[i];
        Float3 *prev_pos = &((LaserCurveSegment *)segments)[i - 1].pos;
        f32 *out_length = &segment->length;
        f32 prev_length = segment[-1].length;
        f32 prev_angle = segment[-1].angle;
        f32 *out_angle = &segment->angle;
        f32 t = segment_timer.current_f - (f32)i;
        for (LaserCurveNode *node = &nodes; node != NULL; node = node->next)
        {
            if (t >= node->start_time && node->end_time > t)
            {
                if (i == 0)
                {
                    node->get_state(&segment->pos, out_length, out_angle, t);
                }
                else
                {
                    node->step_back(&segment->pos, out_length, out_angle, prev_pos, prev_length, prev_angle, t);
                }
                break;
            }
        }
    }
    offscreen_grace.set_inline(30);
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    graze_timer.reset();
    return 0;
}

static_assert(offsetof(AnmVm, pos) == 0x2c, "AnmVm::pos");
static_assert(offsetof(AnmVm, uv_quad_of_sprite) == 0x3a8, "AnmVm::uv_quad_of_sprite");

// The angle halfway from cur to prev, going the short way round.
static __forceinline f32 laser_mid_angle(f32 cur, f32 prev)
{
    f32 d;
    if (prev - cur > ZUN_PI)
    {
        d = prev - (cur + ZUN_2PI);
    }
    else if (cur - prev > ZUN_PI)
    {
        d = prev - (cur - ZUN_2PI);
    }
    else
    {
        d = prev - cur;
    }
    d = wrap_angle(d);
    d = wrap_angle(d * 0.5f);
    return wrap_angle(d + cur);
}

// Draws the body as a triangle strip: two vertices per segment, half the
// laser's width to each side across the segment's direction (averaged with
// the previous segment's), with u running from 0 to 1 along the laser. The
// origin VM sits on the last segment until the whole laser is out.
// FUNCTION: TH16 0x438750
i32 LaserCurveInf::on_draw()
{
    f32 u = 0.0f;
    RenderVertex144 *vertex = (RenderVertex144 *)vertices;
    LaserCurveSegment *segment = (LaserCurveSegment *)segments;
    for (i32 i = 0; i < inner.segment_count; i++, segment++, vertex++)
    {
        vertex->pos.w = 1.0f;
        vertex->diffuse = 0xffffffff;
        vertex->uv.x = u;
        vertex->uv.y = vm_92c.uv_quad_of_sprite[0].y;
        f32 a;
        if (i == 0)
        {
            a = wrap_angle(segment->angle + ZUN_PI / 2);
        }
        else
        {
            f32 cur = wrap_angle(segment->angle + ZUN_PI / 2);
            a = laser_mid_angle(cur, wrap_angle(segment[-1].angle + ZUN_PI / 2));
        }
        laser_sincosmul((Float3 *)&vertex->pos, a, inner.laser_new_arg_4 * 0.5f);
        // x and y as a vector add, z through a pointer to the segment's z
        // (one per vertex): the original loads the segment's z and adds the
        // vertex's.
        D3DXVec2Add((Float2 *)&vertex->pos, (Float2 *)&segment->pos, (Float2 *)&vertex->pos);
        {
            f32 *segment_z = &segment->pos.z;
            vertex->pos.z = *segment_z + vertex->pos.z;
        }
        vertex->pos.x += (f32)g_game_2d_origin_x;
        vertex->pos.y += (f32)g_early_arcade_offset_y;
        vertex->pos.z = 0.0f;
        vertex++;
        vertex->pos.w = 1.0f;
        vertex->diffuse = 0xffffffff;
        vertex->uv.x = u;
        vertex->uv.y = vm_92c.uv_quad_of_sprite[2].y;
        if (i == 0)
        {
            a = wrap_angle(segment->angle - ZUN_PI / 2);
        }
        else
        {
            f32 cur = wrap_angle(segment->angle - ZUN_PI / 2);
            a = laser_mid_angle(cur, wrap_angle(segment[-1].angle - ZUN_PI / 2));
        }
        laser_sincosmul((Float3 *)&vertex->pos, a, inner.laser_new_arg_4 * 0.5f);
        D3DXVec2Add((Float2 *)&vertex->pos, (Float2 *)&segment->pos, (Float2 *)&vertex->pos);
        {
            f32 *segment_z = &segment->pos.z;
            vertex->pos.z = *segment_z + vertex->pos.z;
        }
        vertex->pos.x += (f32)g_game_2d_origin_x;
        vertex->pos.y += (f32)g_early_arcade_offset_y;
        vertex->pos.z = 0.0f;
        u += 1.0f / (f32)(inner.segment_count - 1);
    }
    g_AnmManager->draw_vertex_strip(&vm_92c, (RenderVertex144 *)vertices, inner.segment_count * 2);
    if (inner.segment_count >= segment_timer.current)
    {
        vm_f28.pos = ((LaserCurveSegment *)segments)[inner.segment_count - 1].pos;
        g_AnmManager->draw_vm(&vm_f28);
    }
    return 0;
}

// Runs the laser's pending et_ex instructions, as LaserLineInf::run_ex
// does, except that 4 and 8 add nodes to the curve: one moving by velocity
// or by speed and angle deltas from the given time, followed by a straight
// one after a duration (a) unless that is negative. An instruction already
// running stops the list.
// TODO: the original keeps ex as a pointer (this + 0x600 + index * 0x2c) where ours addresses through this plus the scaled index; case 4 stores start_pos.z after loading the angle, and (f32)b goes to xmm1.
// FUNCTION: TH16 0x438cb0
void LaserCurveInf::run_ex()
{
    while (ex_index < 0x12)
    {
        BulletEx *ex = &inner.ex[ex_index];
        if (ex->type == 0)
        {
            return;
        }
        if (ex->slot == 0 && ex_flags != 0)
        {
            return;
        }
        if (ex->type & ex_flags)
        {
            return;
        }
        switch ((u32)ex->type)
        {
        case BULLET_EX_SPEEDUP:
            ex_flags |= BULLET_EX_SPEEDUP;
            ex_state[0].timer.set_value(0);
            ex_state[0].floats[7] = 0.0f;
            break;
        case BULLET_EX_ACCEL:
        {
            LaserCurveNode *node = append_node((f32)ex->b);
            node->mode = 1;
            node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->end_time);
            node->start_pos.z = 0.0f;
            laser_sincosmul(&node->velocity, node->angle, 1.0f);
            node->velocity.z = 0.0f;
            node->speed_delta = ex->r;
            node->angle_delta = ex->s;
            if (ex->a >= 0)
            {
                node->end_time = (f32)ex->a + (f32)ex->b;
                node = append_node(node->end_time);
                node->mode = 0;
                node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->end_time);
                node->start_pos.z = 0.0f;
                laser_sincosmul(&node->velocity, node->angle, 1.0f);
                node->velocity.z = 0.0f;
                node->end_time = 999999.0f;
            }
            else
            {
                node->end_time = 999999.0f;
            }
            break;
        }
        case BULLET_EX_ANGLE_ACCEL:
        {
            LaserCurveNode *node = append_node((f32)ex->b);
            node->mode = 2;
            node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->end_time);
            node->start_pos.z = 0.0f;
            laser_sincosmul(&node->velocity, node->angle, 1.0f);
            node->velocity.z = 0.0f;
            node->speed_delta = ex->r;
            node->angle_delta = ex->s;
            if (ex->a >= 0)
            {
                node->end_time = (f32)ex->a + (f32)ex->b;
                node = append_node(node->end_time);
                node->mode = 0;
                node->prev->get_state(&node->start_pos, &node->speed, &node->angle, node->prev->end_time);
                node->start_pos.z = 0.0f;
                laser_sincosmul(&node->velocity, node->angle, 1.0f);
                node->velocity.z = 0.0f;
                node->end_time = 999999.0f;
            }
            else
            {
                node->end_time = 999999.0f;
            }
            break;
        }
        case BULLET_EX_ANGLE:
            ex_flags |= ex->type;
            ex_state[3].floats[1] = ex->r;
            ex_state[3].floats[0] = ex->s > -999.0f ? ex->s : length;
            ex_state[3].timer.set_value(0);
            ex_state[3].ints[0] = ex->a;
            ex_state[3].ints[1] = ex->b;
            ex_state[3].ints[2] = 0;
            ex_state[3].ints[3] = ex->c;
            break;
        case BULLET_EX_BOUNCE:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                if (ex->r >= 0.0f)
                {
                    ex_state[4].floats[0] = ex->r;
                }
                else
                {
                    ex_state[4].floats[0] = length;
                }
                ex->a--;
                ex_state[4].ints[1] = ex->a;
                ex_state[4].ints[0] = 0;
                ex_state[4].ints[2] = ex->b;
            }
            break;
        case BULLET_EX_INVULN:
            ex_invuln_remaining_frames = ex->a;
            break;
        case BULLET_EX_OFFSCREEN:
            ex_flags |= ex->type;
            ex_state[11].timer.set_inline(ex->a);
            ex_state[11].ints[0] = ex->b;
            break;
        case BULLET_EX_SET_SPRITE:
        {
            AnmVm *vm = &vm_92c;
            g_BulletManager->bullet_anm->copy_vm(vm, g_bullet_types[ex->a].script + ex->b);
            vm->parent_vm = NULL;
            vm->root_vm = NULL;
            vm->run();
            break;
        }
        case BULLET_EX_DELETE:
            state = LASER_STATE_WARNING;
            break;
        case BULLET_EX_PLAY_SOUND:
            g_SoundManager.play_sound_at_position(ex->a, position.x);
            break;
        case BULLET_EX_WRAP:
            ex_flags |= ex->type;
            ex_state[6].timer.set_value(ex->a);
            break;
        case BULLET_EX_SHOOT:
        {
            EnemyBulletShooter shooter;
            laser_sincosmul(&shooter.pos, angle, hit_length);
            u32 a = ex->a;
            shooter.pos.x += position.x;
            shooter.pos.z = 0.0f;
            *(u16 *)&shooter.aim_type = (a >> 24) & 0x7f;
            shooter.type = (a >> 16) & 0xff;
            shooter.pos.y += position.y;
            shooter.color = (a >> 8) & 0xff;
            shooter.spd1 = ex->r;
            shooter.spd2 = ex->s;
            shooter.start_transform = a & 0xff;
            shooter.count = ex->b;
            ex_index++;
            shooter.layers = ex[1].a;
            shooter.ang_aim = ex[1].r;
            shooter.sfx_flags = ex[1].b;
            shooter.ang_bullet_dist = ex[1].s;
            memcpy(shooter.ex, inner.ex, sizeof(inner.ex));
            g_BulletManager->shoot_bullets(&shooter);
            ex_index++;
            if ((i32)a < 0)
            {
                cancel(0, 0);
                break;
            }
            continue;
        }
        case BULLET_EX_TAG:
            id = ex->a;
            ex_index++;
            continue;
        case BULLET_EX_LOOP:
            ex_index = ex->a;
            continue;
        case BULLET_EX_BLEND:
            if (ex->a != 0)
            {
                vm_92c.flags_lo = vm_92c.flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
            }
            else
            {
                vm_92c.flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
            }
            break;
        case BULLET_EX_FREEZE_SEGMENTS:
            ((LaserDataFlagBits *)(&next + 1))->segments_frozen = ex->a;
            break;
        case BULLET_EX_WAIT:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                ex_state[5].timer.set_value(ex->a);
                break;
            }
            ex_index++;
            continue;
        }
        ex_index++;
    }
}

// Steps a segment back from the previous one's position (pos, speed, angle)
// along the node's motion. In mode 2 the whole part of t is kept as the
// double floor returns: the original converts it with cvtpd2ps.
// The six dead locals are not ZUN's: MSVC orders the x component loads by
// the function's count of named variables (period 8; docs/findings.md), and
// these give the original's dt copy and, with D3DXVec3Add(&sum, &a, &b), the
// x operands of the sum. Matching only.
// TODO: mode 1 still squares sum.y into the register it adds to where the original squares sum.x (the operand order of the sum, a length-squared local, offset_length_sq, D3DXVec2Length and field-wise sums do not change it).
// FUNCTION: TH16 0x438370
void LaserCurveNode::step_back(Float3 *out_pos, f32 *out_speed, f32 *out_angle, Float3 *pos, f32 speed, f32 angle,
                               f32 t)
{
    i32 unused_a = 0;
    i32 unused_b = 0;
    i32 unused_c = 0;
    i32 unused_d = 0;
    i32 unused_e = 0;
    i32 unused_f = 0;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;
    (void)unused_d;
    (void)unused_e;
    (void)unused_f;
    switch (mode)
    {
    case 0:
        *out_pos = *pos - velocity * this->speed;
        *out_speed = this->speed;
        *out_angle = this->angle;
        break;
    case 1:
        if (-990.0f > angle_delta)
        {
            f32 dt = speed - speed_delta;
            *out_pos = *pos - velocity * dt;
            *out_speed = this->speed - speed_delta;
            *out_angle = angle;
        }
        else
        {
            Float3 a;
            Float3 b;
            a.z = 0.0f;
            b.z = 0.0f;
            laser_sincosmul(&a, angle, -speed);
            laser_sincosmul(&b, angle_delta, -speed_delta);
            Float3 sum;
            D3DXVec3Add(&sum, &a, &b);
            *out_pos = *pos + sum;
            *out_speed = (f32)sqrt(sum.x * sum.x + sum.y * sum.y);
            *out_angle = atan2(sum.y, sum.x);
        }
        break;
    case 2:
    {
        Float3 d;
        d.z = 0.0f;
        laser_sincosmul(&d, angle, speed);
        double whole = floor(t);
        *out_pos = *pos - d * (t - (f32)whole);
        *out_speed = speed - speed_delta;
        i32 i = 0;
        f32 a = angle - angle_delta;
        while (a > ZUN_PI)
        {
            a -= ZUN_2PI;
            if (i++ > 32)
            {
                break;
            }
        }
        while (a < -ZUN_PI)
        {
            a += ZUN_2PI;
            if (i++ > 32)
            {
                break;
            }
        }
        *out_angle = a;
        laser_sincosmul(&d, a, *out_speed);
        *out_pos = *out_pos - d * (1.0f - t + (f32)whole);
        break;
    }
    }
}

// The four dead locals are not ZUN's: the function's count of named
// variables decides MSVC's register choices here (docs/findings.md), and
// these come closest to the original. The mode 2 loop is a guarded do/while
// counting down like the original's sub/jne. Matching only.
// TODO: in mode 2's loop the original keeps s in xmm6 and pos.x in its stack slot (ours keeps pos.x in xmm7 and s in memory); the frame is aligned to 64.
// FUNCTION: TH16 0x437ee0
void LaserCurveNode::get_state(Float3 *out_pos, f32 *out_speed, f32 *out_angle, f32 time)
{
    i32 unused_a = 0;
    i32 unused_b = 0;
    i32 unused_c = 0;
    i32 unused_d = 0;
    (void)unused_a;
    (void)unused_b;
    (void)unused_c;
    (void)unused_d;
    time -= start_time;
    switch (mode)
    {
    case 0:
        *out_pos = start_pos + velocity * time * speed;
        *out_speed = speed;
        *out_angle = angle;
        break;
    case 1:
        if (-990.0f > angle_delta)
        {
            *out_pos = start_pos + velocity * (speed + speed + speed_delta * time) * (time + 1.0f) * 0.5f;
            *out_speed = speed_delta * time + speed;
            *out_angle = angle;
        }
        else
        {
            Float3 start = start_pos;
            Float3 a;
            Float3 b;
            a.z = 0.0f;
            b.z = 0.0f;
            laser_sincosmul(&a, angle, speed);
            laser_sincosmul(&b, angle_delta, speed_delta);
            Float3 sum = b + a;
            *out_pos = start + sum * time;
            *out_speed = (f32)sqrt(sum.x * sum.x + sum.y * sum.y);
            *out_angle = atan2(sum.y, sum.x);
        }
        break;
    case 2:
    {
        Float3 pos = start_pos;
        f32 a = angle;
        f32 s = speed;
        Float3 d;
        d.z = 0.0f;
        i32 n = (i32)time;
        if (n > 0)
        {
            do
            {
                laser_sincosmul(&d, a, s);
                i32 i = 0;
                a += angle_delta;
                while (a > ZUN_PI)
                {
                    a -= ZUN_2PI;
                    if (i++ > 32)
                    {
                        break;
                    }
                }
                while (a < -ZUN_PI)
                {
                    a += ZUN_2PI;
                    if (i++ > 32)
                    {
                        break;
                    }
                }
                pos.x += d.x;
                s += speed_delta;
                pos.y += d.y;
                pos.z += d.z;
            } while (--n);
        }
        laser_sincosmul(&d, a, s);
        *out_pos = pos + d * (time - (f32)floor(time));
        *out_speed = s;
        *out_angle = a;
        break;
    }
    }
}

// One frame: the et_ex steps until none asks to run again, growth (or, at
// full length, moving and shrinking to laser_new_arg_3), leaving the screen
// once the two delay timers ran out, then the graze check and the VMs.
// Nonzero once the laser is done.
// The dead double is not ZUN's code: as in LaserLineInf::initialize, it
// makes LTCG realign the frame (and esp, -8) like the original.
// TODO: the original keeps the * 1.0f of the inlined timer decrements, adds the tip offset's x component into its own register, and keeps the zero constant in xmm5 (ours xmm6).
// FUNCTION: TH16 0x432f40
i32 LaserLineInf::on_tick()
{
    double unused = 0.0;
    (void)unused;
    i32 again;
    do
    {
        run_ex();
        if (ex_flags == 0)
        {
            break;
        }
        again = 0;
        if (ex_flags & BULLET_EX_SPEEDUP)
        {
            again = step_ex_speedup();
        }
        if (ex_flags & BULLET_EX_ACCEL)
        {
            again += step_ex_accel();
        }
        if (ex_flags & BULLET_EX_ANGLE_ACCEL)
        {
            again += step_ex_angle_accel();
        }
        if (ex_flags & BULLET_EX_ANGLE)
        {
            switch (ex_state[3].ints[3])
            {
            case 0:
                again += step_ex_angle();
                break;
            case 1:
                again += step_ex_angle_mode_1();
                break;
            case 4:
                again += step_ex_angle_mode_4();
                break;
            }
        }
        if (ex_flags & BULLET_EX_BOUNCE)
        {
            again += step_ex_bounce();
        }
        if (ex_flags & BULLET_EX_WRAP)
        {
            again += step_ex_wrap();
        }
        if ((i32)ex_flags < 0)
        {
            if (ex_state[5].timer.current <= 0)
            {
                ex_flags ^= BULLET_EX_WAIT;
                again++;
            }
            else
            {
                ex_state[5].timer.decrement(1.0f);
            }
        }
        if (ex_invuln_remaining_frames != 0)
        {
            ex_invuln_remaining_frames--;
        }
    } while (again != 0);
    if (hit_length < inner.laser_new_arg_2)
    {
        hit_length += length * g_game_speed;
        if (hit_length > inner.laser_new_arg_2)
        {
            hit_length = inner.laser_new_arg_2;
        }
    }
    else
    {
        unk_7c += length * g_game_speed;
        // position += tip_offset * g_game_speed, through a temporary and
        // D3DXVec3Add with v first: g_game_speed is loaded once and the adds
        // take the original's operand order.
        Float3 v = tip_offset * g_game_speed;
        D3DXVec3Add(&position, &v, &position);
        if (inner.laser_new_arg_3 > 0.0f && hit_length + unk_7c > inner.laser_new_arg_3)
        {
            hit_length = inner.laser_new_arg_3 - unk_7c;
            inner.laser_new_arg_2 = hit_length;
            if (0.0f >= hit_length)
            {
                return 1;
            }
        }
    }
    if (offscreen_grace.current > 0 || timer_5b4.current > 0)
    {
        if (offscreen_grace.current > 0)
        {
            offscreen_grace.decrement(1.0f);
        }
        if (timer_5b4.current > 0)
        {
            timer_5b4.decrement(1.0f);
        }
    }
    else
    {
        Float3 tip;
        laser_sincosmul(&tip, angle, hit_length);
        f32 tip_x = position.x + tip.x;
        f32 tip_y = position.y + tip.y;
        if ((position.x + width <= -192.0f || position.x - width >= 192.0f || position.y + width <= 0.0f ||
             position.y - width >= 448.0f) &&
            (tip_x + width <= -192.0f || tip_x - width >= 192.0f || tip_y + width <= 0.0f || tip_y - width >= 448.0f))
        {
            return 1;
        }
    }
    check_graze_or_kill(0);
    vm_92c.flags_lo |= ANM_VM_SCALE_CHANGED;
    vm_92c.scale.x = width / g_AnmManager->loaded_anms[vm_92c.anm_loaded_index]->sprites[vm_92c.sprite_id].sprite_width;
    vm_92c.flags_lo |= ANM_VM_SCALE_CHANGED;
    vm_92c.scale.y =
        hit_length / g_AnmManager->loaded_anms[vm_92c.anm_loaded_index]->sprites[vm_92c.sprite_id].sprite_height;
    vm_92c.run();
    if (unk_7c == 0.0f)
    {
        vm_f28.run();
    }
    vm_1524.run();
    timer.tick();
    return 0;
}

// One frame: the et_ex steps, then each segment follows the node list to
// its place at segment_timer minus its index (segments not out yet stay at the
// start), leaving the screen once every segment is off it.
// The segments are indexed (segs[i], segs[i - 1]): the loop then walks them
// with a pointer biased by -8 like the original's.
// TODO: the original calls step_ex_accel through eax (ours edx), keeps 192 and 448 in swapped registers, keeps the * 1.0f of the inlined timer decrements and tests the first node for NULL (the for loop form keeps that test but differs more elsewhere).
// FUNCTION: TH16 0x4377d0
i32 LaserCurveInf::on_tick()
{
    i32 again;
    do
    {
        run_ex();
        if (ex_flags == 0)
        {
            break;
        }
        again = 0;
        if (ex_flags & BULLET_EX_SPEEDUP)
        {
            again = step_ex_speedup();
        }
        if (ex_flags & BULLET_EX_ACCEL)
        {
            again += step_ex_accel();
        }
        if (ex_flags & BULLET_EX_ANGLE_ACCEL)
        {
            again += step_ex_angle_accel();
        }
        if (ex_flags & BULLET_EX_ANGLE)
        {
            switch (ex_state[3].ints[3])
            {
            case 0:
                again += step_ex_angle();
                break;
            case 1:
                again += step_ex_angle_mode_1();
                break;
            case 4:
                again += step_ex_angle_mode_4();
                break;
            }
        }
        if (ex_flags & BULLET_EX_BOUNCE)
        {
            again += step_ex_bounce();
        }
        if (ex_flags & BULLET_EX_WRAP)
        {
            again += step_ex_wrap();
        }
        if (ex_flags & BULLET_EX_OFFSCREEN)
        {
            again += step_ex_offscreen();
        }
        if ((i32)ex_flags < 0)
        {
            if (ex_state[5].timer.current <= 0)
            {
                ex_flags ^= BULLET_EX_WAIT;
                again++;
            }
            else
            {
                ex_state[5].timer.decrement(1.0f);
            }
        }
        if (ex_invuln_remaining_frames != 0)
        {
            ex_invuln_remaining_frames--;
        }
    } while (again != 0);
    LaserCurveSegment *segs = (LaserCurveSegment *)segments;
    if (!(flags_rest & 1))
    {
        // volatile keeps the flag in its stack slot as in the original,
        // where out_length holds the register ours would give it.
        volatile i32 placed = 0;
        for (i32 i = 0; i < inner.segment_count; i++)
        {
            f32 t = segment_timer.current_f - (f32)i;
            if (t >= 0.0f)
            {
                f32 prev_length = segs[i - 1].length;
                f32 prev_angle = segs[i - 1].angle;
                f32 *out_length = &segs[i].length;
                f32 *out_angle = &segs[i].angle;
                // A do/while from the first node (never NULL) instead of a
                // for loop: closer to the original's registers.
                LaserCurveNode *node = &nodes;
                do
                {
                    if (t >= node->start_time && node->end_time > t)
                    {
                        if (!placed)
                        {
                            node->get_state(&segs[i].pos, out_length, out_angle, t);
                        }
                        else
                        {
                            node->step_back(&segs[i].pos, out_length, out_angle, &segs[i - 1].pos, prev_length,
                                            prev_angle, t);
                        }
                        break;
                    }
                    node = node->next;
                } while (node != NULL);
                placed = 1;
            }
            else
            {
                segs[i].pos = inner.start_pos;
                *(Float3 *)segs[i].unk_c = g_zero_vec;
                segs[i].angle = inner.ang_aim;
                segs[i].length = inner.speed;
            }
        }
    }
    LaserCurveSegment *segment = (LaserCurveSegment *)segments;
    if (offscreen_grace.current > 0 || (ex_flags & BULLET_EX_OFFSCREEN))
    {
        offscreen_grace.decrement(1.0f);
    }
    else
    {
        for (i32 i = 0; i < inner.segment_count; i++, segment++)
        {
            Float3 head;
            laser_sincosmul(&head, angle, hit_length);
            head += position;
            if (!(segment->pos.x + width <= -192.0f || segment->pos.x - width >= 192.0f ||
                  segment->pos.y + width <= 0.0f || segment->pos.y - width >= 448.0f))
            {
                goto on_screen;
            }
        }
        return 1;
    }
on_screen:
    check_graze_or_kill(0);
    vm_92c.run();
    vm_f28.run();
    segment_timer.tick();
    return 0;
}

// Runs the laser's pending et_ex instructions: each one starts an et_ex
// step (ex_flags and its ex_state), or acts at once (sounds, bullets, the
// sprite, blend mode, jumps).
// TODO: the shooter fields after pos are written through raw offsets; register allocation and the case layout differ.
// FUNCTION: TH16 0x431fe0
DECOMP_NOINLINE void LaserLineInf::run_ex()
{
    while (ex_index < 0x12)
    {
        BulletEx *ex = &inner.ex[ex_index];
        if (ex->type == 0)
        {
            return;
        }
        if (ex->slot == 0 && ex_flags != 0)
        {
            return;
        }
        switch ((u32)ex->type)
        {
        case BULLET_EX_SPEEDUP:
            ex_flags |= BULLET_EX_SPEEDUP;
            ex_state[0].timer.set_value(0);
            ex_state[0].floats[7] = 0.0f;
            break;
        case BULLET_EX_ACCEL:
            ex_flags |= BULLET_EX_ACCEL;
            ex_state[1].floats[0] = ex->r;
            ex_state[1].floats[1] =
                -990.0f >= ex->s ? angle : (ex->s >= 990.0f ? g_Player->angle_to_player(&position) : ex->s);
            ex_state[1].timer.set_value(0);
            ex_state[1].ints[0] = ex->a;
            laser_sincosmul((Float3 *)&ex_state[1].floats[5], ex_state[1].floats[1], ex_state[1].floats[0]);
            if (ex_index != 0 && inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            break;
        case BULLET_EX_ANGLE_ACCEL:
            ex_flags |= BULLET_EX_ANGLE_ACCEL;
            ex_state[2].floats[0] = ex->r;
            ex_state[2].floats[1] = ex->s;
            ex_state[2].timer.set_value(0);
            ex_state[2].ints[0] = ex->a;
            if (ex_index != 0 && inner.shot_transform_sfx >= 0)
            {
                g_SoundManager.play_sound_centered(inner.shot_transform_sfx, 0);
            }
            break;
        case BULLET_EX_ANGLE:
            ex_flags |= ex->type;
            ex_state[3].floats[1] = ex->r;
            ex_state[3].floats[0] = ex->s > -999.0f ? ex->s : length;
            ex_state[3].timer.set_value(0);
            ex_state[3].ints[0] = ex->a;
            ex_state[3].ints[1] = ex->b;
            ex_state[3].ints[2] = 0;
            ex_state[3].ints[3] = ex->c;
            break;
        case BULLET_EX_BOUNCE:
            if (ex->a > 0)
            {
                ex_flags |= ex->type;
                if (ex->r >= 0.0f)
                {
                    ex_state[4].floats[0] = ex->r;
                }
                else
                {
                    ex_state[4].floats[0] = length;
                }
                ex->a--;
                ex_state[4].ints[1] = ex->a;
                ex_state[4].ints[0] = 0;
                ex_state[4].ints[2] = ex->b;
            }
            break;
        case BULLET_EX_INVULN:
            ex_invuln_remaining_frames = ex->a;
            break;
        case BULLET_EX_OFFSCREEN:
            timer_5b4.set_inline(ex->a);
            ex_index++;
            continue;
        case BULLET_EX_SET_SPRITE:
        {
            AnmVm *vm = &vm_92c;
            g_BulletManager->bullet_anm->copy_vm(vm, g_bullet_types[ex->a].script + ex->b);
            vm->parent_vm = NULL;
            vm->root_vm = NULL;
            vm->run();
            break;
        }
        case BULLET_EX_DELETE:
            state = LASER_STATE_WARNING;
            break;
        case BULLET_EX_PLAY_SOUND:
            g_SoundManager.play_sound_at_position(ex->a, position.x);
            ex_index++;
            continue;
        case BULLET_EX_WRAP:
            ex_flags |= ex->type;
            ex_state[6].timer.set_inline(ex->a);
            break;
        case BULLET_EX_SHOOT:
        {
            EnemyBulletShooter shooter;
            laser_sincosmul(&shooter.pos, angle, hit_length);
            u32 a = ex->a;
            shooter.pos.x += position.x;
            shooter.pos.z = 0.0f;
            *(u16 *)&shooter.aim_type = (a >> 24) & 0x7f;
            shooter.type = (a >> 16) & 0xff;
            shooter.pos.y += position.y;
            shooter.color = (a >> 8) & 0xff;
            shooter.spd1 = ex->r;
            shooter.spd2 = ex->s;
            shooter.start_transform = a & 0xff;
            shooter.count = ex->b;
            ex_index++;
            shooter.layers = ex[1].a;
            shooter.ang_aim = ex[1].r;
            shooter.sfx_flags = ex[1].b;
            shooter.ang_bullet_dist = ex[1].s;
            memcpy(shooter.ex, inner.ex, sizeof(inner.ex));
            g_BulletManager->shoot_bullets(&shooter);
            ex_index++;
            if ((i32)a < 0)
            {
                cancel(0, 0);
                break;
            }
            continue;
        }
        case BULLET_EX_TAG:
            id = ex->a;
            ex_index++;
            continue;
        case BULLET_EX_LOOP:
            ex_index = ex->a;
            continue;
        case BULLET_EX_BLEND:
            if (ex->a != 0)
            {
                vm_92c.flags_lo = vm_92c.flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
            }
            else
            {
                vm_92c.flags_lo &= ~ANM_VM_BLEND_MODE_MASK;
            }
            ex_index++;
            continue;
        case BULLET_EX_WAIT:
            ex_flags |= ex->type;
            ex_state[5].timer.set_inline(ex->a);
            break;
        }
        ex_index++;
    }
}

// Sets the laser up from its parameters: the body, origin and tip VMs, the
// delay timers, the shot sound and the start offset along the aim.
// The dead double is not ZUN's code: it stands in for double math the
// optimizer removed from his body. LTCG's double stack alignment pass sees it
// at the IL level, so this function realigns its frame (and esp, -8) like the
// original (see TitleInf::set_substate).
// FUNCTION: TH16 0x431b30
i32 LaserLineInf::initialize(void *params)
{
    double unused = 0.0;
    (void)unused;
    inner = *(LaserLineInner *)params;
    bullet_type = inner.bullet_type;
    state = LASER_STATE_ACTIVE;
    kind = LASER_LINE;
    bullet_color = inner.bullet_color;
    AnmVm *vm = &vm_92c;
    vm->wipe();
    vm_92c.index_of_sprite_mapping_func = ANM_SPRITE_MAPPING_LASER_LINE;
    vm_92c.associated_game_entity = this;
    g_LaserManager->bullet_anm->set_vm_script(vm, g_bullet_types[bullet_type].script);
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    AnmVmFlagsLoFields *fields = (AnmVmFlagsLoFields *)&vm_92c.flags_lo;
    fields->anchor_x = 0;
    fields->anchor_y = 2;
    fields->render_mode = 1;
    vm_92c.flags_hi = vm_92c.flags_hi & ~0x80000 | 0x40000;
    vm = &vm_f28;
    g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x38);
    vm->parent_vm = NULL;
    vm->root_vm = NULL;
    vm->run();
    vm->interrupt(2);
    vm->run();
    vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    ((AnmVmFlagsLoFields *)&vm_f28.flags_lo)->render_mode = 1;
    vm_f28.flags_hi = vm_f28.flags_hi & ~0x80000 | 0x40000;
    if (bullet_type > 0x11 && bullet_type != 0x26)
    {
        vm = &vm_1524;
        g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x53);
        vm->parent_vm = NULL;
        vm->root_vm = NULL;
        vm->run();
    }
    else
    {
        vm = &vm_1524;
        g_LaserManager->bullet_anm->copy_vm(vm, inner.bullet_color + 0x5b);
        vm->parent_vm = NULL;
        vm->root_vm = NULL;
        vm->run();
        vm->flags_lo = vm->flags_lo & ~ANM_VM_BLEND_MODE_MASK | (1 << ANM_VM_BLEND_MODE_SHIFT);
    }
    vm_1524.flags_hi = vm_1524.flags_hi & ~0x80000 | 0x40000;
    offscreen_grace.set_inline(30);
    timer_5b4.set_inline(3);
    if (inner.shot_sfx >= 0)
    {
        g_SoundManager.play_sound_at_position(inner.shot_sfx, 0.0f);
    }
    graze_timer.reset();
    segment_timer.reset();
    position = inner.start_pos;
    if (inner.distance != 0.0f)
    {
        Float3 offset;
        laser_sincosmul(&offset, inner.ang_aim, inner.distance);
        position.x += offset.x;
        position.y += offset.y;
    }
    width = inner.laser_new_arg_4;
    hit_length = inner.laser_new_arg_1;
    length = inner.speed;
    angle = inner.ang_aim;
    if (inner.laser_new_arg_1 > inner.laser_new_arg_2)
    {
        unk_7c = 0.01f;
    }
    else
    {
        unk_7c = 0.0f;
    }
    laser_sincosmul(&tip_offset, angle, length);
    ex_index = inner.start_transform;
    return 0;
}
