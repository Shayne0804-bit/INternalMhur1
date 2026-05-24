# Ch010 Character - ACh010Special Class Analysis

## Overview
Ch010 is a playable character with 2 variations (Variation 0 and Variation 1). The character has multiple special abilities implemented through a comprehensive system of bullet/projectile classes and generators.

---

## ACh010Special Class Definition

**File:** `InGameModule_classes.hpp` (Line 18108-18127)

```cpp
// Class InGameModule.Ch010Special
// 0x00C0 (0x1DC0 - 0x1D00)
class ACh010Special final : public ABullet
{
public:
    float _supplySpeed;  // 0x1D00(0x0004)
    // Inherits from ABullet - all bullet properties and methods

public:
    static class UClass* StaticClass()
    {
        STATIC_CLASS_IMPL("Ch010Special")
    }
    
    static const class FName& StaticName()
    {
        STATIC_NAME_IMPL(L"Ch010Special")
    }
    
    static class ACh010Special* GetDefaultObj()
    {
        return GetDefaultObjImpl<ACh010Special>();
    }
};
```

### Class Properties:
- **Base Class:** `ABullet` (inherits all bullet mechanics)
- **Class Size:** 0x1DC0 bytes
- **Single Property:** 
  - `_supplySpeed` (float, offset 0x1D00): Controls the speed at which this special projectile moves or supplies

---

## ACh010SpecialGen Class Definition

**File:** `InGameModule_classes.hpp` (Line 5603-5622)

```cpp
// Class InGameModule.Ch010SpecialGen
// 0x0010 (0x04B0 - 0x04A0)
class ACh010SpecialGen final : public AProjectileGeneratorBattle
{
public:
    struct FVector _PutPosition;  // 0x04A0(0x000C)
    float _returnDist;             // 0x04AC(0x0004)

public:
    static class UClass* StaticClass()
    {
        STATIC_CLASS_IMPL("Ch010SpecialGen")
    }
    
    static const class FName& StaticName()
    {
        STATIC_NAME_IMPL(L"Ch010SpecialGen")
    }
    
    static class ACh010SpecialGen* GetDefaultObj()
    {
        return GetDefaultObjImpl<ACh010SpecialGen>();
    }
};
```

### Class Properties:
- **Base Class:** `AProjectileGeneratorBattle`
- **Class Size:** 0x04B0 bytes
- **Properties:**
  - `_PutPosition` (FVector, offset 0x04A0): Deployment position for the special projectile
  - `_returnDist` (float, offset 0x04AC): Distance for projectile return/retreat behavior

---

## Ch010 Special Abilities System

### 1. **Ch010Special** (Special Attack)
- **Class:** `ACh010Special` : `ABullet`
- **Generator:** `ACh010SpecialGen` : `AProjectileGeneratorBattle`
- **Function:** Primary special ability that creates bullet projectiles
- **Key Parameters:**
  - Supply Speed control via `_supplySpeed`
  - Deployment position controlled by `_PutPosition`
  - Return distance defined by `_returnDist`

### 2. **Ch010Unique1** (First Unique Ability)
- **Class:** `ACh010Unique1` : `ABullet`
- **Generator:** `ACh010Unique1Gen` : `AProjectileGeneratorBattle`
- **Properties:**
  ```cpp
  float _checkHight;        // Height check threshold
  float _checkInterval;     // Time interval between height checks
  ```
- **Function:** Creates projectiles with height-based detection mechanics

### 3. **Ch010Unique2** (Second Unique Ability - Shield)
- **Class:** `ACh010Unique2` : `ACustomBullet`
- **Generator:** `ACh010Unique2Gen` : `AShieldGen`
- **Properties:**
  ```cpp
  UShieldComponent* _shieldComponent;           // Projectile blocking shield
  UShapeComponent* _pProjectileBlock;           // Collision shape for blocks
  UCustomParticleSystemComponent* _pShieldSonic;  // Shield VFX
  USceneComponent* _installedPoint;             // Installation anchor point
  ```
- **Related:** `ACh010Unique2Put` spawns from this shield
- **Sub-generator:** `ACh010Unique2PutGen` : `AProjectileGeneratorBattle`
  - `_oneSideShieldNum`: Number of shields per side
  - `_checkShieldWidth`: Width check for shield placement
  - `_putHight`: Height of placement

