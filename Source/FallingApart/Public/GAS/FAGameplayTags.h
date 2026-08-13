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
	
	// ~ ==========================Input Tag ===================

	/* Z13 뒤엉킨 신경 / H10 뒤엉킨 신경 -  5초 마다 좌우 입력 반전 */
	FGameplayTag Rule_Input_FlipLR;
	
	
	
	
	// ~ ==========================Vision Tag ===================
	
	/* Z16 백내장 / H17 시야 협착 -  시야각 -35% */
	FGameplayTag Rule_Vision_NarrowFOV;
	
	/* Z17 흑백 / H18 흑백 -  시야거리 8m */
	FGameplayTag Rule_Vision_Desautrate;
	
	
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

private:
	static FAGameplayTags Instance;

	// 태그 추가 
	void AddAllTags(); 

	/** UGameplayTagsManager 등록 + 멤버 대입을 한 줄로. */
	static FGameplayTag AddTag(const TCHAR* TagName, const TCHAR* DevComment);
};
