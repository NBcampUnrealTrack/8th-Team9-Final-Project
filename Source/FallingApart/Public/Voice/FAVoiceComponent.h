// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "Voice/FAVoiceTypes.h"
#include "FAVoiceComponent.generated.h"

class APlayerState;
class UFAVoiceTalker;
class UFAVoiceTransport;
class USceneComponent;
class USoundAttenuation;
class UUserWidget;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFAOnVoiceTalkingStateChanged, bool, bIsTalking);

/**
 * 근접 보이스 컴포넌트.
 *
 * 특정 캐릭터에 의존하지 않는다. AFACharacter(근접)와 AFASpectatorPawn(전역)이 각각 생성자에서 붙인다.
 */
UCLASS(ClassGroup = (FallingApart), meta = (BlueprintSpawnableComponent))
class FALLINGAPART_API UFAVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFAVoiceComponent();

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	EFAVoiceChannel GetVoiceChannel() const { return VoiceChannel; }

	/**
	 * 근접 / 전역 전환. 관전자 Pawn 은 Global
	 * 관전자와 생존 플레이어의 실제 격리는 AFAGameMode 의 게임플레이 뮤트가 담당
	 */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void SetVoiceChannel(EFAVoiceChannel NewChannel);

	/**
	 * 최대 거리(uu). 기본 4000 = 40m.
	 */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	virtual float GetHearingRadius() const;

	/** 감쇠가 시작되는 거리(uu). 기본 2500 = 25m. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	virtual float GetFullVolumeRadius() const;

	/** 반경이 바뀌었을 때(확성/침묵 룰 등) 감쇠를 다시 만들어 적용. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void RefreshVoiceAttenuation();

	/** 이 화자의 목소리를 재생할 배수. 발화 중이면 즉시 반영된다. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void SetVoicePlaybackVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	float GetVoicePlaybackVolume() const { return VoicePlaybackVolume; }

	/** 마이크 송신 시작. 푸시투토크 키의 Pressed 에 연결. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void StartTalking();

	/** 마이크 송신 중지. 푸시투토크 키의 Released 에 연결. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void StopTalking();

	/** 푸시투토크 on/off. 끄면(오픈 마이크) 즉시 송신을 시작. */
	UFUNCTION(BlueprintCallable, Category = "FallingApart|Voice")
	void SetPushToTalkEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	bool IsPushToTalkEnabled() const { return bPushToTalk; }

	/** 이 컴포넌트의 소유자가 지금 말하고 있는가. 목소리를 실제로 수신 중인 머신에서만 true. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	bool IsTalking() const;

	/** 이 소유자의 목소리가 나오는 위치(헤드 소켓). 리스너 오버라이드/아이콘 부착. */
	UFUNCTION(BlueprintPure, Category = "FallingApart|Voice")
	USceneComponent* GetVoiceSourceComponent() const;

	/** 발화 시작/종료 알림. 머리 위 아이콘 외의 연출(입 모양, 팀 UI 등). */
	UPROPERTY(BlueprintAssignable, Category = "FallingApart|Voice")
	FFAOnVoiceTalkingStateChanged OnTalkingStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 목소리가 나올 씬 컴포넌트를 결정. 헤드 소켓이 없는 Pawn 이면 루트 + 오프셋으로 대체. */
	virtual USceneComponent* ResolveVoiceSourceComponent();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_VoiceChannel, Category = "FallingApart|Voice")
	EFAVoiceChannel VoiceChannel = EFAVoiceChannel::Proximity;

	UFUNCTION()
	void OnRep_VoiceChannel();

	/** 들리는 최대 거리(uu). Global 채널에서는 무시. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HearingRadius = 4000.f;

	/** 감쇄 없는 거리. 이 안에서는 거리와 무관하게 같은 음량. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (ClampMin = "0", ForceUnits = "cm"))
	float FullVolumeRadius = 2500.f;

	/** HearingRadius 지점의 음량(dB). -60 무음. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice", meta = (ClampMin = "-60", ClampMax = "0"))
	float AttenuationAtMaxDb = -60.f;

	/**
	 * 이 화자의 목소리 재생 배수. 사용자 출력 볼륨(FAVoice.OutputVolume)과 곱해진다.
	 *
	 * 엔진 기본 재생 배수는 1.5 인데(OnlineSubsystemUtils.cpp) 그레이박스 맵에서 너무 작았다.
	 * 마이크 게인(voice.MicInputGain)으로는 이 이상 못 올린다 — 캡처 경로가 int16 에 게인을
	 * 곱하면서 하드 클리핑하기 때문에 크게 말하면 지직거린다. 그래서 음량은 재생 쪽에서 번다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallingApart|Voice", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "8.0"))
	float VoicePlaybackVolume = 3.5f;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	TObjectPtr<USoundAttenuation> AttenuationOverride;

	/** 목소리가 나올 스켈레탈 메시 소켓. 없으면 루트 + VoiceSourceFallbackOffset. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	FName HeadSocketName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	FVector VoiceSourceFallbackOffset = FVector(0.f, 0.f, 60.f);

	/** 3인칭 카메라 경우에도 캐릭터 헤드에 있도록 옮김. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	bool bOverrideListenerToHead = true;

	/** 기본은 오픈 마이크. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	bool bPushToTalk = false;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	TSubclassOf<UFAVoiceTransport> TransportClass;

	/** 지정하면 발화 중 머리 위에 띄움. HearingRadius 밖에서는 자동으로 컬링. */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	TSubclassOf<UUserWidget> SpeakingIndicatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice")
	FVector SpeakingIndicatorOffset = FVector(0.f, 0.f, 25.f);

	/**
	 * 아이콘 크기(화면 픽셀). Screen space 라 거리와 무관하게 고정된다.
	 * 위젯의 Desired Size 를 쓰면 브러시 원본 해상도(512px 등)가 그대로 화면 크기가 되므로 여기서 잡는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Voice", meta = (ClampMin = "1"))
	FVector2D SpeakingIndicatorDrawSize = FVector2D(48.f, 48.f);

private:
	/** PlayerState 와 UniqueNetId 가 준비될 때까지 재시도. */
	void TryRegisterTalker();

	/** 채널이 바뀐 뒤 등록·감쇠·리스너·아이콘을 현재 채널에 맞춤. */
	void ApplyChannelChange();

	/** 현재 채널에 맞는 재생 위치 / 감쇠를 Talker 에 반영. */
	void ApplyTalkerSettings();

	void SetupVoiceSource();
	void SetupListenerAnchor();
	void SetupSpeakingIndicator();
	void ApplyListenerOverride();
	USoundAttenuation* BuildAttenuation();
	void HandleTalkerStateChanged(bool bIsTalking);
	APlayerState* GetOwningPlayerState() const;
	class APlayerController* GetOwningPlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<UFAVoiceTalker> Talker;

	UPROPERTY(Transient)
	TObjectPtr<UFAVoiceTransport> Transport;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> RuntimeAttenuation;

	/** 목소리가 나오는 위치(헤드 본). 애니메이션을 따라 움직인다. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VoiceSourceComponent;

	/**
	 * 오디오 리스너 위치. 헤드 본이 아니라 루트(캡슐) + 헤드 높이에 붙는다.
	 * 리스너는 위치뿐 아니라 회전도 쓰는데, 헤드 본의 축은 액터 forward 와 무관하고
	 * 애니메이션마다 흔들려서 그대로 쓰면 좌우 정위가 틀어진다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ListenerAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> SpeakingIndicator;

	/**
	 * 마지막으로 유효했던 소유 컨트롤러.
	 * AFAGameMode::RestartAllPlayers 는 라운드마다 UnPossess() 후 Pawn 을 파괴하므로,
	 * EndPlay 시점에는 Pawn->GetController() 가 이미 null. 정리 작업(마이크 끄기·리스너 복구).
	 */
	mutable TWeakObjectPtr<APlayerController> CachedPlayerController;

	FTimerHandle RegisterRetryTimerHandle;

	/** AI 등 PlayerState 가 영영 안 생기는 Pawn 에서 타이머가 무한히 도는 것을 막음. */
	int32 RemainingRegisterAttempts = 0;
};