### 4. **Ch010Unique3** (Third Unique Ability - Complex Projectile)
- **Class:** `ACh010Unique3` : `ACustomBullet`
- **Generator:** `ACh010Unique3Gen` : `AProjectileGeneratorBattle`
- **Properties:**
  ```cpp
  float _easingDelayTime;           // Animation delay
  EEasingFunc _baseEasingType;      // Base movement easing function
  float _baseEasingRate;            // Base easing rate
  float _baseEasingTime;            // Base easing duration
  EEasingFunc _barrelEasingType;    // Barrel rotation easing
  float _barrelEasingRate;          // Barrel rotation rate
  float _barrelEasingTime;          // Barrel rotation time
  FVector _ShotBarrelPosition;      // Barrel firing position
  float _barrelPichMin;             // Min barrel pitch angle
  float _barrelPichMax;             // Max barrel pitch angle
  FAtomSEInfo _onEasingSe;          // Sound effect info
  UCurveFloat* _shotKnockBackCurve; // Knockback curve
  float _shotKnockBackDist;         // Knockback distance
  float _barrelEndPitchAng;         // Final barrel pitch angle
  UCustomParticleSystemComponent* _baseParticle;     // Base VFX
  UCustomParticleSystemComponent* _barrelParticle;   // Barrel VFX
  UPredictionLineComponent* _pPredictionLine;        // Prediction line (aiming aid)
  ```
- **Features:** Complex rotating barrel system with easing animations
- **Child Classes:**
  - `ACh010Unique3Shot` : `ACustomBullet` - Shot projectile
    - `_childGeneratorName`: Child generator reference
    - `_grandChildGeneratorName`: Grand-child generator reference
  - `ACh010Unique3GroundChild` : `ACustomBullet` - Ground impact variant
  - `ACh010Unique3ShotGen` & `ACh010Unique3ShotGrandChildGen` - Generators

---

## Unique Ability Variants (Variation 0)

