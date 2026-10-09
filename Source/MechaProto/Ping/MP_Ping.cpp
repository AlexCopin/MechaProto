#include "MP_Ping.h"
#include "MP_Breakable.h"
#include "MP_Enemy.h"
#include "MP_HullPlate.h"
#include "MP_Item.h"
#include "PDA_Breakable.h"
#include "PDA_Item.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	constexpr float PingTargetCheckInterval = 0.25f;
	//Heard only when it appears, not by a late joiner
	constexpr float PingSoundWindow = 0.5f;
	//Marker sizes are given for this screen height
	constexpr float PingReferenceHeight = 1080.f;

	void DrawTriangle(UCanvas& Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		FCanvasTriangleItem Triangle(A, B, C, GWhiteTexture);
		Triangle.SetColor(Color);
		Triangle.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Triangle);
	}

	void DrawDiamond(UCanvas& Canvas, const FVector2D& Center, float Size, const FLinearColor& Color)
	{
		const FVector2D Top = Center - FVector2D(0.f, Size);
		const FVector2D Bottom = Center + FVector2D(0.f, Size);
		const FVector2D Left = Center - FVector2D(Size * 0.75f, 0.f);
		const FVector2D Right = Center + FVector2D(Size * 0.75f, 0.f);
		DrawTriangle(Canvas, Top, Right, Bottom, Color);
		DrawTriangle(Canvas, Top, Bottom, Left, Color);
	}

	void DrawLabel(UCanvas& Canvas, const FVector2D& Position, const FText& Text, UFont* Font, float Scale, const FLinearColor& Color)
	{
		FCanvasTextItem Item(Position, Text, Font, Color);
		Item.bCentreX = true;
		Item.bCentreY = true;
		Item.Scale = FVector2D(Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A));
		Canvas.DrawItem(Item);
	}
}

AMP_Ping::AMP_Ping()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	SetNetUpdateFrequency(2.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
}

void AMP_Ping::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AMP_Ping, PingData, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AMP_Ping, Pinger, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AMP_Ping, Type, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AMP_Ping, SpawnServerTime, COND_InitialOnly);
	DOREPLIFETIME(AMP_Ping, TargetActor);
	DOREPLIFETIME(AMP_Ping, AnchorComponent);
	DOREPLIFETIME(AMP_Ping, AnchorPoint);
	DOREPLIFETIME(AMP_Ping, EndServerTime);
}

const UPDA_Ping* AMP_Ping::GetPingData() const
{
	return PingData ? PingData.Get() : GetDefault<UPDA_Ping>();
}

void AMP_Ping::InitPing(UPDA_Ping* Data, APlayerState* InPinger, EMP_PingType InType, AActor* Target, USceneComponent* Component, const FVector& Point)
{
	PingData = Data;
	Pinger = InPinger;
	Type = InType;
	TargetActor = Target;
	AnchorComponent = Target ? nullptr : Component;
	AnchorPoint = Point;
	SpawnServerTime = GetWorld()->GetTimeSeconds();
	EndServerTime = SpawnServerTime + GetPingData()->Lifetime;
	//BeginPlay starts the life span from it
	InitialLifeSpan = GetPingData()->Lifetime;
}

void AMP_Ping::BeginPlay()
{
	Super::BeginPlay();

	ApplyAnchor();
	if (HasAuthority() && TargetActor)
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &AMP_Ping::CheckTarget, PingTargetCheckInterval, true);
	}

	const UPDA_Ping* Data = GetPingData();
	if (Data->PingSound && GetNetMode() != NM_DedicatedServer && GetServerTime() - SpawnServerTime < PingSoundWindow)
	{
		UGameplayStatics::PlaySound2D(this, Data->PingSound, Data->PingVolume);
	}
}

void AMP_Ping::OnRep_Anchor()
{
	ApplyAnchor();
}

