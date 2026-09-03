// Generated file. Do not edit by hand.

#pragma once
#include <cstdint>

namespace zonai_survey::glyphs {

enum class GlyphClass : uint8_t {
    Material = 0,
    Food = 1,
    Weapon = 2,
    Shield = 3,
    Bow = 4,
    Arrow = 5,
    KeyItem = 6,
    SpecialParts = 7,
    Rupee = 8,
    Armor = 9,
    SpecialPower = 10,
    Ore = 11,
    Chest = 12,
    Enemy = 13,
    Count
};

struct NameEntry {
    uint32_t hash;        // FNV-1a 32 of the actor name
    uint16_t textOffset;  // into kNameBlob, or kNoName
    uint8_t  cls;         // GlyphClass
    uint8_t  value;       // the game's own SellingPrice, log-compressed to a byte
    uint8_t  icon;        // index into zonai_survey::icons::kIconUtf8
};

struct Placement {
    int16_t  x, y, z;   // world position in kPosScale units
    uint16_t name;      // index into kNames
    uint16_t flags;     // kFlag*
};

inline constexpr uint16_t kNoName = 0xFFFF;
inline constexpr uint16_t kFlagInChest = 0x0001;  // name is the CONTENTS
inline constexpr uint16_t kFlagNoName  = 0x0002;

inline constexpr float kPosScale = 0.25f;
inline constexpr float kCellSize = 256.0f;
inline constexpr int   kGridOriginCellX = -20;
inline constexpr int   kGridOriginCellZ = -16;
inline constexpr int   kGridWidth  = 40;
inline constexpr int   kGridHeight = 32;

inline constexpr uint32_t kNameCount = 2053;
inline constexpr uint32_t kPlacementCount = 38701;

inline constexpr uint32_t kFontGlyphCount = 3721;

extern const NameEntry kNames[kNameCount];
extern const char      kNameBlob[];
extern const Placement kPlacements[kPlacementCount];
extern const uint32_t  kCellStart[kGridWidth * kGridHeight + 1];
extern const uint16_t  kFontGlyphs[kFontGlyphCount];

} 
