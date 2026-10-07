// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogMechaProto, Log, All);

//Object channel of the weapon projectiles, defined in DefaultEngine.ini [/Script/Engine.CollisionProfile]
constexpr ECollisionChannel ECC_Projectile = ECC_GameTraceChannel1;
