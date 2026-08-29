// Fill out your copyright notice in the Description page of Project Settings.

#include "Voice/FAVoiceComponent.h"

#include "Audio/FAAudioSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "Voice/FAVoiceStatics.h"
#include "Voice/FAVoiceTalker.h"
#include "Voice/FAVoiceTransport.h"
#include "Voice/FAVoiceUserSettings.h"

DEFINE_LOG_CATEGORY(LogFAVoice);

namespace FAVoice
{
	/** PlayerState / UniqueNetId 복제를 기다리는 재시도 간격과 횟수 (= 최대 10초). */
	static constexpr float RegisterRetryInterval = 0.5f;
	static constexpr int32 MaxRegisterAttempts = 20;

	/**
	 * 엔진 VoiceTalkerMap 에서 내 화자가 밀려나지 않았는지 확인하는 주기(초).
	 * TMap 조회 한 번이라 8인 방에서도 무시할 만한 비용이다.
	 */
	static constexpr float RegistrationWatchdogInterval = 1.f;
}

UFAVoiceComponent::UFAVoiceComponent()
{
	// 평상시엔 돌 필요가 없다. 발화 중에만 켜서 아이콘 거리 판정을 한다(HandleTalkerStateChanged).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// VoiceChannel 을 서버에서 바꿨을 때 모든 머신의 재생 방식이 따라가야 함.
	SetIsReplicatedByDefault(true);

	TransportClass = UFAEngineVoiceTransport::StaticClass();
}

void UFAVoiceComponent::BeginPlay()
{
	Super::BeginPlay();

	const TSubclassOf<UFAVoiceTransport> ResolvedTransportClass =
		TransportClass ? TransportClass : TSubclassOf<UFAVoiceTransport>(UFAEngineVoiceTransport::StaticClass());
	Transport = NewObject<UFAVoiceTransport>(this, ResolvedTransportClass);

	// 설정창에서 이 사람 볼륨을 움직이면 말하는 도중에도 바로 반영돼야 한다.
	if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
	{
		VoiceSettingsChangedHandle = VoiceSettings->OnPlayerVoiceSettingsChanged.AddUObject(
			this, &UFAVoiceComponent::HandlePlayerVoiceSettingsChanged);
	}

	// 마이크 방식의 주인은 사용자 설정이다.
	bPushToTalk = UFAVoiceStatics::IsPushToTalkEnabled();

	SetupVoiceSource();
	SetupListenerAnchor();
	SetupSpeakingIndicator();
	ApplyListenerOverride();

	if (VoiceChannel == EFAVoiceChannel::Proximity)
	{
		RemainingRegisterAttempts = FAVoice::MaxRegisterAttempts;
		TryRegisterTalker();
	}
	else if (!bPushToTalk)
	{
		//송신만 엶.
		StartTalking();
	}
}

void UFAVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegisterRetryTimerHandle);
		World->GetTimerManager().ClearTimer(RegistrationWatchdogTimerHandle);
	}

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (PlayerController->IsLocalController())
		{
			StopTalking();

			if (bOverrideListenerToHead)
			{
				PlayerController->ClearAudioListenerOverride();
			}
		}
	}

	if (Talker)
	{
		Talker->OnTalkerStateChanged.RemoveAll(this);
	}

	if (VoiceSettingsChangedHandle.IsValid())
	{
		if (UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this))
		{
			VoiceSettings->OnPlayerVoiceSettingsChanged.Remove(VoiceSettingsChangedHandle);
		}
		VoiceSettingsChangedHandle.Reset();
	}

	// 라운드 리스폰(Destroyed)에서는 전송 계층을 유지해야 다음 Pawn 이 바로 이어받는다.
	// 월드 자체가 내려갈 때만 세션·원격 토커를 정리한다.
	if (EndPlayReason != EEndPlayReason::Destroyed)
	{
		if (Transport != nullptr)
		{
			Transport->ShutdownTransport(GetOwningPlayerController());
		}

		if (Talker)
		{
			Talker->DestroyComponent();
			Talker = nullptr;
		}
	}

	Super::EndPlay(EndPlayReason);
}

float UFAVoiceComponent::GetHearingRadius() const
{
	// TODO: GAS 도입 후 VoiceRadius Attribute 를 읽도록 오버라이드(확성/침묵 룰).
	return HearingRadius;
}

