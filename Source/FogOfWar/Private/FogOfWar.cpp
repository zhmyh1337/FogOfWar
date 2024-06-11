// Copyright 2024 zhmyh1337 (https://github.com/zhmyh1337/). All Rights Reserved.


#include "FogOfWar.h"

#include "VisionComponent.h"
#include "Components/BrushComponent.h"
#include "Components/PostProcessComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Utils/ManagerComponent.h"
#include "Utils/ManagerStatics.h"
#include "Utils/Macros.h"

DEFINE_LOG_CATEGORY(LogFogOfWar);

DECLARE_STATS_GROUP(TEXT("FogOfWar"), STATGROUP_FogOfWar, STATCAT_Advanced);

namespace Names
{
	DECLARE_STATIC_FNAME(FOW_AccumulatedMask);
	DECLARE_STATIC_FNAME(FOW_NewSnapshot);
	DECLARE_STATIC_FNAME(FOW_MinimalVisibility);
	DECLARE_STATIC_FNAME(FOW_NewSnapshotAbsorption);
	DECLARE_STATIC_FNAME(FOW_VisibilityTextureRenderTarget);
	DECLARE_STATIC_FNAME(FOW_PreFinalVisibilityTextureRenderTarget);
	DECLARE_STATIC_FNAME(FOW_FinalVisibilityTexture);
	DECLARE_STATIC_FNAME(FOW_NotVisibleRegionBrightness);
	DECLARE_STATIC_FNAME(FOW_GridResolution);
	DECLARE_STATIC_FNAME(FOW_TileSize);
	DECLARE_STATIC_FNAME(FOW_BottomLeftWorldLocation);
}

AFogOfWar::AFogOfWar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	PostProcess->SetupAttachment(RootComponent);
}

void AFogOfWar::RegisterVisionComponent(UVisionComponent* VisionComponent)
{
	if (RegisteredVisions.Contains(VisionComponent))
	{
		return;
	}
	RegisteredVisions.Add(VisionComponent, CreateVisionUnitDataFromVisionComponent(VisionComponent));

#if WITH_EDITORONLY_DATA
	RegisteredVisionsNum = RegisteredVisions.Num();
	TotalRegisteredVisionsCacheTilesNum += RegisteredVisions[VisionComponent].LocalAreaTilesCachedStates.Num();
#endif

	UE_LOG(LogFogOfWar, Log, TEXT("Registered %s with FogOfWar"), *VisionComponent->GetOwner()->GetName());
}

void AFogOfWar::UnregisterVisionComponent(UVisionComponent* VisionComponent)
{
	if (!ensure(RegisteredVisions.Contains(VisionComponent)))
	{
		return;
	}
	ResetCachedVisibilities(RegisteredVisions[VisionComponent]);

#if WITH_EDITORONLY_DATA
	TotalRegisteredVisionsCacheTilesNum -= RegisteredVisions[VisionComponent].LocalAreaTilesCachedStates.Num();
#endif

	RegisteredVisions.Remove(VisionComponent);

#if WITH_EDITORONLY_DATA
	RegisteredVisionsNum = RegisteredVisions.Num();
#endif

	UE_LOG(LogFogOfWar, Log, TEXT("Unregistered %s from FogOfWar"), *VisionComponent->GetOwner()->GetName());
}

bool AFogOfWar::IsLocationVisible(FVector WorldLocation)
{
	FIntVector2 TileIJ = ConvertWorldLocationToTileIJ(FVector2D(WorldLocation));
	if (!IsGlobalIJValid(TileIJ))
	{
		return false;
	}

	const FTile& Tile = GetGlobalTile(TileIJ);
	bool bIsVisible = Tile.VisibilityCounter > 0;
	return bIsVisible;
}

UTexture* AFogOfWar::GetFinalVisibilityTexture()
{
	return Cast<UTexture>(FinalVisibilityTextureRenderTarget);
}

void AFogOfWar::SetCommonMIDParameters(UMaterialInstanceDynamic* MID)
{
	MID->SetTextureParameterValue(Names::FOW_FinalVisibilityTexture, GetFinalVisibilityTexture());
	MID->SetVectorParameterValue(Names::FOW_GridResolution, FVector(GridResolution.X, GridResolution.Y, 0));
	MID->SetScalarParameterValue(Names::FOW_TileSize, TileSize);
	MID->SetVectorParameterValue(Names::FOW_BottomLeftWorldLocation, FVector(GridBottomLeftWorldLocation.X, GridBottomLeftWorldLocation.Y, 0));
}

