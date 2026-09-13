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
    "\xD0\x90",
    "\xD0\x91",
    "\xD0\x92",
    "\xD0\x93",
    "\xD0\x94",
    "\xD0\x95",
    "\xD0\x96",
    "\xD0\x97",
    "\xD0\x98",
    "\xD0\x99",
    "\xD0\x9A",
    "\xD0\x9B",
    "\xD0\x9C",
    "\xD0\x9D",
    "\xD0\x9E",
    "\xD0\x9F",
    "\xD0\xA0",
    "\xD0\xA1",
    "\xD0\xA2",
    "\xD0\xA3",
    "\xD0\xA4",
    "\xD0\xA5",
    "\xD0\xA6",
    "\xD0\xA7",
    "\xD0\xA8",
    "\xD0\xA9",
    "\xD0\xAA",
    "\xD0\xAB",
    "\xD0\xAC",
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
