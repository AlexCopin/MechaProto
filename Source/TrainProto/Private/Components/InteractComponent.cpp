#include "Components/InteractComponent.h"

UInteractComponent::UInteractComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractComponent::BeginPlay() {
	Super::BeginPlay();
}

void UInteractComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UInteractComponent::TryInteract() {
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Interacted!"));
}