void AFogOfWar::Activate()
{
	if (!ensure(!bActivated))
	{
		return;
	}
	bActivated = true;

	checkf(IsValid(GridVolume), TEXT("Volume was not set for the FogOfWar Volume"));
	check(TileSize > 0);

	Initialize();

	checkf(GridResolution.X + GridResolution.Y <= 10000, TEXT("Grid resolution is too big (possible int32 overflow when calculating square distance)"));

	const int GridTilesNum = GridResolution.X * GridResolution.Y;
	Tiles.SetNum(GridTilesNum);
	TextureDataBuffer.SetNum(GridTilesNum);

	for (int I = 0; I < GridResolution.X; I++)
	{
		for (int J = 0; J < GridResolution.Y; J++)
		{
			FTile& Tile = GetGlobalTile({ I, J });
			CalculateTileHeight(Tile, { I,J });
		}
	}

#if WITH_EDITORONLY_DATA
	HeightmapTexture = CreateSnapshotTexture();
	HeightmapTexture->Filter = TF_Nearest;
	WriteHeightmapDataToTexture(HeightmapTexture);
#endif

	SnapshotTexture = CreateSnapshotTexture();
	VisibilityTextureRenderTarget = CreateRenderTarget();
	PreFinalVisibilityTextureRenderTarget = CreateRenderTarget();
	FinalVisibilityTextureRenderTarget = CreateRenderTarget();

	InterpolationMID = UMaterialInstanceDynamic::Create(InterpolationMaterial, this);
	InterpolationMID->SetTextureParameterValue(Names::FOW_AccumulatedMask, VisibilityTextureRenderTarget);
	InterpolationMID->SetTextureParameterValue(Names::FOW_NewSnapshot, SnapshotTexture);

	AfterInterpolationMID = UMaterialInstanceDynamic::Create(AfterInterpolationMaterial, this);
	AfterInterpolationMID->SetTextureParameterValue(Names::FOW_VisibilityTextureRenderTarget, VisibilityTextureRenderTarget);
	AfterInterpolationMID->SetScalarParameterValue(Names::FOW_MinimalVisibility, MinimalVisibility);

	SuperSamplingMID = UMaterialInstanceDynamic::Create(SuperSamplingMaterial, this);
	SuperSamplingMID->SetTextureParameterValue(Names::FOW_PreFinalVisibilityTextureRenderTarget, PreFinalVisibilityTextureRenderTarget);
	SuperSamplingMID->SetVectorParameterValue(Names::FOW_GridResolution, FVector(GridResolution.X, GridResolution.Y, 0));

	PostProcessingMID = UMaterialInstanceDynamic::Create(PostProcessingMaterial, this);
	SetCommonMIDParameters(PostProcessingMID);
	PostProcessingMID->SetScalarParameterValue(Names::FOW_NotVisibleRegionBrightness, NotVisibleRegionBrightness);

	PostProcess->AddOrUpdateBlendable(PostProcessingMID);

	auto GameManager = UManagerStatics::GetGameManager(this);
	GameManager->Register<ThisClass>(this);
	PrimaryActorTick.SetTickFunctionEnable(true);
}

void AFogOfWar::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoActivate)
	{
		Activate();
	}
}

#if WITH_EDITOR
void AFogOfWar::RefreshVolumeInEditor()
{
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		Initialize();
	}
}

bool AFogOfWar::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	const FName PropertyName = InProperty->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, TileSize) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, GridVolume) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, InterpolationMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, AfterInterpolationMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, SuperSamplingMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, PostProcessingMaterial))
	{
		return !GetWorld() || !GetWorld()->IsGameWorld();
	}

	return true;
}