float UFAVoiceComponent::GetFullVolumeRadius() const
{
	return FMath::Min(FullVolumeRadius, GetHearingRadius());
}

void UFAVoiceComponent::SetVoiceChannel(EFAVoiceChannel NewChannel)
{
	if (VoiceChannel == NewChannel)
	{
		return;
	}

	VoiceChannel = NewChannel;
	ApplyChannelChange();
}

void UFAVoiceComponent::OnRep_VoiceChannel()
{
	ApplyChannelChange();
}

void UFAVoiceComponent::ApplyChannelChange()
{
	if (!HasBegunPlay())
	{
		return;
	}

	if (VoiceChannel == EFAVoiceChannel::Proximity && Talker == nullptr)
	{
		RemainingRegisterAttempts = FAVoice::MaxRegisterAttempts;
		TryRegisterTalker();
	}

	RefreshVoiceAttenuation();
	ApplyListenerOverride();
}

void UFAVoiceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFAVoiceComponent, VoiceChannel);
}

void UFAVoiceComponent::RefreshVoiceAttenuation()
{
	ApplyTalkerSettings();
}

void UFAVoiceComponent::ApplyTalkerSettings()
{
	if (!Talker)
	{
		return;
	}

	// FVoiceSettings 는 ComponentToAttachTo 가 없으면 정위 자체를 하지 않음.
	// 런타임에 Proximity -> Global 로 바꾼 경우 이미 등록된 Talker 를 2D 로 되돌리기 위한 처리.
	const bool bProximity = (VoiceChannel == EFAVoiceChannel::Proximity);

	Talker->Settings.ComponentToAttachTo = bProximity ? VoiceSourceComponent : nullptr;
	Talker->Settings.AttenuationSettings = bProximity ? BuildAttenuation() : nullptr;

	// FVoiceSettings 에는 음량 항목이 없다. 재생 배수는 Talker 가 직접 들고 있다가
	// 엔진이 재생 컴포넌트를 건네주는 순간(OnTalkingBegin) 적용한다.
	// 듣는 사람이 이 화자에게 따로 준 배수를 여기서 곱한다 (설정창 접속자 목록).
	Talker->SetPlaybackVolumeMultiplier(VoicePlaybackVolume * ListenerVolumeScale);
}

void UFAVoiceComponent::RefreshListenerVolumeScale()
{
	const UFAVoiceUserSettings* VoiceSettings = UFAVoiceUserSettings::Get(this);

	ListenerVolumeScale = (VoiceSettings != nullptr && !OwnerVoiceKey.IsEmpty())
		? VoiceSettings->GetPlayerVolumeScale(OwnerVoiceKey)
		: 1.f;

	if (Talker)
	{
		Talker->SetPlaybackVolumeMultiplier(VoicePlaybackVolume * ListenerVolumeScale);
	}
}

void UFAVoiceComponent::HandlePlayerVoiceSettingsChanged(const FString& PlayerKey, float NewScale)
{
	// 8인 파티면 이 알림이 사람 수만큼 온다. 내 화자가 아니면 여기서 끝낸다.
	if (OwnerVoiceKey.IsEmpty() || PlayerKey != OwnerVoiceKey)
	{
		return;
	}

	ListenerVolumeScale = NewScale;

	if (Talker)
	{
		Talker->SetPlaybackVolumeMultiplier(VoicePlaybackVolume * ListenerVolumeScale);
	}
}

void UFAVoiceComponent::SetVoicePlaybackVolume(float NewVolume)
{
	VoicePlaybackVolume = FMath::Max(0.f, NewVolume);

	if (Talker)
	{
		Talker->SetPlaybackVolumeMultiplier(VoicePlaybackVolume * ListenerVolumeScale);
	}
}

void UFAVoiceComponent::StartTalking()
{
	if (Transport)
	{
		Transport->SetLocalTalking(GetOwningPlayerController(), true);
	}
}

void UFAVoiceComponent::StopTalking()
{
	if (Transport)
	{
		Transport->SetLocalTalking(GetOwningPlayerController(), false);
	}
}

