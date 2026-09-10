#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 프로젝트 전역 네이티브 GameplayTag 모음
 *
 * 전부 여기서 한 번 등록하고 FAGameplayTags::Get().Rule_Move_NoJump 처럼 참조할 것.
 *
 *  룰 태그를 새로 추가할 때:
 *   1) 여기에 멤버 선언 → 2) FAGameplayTags.cpp 의 AddAllTags() 에 등록 → 3) DT 행의 RuleTag 에 지정.
 *   DataTable 의 FGameplayTag 필드는 여기 등록된 태그만 에디터 드롭다운에 뜬다.
 */
struct FALLINGAPART_API FAGameplayTags
{
public:
	static const FAGameplayTags& Get() { return Instance; }

	/** 모듈 StartupModule 에서 1회 호출. 태그 매니저에 네이티브 태그를 등록한다. */
	static void InitializeNativeTags();

	//~ ========================= 진영 태그 =========================
	// GE 애셋은 좀비/인간 공용 1개만 만들고, 적용 시점에 이 태그를 DynamicGrantedTags 로 붙인다.
	// 감염 시 UFAAbilitySystemComponent::RemoveRulesOfTeam() 이 이 태그로 인간 룰만 골라 제거한다.

	FGameplayTag Rule_Team_Zombie;
	FGameplayTag Rule_Team_Human;

	//~ ========================= 룰 태그 (Move 계열) =========================
	// 공통 룰 14종 중 이번 1차 구현분 5종. 나머지는 인수인계 문서 참고.

	/** Z01 굳은 관절 / H01 삔 발목 — 점프 불가. GA_Jump 의 ActivationBlockedTags. */
	FGameplayTag Rule_Move_NoJump;

	/** Z02 시체의 다리 / H02 지친 다리 — 이동속도 -15%. MoveSpeed Attribute 배수 수정. */
	FGameplayTag Rule_Move_SpeedDown_S;

	/** Z04 미끄러짐 / H04 젖은 바닥 — 마찰 -70%. GroundFriction Attribute 배수 수정. */
	FGameplayTag Rule_Move_LowFriction;

	/** Z05 전진 강박 / H05 공황 질주 — 자동 전진. CMC 가 입력을 오버라이드한다. */
	FGameplayTag Rule_Move_ForceForward;

	/** Z07 경련 / H06 발작 — 6초마다 1.5초 정지. Periodic GE 가 State.Stunned 를 1.5초 부여. */
	FGameplayTag Rule_Move_Seizure;

	/* Z09 공중 마비 /H08 공중 마비 -공중 제어 0  */
	FGameplayTag Rule_Move_NoAriControl;

	/* Z11 착지 경직 /H09 착지 경직 - 착지 시 1초 정지  */
	FGameplayTag Rule_Move_LandStun;

	/* Z13 등반 불가 /H12 등반 불가 - 오르기·넘기 봉인  */
	FGameplayTag Rule_Move_NoMantle;

	// ~ ========================== Input Tag ===================

	/* Z13 뒤엉킨 신경 / H10 뒤엉킨 신경 -  5초 마다 좌우 입력 반전 */
	FGameplayTag Rule_Input_FlipLR;

	// ~ ========================== Vision Tag ===================

	/* Z16 백내장 / H17 시야 협착 -  시야각 -35% */
	FGameplayTag Rule_Vision_NarrowFOV;

	/* Z17 흑백 / H18 흑백 -  로컬 PostProcess 채도 0 */
	FGameplayTag Rule_Vision_Desaturate;

	/* Z18 안개 / H19 안개 - VisionRange 800uu + PostProcess 안개 */
	FGameplayTag Rule_Vision_Fog;

	/* Z19 명멸 / H20 암전 - Periodic GE + PostProcess 암전 */
	FGameplayTag Rule_Vision_Blackout;

	/* Z20 1인칭 강제 / H21 1인칭 강제 - 토글 강제 전환 후 잠금 + FOV -10% */
	FGameplayTag Rule_Vision_ForceFPS;

	/* Z21 상하 반전 / H22 상하 반전 - 입력 Y축 반전 */
	FGameplayTag Rule_Vision_FlipY;

	/* H23 투명한 적 - 좀비가 보이지 않음 (발소리·실루엣만, CustomDepth 아웃라인으로 표시) */
	FGameplayTag Rule_Vision_NoZombie;

	/* H24 고독 - 다른 생존자(인간)가 보이지 않음 (완전히 숨김) */
	FGameplayTag Rule_Vision_NoAlly;