void AFogOfWar::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, MinimalVisibility))
		{
			if (IsValid(AfterInterpolationMID))
			{
				AfterInterpolationMID->SetScalarParameterValue(Names::FOW_MinimalVisibility, MinimalVisibility);
			}
			return;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, NotVisibleRegionBrightness))
		{
			if (IsValid(PostProcessingMID))
			{
				PostProcessingMID->SetScalarParameterValue(Names::FOW_NotVisibleRegionBrightness, NotVisibleRegionBrightness);
			}
			return;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, ApproximateSecondsToAbsorbNewSnapshot))
		{
			bFirstTick = true;
			return;
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, VisionBlockingDeltaHeightThreshold))
		{
			for (auto& [key, value] : RegisteredVisions)
			{
				ResetCachedVisibilities(value);
			}
			return;
		}
	}

	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		if (PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, TileSize) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(AFogOfWar, GridVolume))
		{
			RefreshVolumeInEditor();
			return;
		}
	}
}
#endif

void AFogOfWar::Tick(float DeltaSeconds)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Tick"), STAT_FogOfWarTick, STATGROUP_FogOfWar);

	Super::Tick(DeltaSeconds);

	for (auto& [VisionComponent, VisionUnitData] : RegisteredVisions)
	{
		FVector3d OwnerActorLocation = VisionComponent->GetOwner()->GetActorLocation();
		FIntVector2 GridIJ = ConvertWorldLocationToTileIJ(FVector2D(OwnerActorLocation));
		int GridIndex = GetGlobalIndex(GridIJ);

#if WITH_EDITORONLY_DATA
		if (!bDebugStressTestIgnoreCache)
#endif
			if (VisionUnitData.HasCachedData() && VisionUnitData.CachedOriginGlobalIndex == GridIndex)
			{
				// the actor didn't change the tile. skipping...
				continue;
			}

		UpdateVisibilities(OwnerActorLocation, VisionUnitData);
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pipeline"), STAT_FogOfWarPipeline, STATGROUP_FogOfWar);
		{
			// step 1: creating a snapshot texture from the newest vision data
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pipeline: step 1"), STAT_FogOfWarPipelineStep1, STATGROUP_FogOfWar);
			WriteVisionDataToTexture(SnapshotTexture);
		}
		{
			// step 2: interpolating the snapshot with the previous visibility texture (to avoid flickering)
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pipeline: step 2"), STAT_FogOfWarPipelineStep2, STATGROUP_FogOfWar);
			const float NewSnapshotAbsorption = bFirstTick ? 1.0f : FMath::Min(DeltaSeconds / ApproximateSecondsToAbsorbNewSnapshot, 1.0f);
			InterpolationMID->SetScalarParameterValue(Names::FOW_NewSnapshotAbsorption, NewSnapshotAbsorption);
			UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, VisibilityTextureRenderTarget, InterpolationMID);
		}
		{
			// step 3: cutting off the minimal visibility
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pipeline: step 3"), STAT_FogOfWarPipelineStep3, STATGROUP_FogOfWar);
			UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, PreFinalVisibilityTextureRenderTarget, AfterInterpolationMID);
		}
		{
			// step 4: super sampling
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Pipeline: step 4"), STAT_FogOfWarPipelineStep4, STATGROUP_FogOfWar);
			UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, FinalVisibilityTextureRenderTarget, SuperSamplingMID);
		}
	}

	bFirstTick = false;
}

void AFogOfWar::Initialize()
{
	if (!IsValid(GridVolume))
	{
		GridSize = FVector2D::Zero();
		GridBottomLeftWorldLocation = FVector2D::Zero();
		GridResolution = {};

		return;
	}

	UBrushComponent* VolumeBrush = GridVolume->GetBrushComponent();
	FBoxSphereBounds Bounds = VolumeBrush->CalcBounds(VolumeBrush->GetComponentTransform());

	GridSize = {
		Bounds.BoxExtent.X * 2,
		Bounds.BoxExtent.Y * 2
	};
	GridBottomLeftWorldLocation = {
		Bounds.Origin.X - GridSize.X / 2,
		Bounds.Origin.Y - GridSize.Y / 2
	};
	GridResolution = {
		FMath::CeilToInt32(GridSize.X / TileSize),
		FMath::CeilToInt32(GridSize.Y / TileSize)
	};
}