void UFAVoiceComponent::SetPushToTalkEnabled(bool bEnabled)
{
	bPushToTalk = bEnabled;

	// 오픈 마이크로 바꾸면 키 입력 없이 바로 열려야 하고, 푸시투토크로 바꾸면 즉시 닫혀야 함.
	if (bPushToTalk)
	{
		StopTalking();
	}
	else
	{
		StartTalking();
	}
}

bool UFAVoiceComponent::IsTalking() const
{
	return Talker != nullptr && Talker->IsTalking();
}

USceneComponent* UFAVoiceComponent::GetVoiceSourceComponent() const
{
	return VoiceSourceComponent;
}

void UFAVoiceComponent::TryRegisterTalker()
{
	APlayerState* PlayerState = GetOwningPlayerState();

	//UUniqueId 없을 시.
	if (PlayerState == nullptr || !PlayerState->GetUniqueId().IsValid())
	{
		if (--RemainingRegisterAttempts <= 0)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(RegisterRetryTimerHandle);
			}

			UE_LOG(LogFAVoice, Warning,
				TEXT("%s: PlayerState/UniqueNetId 를 기다리다 포기했습니다. 이 Pawn 은 보이스에 참여하지 않습니다."),
				*GetNameSafe(GetOwner()));
		}
		else if (UWorld* World = GetWorld())
		{
			if (!RegisterRetryTimerHandle.IsValid())
			{
				World->GetTimerManager().SetTimer(RegisterRetryTimerHandle, this, &UFAVoiceComponent::TryRegisterTalker,
					FAVoice::RegisterRetryInterval, true);
			}
		}

		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegisterRetryTimerHandle);
	}

	if (Talker == nullptr)
	{
		Talker = NewObject<UFAVoiceTalker>(GetOwner());
		Talker->OnTalkerStateChanged.AddUObject(this, &UFAVoiceComponent::HandleTalkerStateChanged);
		Talker->RegisterComponent();
	}

	// 여기까지 왔으면 UniqueNetId 가 유효하다 — 저장 키를 잡을 수 있는 첫 시점이다.
	OwnerVoiceKey = UFAVoiceUserSettings::MakePlayerKey(PlayerState);
	RefreshListenerVolumeScale();

	ApplyTalkerSettings();
	Talker->RegisterForPlayer(PlayerState);
	StartRegistrationWatchdog();

	if (VoiceChannel == EFAVoiceChannel::Proximity)
	{
		UE_LOG(LogFAVoice, Log, TEXT("%s: 보이스 등록 완료 (근접, 백엔드 %s, 반경 %.0f/%.0fuu)"),
			*GetNameSafe(GetOwner()), *Transport->GetBackendName().ToString(),
			GetFullVolumeRadius(), GetHearingRadius());
	}
	else
	{
		UE_LOG(LogFAVoice, Log, TEXT("%s: 보이스 등록 완료 (전역/관전, 백엔드 %s)"),
			*GetNameSafe(GetOwner()), *Transport->GetBackendName().ToString());
	}

	ApplyListenerOverride();

	// 오픈 마이크가 기본.
	if (!bPushToTalk)
	{
		StartTalking();
	}
}

void UFAVoiceComponent::StartRegistrationWatchdog()
{
	UWorld* World = GetWorld();
	if (World == nullptr || RegistrationWatchdogTimerHandle.IsValid())
	{
		return;
	}

	World->GetTimerManager().SetTimer(RegistrationWatchdogTimerHandle, this, &UFAVoiceComponent::EnsureTalkerRegistration,
		FAVoice::RegistrationWatchdogInterval, true);
}

void UFAVoiceComponent::EnsureTalkerRegistration()
{
	if (Talker == nullptr || Talker->IsRegisteredWithVoiceEngine())
	{
		return;
	}

	APlayerState* PlayerState = GetOwningPlayerState();
	if (PlayerState == nullptr || !PlayerState->GetUniqueId().IsValid())
	{
		return;
	}

	UE_LOG(LogFAVoice, Warning, TEXT("%s: VOIP 화자 등록이 풀려 있어 다시 등록합니다."), *GetNameSafe(GetOwner()));

	Talker->RegisterForPlayer(PlayerState);
	ApplyTalkerSettings();
}

void UFAVoiceComponent::SetupVoiceSource()
{
	VoiceSourceComponent = ResolveVoiceSourceComponent();
}