	/* Z25 심박 왜곡 - 좀비 전용. 인간 근접 시 카메라 흔들림 심화 */
	FGameplayTag Rule_Vision_Shake;



	//~ ========================= 상태 태그 =========================
	// 룰이 아니라 '지금 이 캐릭터가 어떤 상태인가'. 룰 GE 도, 포획/QTE 어빌리티도 공유해 쓴다.

	FGameplayTag State_Human;
	FGameplayTag State_Zombie;

	/** 이동·점프 전면 봉인. 경련·착지 경직·QTE 패배 경직이 전부 이 태그를 공유한다. */
	FGameplayTag State_Stunned;

	/** 감염 직후 2초 무적 (GDD 3.4) — 연속 드래프트 중 즉사 방지. */
	FGameplayTag State_Invulnerable;

	/** 드래프트 UI 가 열려 있는 동안. 숫자키 입력 라우팅에 쓴다. */
	FGameplayTag State_Drafting;

	//~ 포획 · 구출 QTE 담당자용 (설계문서 §3). 지금은 등록만 해두고 소비는 나중에.
	FGameplayTag State_Grabbing;
	FGameplayTag State_Grabbed;
	FGameplayTag State_Contesting;

	/** 5초 주기 입력 반전 룰에서 현재 앞뒤좌우 입력이 반전된 구간. */
	FGameplayTag State_Input_Flipped;

	/** 캐릭터의 위치가 외곽선(X-Ray)으로 노출되는 상태 */
	FGameplayTag State_Exposed;

	/** 사다리를 타고 있는 동안. 이동 모드가 Flying 이라 다른 판정이 이 태그로 등반을 구분한다. */
	FGameplayTag State_Climbing;

	/** 캐비넷 안에 숨어 있는 동안. 포획 제외 · 애니메이션 분기 · 룰 판정이 이 태그를 본다. */
	FGameplayTag State_Hiding;

	//~ ========================= Group =========================

	/* H34 무리 강박 . 가장 가까운 생존자와 15m 초과 시 이동속도 -40% */
	FGameplayTag Rule_Group_Leash;
	FGameplayTag State_Group_Leash;

	/* H35 대인 기피 . 가장 가까운 생존자와 5m 이내 시 이동속도 -30% */
	FGameplayTag Rule_Group_Repel;
	FGameplayTag State_Group_Repel;

	//~ ========================= Trade Tag =========================

	/* H48 예지 / Z48 사냥꾼의 눈 - 상대 진영 전원 위치 표시, 내 위치 노출 */
	FGameplayTag Rule_Trade_MutualSight;

	//~ ========================= 어빌리티 태그 =========================

	FGameplayTag Ability_Jump;
	FGameplayTag Ability_Grab;
	FGameplayTag Ability_Rescue;
	FGameplayTag Ability_RescueContest;
	FGameplayTag Ability_ContestMash;

	//~ ========================= 이벤트 태그 =========================

	FGameplayTag Event_Contest_Mash;

	//~ ========================= SetByCaller 데이터 태그 =========================

	/**
	 * 룰 GE 의 Modifier 크기를 DataTable 이 결정하게 하는 통로.
	 * GE 에디터에서 Magnitude Calculation Type = Set By Caller, Data Tag = Data.Rule.Magnitude 로 두면
	 * 밸런싱 수치를 애셋 재저장 없이 DT 한 칸만 고쳐서 바꿀 수 있다.
	 */
	FGameplayTag Data_Rule_Magnitude;

	//~ ========================= Gamble 룰 태그 =========================

	/** H50 저주받은 행운 / Z49  룰렛 . 10초마다 이동속도가 -50%~+50% 범위에서 무작위로 변한다. */
	FGameplayTag Rule_Gamble_Speed;

	/* Z50 전염 도박 .  감염 시 1명 추가 감염, 실패 시 20초 정지 ( 감염될 확률 35% ) */
	FGameplayTag Rule_Gamble_Plague;

	/* H43 도박꾼.  40초마다 맵 랜덤 위치로 순간이동 */
	FGameplayTag Rule_Gamble_Blink;

	//~ ========================= Cooldown Tag =========================

	FGameplayTag Cooldown_Bite;

#pragma region Zombie Rule

	// Move
	/* Z03  썩은 발목 . 이동속도 -30%.  */
	FGameplayTag Rule_Move_SpeedDown_L;

