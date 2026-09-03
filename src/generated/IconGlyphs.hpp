// Generated file. Do not edit by hand.
#pragma once

#include <cstdint>

namespace zonai_survey::icons {

enum class Icon : std::uint8_t {
    Material,
    Mushroom,
    Plant,
    Wood,
    Glow,
    Fire,
    Shock,
    Ice,
    Splash,
    Fish,
    Critter,
    Ore,
    Meat,
    Food,
    Enemy,
    MonsterPart,
    Sword,
    BigSword,
    Spear,
    Shield,
    Bow,
    Arrow,
    Armor,
    Chest,
    Rupee,
    Zonai,
    KeyItem,
    Stamina,
    Heart,
    Count,
};

inline constexpr std::uint8_t kIconCount = 29;

inline constexpr const char* kIconUtf8[kIconCount] = {
    "\xD0\x90",  // Material: IconMaterial_72x72 (U+0410, cell 152)
    "\xD0\x91",  // Mushroom: MapIconBuildingHouse_58x58 (U+0411, cell 153)
    "\xD0\x92",  // Plant: MapIconBuildingNewsPaper_58x58 (U+0412, cell 154)
    "\xD0\x93",  // Wood: MapIconLeaf_72x72 (U+0413, cell 155)
    "\xD0\x94",  // Glow: BonusIcon72_Luminous_00 (U+0414, cell 156)
    "\xD0\x95",  // Fire: BonusIcon72_Fire_00 (U+0415, cell 157)
    "\xD0\x96",  // Shock: BonusIcon72_Elect_00 (U+0416, cell 158)
    "\xD0\x97",  // Ice: BonusIcon72_Cool_00 (U+0417, cell 159)
    "\xD0\x98",  // Splash: BonusIcon72_Slip_00 (U+0418, cell 160)
    "\xD0\x99",  // Fish: BonusIcon72_Swim_00 (U+0419, cell 161)
    "\xD0\x9A",  // Critter: MapIconFairy_72x72 (U+041A, cell 162)
    "\xD0\x9B",  // Ore: MapIconOre_72x72 (U+041B, cell 163)
    "\xD0\x9C",  // Meat: MapIconShopMilk_58x58 (U+041C, cell 164)
    "\xD0\x9D",  // Food: IconCuisine_72x72 (U+041D, cell 165)
    "\xD0\x9E",  // Enemy: BonusIcon72_AttackSp_00 (U+041E, cell 166)
    "\xD0\x9F",  // MonsterPart: MapIconShopMonster_58x58 (U+041F, cell 167)
    "\xD0\xA0",  // Sword: WeaponType_36x36_Sword^t (U+0420, cell 168)
    "\xD0\xA1",  // BigSword: WeaponType_36x36_BigSword^t (U+0421, cell 169)
    "\xD0\xA2",  // Spear: WeaponType_36x36_Spear^t (U+0422, cell 170)
    "\xD0\xA3",  // Shield: IconShield_72x72 (U+0423, cell 171)
    "\xD0\xA4",  // Bow: IconBow_72x72 (U+0424, cell 172)
    "\xD0\xA5",  // Arrow: IconArrow_72x72 (U+0425, cell 173)
    "\xD0\xA6",  // Armor: IconEquipment_72x72 (U+0426, cell 174)
    "\xD0\xA7",  // Chest: MapIconTreasureBox_72x72 (U+0427, cell 175)
    "\xD0\xA8",  // Rupee: BonusIcon72_Rupee_00 (U+0428, cell 176)
    "\xD0\xA9",  // Zonai: IconRecycle_72x72 (U+0429, cell 177)
    "\xD0\xAA",  // KeyItem: MapIconStar_72x72 (U+042A, cell 178)
    "\xD0\xAB",  // Stamina: BonusIcon72_Ganbari_00 (U+042B, cell 179)
    "\xD0\xAC",  // Heart: BonusIcon72_Heart_00 (U+042C, cell 180)
};

inline constexpr const char* kIconLabel[kIconCount] = {
    "Material",
    "Mushroom",
    "Herb / Flower",
    "Wood / Seed",
    "Brightbloom",
    "Fire",
    "Electric",
    "Chilly",
    "Water",
    "Fish",
    "Critter",
    "Ore / Gem",
    "Meat / Egg",
    "Cooked Food",
    "Enemy",
    "Monster Part",
    "Sword",
    "Two-Hander",
    "Spear",
    "Shield",
    "Bow",
    "Arrow",
    "Armor",
    "Chest",
    "Rupee",
    "Zonai Device",
    "Key Item",
    "Stamina Food",
    "Hearty Food",
};

inline const char* iconUtf8(std::uint8_t icon) {
    return icon < kIconCount ? kIconUtf8[icon] : kIconUtf8[0];
}

inline const char* iconLabel(std::uint8_t icon) {
    return icon < kIconCount ? kIconLabel[icon] : kIconLabel[0];
}

}  
