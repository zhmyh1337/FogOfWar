// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ManagerComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogManagerComponent, Log, All)

DECLARE_DELEGATE_OneParam(FObjectRegisteredInManager, UObject* /*ResolvedObject*/);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOGOFWAR_API UManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void Register(TSubclassOf<UObject> Class, UObject* Object);

	template<typename T>
	void Register(T* Object)
	{
		Register(T::StaticClass(), Object);
	}

	UFUNCTION(BlueprintPure)
	UObject* Resolve(TSubclassOf<UObject> Class) const;

	template<typename T>
	T* Resolve() const
	{
		return Cast<T>(Resolve(T::StaticClass()));
	}

	void WaitForRegistrationAsync(TSubclassOf<UObject> Class, const FObjectRegisteredInManager& Callback);

	template<typename T>
	void WaitForRegistrationAsync(const FObjectRegisteredInManager& Callback)
	{
		WaitForRegistrationAsync(T::StaticClass(), Callback);
	}

private:
	UPROPERTY(VisibleInstanceOnly)
	TMap<TSubclassOf<UObject>, UObject*> RegisteredMap;

	TMap<TSubclassOf<UObject>, TArray<FObjectRegisteredInManager>> RegistrationCallbacks;
};
