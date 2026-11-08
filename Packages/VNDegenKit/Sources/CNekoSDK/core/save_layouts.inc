// The parts of a save that hold the same bytes every time, from saves the PC games wrote: offsets and hex. Numbers only.
const SaveLayout kChristmas = {
    0,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff00050000d002"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {251, "803f"}, {271, "803f"}, {291, "803f"}, {311, "803f"}, {383, "803f0000803f"}, {398, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {251, "803f"}, {271, "803f"}, {291, "803f"}, {311, "803f"}, {381, "cdcc8c3fcdcc8c3f8002000068010000ff"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff0000f041"}, {72, "ffffffff"}},
    7514, {{9, "68"}, {819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1045, "3c"}, {6672, "68"}, {7482, "ffffffff"}},
};
const SaveLayout kAlchemist = {
    69,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {{112, "02"}, {116, "02"}, {128, "01"}},
    {{16, "ff400600008403"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {281, "65"}, {285, "3c"}, {289, "78"}, {293, "1e"}, {297, "3c"}, {308, "803f"}, {328, "803f"}, {348, "803f"}, {368, "803f"}, {452, "803f0000803f"}, {467, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {281, "65"}, {285, "3c"}, {289, "78"}, {293, "1e"}, {297, "3c"}, {308, "803f"}, {328, "803f"}, {348, "803f"}, {368, "803f"}, {450, "3333b33f3333b33fc00300001c020000ff"}, {470, "0114"}},
    {{116, "ffffffff64"}},
    120, 22, {{2, "f0"}, {9, "42"}, {13, "c0"}, {20, "4040"}, {30, "ffffffff"}, {84, "ffffffff"}, {112, "ffffffff"}},
    7538, {{819, "ffffffff"}, {893, "ffffffff"}, {921, "ffffffff"}, {975, "ffffffff"}, {1003, "ffffffff"}, {1037, "04"}, {1069, "3c"}, {6696, "68"}, {7506, "ffffffff"}},
};
const SaveLayout kMother3 = {
    -84,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff200300005802"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {171, "803f"}, {191, "803f"}, {211, "803f"}, {231, "803f"}, {299, "803f0000803f"}, {314, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {171, "803f"}, {191, "803f"}, {211, "803f"}, {231, "803f"}, {299, "803f0000803f"}, {313, "ff"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff0000f041"}, {13, "40"}, {21, "40"}, {72, "ffffffff"}},
    7510, {{819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1041, "3c"}, {6668, "68"}, {7478, "ffffffff"}},
    12,  // a text record is 4 bytes shorter in this engine
};
const SaveLayout kTsumaGibo = {
    -136,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff00050000d002"}, {119, "803f"}, {139, "803f"}, {159, "803f"}, {179, "803f"}, {247, "803f0000803f"}, {262, "01"}},
    {{119, "803f"}, {139, "803f"}, {159, "803f"}, {179, "803f"}, {245, "cdcc8c3fcdcc8c3f8002000068010000ff"}, {265, "010a"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff0000f041"}, {72, "ffffffff"}},
    7510, {{9, "68"}, {819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1041, "3c"}, {6668, "68"}, {7478, "ffffffff"}},
    12,  // the bytes of a text record after its strings
};
const SaveLayout kFunbagFantasy = {
    -140,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{12, "ff00050000d002"}, {115, "803f"}, {135, "803f"}, {155, "803f"}, {175, "803f"}, {243, "803f0000803f"}, {258, "01"}},
    {{115, "803f"}, {135, "803f"}, {155, "803f"}, {175, "803f"}, {241, "cdcc8c3fcdcc8c3f8002000068010000ff"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff0000f041"}, {72, "ffffffff"}},
    7510, {{819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1041, "3c"}, {6668, "68"}, {7478, "ffffffff"}},
    12,  // the bytes of a text record after its strings
    13,  // where the first record holds the screen size
};
const SaveLayout kFunbag4 = {
    57,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff00050000d002"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {281, "65"}, {285, "3c"}, {289, "78"}, {293, "1e"}, {297, "3c"}, {308, "803f"}, {328, "803f"}, {348, "803f"}, {368, "803f"}, {440, "803f0000803f"}, {455, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {281, "65"}, {285, "3c"}, {289, "78"}, {293, "1e"}, {297, "3c"}, {308, "803f"}, {328, "803f"}, {348, "803f"}, {368, "803f"}, {420, "c84200002043"}, {440, "c03f0000c03f8002000068010000ff"}, {492, "01"}},
    {{116, "ffffffff64"}, {125, "0a"}, {130, "c8"}, {140, "a0400a"}, {156, "a0400a"}},
    120, 22, {{2, "ffffffff"}, {12, "a040"}, {20, "a040"}, {30, "ffffffff"}, {84, "ffffffff"}, {112, "ffffffff"}},
    7538, {{9, "68"}, {819, "ffffffff"}, {893, "ffffffff"}, {921, "ffffffff"}, {975, "ffffffff"}, {1003, "ffffffff"}, {1037, "04"}, {1069, "3c"}, {6696, "68"}, {7506, "ffffffff"}},
    16,  // the bytes of a text record after its strings
    17,  // where the first record holds the screen size
};
const SaveLayout kMoralOrc = {
    -48,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff00050000d002"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {203, "803f"}, {223, "803f"}, {243, "803f"}, {263, "803f"}, {335, "803f0000803f"}, {350, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {203, "803f"}, {223, "803f"}, {243, "803f"}, {263, "803f"}, {333, "cdcc8c3fcdcc8c3f0002"}, {346, "010000ff"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff"}, {72, "ffffffff"}},
    7514, {{9, "68"}, {819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1045, "3c"}, {6672, "68"}, {7482, "ffffffff"}},
    16,  // the bytes of a text record after its strings
};
// Test Game 94's PC saves, from a scene whose background has camera work: sprite records 4 bytes shorter than Test Game
// 92's, text records with 8 bytes after their strings, and 183 sound channels.
const SaveLayout kRinjin = {
    -140,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{12, "ff00050000d002"}, {115, "803f"}, {135, "803f"}, {155, "803f"}, {175, "803f"}, {243, "803f0000803f"}, {258, "01"}},
    {{115, "803f"}, {135, "803f"}, {155, "803f"}, {175, "803f"}, {249, "8002000068010000ff"}, {295, "01"}},
    {{116, "ffffffff64"}},
    96, 22, {{2, "ffffffff"}, {72, "ffffffff"}},
    6126, {{819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1037, "3c"}, {5284, "68"}, {6094, "ffffffff"}},
    8,  // the bytes of a text record after its strings
    13,  // where the first record holds the screen size
    183,  // the sound channels saved
};
