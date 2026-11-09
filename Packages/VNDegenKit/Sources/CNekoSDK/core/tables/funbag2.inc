// Test Game 83, Funbag Fantasy 2. Its story order is in the exe; the flow below was traced under Wine, which scene
// scripts the PC reads through each route and what its saves at each choice hold. Picture names and numbers only.
const char kFunbag2[] = R"(
screen defaults
layoutcgslot 102
lastchoice made

screen story
# The six choices are obj\l000_000 to _005, two options each. Sex scenes end on l000_000, which only picks the next
# scene; option 0 also counts in S 5001 (option 1 never set it, in a run that took it every time). S 5000 is the
# route's chapter as the PC's saves hold it.
# New Game plays the prologue. Once global 2 is set, Start offers Prologue or Main Story, and Main Story starts at
# the flow `story` (traced: with global 100 set and 2 not, Start went straight to the prologue).
flow prologue
scene scr\ep_00_00_00.txt @1
set G 2 = 1
jump story

flow story
# Global 100 is set in every cfg.sav the PC wrote after any scene past the prologue; where exactly is inferred.
set G 100 = 1
set S 5000 = 1
scene scr\ep_01_01_00.txt @1001
scene scr\ep_01_02_00.txt @1002
scene scr\ep_01_03_00.txt @1003
scene scr\ep_01_04_00.txt @1004
scene scr\ep_01_04_01.txt @1005
choice obj\l000_000.bmp 2 @1006
on 1 goto h1b
set S 5001 += 1
scene scr\ep_01_04_02.txt @1030
goto h1j
label h1b
scene scr\ep_01_04_03.txt @1030
label h1j
scene scr\ep_01_04_04.txt @1031
scene scr\ep_01_05_00.txt @1032
scene scr\ep_01_05_01.txt @1033
choice obj\l000_000.bmp 2 @1034
on 1 goto h2b
set S 5001 += 1
scene scr\ep_01_05_02.txt @1060
goto h2j
label h2b
scene scr\ep_01_05_03.txt @1060
label h2j
scene scr\ep_01_05_04.txt @1061
scene scr\ep_01_06_00.txt @1062
scene scr\ep_01_07_00.txt @1063
scene scr\ep_01_07_01.txt @1064
choice obj\l000_000.bmp 2 @1065
on 1 goto h3b
set S 5001 += 1
scene scr\ep_01_07_02.txt @1090
goto h3j
label h3b
scene scr\ep_01_07_03.txt @1090
label h3j
scene scr\ep_01_08_00.txt @1091
scene scr\ep_01_09_00.txt @1092
# Castellan of Boobshire (chapters 3 and 4) or prefect of Molgrey (chapter 2 and on).
choice obj\l000_001.bmp 2 @1093
on 1 goto molgrey
scene scr\ep_01_09_01.txt @1101
goto boobshire
label molgrey
scene scr\ep_01_09_02.txt @1111
scene scr\ep_01_10_00.txt @1112

# Chapter 2.
set S 5000 = 2
scene scr\ep_02_01_00.txt @2001
scene scr\ep_02_01_01.txt @2002
choice obj\l000_000.bmp 2 @2003
on 1 goto h4b
set S 5001 += 1
scene scr\ep_02_01_02.txt @2030
goto h4j
label h4b
scene scr\ep_02_01_03.txt @2030
label h4j
scene scr\ep_02_01_04.txt @2031
scene scr\ep_02_02_00.txt @2032
scene scr\ep_02_03_00.txt @2033
scene scr\ep_02_04_00.txt @2034
scene scr\ep_02_04_01.txt @2035
scene scr\ep_02_05_00.txt @2036
# Take Lorelia (an ending) or keep to the political career.
choice obj\l000_002.bmp 2 @2037
on 1 goto career
scene scr\ep_02_05_01.txt @2041
scene scr\ep_staff.txt
scene scr\ep_02_05_02.txt @999
end
label career
scene scr\ep_02_06_00.txt @2051
scene scr\ep_02_07_00.txt @2052
scene scr\ep_02_08_00.txt @2053
scene scr\ep_02_09_00.txt @2054
scene scr\ep_02_10_00.txt @2055
scene scr\ep_02_11_00.txt @2056
scene scr\ep_02_12_00.txt @2057
scene scr\ep_02_13_00.txt @2058
scene scr\ep_02_14_00.txt @2059
scene scr\ep_02_15_00.txt @2060
scene scr\ep_02_15_01.txt @2061
scene scr\ep_02_16_00.txt @2062
# Equal castellans (chapter 5) or husband rather than a pet (an ending).
choice obj\l000_003.bmp 2 @2063
on 1 goto husband
scene scr\ep_02_16_01.txt @2071
goto chapter5
label husband
scene scr\ep_02_16_02.txt @2081
scene scr\ep_02_17_00.txt @2082
scene scr\ep_staff.txt
scene scr\ep_02_17_01.txt @999
end

