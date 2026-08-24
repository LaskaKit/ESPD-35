// =============================================================================
//  ESPD35_MeteoPlaneRadar - verze firmwaru. JEDINE misto, kde se pri vydani
//  cislo meni.
//
//  Proc hlavicka a ne .ino: Arduino preklada kazdy .cpp jako samostatnou
//  prekladovou jednotku, takze #define zijici v .ino je pro ScreenSettings.cpp
//  nebo OTA.cpp neviditelny. Odsud ho vidi vsechny moduly a vsude se ukazuje
//  stejny retezec.
// =============================================================================
#pragma once

// Pri kazdem vydani zvysit (zobrazuje se na obrazovce Nastaveni, na OTA
// obrazovce a v seriovem vypisu pri startu). Zmenu popsat v CHANGELOG.md.
#define FW_VERSION "0.4.1"
