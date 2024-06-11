// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ManagerStatics.generated.h"

/**
 * 
 */
UCLASS()
class FOGOFWAR_API UManagerStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContextObject"))
	static class UManagerComponent* GetGameManager(const UObject* WorldContextObject);
};
