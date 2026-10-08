#pragma once

#include "CoreMinimal.h"
#include "MP_Alert.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EMP_AlertSeverity : uint8
{
	Info,
	Warning,
	Critical,
};

//An alert on the players' HUD (hull damage, a broken station, an arm...), raised with AMP_HUD::RaiseAlert
USTRUCT(BlueprintType)
struct MECHAPROTO_API FMP_Alert
{
	GENERATED_BODY()

	//With Source, identifies the alert: raising the same one again refreshes it instead of stacking a new one
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FName Key;

	//What it is about (the plate, the station...), the widget can point at it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	TObjectPtr<AActor> Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert")
	EMP_AlertSeverity Severity = EMP_AlertSeverity::Warning;

	//On screen this long, 0 = until cleared (AMP_HUD::ClearAlert)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Alert", meta = (ClampMin = "0", Units = "s"))
	float Duration = 4.f;

	bool Matches(FName InKey, const AActor* InSource) const
	{
		return Key == InKey && Source == InSource;
	}
};