USceneComponent* UFAVoiceComponent::ResolveVoiceSourceComponent()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return nullptr;
	}

	USceneComponent* Anchor = NewObject<USceneComponent>(Owner);
	if (Anchor == nullptr)
	{
		return nullptr;
	}

	Anchor->SetMobility(EComponentMobility::Movable);
	Anchor->RegisterComponent();

	// 옷·헤어가 별도 스켈레탈 메시로 붙는 경우 FindComponentByClass 는 엉뚱한 걸 집는다.
	// ACharacter 는 본체 메시가 GetMesh() 로 확정돼 있으므로 그쪽을 우선한다.
	USkeletalMeshComponent* Mesh = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Mesh = Character->GetMesh();
	}
	if (Mesh == nullptr)
	{
		Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}

	if (Mesh != nullptr)
	{
		// DoesSocketExist 는 소켓이 없으면 본 이름으로 폴백한다(Mixamo 임포트의 Head 본이 그대로 매칭).
		if (Mesh->DoesSocketExist(HeadSocketName))
		{
			Anchor->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadSocketName);
			return Anchor;
		}

		// 조용히 발 밑에서 소리가 나는 것보다 로그를 남기는 편이 낫다.
		UE_LOG(LogFAVoice, Warning,
			TEXT("%s: 스켈레탈 메시에 소켓/본 '%s' 이 없어 루트 + 오프셋으로 대체합니다."),
			*GetNameSafe(Owner), *HeadSocketName.ToString());
	}

	// 헤드 소켓이 없는 Pawn(DefaultPawn 등)에서도 튜닝은 가능해야 하므로 눈높이로 대체.
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		Anchor->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Anchor->SetRelativeLocation(VoiceSourceFallbackOffset);
	}

	return Anchor;
}

void UFAVoiceComponent::SetupListenerAnchor()
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner != nullptr ? Owner->GetRootComponent() : nullptr;
	if (Root == nullptr)
	{
		return;
	}

	// 회전은 캡슐(= 액터 forward)을 그대로 쓰고, 높이만 헤드에 맞춘다.
	float HeadHeight = VoiceSourceFallbackOffset.Z;
	if (VoiceSourceComponent != nullptr && VoiceSourceComponent->GetAttachParent() != Root)
	{
		// 헤드 본에 붙은 경우에만 유효. RTS_Actor 라 액터 기준 로컬 Z 가 그대로 높이가 된다.
		HeadHeight = VoiceSourceComponent->GetComponentTransform().GetRelativeTransform(
			Owner->GetActorTransform()).GetLocation().Z;
	}

	ListenerAnchor = NewObject<USceneComponent>(Owner);
	if (ListenerAnchor == nullptr)
	{
		return;
	}

	ListenerAnchor->SetMobility(EComponentMobility::Movable);
	ListenerAnchor->RegisterComponent();
	ListenerAnchor->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	ListenerAnchor->SetRelativeLocation(FVector(0.f, 0.f, HeadHeight));
}

void UFAVoiceComponent::SetupSpeakingIndicator()
{
	// 관전자 머리위 아이콘 의미 없음.
	if (VoiceChannel != EFAVoiceChannel::Proximity)
	{
		return;
	}

	if (SpeakingIndicatorWidgetClass == nullptr || VoiceSourceComponent == nullptr)
	{
		return;
	}

	SpeakingIndicator = NewObject<UWidgetComponent>(GetOwner());
	SpeakingIndicator->SetWidgetSpace(EWidgetSpace::Screen);

	// DrawAtDesiredSize 를 켜면 브러시 원본 해상도가 그대로 화면 픽셀이 된다(512px 아이콘 → 캐릭터만 한 크기).
	SpeakingIndicator->SetDrawAtDesiredSize(false);
	SpeakingIndicator->SetDrawSize(SpeakingIndicatorDrawSize);
	SpeakingIndicator->SetWidgetClass(SpeakingIndicatorWidgetClass);
	SpeakingIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpeakingIndicator->RegisterComponent();
	SpeakingIndicator->AttachToComponent(VoiceSourceComponent, FAttachmentTransformRules::KeepRelativeTransform);
	SpeakingIndicator->SetRelativeLocation(SpeakingIndicatorOffset);
	SpeakingIndicator->SetVisibility(false);
}