void AFogOfWar::ResetCachedVisibilities(FVisionUnitData& VisionUnitData)
{
	if (!VisionUnitData.HasCachedData())
	{
		return;
	}

	for (int I = 0; I < VisionUnitData.LocalAreaTilesResolution; I++)
	{
		for (int J = 0; J < VisionUnitData.LocalAreaTilesResolution; J++)
		{
			if (VisionUnitData.GetLocalTileState({ I, J }) == FVisionUnitData::TileState::Visible)
			{
				FIntVector2 GlobalIJ = VisionUnitData.LocalToGlobal({ I, J });
				FTile& GlobalTile = GetGlobalTile(GlobalIJ);
				checkSlow(GlobalTile.VisibilityCounter > 0);
				GlobalTile.VisibilityCounter--;
			}
		}
	}

	VisionUnitData.bHasCachedData = false;
}

void AFogOfWar::UpdateVisibilities(const FVector3d& OriginWorldLocation, FVisionUnitData& VisionUnitData)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UpdateVisibilities"), STAT_FogOfWarUpdateVisibilities, STATGROUP_FogOfWar);

	const FVector2f OriginGridLocation = ConvertWorldSpaceLocationToGridSpace(FVector2D(OriginWorldLocation));

	// check that we have allocated enough local area cached tiles to fit the radius. THIS IS A MUST!
	checkSlow(ConvertGridLocationToTileIJ(OriginGridLocation + VisionUnitData.GridSpaceRadius).X - ConvertGridLocationToTileIJ(OriginGridLocation - VisionUnitData.GridSpaceRadius).X + 1 <= VisionUnitData.LocalAreaTilesResolution);
	checkSlow(ConvertGridLocationToTileIJ(OriginGridLocation + VisionUnitData.GridSpaceRadius).Y - ConvertGridLocationToTileIJ(OriginGridLocation - VisionUnitData.GridSpaceRadius).Y + 1 <= VisionUnitData.LocalAreaTilesResolution);
	// check that we have allocated not too much local area cached tiles to fit the radius. this is not a must, but saves memory
	checkSlow(ConvertGridLocationToTileIJ(OriginGridLocation + VisionUnitData.GridSpaceRadius).X - ConvertGridLocationToTileIJ(OriginGridLocation - VisionUnitData.GridSpaceRadius).X + 1 + 2 > VisionUnitData.LocalAreaTilesResolution);
	checkSlow(ConvertGridLocationToTileIJ(OriginGridLocation + VisionUnitData.GridSpaceRadius).Y - ConvertGridLocationToTileIJ(OriginGridLocation - VisionUnitData.GridSpaceRadius).Y + 1 + 2 > VisionUnitData.LocalAreaTilesResolution);

	ResetCachedVisibilities(VisionUnitData);

	VisionUnitData.LocalAreaTilesCachedStates.Init(FVisionUnitData::TileState::Unknown, VisionUnitData.LocalAreaTilesCachedStates.Num());
	const FIntVector2 OriginGlobalIJ = ConvertGridLocationToTileIJ(OriginGridLocation);
	// if the vision unit is outside the grid, we ignore it (normally this shouldn't happen)
	if (!ensureMsgf(IsGlobalIJValid(OriginGlobalIJ), TEXT("Vision actor is outside the grid")))
	{
		return;
	}

	if (VisionUnitData.LocalAreaTilesResolution == 0)
	{
		return;
	}

	VisionUnitData.CachedOriginGlobalIndex = GetGlobalIndex(OriginGlobalIJ);
	// the "bottom-left" tile of the local area in the global grid space
	VisionUnitData.LocalAreaCachedMinIJ = ConvertGridLocationToTileIJ(OriginGridLocation - VisionUnitData.GridSpaceRadius);
	const FIntVector2 OriginLocalIJ = VisionUnitData.GlobalToLocal(OriginGlobalIJ);

	// we see the tile we're currently on
	VisionUnitData.GetLocalTileState(OriginLocalIJ) = FVisionUnitData::TileState::Visible;

	const float GridSpaceRadiusSqr = FMath::Square(VisionUnitData.GridSpaceRadius);

	// going in spiral (spooky code)
	{
#if DO_GUARD_SLOW
		int SafetyIterations = VisionUnitData.LocalAreaTilesCachedStates.Num();
		TArray<bool> IsTileVisited;
		IsTileVisited.Init(false, VisionUnitData.LocalAreaTilesCachedStates.Num());
#endif

		// in the order of spiral traversal
		enum class EDirection
		{
			Right,
			Up,
			Left,
			Down,
		};
		const FIntVector2 DirectionDeltas[] = {
			{0, 1},
			{1, 0},
			{0, -1},
			{-1, 0},
		};

		EDirection CurrentDirection = EDirection::Right;
		bool Clock = true;
		int CurrentStepSize = VisionUnitData.LocalAreaTilesResolution;
		int LeftToSpend = CurrentStepSize;
		FIntVector2 CurrentLocalIJ = FIntVector2(0, 0) - DirectionDeltas[static_cast<int>(CurrentDirection)];

		while (true)
		{
			checkSlow(LeftToSpend > 0);
			CurrentLocalIJ += DirectionDeltas[static_cast<int>(CurrentDirection)];
			LeftToSpend--;

			{
				checkSlow(VisionUnitData.IsLocalIJValid(CurrentLocalIJ));

#if DO_GUARD_SLOW
				SafetyIterations--;
				IsTileVisited[VisionUnitData.GetLocalIndex(CurrentLocalIJ)] = true;
#endif

				FIntVector2 GlobalIJ = VisionUnitData.LocalToGlobal(CurrentLocalIJ);

				if (IsGlobalIJValid(GlobalIJ))
				{
					// the distance between bottom-left corners of the tiles is the same as the distance between their centers, no need to add 0.5
					int DistToTileSqr = FMath::Square(OriginGlobalIJ.X - GlobalIJ.X) + FMath::Square(OriginGlobalIJ.Y - GlobalIJ.Y);
					if (DistToTileSqr <= GridSpaceRadiusSqr)
					{
						ExecuteDDAVisibilityCheck(OriginWorldLocation.Z, CurrentLocalIJ, OriginLocalIJ, VisionUnitData);
						checkSlow(VisionUnitData.GetLocalTileState(CurrentLocalIJ) != FVisionUnitData::TileState::Unknown);
					}
				}
			}

			if (LeftToSpend == 0)
			{
				if (Clock)
				{
					if (CurrentStepSize == 1)
					{
						break;
					}
					CurrentStepSize--;
				}
				Clock ^= 1;
				CurrentDirection = static_cast<EDirection>((static_cast<int>(CurrentDirection) + 1) % 4);
				LeftToSpend = CurrentStepSize;
			}
		}

#if DO_GUARD_SLOW
		check(SafetyIterations == 0);

		for (auto bVisited : IsTileVisited)
		{
			check(bVisited);
		}
#endif
	}

	for (int I = 0; I < VisionUnitData.LocalAreaTilesResolution; I++)
	{
		for (int J = 0; J < VisionUnitData.LocalAreaTilesResolution; J++)
		{
			FIntVector2 GlobalIJ = VisionUnitData.LocalToGlobal({ I, J });

			if (IsGlobalIJValid(GlobalIJ))
			{
				// the distance between bottom-left corners of the tiles is the same as the distance between their centers, no need to add 0.5
				int DistToTileSqr = FMath::Square(OriginGlobalIJ.X - GlobalIJ.X) + FMath::Square(OriginGlobalIJ.Y - GlobalIJ.Y);
				if (DistToTileSqr <= GridSpaceRadiusSqr)
				{
					if (VisionUnitData.GetLocalTileState({ I, J }) == FVisionUnitData::TileState::Visible)
					{
						FTile& GlobalTile = GetGlobalTile(GlobalIJ);
						GlobalTile.VisibilityCounter++;
					}
				}
			}
		}
	}

	VisionUnitData.bHasCachedData = true;
}

