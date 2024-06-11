// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.


#include "VisionComponent.h"

#include "FogOfWar.h"
#include "Utils/ManagerComponent.h"
#include "Utils/ManagerStatics.h"

void UVisionComponent::BeginPlay()
{
	Super::BeginPlay();

	auto GameManager = UManagerStatics::GetGameManager(this);
	GameManager->WaitForRegistrationAsync<AFogOfWar>(FObjectRegisteredInManager::CreateWeakLambda(this,
		[this](UObject* Object)
		{
			FogOfWar = Cast<AFogOfWar>(Object);

			FogOfWar->RegisterVisionComponent(this);
		}));
}

void UVisionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (IsValid(FogOfWar))
	{
		FogOfWar->UnregisterVisionComponent(this);
	}
}

#if WITH_EDITOR
void UVisionComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (!GetWorld())
	{
		Super::PostEditChangeChainProperty(PropertyChangedEvent);
		return;
	}

	if (!GetWorld()->IsGameWorld())
	{
		Super::PostEditChangeChainProperty(PropertyChangedEvent);
		return;
	}
	// I'm not calling Super here because Unreal Engine is overcomplicated garbage and for some reason it recreates the component when I call Super
	// explanation with another solution: https://victor-istomin.github.io/c-with-crosses/posts/ue-post-edit-property/#conclusion-how-does-the-editor-change-a-property-of-a-_blueprint-constructed_-component

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UVisionComponent, SightRadius))
	{
		if (GetWorld()->IsGameWorld())
		{
			ReregisterToFogOfWar();
		}
	}
}
#endif

void UVisionComponent::SetSightRadius(float NewSightRadius)
{
	SightRadius = NewSightRadius;
	ReregisterToFogOfWar();
}

void UVisionComponent::ReregisterToFogOfWar()
{
	if (IsValid(FogOfWar))
	{
		FogOfWar->UnregisterVisionComponent(this);
		FogOfWar->RegisterVisionComponent(this);
	}
}
