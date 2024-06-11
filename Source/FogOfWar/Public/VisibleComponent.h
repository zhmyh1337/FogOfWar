// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisibleComponent.generated.h"


class AFogOfWar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVisibilityChanged, bool, bNewIsVisible);

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOGOFWAR_API UVisibleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bManageOwnerVisibility = true;

	UPROPERTY(BlueprintAssignable)
	FVisibilityChanged OnVisibilityChanged;

	UFUNCTION(BlueprintPure)
	FORCEINLINE_DEBUGGABLE bool IsVisible() const { return bIsVisible; }

public:
	UVisibleComponent();

protected:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void UpdateVisibility(bool bForceChanged = false);

	void SetIsVisible(bool bNewIsVisible, bool bForceChanged = false);

protected:
	bool bIsVisible = true;

	UPROPERTY()
	AFogOfWar* FogOfWar = nullptr;
};