void AFogOfWar::CalculateTileHeight(FTile& Tile, FIntVector2 TileIJ)
{
	FVector2D WorldLocation = ConvertTileIJToTileCenterWorldLocation(TileIJ);
	FHitResult HitResult;
	bool bFoundBlockingHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		FVector(WorldLocation.X, WorldLocation.Y, 10000.0),
		FVector(WorldLocation.X, WorldLocation.Y, -10000.0),
		HeightScanCollisionChannel);

	if (bFoundBlockingHit && HitResult.HasValidHitObjectHandle())
	{
		Tile.Height = HitResult.ImpactPoint.Z;
		return;
	}

	Tile.Height = -std::numeric_limits<decltype(Tile.Height)>::infinity();
}

AFogOfWar::FVisionUnitData AFogOfWar::CreateVisionUnitDataFromVisionComponent(UVisionComponent* VisionComponent)
{
	int LocalAreaTilesResolution = FMath::CeilToInt32(VisionComponent->GetSightRadius() * 2 / TileSize) + 1;
	TArray<FVisionUnitData::TileState> LocalAreaTilesStates;
	LocalAreaTilesStates.Init(FVisionUnitData::TileState::NotVisible, LocalAreaTilesResolution * LocalAreaTilesResolution);
	return {
		.LocalAreaTilesResolution = LocalAreaTilesResolution,
		.GridSpaceRadius = VisionComponent->GetSightRadius() / TileSize,
		.LocalAreaTilesCachedStates = std::move(LocalAreaTilesStates),
	};
}