void AMP_Ping::ApplyAnchor()
{
	//Each machine attaches it itself, so it sits on that machine's own pose of the mech
	if (USceneComponent* Component = AnchorComponent.Get())
	{
		if (GetRootComponent()->GetAttachParent() != Component)
		{
			AttachToComponent(Component, FAttachmentTransformRules::KeepRelativeTransform);
		}
		SetActorRelativeLocation(AnchorPoint);
	}
	else if (!TargetActor)
	{
		SetActorLocation(AnchorPoint);
	}
}

void AMP_Ping::CheckTarget()
{
	const AMP_Enemy* Enemy = Cast<AMP_Enemy>(TargetActor);
	if (IsValid(TargetActor) && !(Enemy && Enemy->IsDead()))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CheckTimer);
	const float Linger = GetPingData()->TargetLostLinger;
	EndServerTime = FMath::Min(EndServerTime, GetWorld()->GetTimeSeconds() + Linger);
	SetLifeSpan(FMath::Max(0.01f, Linger));
	ForceNetUpdate();
}

float AMP_Ping::GetServerTime() const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	return GameState ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : GetWorld()->GetTimeSeconds();
}

float AMP_Ping::GetAlpha() const
{
	const float FadeTime = GetPingData()->FadeTime;
	const float Left = EndServerTime - GetServerTime();
	return FadeTime > 0.f ? FMath::Clamp(Left / FadeTime, 0.f, 1.f) : (Left > 0.f ? 1.f : 0.f);
}

float AMP_Ping::GetPopScale() const
{
	const float PopTime = GetPingData()->PopTime;
	const float Age = GetServerTime() - SpawnServerTime;
	return PopTime > 0.f ? 1.f + FMath::Clamp(1.f - Age / PopTime, 0.f, 1.f) : 1.f;
}

FVector AMP_Ping::GetMarkerLocation() const
{
	//Above the target's top, or the spot itself
	if (IsValid(TargetActor))
	{
		const FBox Bounds = TargetActor->GetComponentsBoundingBox(true);
		LastTargetLocation = Bounds.IsValid ? FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z) : TargetActor->GetActorLocation();
		LastTargetLocation.Z += GetPingData()->MarkerHeight;
		return LastTargetLocation;
	}
	//A spot ping, or where its target was last seen (dead and gone)
	const bool bTargetGone = TargetActor != nullptr || !LastTargetLocation.IsZero();
	return bTargetGone ? LastTargetLocation : GetActorLocation();
}

FText AMP_Ping::GetLabel() const
{
	const UPDA_Ping* Data = GetPingData();
	FText Label = Data->GetStyle(Type).Label;
	FText Name;
	if (const AMP_Breakable* System = Cast<AMP_Breakable>(TargetActor))
	{
		Name = System->GetBreakableData()->DisplayName;
		if (System->IsBroken())
		{
			Label = Data->BrokenSystemLabel;
		}
	}
	else if (const AMP_Item* Item = Cast<AMP_Item>(TargetActor))
	{
		Name = Item->GetItemData()->ItemName;
	}
	else if (const AMP_HullPlate* Plate = Cast<AMP_HullPlate>(TargetActor))
	{
		Name = Plate->GetPlateName();
	}
	return FText::Format(Label, Name);
}

