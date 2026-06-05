/*  JucePluginDefines.h
    Fuente canónica de todos los valores de identidad del plugin.

    En un build CMake + juce_add_plugin() estas macros también se inyectan
    como flags de compilador (-DJucePlugin_Name="SideMk" etc.).  Los guards
    #ifndef garantizan que los flags del compilador (que llegan antes que
    cualquier #include) siempre tienen prioridad, por lo que no hay conflicto
    de redefinición.

    Si modificas un valor aquí, actualiza CMakeLists.txt (y viceversa):
      JucePlugin_Name              ↔  PRODUCT_NAME
      JucePlugin_Manufacturer      ↔  COMPANY_NAME
      JucePlugin_ManufacturerCode  ↔  PLUGIN_MANUFACTURER_CODE  "Yrcm"
      JucePlugin_PluginCode        ↔  PLUGIN_CODE               "Smk1"
*/

#pragma once

// ─── Identidad ───────────────────────────────────────────────────────────────
#ifndef JucePlugin_Name
 #define JucePlugin_Name                  "SideMk"
#endif

#ifndef JucePlugin_Desc
 #define JucePlugin_Desc                  "Sidechain Compressor"
#endif

#ifndef JucePlugin_Manufacturer
 #define JucePlugin_Manufacturer          "YourCompany"
#endif

#ifndef JucePlugin_ManufacturerWebsite
 #define JucePlugin_ManufacturerWebsite   "https://example.com"
#endif

#ifndef JucePlugin_ManufacturerEmail
 #define JucePlugin_ManufacturerEmail     ""
#endif

// Códigos OSType de 4 caracteres — deben coincidir con CMakeLists.txt
//   'Y'=0x59  'r'=0x72  'c'=0x63  'm'=0x6D  →  "Yrcm" = 0x5972636D
//   'S'=0x53  'm'=0x6D  'k'=0x6B  '1'=0x31  →  "Smk1" = 0x536D6B31
#ifndef JucePlugin_ManufacturerCode
 #define JucePlugin_ManufacturerCode      0x5972636D
#endif

#ifndef JucePlugin_PluginCode
 #define JucePlugin_PluginCode            0x536D6B31
#endif

// ─── Versión ─────────────────────────────────────────────────────────────────
#ifndef JucePlugin_VersionCode
 #define JucePlugin_VersionCode           0x010000   // 1.0.0  (major.minor.patch)
#endif

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString         "1.0.0"
#endif

// ─── Capacidades ─────────────────────────────────────────────────────────────
#ifndef JucePlugin_IsSynth
 #define JucePlugin_IsSynth               0
#endif

#ifndef JucePlugin_WantsMidiInput
 #define JucePlugin_WantsMidiInput        0
#endif

#ifndef JucePlugin_ProducesMidiOutput
 #define JucePlugin_ProducesMidiOutput    0
#endif

#ifndef JucePlugin_IsMidiEffect
 #define JucePlugin_IsMidiEffect          0
#endif

#ifndef JucePlugin_EditorRequiresKeyboardFocus
 #define JucePlugin_EditorRequiresKeyboardFocus 0
#endif

#ifndef JucePlugin_SilenceInProducesSilenceOut
 #define JucePlugin_SilenceInProducesSilenceOut 0
#endif

// ─── Layout de canales ───────────────────────────────────────────────────────
// Entradas: Main L, Main R, Sidechain L, Sidechain R  (4 canales)
// Salidas:  Main L, Main R                             (2 canales)
#ifndef JucePlugin_MaxNumInputChannels
 #define JucePlugin_MaxNumInputChannels         4
#endif

#ifndef JucePlugin_MaxNumOutputChannels
 #define JucePlugin_MaxNumOutputChannels        2
#endif

// Par {entradas_main, salidas_main} del bus principal (el sidechain es extra)
#ifndef JucePlugin_PreferredChannelConfigurations
 #define JucePlugin_PreferredChannelConfigurations  {2, 2}
#endif

// ─── Flags de formato de build ───────────────────────────────────────────────
#ifndef JucePlugin_Build_VST
 #define JucePlugin_Build_VST             0
#endif

#ifndef JucePlugin_Build_VST3
 #define JucePlugin_Build_VST3            1
#endif

#ifndef JucePlugin_Build_AU
 #define JucePlugin_Build_AU              0   // solo macOS
#endif

#ifndef JucePlugin_Build_AUv3
 #define JucePlugin_Build_AUv3            0
#endif

#ifndef JucePlugin_Build_AAX
 #define JucePlugin_Build_AAX             0
#endif

#ifndef JucePlugin_Build_Standalone
 #define JucePlugin_Build_Standalone      1
#endif

// ─── Categorías de host ──────────────────────────────────────────────────────
#ifndef JucePlugin_Vst3Category
 #define JucePlugin_Vst3Category          "Fx|Dynamics"
#endif

// ─── AU (macOS) — sin efecto en Windows ──────────────────────────────────────
#ifndef JucePlugin_AUMainType
 #define JucePlugin_AUMainType            'aufx'
#endif

#ifndef JucePlugin_AUSubType
 #define JucePlugin_AUSubType             JucePlugin_PluginCode
#endif

#ifndef JucePlugin_AUExportPrefix
 #define JucePlugin_AUExportPrefix        SideMkAU
#endif

#ifndef JucePlugin_AUExportPrefixQuoted
 #define JucePlugin_AUExportPrefixQuoted  "SideMkAU"
#endif

#ifndef JucePlugin_AUManufacturerCode
 #define JucePlugin_AUManufacturerCode    JucePlugin_ManufacturerCode
#endif

#ifndef JucePlugin_CFBundleIdentifier
 #define JucePlugin_CFBundleIdentifier    "com.yourcompany.sidemk"
#endif