UTexture2D* AFogOfWar::CreateSnapshotTexture()
{
	UTexture2D* Texture = UTexture2D::CreateTransient(GridResolution.Y, GridResolution.X, PF_R8);
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->SRGB = 0;
#if WITH_EDITORONLY_DATA
	if (bDebugFilterNearest)
	{
		Texture->Filter = TF_Nearest;
	}
#endif

	return Texture;
}

UTextureRenderTarget2D* AFogOfWar::CreateRenderTarget()
{
	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, GridResolution.Y, GridResolution.X, RTF_R8);
	RenderTarget->AddressX = TA_Clamp;
	RenderTarget->AddressY = TA_Clamp;
	RenderTarget->SRGB = 0;
#if WITH_EDITORONLY_DATA
	if (bDebugFilterNearest)
	{
		RenderTarget->Filter = TF_Nearest;
	}
#endif

	return RenderTarget;
}

#if WITH_EDITORONLY_DATA
void AFogOfWar::WriteHeightmapDataToTexture(UTexture2D* Texture)
{
	TArray<uint8> HeightmapDataBuffer;
	HeightmapDataBuffer.SetNum(Tiles.Num());

	for (int TileIndex = 0; TileIndex < Tiles.Num(); TileIndex++)
	{
		const FTile& Tile = Tiles[TileIndex];
		HeightmapDataBuffer[TileIndex] = FMath::RoundToInt(FMath::Clamp(FMath::GetRangePct(DebugHeightmapLowestZ, DebugHeightmapHightestZ, Tile.Height), 0.0f, 1.0f) * 0xFF);
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, HeightmapDataBuffer.GetData(), sizeof(HeightmapDataBuffer[0]) * HeightmapDataBuffer.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	// TODO: likely a better version exists
	Texture->UpdateResource();
}
#endif

void AFogOfWar::WriteVisionDataToTexture(UTexture2D* Texture)
{
	for (int TileIndex = 0; TileIndex < Tiles.Num(); TileIndex++)
	{
		const FTile& Tile = Tiles[TileIndex];
		TextureDataBuffer[TileIndex] = Tile.VisibilityCounter > 0 ? 0xFF : 0;
	}

	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, TextureDataBuffer.GetData(), sizeof(TextureDataBuffer[0]) * TextureDataBuffer.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	// TODO: likely a better version exists
	Texture->UpdateResource();
}

FVector2f AFogOfWar::ConvertWorldSpaceLocationToGridSpace(const FVector2D& WorldLocation)
{
	return {
		static_cast<float>((WorldLocation.X - GridBottomLeftWorldLocation.X) / TileSize),
		static_cast<float>((WorldLocation.Y - GridBottomLeftWorldLocation.Y) / TileSize)
	};
}

FVector2D AFogOfWar::ConvertTileIJToTileCenterWorldLocation(const FIntVector2& IJ)
{
	return {
		GridBottomLeftWorldLocation.X + TileSize * IJ.X + TileSize / 2,
		GridBottomLeftWorldLocation.Y + TileSize * IJ.Y + TileSize / 2
	};
}

FIntVector2 AFogOfWar::ConvertGridLocationToTileIJ(const FVector2f& GridLocation)
{
	return {
		FMath::FloorToInt(GridLocation.X),
		FMath::FloorToInt(GridLocation.Y)
	};
}

FIntVector2 AFogOfWar::ConvertWorldLocationToTileIJ(const FVector2D& WorldLocation)
{
	FVector2f GridSpaceLocation = ConvertWorldSpaceLocationToGridSpace(WorldLocation);
	return ConvertGridLocationToTileIJ(GridSpaceLocation);
}

bool AFogOfWar::IsBlockingVision(float ObserverHeight, float PotentialObstacleHeight)
{
	return PotentialObstacleHeight - ObserverHeight > VisionBlockingDeltaHeightThreshold;
}

// Extremely frequently called function!
// Performs DDA ray casting. Explanation here: https://www.youtube.com/watch?v=NbSee-XM7WA
void AFogOfWar::ExecuteDDAVisibilityCheck(float ObserverHeight, FIntVector2 LocalIJ, const FIntVector2 OriginLocalIJ, FVisionUnitData& VisionUnitData)
{
#if UE_BUILD_DEBUG && 0
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ExecuteDDAVisibilityCheck"), STAT_FogOfWarExecuteDDAVisibilityCheck, STATGROUP_FogOfWar);
#endif

	checkSlow(DDALocalIndexesStack.IsEmpty());

	int LocalIndex = VisionUnitData.GetLocalIndex(LocalIJ);
	if (VisionUnitData.GetLocalTileState(LocalIndex) != FVisionUnitData::TileState::Unknown)
	{
		return;
	}

	const FIntVector2 Direction = OriginLocalIJ - LocalIJ;
	checkSlow(FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) != 0);
	const FIntVector2 DirectionSign = {
		Direction.X >= 0 ? 1 : -1,
		Direction.Y >= 0 ? 1 : -1
	};
	const float S_x = FMath::Sqrt(FMath::Square(1.0) + FMath::Square(static_cast<float>(Direction.Y) / Direction.X));
	const float S_y = FMath::Sqrt(FMath::Square(1.0) + FMath::Square(static_cast<float>(Direction.X) / Direction.Y));
	// this represents the total ray length after we went a step accordingly.
	// note that the first step has the multiplier of 0.5 as we start from the tile center, after that it will be 1
	float NextAccumulatedDxLength = 0.5 * S_x;
	float NextAccumulatedDyLength = 0.5 * S_y;

	bool bIsBlocking = false;
	// the total amount of transitions is mathematically not more than the manhattan distance
	// double-checking to avoid infinite loops if something bad happens
	const int SafetyIterations = FMath::Abs(Direction.X) + FMath::Abs(Direction.Y) + 1;
	checkSlow(SafetyIterations < 10000);
	int SafetyCounter;

	for (SafetyCounter = 0; SafetyCounter < SafetyIterations; SafetyCounter++)
	{
		DDALocalIndexesStack.Push(LocalIndex);

		if (LocalIJ == OriginLocalIJ)
		{
			break;
		}

		auto CurrentHeight = GetGlobalTile(VisionUnitData.LocalToGlobal(LocalIJ)).Height;
		if (IsBlockingVision(ObserverHeight, CurrentHeight))
		{
			bIsBlocking = true;
			break;
		}

		if (NextAccumulatedDxLength < NextAccumulatedDyLength)
		{
			NextAccumulatedDxLength += S_x;
			LocalIJ.X += DirectionSign.X;
		}
		else
		{
			NextAccumulatedDyLength += S_y;
			LocalIJ.Y += DirectionSign.Y;
		}

		checkSlow(VisionUnitData.IsLocalIJValid(LocalIJ));
		checkSlow(IsGlobalIJValid(VisionUnitData.LocalToGlobal(LocalIJ)));

		LocalIndex = VisionUnitData.GetLocalIndex(LocalIJ);
	}

	checkSlow(SafetyCounter < SafetyIterations);

	if (bIsBlocking)
	{
		while (!DDALocalIndexesStack.IsEmpty())
		{
			int LocalIndexFromStack = DDALocalIndexesStack.Pop(false);
			auto& TileState = VisionUnitData.GetLocalTileState(LocalIndexFromStack);
			if (TileState != FVisionUnitData::TileState::Visible)
			{
				TileState = FVisionUnitData::TileState::NotVisible;
			}
		}
	}
	else
	{
		while (!DDALocalIndexesStack.IsEmpty())
		{
			int LocalIndexFromStack = DDALocalIndexesStack.Pop(false);
			auto& TileState = VisionUnitData.GetLocalTileState(LocalIndexFromStack);
			TileState = FVisionUnitData::TileState::Visible;
		}
	}
}