void AMP_Ping::DrawPings(AHUD& HUD, UCanvas& Canvas)
{
	APlayerController* Controller = HUD.PlayerOwner;
	if (!Controller)
	{
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const float Scale = Canvas.ClipY / PingReferenceHeight;
	for (TActorIterator<AMP_Ping> It(HUD.GetWorld()); It; ++It)
	{
		It->DrawMarker(Canvas, ViewLocation, ViewRotation, Scale);
	}
}

void AMP_Ping::DrawMarker(UCanvas& Canvas, const FVector& ViewLocation, const FRotator& ViewRotation, float Scale) const
{
	const UPDA_Ping* Data = GetPingData();
	const float Alpha = GetAlpha();
	if (Alpha <= 0.f)
	{
		return;
	}

	//On screen, or on its edge toward the ping (also when it is behind the view)
	const FVector Location = GetMarkerLocation();
	const FVector Local = ViewRotation.UnrotateVector(Location - ViewLocation);
	const FVector2D ScreenCenter(Canvas.ClipX * 0.5f, Canvas.ClipY * 0.5f);
	const FVector2D Inner = ScreenCenter - FVector2D(Data->EdgeMargin * Scale);
	FVector2D Screen = ScreenCenter;
	FVector2D Direction(0.f, 1.f);
	bool bOnScreen = false;
	if (Local.X > 1.f)
	{
		const FVector Projected = Canvas.Project(Location);
		Screen = FVector2D(Projected.X, Projected.Y);
		bOnScreen = FMath::Abs(Screen.X - ScreenCenter.X) <= Inner.X && FMath::Abs(Screen.Y - ScreenCenter.Y) <= Inner.Y;
		Direction = (Screen - ScreenCenter).GetSafeNormal();
	}
	else if (!FVector2D(Local.Y, -Local.Z).IsNearlyZero())
	{
		Direction = FVector2D(Local.Y, -Local.Z).GetSafeNormal();
	}
	if (!bOnScreen)
	{
		if (Direction.IsNearlyZero())
		{
			Direction = FVector2D(0.f, 1.f);
		}
		const float ToEdgeX = FMath::Abs(Direction.X) > UE_KINDA_SMALL_NUMBER ? Inner.X / FMath::Abs(Direction.X) : UE_BIG_NUMBER;
		const float ToEdgeY = FMath::Abs(Direction.Y) > UE_KINDA_SMALL_NUMBER ? Inner.Y / FMath::Abs(Direction.Y) : UE_BIG_NUMBER;
		Screen = ScreenCenter + Direction * FMath::Min(ToEdgeX, ToEdgeY);
	}

	FLinearColor Color = Data->GetStyle(Type).Color;
	Color.A *= Alpha;
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.6f * Alpha);
	UFont* LabelFont = GEngine->GetMediumFont();
	UFont* InfoFont = GEngine->GetSmallFont();
	const float TextScale = Scale * Data->TextScale;

	FString Info;
	if (Data->bShowDistance)
	{
		Info = FString::Printf(TEXT("%d m"), FMath::RoundToInt(FVector::Dist(ViewLocation, Location) / 100.f));
	}
	if (Data->bShowPingerName && Pinger)
	{
		Info += (Info.IsEmpty() ? TEXT("") : TEXT(" - ")) + Pinger->GetPlayerName();
	}

	if (bOnScreen)
	{
		const float Size = Data->MarkerSize * Scale * GetPopScale();
		DrawDiamond(Canvas, Screen, Size + 2.f * Scale, Shadow);
		DrawDiamond(Canvas, Screen, Size, Color);
		DrawLabel(Canvas, Screen - FVector2D(0.f, Size + 14.f * TextScale), GetLabel(), LabelFont, TextScale, Color);
		if (!Info.IsEmpty())
		{
			DrawLabel(Canvas, Screen + FVector2D(0.f, Size + 12.f * TextScale), FText::FromString(Info), InfoFont, TextScale, Color);
		}
		return;
	}

	//Arrow pointing out of the screen, its label toward the center
	const float Size = Data->ArrowSize * Scale * GetPopScale();
	const FVector2D Side(-Direction.Y, Direction.X);
	const FVector2D Tip = Screen + Direction * Size;
	const FVector2D Back = Screen - Direction * Size * 0.6f;
	DrawTriangle(Canvas, Tip + Direction * 2.f * Scale, Back + Side * (Size + 2.f * Scale), Back - Side * (Size + 2.f * Scale), Shadow);
	DrawTriangle(Canvas, Tip, Back + Side * Size, Back - Side * Size, Color);
	const FVector2D TextCenter = Screen - Direction * Size * 3.f;
	DrawLabel(Canvas, TextCenter, GetLabel(), LabelFont, TextScale, Color);
	if (!Info.IsEmpty())
	{
		DrawLabel(Canvas, TextCenter + FVector2D(0.f, 18.f * TextScale), FText::FromString(Info), InfoFont, TextScale, Color);
	}
}
