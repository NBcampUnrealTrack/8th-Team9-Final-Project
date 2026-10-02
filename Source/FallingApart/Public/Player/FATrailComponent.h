#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FATrailComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/** 서버가 복제하는 발자국 한 개의 기록. Niagara 컴포넌트 자체는 복제하지 않는다. */
USTRUCT()
struct FATrailPoint
{
	GENERATED_BODY()

	// 캐릭터의 TrailComponent 내부에서 고유한 번호. 같은 효과의 중복 생성을 막는다.
	UPROPERTY()
	int32 Id = 0;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY()
	double SpawnTime = 0.0; // 서버 월드 시각(초). 수신 시각과 무관하게 20초 수명을 계산한다.
};

/** H40: 서버에서 경로를 기록하고 로컬 좀비 화면에만 표시한다. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FALLINGAPART_API UFATrailComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFATrailComponent();
	void StartTrail();
	void StopTrail();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateTrail();
	void RecordTrailPoint(double Now);
	void RefreshTrailVisuals();
	void ClearTrailVisuals();

	// 서버가 추가/만료 처리하고 클라이언트가 화면 표시의 근거로 사용하는 기록.
	UPROPERTY(Replicated)
	TArray<FATrailPoint> TrailPoints;
	// 이 화면에서만 생성한 효과: 기록 ID → Niagara 인스턴스. 진영 변경 시 모두 정리한다.
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UNiagaraComponent>> LocalEffects;

	/** 캐릭터 BP에서 지정할 Niagara System 에셋. 비어 있으면 기록만 하고 표시하지 않는다.
	 * 실제 캐릭터 메시를 User.SourceMesh로 전달한다. Particle Spawn에서만 포즈를 읽어야 한다. */
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Trail")
	TObjectPtr<UNiagaraSystem> TrailSystem;
	
	// Niagara의 Skeletal Mesh 데이터 인터페이스 User Parameter 이름. 
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Trail")
	FName TrailMeshParameterName = TEXT("User.SourceMesh");
	// 생성할 Niagara 컴포넌트의 배율. 실제 입자 크기 반영은 Niagara 에셋 구성에 따른다.
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Trail")
	FVector EffectScale = FVector::OneVector;
	// 이전 바닥 효과용 설정. 현재 포즈 잔상은 실제 메시 회전을 사용하므로 적용하지 않는다. 
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Trail")
	FRotator EffectRotationOffset = FRotator::ZeroRotator;
	// 마지막 기록 위치와의 수평 거리 기준  발 애니메이션/좌우 발 소켓과는 무관 
	UPROPERTY(EditDefaultsOnly, Category="FallingApart|Trail", meta=(ClampMin="1.0"))
	float PointSpacing = 75.0f;

	static constexpr double TrailLifetime = 10.0;
	FTimerHandle UpdateTimer;
	bool bRecording = false;
	bool bHasLastLocation = false;
	FVector LastLocation = FVector::ZeroVector;
	int32 NextPointId = 0;
	
	
	
	
};
