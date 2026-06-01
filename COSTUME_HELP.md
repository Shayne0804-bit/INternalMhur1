# COSTUME SYSTEM - COMPLETE DOCUMENTATION

## Overview
The costume system maps each character (Ch001-Ch038, Ch111) to their available costume codes. The system is implemented in `CostumeHelper.h` and integrated into the ImGui menu with dynamic costume selection for each character variant.

---

## Costume Mapping Reference

### Ch001 - Izuku Midoriya (104 costumes)
- Categories: Default (1000000-1000006), Hero Costume (1001100-1001106, 1003100-1003106, etc.)
- Notable: Multiple outfit variants, various transformation states

### Ch002 - Katsuki Bakugo (104 costumes)
- Categories: Default (2000000-2000006), Casual (2000100-2000106), Hero Suit (2001100-2001106)
- Notable: Explosive outfit variations

### Ch003 - Shoto Todoroki (83 costumes)
- Categories: Default (3000000-3000006), Hero Costume (3000100-3000106, 3001100-3001106)
- Notable: Hot & Cold balance themes

### Ch004 - Ochaco Uraraka (76 costumes)
- Categories: Default (4000000-4000006), Hero Costume (4000100-4000106, 4001100-4001106)

### Ch005 - Tsuyu Asui (46 costumes)
- Categories: Default (5000000-5000006), Combat Suit (5000100-5000106, 5004100-5004106)

### Ch006 - Tenya Iida (73 costumes)
- Categories: Default (6000000-6000006), Hero Costume (6000100-6000106, 6005100-6005106)

### Ch007 - Denki Kaminari (46 costumes)
- Categories: Default (7000000-7000006), Hero Costume (7000100-7000106)

### Ch008 - Jirou Kyoka (67 costumes)
- Categories: Default (8000000-8000006), Hero Costume (8002100-8002106)

### Ch010 - Momo Yaoyorozu (37 costumes)
- Categories: Default (10000000-10000006), Battle Outfit (10100100-10100106)

### Ch011 - Fumikage Tokoyami (16 costumes)
- Categories: Default (11000000-11000006), Hero Costume (11000100-11000106)

### Ch012 - Sero Hanta (70 costumes)
- Categories: Default (12000000-12000006), Battle Suit (12004100-12004106)

### Ch013 - Koda Koji (67 costumes)
- Categories: Default (13000000-13000006), Combat Outfit (13001100-13001106)

### Ch015 - Toru Hagakure (55 costumes)
- Categories: Default (15000000-15000006), Hero Costume (15000100-15000106)
- Note: Some costumes have variant codes (15000001, 15000003, etc.)

### Ch016 - Ojiro Mashirao (32 costumes)
- Categories: Default (16000000-16000006), Battle Suit (16000100-16000106)

### Ch017 - Aoyama Yuuga (55 costumes)
- Categories: Default (17000000-17000006), Hero Costume (17000100-17000106)

### Ch018 - Sato Rikido (55 costumes)
- Categories: Default (18000000-18000006), Battle Outfit (18100100-18100106)

### Ch023 - Present Mic (52 costumes)
- Categories: Default (23000000-23000006), Radio Show (23000100-23000106)

### Ch024 - Dabi (31 costumes)
- Categories: Default (24000000-24000006), Combat (24100100-24100106)

### Ch025 - Toga Himiko (37 costumes)
- Categories: Default (25000000-25000006), Battle Outfit (25303100-25303106)

### Ch026 - Twice (30 costumes)
- Categories: Default (26000000-26000006), Combat Suit (26100100-26100106)

### Ch034 - Star and Stripe (31 costumes)
- Categories: Default (34000000-34000006), Battle Mode (34000100-34000106)

### Ch037 - Mirko (Legacy) (37 costumes)
- Categories: Default (37000000-37000006), Combat (37000100-37000106)

### Ch038 - Shigaraki (52 costumes)
- Categories: Default (38000000-38000006), Battle Outfit (38100100-38100106)

### **Ch111 - Mirko (NEW - 18 costumes)**
- Categories: Default (111000000-111000006), Casual (111000100-111000106), Special (111622100-111622106)
- **This is the new character added to the system**
- Note: Follows the standard costume naming convention with base ID 111

---

## Costume Code Format

Each costume code encodes information:
- **First digit(s)**: Character ID (1=Ch001, 2=Ch002, ..., 111=Ch111)
- **Middle digits**: Outfit category (0=Default, 1=Hero, 2=Casual, 3=Special, etc.)
- **Last digit(s)**: Color/variant variant (00=base, 02=variant2, 03=variant3, etc.)