### **UCh010_Var0_RollSlotUniqueSkill** (Roll Slot Unique Skill)
- **Base Class:** `UCharacterRollSlotUniqueSkillBase`
- **File:** [InGameModule_classes.hpp](InGameModule_classes.hpp#L18069)
- **Type:** Character roll/dodge special ability
- **Blueprint:** `UBP_Ch010_Variation0_UniqueSkill_C`

---

## Related Data Structures

### **FCh010Unique2PutParts** (Shield Part Structure)
```cpp
struct FCh010Unique2PutParts final
{
    class UCustomParticleSystemComponent* pParticle;  // 0x0000(0x0008)
    uint8 Pad_8[0x10];
};
```
**Purpose:** Defines individual shield parts with particle effects

### **FCh010Unique2PutGenRep** (Shield Generator Network Replication)
```cpp
struct FCh010Unique2PutGenRep final
{
    uint8 OpenNumR;                        // Right side open count
    uint8 OpenNumL;                        // Left side open count
    FVector_NetQuantize100 locate;         // Placement location
    float Yaw;                             // Rotation yaw angle
};
```
**Purpose:** Network synchronization for shield placement

### **FCh010Unique3GenRep** (Unique3 Generator Network Replication)
```cpp
struct FCh010Unique3GenRep final
{
    int8 serialID;                         // Serial identification
    FVector_NetQuantize100 fixCreateLocate;  // Creation location
    FVector_NetQuantize100 fixTargetLocate;  // Target location
};
```
**Purpose:** Network sync for barrel projectile trajectory

### **FCh010Unique3ShotGenRep** (Unique3 Shot Generator Network Replication)
```cpp
struct FCh010Unique3ShotGenRep final
{
    int8 serialID;                  // Serial ID
    float addtime;                  // Additional time
    int32 Seed;                     // Random seed
};
```
**Purpose:** Network synchronization for shot generation

### **FCh010Unique3ShotGrandChildGenRep** (Grand-Child Shot Network Replication)
```cpp
struct FCh010Unique3ShotGrandChildGenRep final
{
    int8 serialID;                       // Serial ID
    int8 Seed;                           // Random seed
    FVector_NetQuantize10 Normal;        // Direction normal vector
};
```
**Purpose:** Network sync for recursive/child projectile spawning

---

## Character Variations

### **Variation Mapping**
```
Ch010_Variation0 = EVariationCharacterId::18 (SDK ID)
Ch010_Variation1 = EVariationCharacterId::19 (SDK ID)
ECharacterId::Ch010 = 10
```

---

## Ability Hierarchy & Cross-References

```
ACh010Special (Main Special Ability)
├── ACh010SpecialGen (Generator)
├── Properties: _supplySpeed, _PutPosition, _returnDist

ACh010Unique1 (First Unique)
├── ACh010Unique1Gen (Generator)
├── Properties: _checkHight, _checkInterval

ACh010Unique2 (Second Unique - Shield)
├── ACh010Unique2Gen (Generator)
├── ACh010Unique2Put (Shield projectile)
│   ├── ACh010Unique2PutGen (Generator)
│   └── Properties: _RPartsTbl[], _LPartsTbl[]
├── Properties: _shieldComponent, _pProjectileBlock, _pShieldSonic

ACh010Unique3 (Third Unique - Barrel System)
├── ACh010Unique3Gen (Generator with complex controls)
├── ACh010Unique3Shot (Shot projectile)
│   ├── ACh010Unique3ShotGen (Generator)
│   ├── ACh010Unique3ShotGrandChildGen (Child bullet generator)
│   └── _childGeneratorName, _grandChildGeneratorName
├── ACh010Unique3GroundChild (Ground impact variant)
├── Properties: Easing params, barrel physics, prediction line, particles
```

---

## Network RPC Methods

### **ACh010Unique3Gen**
```cpp
void MakingBreak_RPC();                          // Break signal (Net, NetReliable)
void MakingComlate_RPC(const FCh010Unique3GenRep& gen);  // Complete signal (Net, NetReliable)
void OnRep_MakingBreak();                        // Replication callback
void OnRep_MakingComlate();                      // Completion callback
```

### **ACh010Unique2PutGen**
```cpp
void CreateBullet_RPC(const FCh010Unique2PutGenRep& gen);  // Create shield bullet (Net, NetReliable)
void OnRep_CreateBullet();                       // Creation callback
```

### **ACh010Unique3ShotGen**
```cpp
void Create_RPC(const FCh010Unique3ShotGenRep& gen);     // Create shot (Net, NetReliable)
void OnRep_Create();                             // Creation callback
```

### **ACh010Unique3ShotGrandChildGen**
```cpp
void Create_RPC(const FCh010Unique3ShotGrandChildGenRep& gen);  // Create child shot
void OnRep_Create();
```

---

## Class Size Summary

| Class | Size | Offset |
|-------|------|--------|
| ACh010Special | 0x1DC0 | +0xC0 |
| ACh010SpecialGen | 0x04B0 | +0x10 |
| ACh010Unique1 | 0x1D10 | +0x10 |
| ACh010Unique1Gen | 0x04A0 | +0x00 |
| ACh010Unique2 | 0x1F20 | +0x20 |
| ACh010Unique2Gen | 0x0550 | +0x50 |
| ACh010Unique2Put | 0x2050 | +0x150 |
| ACh010Unique2PutGen | 0x04D0 | +0x30 |
| ACh010Unique3 | 0x2180 | +0x280 |
| ACh010Unique3Gen | 0x0540 | +0xA0 |
| ACh010Unique3Shot | 0x1FA0 | +0xA0 |

---

## Key Characteristics

1. **Multi-Layered Ability System**: Ch010 has 4 main abilities (Special, Unique1, Unique2, Unique3)
2. **Shield Mechanics**: Unique2 uses a shield component with left/right symmetry
3. **Complex Barrel Rotation**: Unique3 features easing animations and prediction lines
4. **Network Synchronized**: All generators use RPC for multiplayer synchronization
5. **Recursive Projectiles**: Unique3Shot generates child and grand-child projectiles
6. **Particle Effects**: Multiple custom particle system components for VFX
7. **Physics Integration**: Uses easing functions, knockback curves, and collision detection

---

## GObjects Registry Entries

- **Ch010_Var0_RollSlotUniqueSkill**: Class [00000ED2]
- **Ch010Special**: Class [00000ED3]
- **Ch010SpecialGen**: Class [00000ED4]
- **Ch010Unique1**: Class [00000ED5]
- **Ch010Unique1Gen**: Class [00000ED6]
- **Ch010Unique2**: Class [00000ED7]
- **WrappedCh010Unique2Gen**: Class [00000ED8]
- **Ch010Unique2Gen**: Class [00000ED9]
- **Ch010Unique2Put**: Class [00000EDA]
- **Ch010Unique2PutGen**: Class [00000EDB]
- **Ch010Unique3**: Class [00000EDC]
- **WrappedCh010Unique3Gen**: Class [00000EDD]
- **Ch010Unique3Gen**: Class [00000EDE]
- **Ch010Unique3GroundChild**: Class [00000EDF]
- **Ch010Unique3Shot**: Class [00000EE0]
- **Ch010Unique3ShotChildGen**: Class [00000EE1]
- **Ch010Unique3ShotGen**: Class [00000EE2]
- **Ch010Unique3ShotGrandChildGen**: Class [00000EE3]