	/* Z06  역행 . 후진만 가능  */ //전진 강박 존제시 적용 불가능하게 나올것
	FGameplayTag Rule_Move_BackwardOnly;

	/* Z12  낮은 도약 . 점프높이 -60%  */
	FGameplayTag Rule_Move_LowJump;

	// Bite
	/* Z26 무딘 이빨 . 감염 시간 3초 → 5초 */
	FGameplayTag Rule_Bite_TimeUp;

	/* Z27 짧은 팔 . 포획 사거리 2.0m → 1.2m */
	FGameplayTag Rule_Bite_RangedDown;

	/* Z28 빗나감 . 포획 실패 시 5초 경직 */
	FGameplayTag Rule_Bite_FailStun;

	/* Z29 고정 턱 . 포획 중 완전 이동 불가 */
	FGameplayTag Rule_Bite_RootedGrab;

	/* Z30 구조 트라우마 . QTE 패배 시 8초 기절 */
	FGameplayTag Rule_Bite_RescueStun;

	/* Z33 재장전. 포획 쿨타임 8초 */
	FGameplayTag Rule_Bite_Cooldown;

	// Social
	/* Z38 상시 표적 . 내 위치 상시 노출 */
	FGameplayTag Rule_Social_Exposed;

	/* Z39 좀비 혐오 . 다른 좀비 3m 이내 시 이동속도 -40% */
	FGameplayTag Rule_Social_Repel;
	FGameplayTag State_Social_Repel;

	// Trade
	/* Z43 광폭 . 이동속도 +35%, 감염 시간 6초 */
	FGameplayTag Rule_Trade_Frenzy;

	/* Z44 유령 . 반투명, 이동속도 -35% */
	/* Z45 즉살 이빨 . 감염 시간 1초, 최대 저항 5초, 이동속도 -80% */
	FGameplayTag Rule_Trade_Instant;

	/** Z46 위상 이동 . PhaseWall 충돌 벽만 통과 */
	FGameplayTag Rule_Trade_WallPhase;

	/* Z44 유령. 반투명, 이동속도 -35% */
	FGameplayTag Rule_Trade_Ghost;
	// ~ ====== State
	// * H36 뿌리내림 /  */
	FGameplayTag Rule_State_Camp;

	/** H37 문맹 — 상호작용 오브젝트 사용 불가 */
	FGameplayTag Rule_State_NoInteract;
	FGameplayTag State_NoInteract;

	/** H38 유리 몸 — 일반 포획 저항 입력 무효, 구출 QTE는 정상 */
	FGameplayTag Rule_State_NoStruggle;
	FGameplayTag State_NoStruggle;

	/** H41 — 15m 이내 좀비에게 보유자의 위치를 점멸 표시한다. */
	FGameplayTag Rule_State_Heartbeat;
	FGameplayTag Rule_State_Trail;

	// 4초 정지 조건을 충족한 상태
	FGameplayTag State_Camp;

#pragma endregion

#pragma region Human Rule

	// Move
	/* H03 탈진 . 전력질주 봉인*/
	FGameplayTag Rule_Move_NoSprint;

	/* H07 포복 . 웅크린 상태로만 이동*/
	FGameplayTag Rule_Move_CrouchOnly;

	FGameplayTag Rule_Move_Burden;

	FGameplayTag Rule_Move_SlowTurn;

	// Rescue
	/* H32 겁쟁이 . 구출(좌클릭) 입력 봉인 — QTE 진입 불가*/
	FGameplayTag Rule_Rescue_Disabled;

	/* H33 굼뜬 손. QTE 개시 전 2초 선딜레이 */
	FGameplayTag Rule_Rescue_SlowHands;

	/* H39 즉시 감염. 포획 성공 즉시 감염 (3초 없음) */
	FGameplayTag Rule_Bite_InstantInfect;

	// Trade
	/* H44 질주 . 이동속도 +40%, 위치 노출 */
	FGameplayTag Rule_Trade_Sprinter;

	FGameplayTag Rule_Trade_Hide;
	FGameplayTag State_Hidden;

	FGameplayTag Rule_Trade_SecondLife;
	FGameplayTag State_SecondLife_Consumed;
#pragma endregion

private:
	static FAGameplayTags Instance;

	// 태그 추가
	void AddAllTags();

	/** UGameplayTagsManager 등록 + 멤버 대입을 한 줄로. */
	static FGameplayTag AddTag(const TCHAR* TagName, const TCHAR* DevComment);
};
