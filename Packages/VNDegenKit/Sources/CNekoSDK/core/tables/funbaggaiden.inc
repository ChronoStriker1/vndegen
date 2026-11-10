const char kFunbagGaiden[] = R"(
screen title
menustages
startmenu 2
pic obj\d000_000.bmp 0 0
pic obj\d000_004.bmp 461 672
pic obj\d000_005.bmp 159 0 stage 1
pic obj\d001_000.bmp 397 506 stage 1
pic obj\d000_003.bmp 240 17 stage 2
button start obj\d002_000.bmp 486 408 6 5 0 stage 2
button load obj\d002_000.bmp 486 448 6 5 1 stage 2
button config obj\d002_000.bmp 486 528 6 5 3 stage 2
button quit obj\d002_000.bmp 486 608 6 5 5 stage 2
button start_prologue obj\d002_001.bmp 810 408 2 5 0 stage 2 group start
button start_main obj\d002_001.bmp 810 448 2 5 1 stage 2 group start

screen start
flow main

screen defaults
parts scroll particles
colorkey 0 255 0
textfont 9
textsize 22
textline 32
textshift 0 -1
twobyteadvance
inneralways
default opacity 100

screen cfgsav
size 96000
tracks 92000
nostring

screen staffroll_a
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n004_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n004_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n004_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n004_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n004_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n004_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n004_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n004_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen staffroll_b
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n003_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n003_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n003_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n003_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n003_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n003_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n003_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n003_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen staffroll_c
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n002_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n002_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n002_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n002_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n002_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n002_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n002_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n002_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen story
flow prologue
scene scr\se_00_00_00.txt @1
scene scr\se_00_00_01.txt @2
scene scr\se_00_00_02.txt @3
scene scr\se_00_00_03.txt @4
scene scr\se_00_00_04.txt @5
set G 2 = 1
jump main
flow main
set S 5000 = 1
scene scr\se_00_01_00.txt @6
scene scr\se_00_01_01.txt @7
scene scr\se_00_01_02.txt @8
scene scr\se_00_01_03.txt @9
scene scr\se_00_01_04.txt @10
scene scr\se_00_01_06.txt @400
scene scr\se_00_01_06b.txt @11
scene scr\se_00_01_07.txt @12
scene scr\se_00_01_08.txt @13
scene scr\se_00_01_09.txt @14
scene scr\se_00_01_11.txt @15
scene scr\se_00_01_12.txt @16
scene scr\se_00_01_13.txt @17
scene scr\se_00_02_00.txt @18
scene scr\se_00_02_01.txt @19
scene scr\se_00_02_02.txt @20
scene scr\se_00_02_03.txt @21
scene scr\se_00_02_04.txt @22
scene scr\se_00_02_05.txt @23
scene scr\se_00_02_07.txt @301
scene scr\se_00_02_07b.txt @24
scene scr\se_00_02_08.txt @25
scene scr\se_00_02_09.txt @26
scene scr\se_00_03_00.txt @27
scene scr\se_00_03_01.txt @28
scene scr\se_00_03_02.txt @29
scene scr\se_00_03_03.txt @30
scene scr\se_00_03_04.txt @31
scene scr\se_00_03_05.txt @32
scene scr\se_00_03_06.txt @33
scene scr\se_00_03_07.txt @34
set S 5000 = 2
scene scr\se_01_00_00.txt @35
scene scr\se_01_00_01.txt @36
scene scr\se_01_00_02.txt @37
scene scr\se_01_00_04.txt @38
scene scr\se_01_00_05.txt @39
scene scr\se_01_01_00.txt @40
scene scr\se_01_01_01.txt @41
scene scr\se_01_01_02.txt @42
scene scr\se_01_01_03.txt @43
scene scr\se_01_01_04.txt @44
scene scr\se_01_01_05.txt @45
scene scr\se_01_01_06.txt @46
scene scr\se_01_01_07.txt @47
scene scr\se_01_02_00.txt @48
scene scr\se_01_02_01.txt @49
scene scr\se_01_02_02.txt @50
scene scr\se_01_02_03.txt @51
scene scr\se_01_02_04.txt @52
scene scr\se_01_02_05.txt @53
scene scr\se_01_03_00.txt @54
scene scr\se_01_03_01.txt @55
scene scr\se_01_03_02.txt @56
scene scr\se_01_03_03.txt @57
scene scr\se_01_03_04.txt @58
scene scr\se_01_03_06.txt @59
scene scr\se_01_03_07.txt @60
scene scr\se_01_03_08.txt @61
scene scr\se_01_04_00.txt @62
scene scr\se_01_04_01.txt @63
scene scr\se_01_04_02.txt @64
scene scr\se_01_04_03.txt @65
choice obj\p001_000.bmp 2 @66
on 1 goto leave
scene scr\se_01_04_04.txt @70
set S 5000 = 3
scene scr\se_02_00_00.txt @71
scene scr\se_02_00_01.txt @72
scene scr\se_02_00_02.txt @73
scene scr\se_02_00_03.txt @74
scene scr\se_02_00_04.txt @75
scene scr\se_02_00_05.txt @76
scene scr\se_02_00_06.txt @77
scene scr\se_02_00_07.txt @78
scene scr\se_02_01_00.txt @79
scene scr\se_02_01_01.txt @80
scene scr\se_02_01_02.txt @81
scene scr\se_02_01_03.txt @82
scene scr\se_02_01_05.txt @83
scene scr\se_02_01_07.txt @84
scene scr\se_02_01_08.txt @85
scene scr\se_02_01_10.txt @86
scene scr\se_02_01_11.txt @87
scene scr\se_02_01_12.txt @88
scene scr\se_02_01_14.txt @89
scene scr\se_02_01_15.txt @90
scene scr\se_02_01_16.txt @91
scene scr\se_02_01_17.txt @92
scene scr\se_02_02_00.txt @93
scene scr\se_02_02_01.txt @94
scene scr\se_02_02_02.txt @95
choice obj\p001_001.bmp 2 @96
on 1 goto other
scene scr\se_02_02_03.txt @97
goto joined
label other
scene scr\se_02_02_04.txt @97
label joined
scene scr\se_02_02_05.txt @98
scene scr\se_02_02_06.txt @99
choice obj\p001_002.bmp 2 @100
on 1 goto on
scene scr\se_02_02_07.txt @210
staffroll b
scene scr\se_02_02_08.txt @212
end
label on
scene scr\se_02_03_00.txt @102
scene scr\se_02_03_01.txt @103
scene scr\se_02_03_02.txt @104
scene scr\se_02_03_03.txt @105
scene scr\se_02_03_04.txt @220
staffroll c
scene scr\se_02_04_00.txt @222
scene scr\se_02_04_01.txt @223
scene scr\se_02_04_01a.txt @224
scene scr\se_02_04_02.txt @225
scene scr\se_02_04_03.txt @226
scene scr\se_02_04_05.txt @227
scene scr\se_02_04_06.txt @228
end
label leave
scene scr\se_01_04_05.txt @200
staffroll a
scene scr\se_01_04_06.txt @202
end
)";