### Examples:
```
1000000 = Ch001 (Izuku) - Default outfit, base color
1000002 = Ch001 (Izuku) - Default outfit, color variant 2
1001100 = Ch001 (Izuku) - Hero outfit, variant 1
111000000 = Ch111 (Mirko) - Default outfit, base color
111622100 = Ch111 (Mirko) - Special/Event outfit, variant 1
```

---

## Implementation

### File: `src/Utils/CostumeHelper.h`
Contains:
- `CostumeHelper::CHARACTER_COSTUMES` - Static map of all character costumes
- `GetCostumesForCharacter(characterId)` - Returns vector of costume codes for a character
- `GetDefaultCostumeForCharacter(characterId)` - Returns first costume code
- `FormatCostumeName(costumeCode, index)` - Formats costume display name

### Menu Integration
Costume selectors added to 4 menu sections:
1. **OWNER CHARACTER SWAP** - Change own character with costume
2. **APPLY TO SPECIFIC PLAYER** - Change specific player with costume
3. **APPLY TO ALL PLAYERS** - Change all players with same costume
4. **APPLY TO TEAM** - Change team with same costume

Each section dynamically updates the costume list when character is selected.

---

## Usage in Menu

### Adding Costume Selection
When a character is selected from the Character/Variation dropdown:
1. System retrieves all available costumes for that character
2. Costume dropdown updates with formatted names
3. User selects desired costume from dropdown
4. Selected costume code is passed to the apply function

### Example Flow:
```
1. Select "Izuku Midoriya - Variation 0"
   → System loads 104 costumes for Ch001
   
2. Costume dropdown shows:
   - Costume #1 (1000000)
   - Costume #2 (1000002)
   - Costume #3 (1000003)
   - ... and 101 more
   
3. Select "Costume #42 (1600100)"
   
4. Click "APPLY CONFIGURATION"
   → Character changes to Izuku with costume code 1600100
```

---

## Adding New Costumes

To add new costumes for existing or new characters:

1. **Update `CostumeHelper.h`**:
   ```cpp
   // In CHARACTER_COSTUMES map:
   {characterId, {costume1, costume2, costume3, ...}},
   ```

2. **Add to CharacterId enum** (if new character):
   ```cpp
   Ch112 = 112,  // New character
   ```

3. **Rebuild project** - Menu will automatically pick up new costumes

---

## Character Stats Reference

| Character | ID | Total Costumes | Default Code | Status |
|-----------|----|----|---|---|
| Izuku Midoriya | 1 | 104 | 1000000 | ✓ |
| Katsuki Bakugo | 2 | 104 | 2000000 | ✓ |
| Shoto Todoroki | 3 | 83 | 3000000 | ✓ |
| Ochaco Uraraka | 4 | 76 | 4000000 | ✓ |
| Tsuyu Asui | 5 | 46 | 5000000 | ✓ |
| Tenya Iida | 6 | 73 | 6000000 | ✓ |
| Denki Kaminari | 7 | 46 | 7000000 | ✓ |
| Jirou Kyoka | 8 | 67 | 8000000 | ✓ |
| Momo Yaoyorozu | 10 | 37 | 10000000 | ✓ |
| Fumikage Tokoyami | 11 | 16 | 11000000 | ✓ |
| Sero Hanta | 12 | 70 | 12000000 | ✓ |
| Koda Koji | 13 | 67 | 13000000 | ✓ |
| Toru Hagakure | 15 | 55 | 15000000 | ✓ |
| Ojiro Mashirao | 16 | 32 | 16000000 | ✓ |
| Aoyama Yuuga | 17 | 55 | 17000000 | ✓ |
| Sato Rikido | 18 | 55 | 18000000 | ✓ |
| Present Mic | 23 | 52 | 23000000 | ✓ |
| Dabi | 24 | 31 | 24000000 | ✓ |
| Toga Himiko | 25 | 37 | 25000000 | ✓ |
| Twice | 26 | 30 | 26000000 | ✓ |
| Star and Stripe | 34 | 31 | 34000000 | ✓ |
| Mirko (Legacy) | 37 | 37 | 37000000 | ✓ |
| Shigaraki | 38 | 52 | 38000000 | ✓ |
| **Mirko (NEW)** | **111** | **18** | **111000000** | **✓ NEW** |

---

## Costume Data Source

Original costume data extracted from: `src/Hacks/costume.txt`

This file contains complete, validated costume codes for all 23 supported characters with multiple variations each.

---

## Related Files

- `src/Utils/CostumeHelper.h` - Costume mapping system
- `src/Menu/ImGuiMenu.cpp` - Menu integration
- `src/Hacks/InGameModuleHacks.h/cpp` - Character application functions
- `src/Hacks/costume.txt` - Raw costume data reference

