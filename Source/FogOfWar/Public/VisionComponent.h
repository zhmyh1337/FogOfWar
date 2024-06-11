// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisionComponent.generated.h"


class AFogOfWar;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FOGOFWAR_API UVisionComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float SightRadius = 1000.0f;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

public:
	UFUNCTION(BlueprintPure)
	FORCEINLINE_DEBUGGABLE float GetSightRadius() const { return SightRadius; }

	UFUNCTION(BlueprintCallable)
	void SetSightRadius(float NewSightRadius);

protected:
	void ReregisterToFogOfWar();

private:
	UPROPERTY()
	AFogOfWar* FogOfWar = nullptr;
};
