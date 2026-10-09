#include "PDA_Ping.h"

UPDA_Ping::UPDA_Ping()
{
	auto AddStyle = [this](EMP_PingType Type, const TCHAR* Label, const FLinearColor& Color)
	{
		FMP_PingStyle Style;
		Style.Label = FText::FromString(Label);
		Style.Color = Color;
		Styles.Add(Type, Style);
	};
	AddStyle(EMP_PingType::Location, TEXT("Look here"), FLinearColor(1.f, 1.f, 1.f, 1.f));
	AddStyle(EMP_PingType::Enemy, TEXT("Enemy"), FLinearColor(1.f, 0.3f, 0.1f, 1.f));
	AddStyle(EMP_PingType::System, TEXT("{0}"), FLinearColor(1.f, 0.85f, 0.1f, 1.f));
	AddStyle(EMP_PingType::Item, TEXT("{0}"), FLinearColor(0.3f, 1.f, 0.4f, 1.f));
	AddStyle(EMP_PingType::Hull, TEXT("Hull breach - {0}"), FLinearColor(1.f, 0.55f, 0.1f, 1.f));
}

const FMP_PingStyle& UPDA_Ping::GetStyle(EMP_PingType Type) const
{
	static const FMP_PingStyle Fallback;
	const FMP_PingStyle* Style = Styles.Find(Type);
	return Style ? *Style : Fallback;
}
