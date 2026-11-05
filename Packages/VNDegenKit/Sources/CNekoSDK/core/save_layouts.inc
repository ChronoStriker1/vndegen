// The parts of a save that hold the same bytes every time, from saves the PC games wrote: offsets and hex. Numbers only.
const SaveLayout kChristmas = {
    0,  // the extra bytes of this engine's sprite record over the oldest layout seen
    {},
    {{16, "ff00050000d002"}, {125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {251, "803f"}, {271, "803f"}, {291, "803f"}, {311, "803f"}, {383, "803f0000803f"}, {398, "01"}},
    {{125, "ffffffff"}, {149, "f4010000dc0500001e"}, {161, "0a"}, {165, "3c"}, {181, "65"}, {189, "40"}, {197, "dc05"}, {209, "ffffffff"}, {251, "803f"}, {271, "803f"}, {291, "803f"}, {311, "803f"}, {381, "cdcc8c3fcdcc8c3f8002000068010000ff"}},
    {{116, "ffffffff64"}},
    96, -1, {{2, "ffffffff0000f041"}, {72, "ffffffff"}},
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
    96, -1, {{2, "ffffffff0000f041"}, {13, "40"}, {21, "40"}, {72, "ffffffff"}},
    7510, {{819, "ffffffff"}, {893, "ffffffff"}, {963, "ffffffff"}, {1013, "04"}, {1041, "3c"}, {6668, "68"}, {7478, "ffffffff"}},
    12,  // a text record is 4 bytes shorter in this engine
};