# Chapter 3.
label boobshire
set S 5000 = 3
scene scr\ep_03_01_00.txt @3001
scene scr\ep_03_02_00.txt @3002
scene scr\ep_03_03_00.txt @3003
scene scr\ep_03_04_00.txt @3004
scene scr\ep_03_05_00.txt @3005
scene scr\ep_03_06_00.txt @3006
scene scr\ep_03_07_00.txt @3007
scene scr\ep_03_08_00.txt @3008
scene scr\ep_03_09_00.txt @3009
scene scr\ep_03_10_00.txt @3010
scene scr\ep_03_11_00.txt @3011
scene scr\ep_03_12_00.txt @3012
scene scr\ep_03_13_00.txt @3013
# Don't dispatch a detachment (chapter 4) or let Zebia lead it (an ending). The exe names ep_03_17_00 and _01 in a
# sys_1 patch this copy doesn't have, and the PC goes from ep_03_16_00 to ep_03_18_00.
choice obj\l000_004.bmp 2 @3014
on 1 goto zebialeads
scene scr\ep_03_14_00.txt @3021
scene scr\ep_03_15_01.txt @4000
goto chapter4
label zebialeads
scene scr\ep_03_15_00.txt @3031
scene scr\ep_03_15_02.txt @3032
scene scr\ep_03_16_00.txt @3033
scene scr\ep_03_18_00.txt
scene scr\ep_03_18_01.txt @3052
scene scr\ep_staff.txt
scene scr\ep_03_19_00.txt @999
end

# Chapter 4.
label chapter4
scene scr\ep_04_01_00.txt @4001
scene scr\ep_04_02_00.txt @4002
scene scr\ep_04_03_00.txt @4003
scene scr\ep_04_04_00.txt @4004
scene scr\ep_04_05_00.txt @4005
scene scr\ep_04_05_01.txt @4006
scene scr\ep_04_06_00.txt @4007
scene scr\ep_04_07_00.txt @4008
scene scr\ep_04_08_00.txt @4009
scene scr\ep_04_09_00.txt @4010
scene scr\ep_04_10_00.txt @4011
scene scr\ep_04_11_00.txt @4012
scene scr\ep_staff.txt
scene scr\ep_04_11_01.txt @999
end

# Chapter 5, after the equal castellans.
label chapter5
set S 5000 = 4
scene scr\ep_05_01_00.txt @5001
scene scr\ep_05_01_01.txt @5002
scene scr\ep_05_02_00.txt @5003
# Use Zebia's body (an ending) or thank her (chapters 6 and 7).
choice obj\l000_005.bmp 2 @5004
on 1 goto thanks
scene scr\ep_05_02_01.txt @5030
scene scr\ep_05_03_00.txt @5031
scene scr\ep_05_03_01.txt
scene scr\ep_05_03_02.txt @5051
scene scr\ep_05_03_03.txt @5052
scene scr\ep_05_04_00.txt @5053
scene scr\ep_05_04_02.txt
scene scr\ep_staff.txt
scene scr\ep_05_04_03.txt
end
label thanks
scene scr\ep_05_02_02.txt @5030
scene scr\ep_05_03_00.txt @5031
scene scr\ep_05_03_02.txt @5051
scene scr\ep_05_03_03.txt @5052
scene scr\ep_05_04_00.txt @5053
scene scr\ep_05_04_01.txt @5071
scene scr\ep_05_05_00.txt @5072
scene scr\ep_05_06_00.txt @5073
scene scr\ep_05_06_01.txt @5074
scene scr\ep_05_07_00.txt @5075
scene scr\ep_05_07_01.txt @5076
scene scr\ep_05_07_02.txt @6000

# Chapter 6, which has no choices. The exe names ep_06_08_01 and ep_06_09_00b in the missing sys_1 patch.
scene scr\ep_06_01_00.txt @6001
scene scr\ep_06_02_00.txt @6002
scene scr\ep_06_02_01.txt @6003
scene scr\ep_06_02_02.txt @6004
scene scr\ep_06_03_00.txt @6005
scene scr\ep_06_03_01.txt @6006
scene scr\ep_06_04_00.txt @6007
scene scr\ep_06_04_01.txt @6008
scene scr\ep_06_04_02.txt @6009
scene scr\ep_06_05_00.txt @6010
scene scr\ep_06_05_01.txt @6011
scene scr\ep_06_06_00.txt @6012
scene scr\ep_06_06_01.txt @6013
scene scr\ep_06_07_00.txt @6014
scene scr\ep_06_07_01.txt @6015
scene scr\ep_06_08_00.txt @6016
scene scr\ep_06_08_02.txt @6031
scene scr\ep_06_08_03.txt @6032
scene scr\ep_06_09_00.txt @6100
scene scr\ep_06_09_00a.txt @6130
scene scr\ep_06_09_00c.txt @6033
scene scr\ep_06_09_01.txt @6034
scene scr\ep_06_09_02.txt @6035
scene scr\ep_06_09_03.txt @6036
scene scr\ep_06_10_00.txt @6037
scene scr\ep_06_10_01.txt @6038
scene scr\ep_06_10_02.txt @6039
scene scr\ep_06_10_03.txt @6040
scene scr\ep_06_11_00.txt @6041
scene scr\ep_06_12_00.txt @6042
scene scr\ep_06_12_01.txt @6043
scene scr\ep_06_13_00.txt @6044
scene scr\ep_06_13_01.txt @6045
scene scr\ep_06_13_02.txt @6046
scene scr\ep_06_13_03.txt @6047
scene scr\ep_06_14_00.txt @6048
scene scr\ep_06_14_01.txt @6049
scene scr\ep_06_15_00.txt @6050
scene scr\ep_06_15_01.txt @6051
scene scr\ep_06_15_02.txt @6052
scene scr\ep_06_15_03.txt
scene scr\ep_06_15_04.txt @6054
scene scr\ep_06_15_05.txt @6055
scene scr\ep_06_15_06.txt @6056
scene scr\ep_06_15_07.txt @6057
scene scr\ep_06_15_08.txt @6058
scene scr\ep_staff.txt
# Chapter 7, the epilogue.
scene scr\ep_07_00_00.txt @7001
scene scr\ep_07_00_01.txt
scene scr\ep_07_00_02.txt
scene scr\ep_07_00_03.txt
scene scr\ep_07_00_04.txt
end
)";