void UFAVoiceComponent::ApplyListenerOverride()
{
	if (VoiceChannel != EFAVoiceChannel::Proximity)
	{
		return;
	}

	if (!bOverrideListenerToHead || ListenerAnchor == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerController->SetAudioListenerOverride(ListenerAnchor, FVector::ZeroVector, FRotator::ZeroRotator);
}

USoundAttenuation* UFAVoiceComponent::BuildAttenuation()
{
	if (AttenuationOverride != nullptr)
	{
		return AttenuationOverride;
	}

	if (RuntimeAttenuation == nullptr)
	{
		RuntimeAttenuation = NewObject<USoundAttenuation>(this);
	}

	const float FullVolume = GetFullVolumeRadius();

	FSoundAttenuationSettings& Settings = RuntimeAttenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Settings.dBAttenuationAtMax = AttenuationAtMaxDb;

	// 반경 밖에서는 확실히 끊김.
	Settings.FalloffMode = ENaturalSoundFalloffMode::Silent;

	Settings.AttenuationShapeExtents = FVector(FullVolume, 0.f, 0.f);
	Settings.FalloffDistance = FMath::Max(GetHearingRadius() - FullVolume, 1.f);

	// 벽 오클루전 미적용
	Settings.bEnableOcclusion = false;

	return RuntimeAttenuation;
}

void UFAVoiceComponent::HandleTalkerStateChanged(bool bIsTalking)
{
	if (SpeakingIndicator)
	{
		// 발화 중에만 틱을 켠다. 말하지 않는 동안 거리를 잴 이유가 없다(12인 방 × 매 프레임).
		SetComponentTickEnabled(bIsTalking);

		if (bIsTalking)
		{
			UpdateSpeakingIndicator();
		}
		else
		{
			SpeakingIndicator->SetVisibility(false);
		}
	}

	// 발화 시작 신호는 본인에게만 아주 작게. 남의 것까지 울리면 12인 방이 시끄러워진다.
	if (bIsTalking && GetOwner() && GetOwner()->HasLocalNetOwner())
	{
		UFAAudioSubsystem::PlayFASound(this, TEXT("Voice_TalkStart"));
	}

	OnTalkingStateChanged.Broadcast(bIsTalking);
}

void UFAVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 틱은 발화 중에만 켜져 있다
	UpdateSpeakingIndicator();
}

bool UFAVoiceComponent::ShouldShowSpeakingIndicator() const
{
	if (!IsTalking())
	{
		return false;
	}

	// 전역/관전 채널은 거리 개념이 없다.
	if (VoiceChannel != EFAVoiceChannel::Proximity)
	{
		return true;
	}

	const float CutoffDistance = GetHearingRadius();
	if (CutoffDistance <= 0.f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
	if (LocalController == nullptr)
	{
		return false;
	}

	// 기준점은 카메라가 아니라 로컬 플레이어의 Pawn
	FVector ListenerLocation;
	if (const APawn* LocalPawn = LocalController->GetPawn())
	{
		ListenerLocation = LocalPawn->GetActorLocation();
	}
	else
	{
		FRotator ViewRotation;
		LocalController->GetPlayerViewPoint(ListenerLocation, ViewRotation);
	}

	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return false;
	}

	return FVector::DistSquared(ListenerLocation, Owner->GetActorLocation()) <= FMath::Square(CutoffDistance);
}

void UFAVoiceComponent::UpdateSpeakingIndicator()
{
	if (SpeakingIndicator == nullptr)
	{
		return;
	}

	const bool bShouldShow = ShouldShowSpeakingIndicator();
	if (bShouldShow != SpeakingIndicator->IsVisible())
	{
		SpeakingIndicator->SetVisibility(bShouldShow);
	}
}

APlayerState* UFAVoiceComponent::GetOwningPlayerState() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn != nullptr && Pawn->GetPlayerState() != nullptr)
	{
		return Pawn->GetPlayerState();
	}

	const APlayerController* PlayerController = GetOwningPlayerController();
	return PlayerController != nullptr ? PlayerController->PlayerState : nullptr;
}

APlayerController* UFAVoiceComponent::GetOwningPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	if (PlayerController != nullptr)
	{
		CachedPlayerController = PlayerController;
		return PlayerController;
	}

	return CachedPlayerController.Get();
}
