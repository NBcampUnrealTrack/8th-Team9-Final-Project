#include "GAS/FAGameplayTags.h"

#include "GameplayTagsManager.h"

FAGameplayTags FAGameplayTags::Instance;

void FAGameplayTags::InitializeNativeTags()
{
	Instance.AddAllTags();
}

FGameplayTag FAGameplayTags::AddTag(const TCHAR* TagName, const TCHAR* DevComment)
{
	return UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(DevComment));
}

void FAGameplayTags::AddAllTags()
{
	//~ 진영
	Rule_Team_Zombie = AddTag(TEXT("Rule.Team.Zombie"), TEXT("좀비 진영이 보유한 룰 GE 에 붙는 태그. 감염 시 일괄 제거 대상 식별용."));
	Rule_Team_Human  = AddTag(TEXT("Rule.Team.Human"),  TEXT("인간 진영이 보유한 룰 GE 에 붙는 태그. 감염 시 이 태그를 가진 GE 전부 제거."));

	//~ 룰 — Move 계열
	Rule_Move_Freeze = AddTag(TEXT("Rule.Move.Freeze"), TEXT("H15 얼어붙음 — 좀비 5m 이내 진입 시 0.8초 경직."));
	Rule_Move_NoJump       = AddTag(TEXT("Rule.Move.NoJump"),       TEXT("Z01/H01 점프 불가 . GA_Jump 의 ActivationBlockedTags."));
	Rule_Move_SpeedDown_S  = AddTag(TEXT("Rule.Move.SpeedDown.S"),  TEXT("Z02/H02 이동속도 -15%."));
	Rule_Move_LowFriction  = AddTag(TEXT("Rule.Move.LowFriction"),  TEXT("Z04/H04 지면 마찰 -70%. 관성이 커져 미끄러진다."));
	Rule_Move_ForceForward = AddTag(TEXT("Rule.Move.ForceForward"), TEXT("Z05/H05 자동 전진 . 멈출 수 없고 좌우 조향만 가능."));
	Rule_Move_Seizure      = AddTag(TEXT("Rule.Move.Seizure"),      TEXT("Z07/H06 경련·발작 . 6초마다 1.5초 State.Stunned."));
	Rule_Move_NoAriControl = AddTag(TEXT("Rule.Move.NoAriControl"), TEXT("Z09/H08 공중 마비 . 공중 제어 0 "));
	Rule_Move_LandStun	   = AddTag(TEXT("Rule.Move.LandStun"),     TEXT("Z11/H09 착지 경직 . 착지 시 1초 정지 "));
	Rule_Move_NoMantle	   = AddTag(TEXT("Rule.Move.NoMantle"),		TEXT("Z13/H12 등반 불가 . 오르기·넘기 봉인 "));

	//~ 룰 - Input
	Rule_Input_FlipLR      = AddTag(TEXT("Rule.Input.FlipLR"),TEXT("Z13/H10 . 5초 마다 좌우 입력 반전 "));

	//~ 룰 - Vision
	Rule_Vision_NarrowFOV  = AddTag(TEXT("Rule.Vision.NarrowFov"), TEXT("Z16/H17. 시야각 -35% "));
	Rule_Vision_Desaturate = AddTag(TEXT("Rule.Vision.Desaturate"), TEXT("Z17/H18 흑백 — 로컬 PostProcess 채도 0. "));
	Rule_Vision_Fog       = AddTag(TEXT("Rule.Vision.Fog"),       TEXT("Z18/H19 안개 — VisionRange 800uu + PostProcess 안개."));
	Rule_Vision_Blackout  = AddTag(TEXT("Rule.Vision.Blackout"),  TEXT("Z19/H20 명멸/암전 — Periodic GE + PostProcess 암전."));
	Rule_Vision_ForceFPS  = AddTag(TEXT("Rule.Vision.ForceFPS"),  TEXT("Z20/H21 1인칭 강제 — 토글 잠금 + FOV -10%."));
	Rule_Vision_FlipY     = AddTag(TEXT("Rule.Vision.FlipY"),     TEXT("Z21/H22 상하 반전 — 입력 Y축 반전."));
	Rule_Vision_NoZombie  = AddTag(TEXT("Rule.Vision.NoZombie"),  TEXT("H23 투명한 적 — 좀비가 보이지 않음(발소리·실루엣만)."));
	Rule_Vision_NoAlly    = AddTag(TEXT("Rule.Vision.NoAlly"),    TEXT("H24 고독 / Z37 고립 — 다른 생존자가 보이지 않음."));
	Rule_Vision_Shake     = AddTag(TEXT("Rule.Vision.Shake"),     TEXT("Z25 심박 왜곡 — 인간 근접 시 카메라 흔들림 심화."));

	//~ 룰 - Audio
	Rule_Audio_Phantom = AddTag(TEXT("Rule.Audio.Phantom"), TEXT("Z23 환청 — 가짜 발소리를 로컬에서 랜덤 재생."));
	// [Z22 귀먹음 추가] 청취 서브시스템에서 조회할 공통 태그를 등록한다.
	Rule_Audio_Deaf = AddTag(TEXT("Rule.Audio.Deaf"), TEXT("Z22 귀먹음 — 발소리와 수신 음성 차단."));

	//~ 상태
	State_Human         = AddTag(TEXT("State.Human"),         TEXT("현재 인간. 진영 전환 시 서버가 갈아끼운다."));
	State_Zombie        = AddTag(TEXT("State.Zombie"),        TEXT("현재 좀비."));
	State_Stunned       = AddTag(TEXT("State.Stunned"),       TEXT("이동·점프 전면 봉인. 경련/착지 경직/QTE 패배 경직 공용."));
	State_Invulnerable  = AddTag(TEXT("State.Invulnerable"),  TEXT("감염 직후 2초 무적. 연속 드래프트 중 즉사 방지."));
	State_Drafting      = AddTag(TEXT("State.Drafting"),      TEXT("드래프트 UI 활성 중."));
	State_Grabbing      = AddTag(TEXT("State.Grabbing"),      TEXT("좀비가 포획 중 (설계문서 §3.1)."));
	State_Grabbed       = AddTag(TEXT("State.Grabbed"),       TEXT("인간이 포획당한 상태."));
	State_Contesting    = AddTag(TEXT("State.Contesting"),    TEXT("구출 QTE 참여 중 — 이동·점프 봉인 (설계문서 §3.3)."));
	State_Input_Flipped = AddTag(TEXT("State.Input.Flipped"), TEXT("현재 앞뒤좌우 이동 입력이 반전된 구간."));
	State_Climbing      = AddTag(TEXT("State.Climbing"),      TEXT("사다리 등반 중. 이동 모드가 Flying 이라 등반 여부를 태그로 구분한다."));
	State_Hiding        = AddTag(TEXT("State.Hiding"),        TEXT("캐비넷 안에 숨은 상태. 문이 닫혀 있는 동안은 포획 대상에서 빠진다."));
	State_Exposed		= AddTag(TEXT("State.Exposed"),		  TEXT("캐릭터의 위치가 외곽선(X-Ray)으로 노출되는 상태. Z38, H44 등에서 사용."));
	State_Vision_Blind  = AddTag(TEXT("State.Vision.Blind"),  TEXT("일시적인 완전 암전 상태"));

	//~ 룰 - Trade
	Rule_Trade_MutualSight = AddTag(TEXT("Rule.Trade.MutualSight"), TEXT("H48/Z48 상호 투시 — 상대 위치 표시 및 내 위치 노출"));

	//~ 어빌리티
	Ability_Jump = AddTag(TEXT("Ability.Jump"),						TEXT("GA_Jump 식별 태그."));
	Ability_Grab = AddTag(TEXT("Ability.Grab"),						TEXT("GA_Grab 좀비 포획 어빌리티 식별 태그."));
	Ability_Rescue = AddTag(TEXT("Ability.Rescue"),					TEXT("GA_Rescue 인간 구출 어빌리티 식별 태그."));
	Ability_RescueContest = AddTag(TEXT("Ability.RescueContest"),	TEXT("GA_RescueContest QTE 줄다리기 어빌리티 식별 태그."));
	Ability_ContestMash = AddTag(TEXT("Ability.ContestMash"),		TEXT("GA_ContestMash QTE 스페이스바 연타 어빌리티 식별 태그."));

	//~ 이벤트
	Event_Contest_Mash = AddTag(TEXT("Event.Contest.Mash"), TEXT("QTE 줄다리기 스페이스바 연타 1회 이벤트."));

	//~ SetByCaller
	Data_Rule_Magnitude = AddTag(TEXT("Data.Rule.Magnitude"), TEXT("룰 GE Modifier 의 SetByCaller 수치. DataTable 의 Magnitude 열이 주입한다."));

	//~ 룰 - Gamble
	Rule_Gamble_Speed = AddTag(TEXT("Rule.Gamble.Speed"), TEXT("H50 / Z49 10초마다 이동속도 -50%~+50% 랜덤."));
	Rule_Gamble_Plague = AddTag(TEXT("Rule.Gamble.Plague"), TEXT("Z50 감염 시 1명 추가 감염, 실패 시 20초 정지 ( 감염될 확률 35% ) ."));
	Rule_State_Recall = AddTag(TEXT("Rule.State.Recall"), TEXT("H43 도박꾼 / Z40 귀환. 40초마다 공유 스폰 지점 중 무작위 위치로 순간이동"));

	//~ 쿨타임
	Cooldown_Bite = AddTag(TEXT("Cooldown.Bite"), TEXT("좀비 포획(Bite) 어빌리티 쿨타임 상태를 나타내는 태그"));

#pragma region Zombie Rule

	//~ 룰 - Move
	Rule_Move_SpeedDown_L  = AddTag(TEXT("Rule.Move.SpeedDown.L"),  TEXT("Z03 이동속도 -30%."));
	Rule_Move_BackwardOnly  = AddTag(TEXT("Rule.Move.BackwardOnly"),  TEXT("Z06 후진만 가능"));
	Rule_Move_LowJump  = AddTag(TEXT("Rule.Move.LowJump"),  TEXT("Z12 점프 높이 -60%"));

	//~ 룰 - Bite
	Rule_Bite_TimeUp = AddTag(TEXT("Rule.Bite.TimeUp"), TEXT("Z26 무딘 이빨 — 감염 시간 3초 → 5초"));
	Rule_Bite_RangedDown = AddTag(TEXT("Rule.Bite.RangedDown"), TEXT("Z27 짧은 팔 — 포획 사거리 2.0m → 1.2m"));
	Rule_Bite_FailStun = AddTag(TEXT("Rule.Bite.FailStun"), TEXT("Z28 빗나감 — 포획 실패 시 5초 경직"));
	Rule_Bite_RootedGrab = AddTag(TEXT("Rule.Bite.RootedGrab"), TEXT("Z29 고정 턱 — 포획 중 완전 이동 불가"));
	Rule_Bite_RescueStun = AddTag(TEXT("Rule.Bite.RescueStun"), TEXT("Z30 구조 트라우마 — QTE 패배 시 8초 기절"));\
	Rule_Bite_BlindAfter = AddTag(TEXT("Rule.Bite.BlindAfter"), TEXT("Z32 포식 후 실명 — 감염 성공 후 5초 암전"));
	Rule_Bite_Cooldown = AddTag(TEXT("Rule.Bite.Cooldown"), TEXT("Z33 재장전 — 포획 쿨타임 8초"));

	//~ 룰 - Social
	Rule_Social_ForceNoise = AddTag(TEXT("Rule.Social.ForceNoise"), TEXT("Z34 신음 강박 — 3초마다 자동으로 큰 소리 발생"));
	Rule_Social_Exposed = AddTag(TEXT("Rule.Social.Exposed"), TEXT("Z38 상시 표적 — 내 위치가 인간 전원에게 상시 표시"));
	Rule_Social_Repel = AddTag(TEXT("Rule.Social.Repel"), TEXT("Z39 좀비 혐오 — 가까운 좀비와 3m 이내 시 이속 -40%"));
	State_Social_Repel = AddTag(TEXT("State.Social.Repel"), TEXT("Z39 좀비 혐오 — 3m 이내 조건 충족 상태"));

	//~ 룰 - Group
	Rule_Group_Leash = AddTag(TEXT("Rule.Group.Leash"), TEXT("H34 무리 강박 — 가까운 생존자와 15m 초과 시 이속 -40%"));
	State_Group_Leash = AddTag(TEXT("State.Group.Leash"), TEXT("H34 무리 강박 거리 조건(15m 초과) 충족 상태"));
	Rule_Group_Repel = AddTag(TEXT("Rule.Group.Repel"), TEXT("H35 대인 기피 — 가까운 생존자와 5m 이내 시 이속 -30%"));
	State_Group_Repel = AddTag(TEXT("State.Group.Repel"), TEXT("H35 대인 기피 거리 조건(5m 이내) 충족 상태"));

	//~ 룰 - Social
	Rule_Social_Mute = AddTag(TEXT("Rule.Social.Mute"), TEXT("H29 / Z35 침묵 — 보이스 송신 차단."));

	//~ 룰 - Trade
	Rule_Trade_Frenzy = AddTag(TEXT("Rule.Trade.Frenzy"), TEXT("Z43 광폭 — 이동속도 +35%, 감염 시간 10초, 최대 저항 시 20초"));
	Rule_Trade_Ghost = AddTag(TEXT("Rule.Trade.Ghost"), TEXT("Z44 유령 — 반투명, 이동속도 -35%"));
	Rule_Trade_Instant = AddTag(TEXT("Rule.Trade.Instant"), TEXT("Z45 즉살 이빨 — 감염 시간 1초, 최대 저항 5초, 이동속도 -80%"));
	Rule_Trade_WallPhase = AddTag(TEXT("Rule.Trade.WallPhase"),TEXT("Z46 위상 이동 — 특정 벽 통과"));

	Rule_State_Decay = AddTag(TEXT("Rule.State.Decay"),TEXT("Z42 붕괴. 인간 10m 이내에서 가까워질수록 최대 35% 감속"));
	Rule_State_Knockback = AddTag(TEXT("Rule.State.Knockback"), TEXT("Z41 반발 — 물기 중이 아닐 때 인간에게 닿으면 밀려남"));

#pragma endregion

#pragma region Human Rule

	//~ 룰 - Move
	Rule_Move_NoSprint = AddTag(TEXT("Rule.Move.NoSprint"), TEXT("H03 전력질주 봉인"));
	Rule_Move_CrouchOnly = AddTag(TEXT("Rule.Move.CrouchOnly"), TEXT("H07 웅크린 상태로만 이동"));
	Rule_Move_Burden = AddTag(TEXT("Rule.Move.Burden"), TEXT("H11 이동속도-25%,점프높이 -40%"));
	Rule_Move_SlowTurn = AddTag(TEXT("Rule.Move.SlowTurn"), TEXT("H14 카메라 선회속도 -45%"));
	Rule_Move_NoisyJump = AddTag(TEXT("Rule.Move.NoisyJump"), TEXT("H13 삐걱대는 무릎 — 점프 시 반경 25m에 소음 발생."));

	//~ 룰 - Social
	Rule_Social_ForceScream = AddTag(TEXT("Rule.Social.ForceScream"), TEXT("H31 절규 — 8초마다 자동으로 비명 발생"));

	//~ 룰 - Rescue
	Rule_Rescue_Disabled = AddTag(TEXT("Rule.Rescue.Disabled"), TEXT("H32 겁쟁이 — 구출 입력 봉인"));
	Rule_Rescue_SlowHands = AddTag(TEXT("Rule.Rescue.SlowHands"), TEXT("H33 굼뜬 손 — QTE 개시 전 2초 선딜레이"));

	//~ 룰 - Bite
	Rule_Bite_InstantInfect = AddTag(TEXT("Rule.Bite.InstantInfect"), TEXT("H39 즉시 감염 — 포획 성공 즉시 감염"));

	//~ 룰 - Trade
	Rule_Trade_Sprinter = AddTag(TEXT("Rule.Trade.Sprinter"), TEXT("H44 질주 — 이동속도 +40%, 위치 상시 노출"));
	Rule_Trade_Hide = AddTag(TEXT("Rule.Trade.Hide"), TEXT("H45 은둔 — 정지 시 2초 후 반투명, 감염 시 구출·저항 불가"));
	Rule_Trade_SecondLife = AddTag(TEXT("Rule.Trade.SecondLife"), TEXT("H49 불사 — 첫 포획 1회 무효, 이동속도 -30%"));

	State_SecondLife_Consumed = AddTag(TEXT("State.SecondLife.Consumed"), TEXT("H49 첫 포획 무효 소비 상태"));
	State_Hidden = AddTag(TEXT("State.Hidden"), TEXT("H45 은둔 반투명 발동 상태"));

	//~ 룰 - State
	Rule_State_Camp = AddTag(TEXT("Rule.State.Camp"),TEXT("H36 뿌리내림 — 한 자리에 4초 이상 머물면 위치 노출."));

	Rule_State_NoInteract = AddTag(TEXT("Rule.State.NoInteract"),TEXT("H37 문맹 — 문·사다리 등 상호작용 오브젝트 사용 불가."));
	State_NoInteract = AddTag(TEXT("State.NoInteract"),TEXT("상호작용 오브젝트 사용이 제한된 상태."));

	Rule_State_NoStruggle = AddTag(TEXT("Rule.State.NoStruggle"),TEXT("H38 유리 몸 — 일반 포획 저항 불가."));
	State_NoStruggle = AddTag(TEXT("State.NoStruggle"),TEXT("일반 포획 저항 입력이 무효인 상태."));

	Rule_State_Trail = AddTag(TEXT("Rule.State.Trail"), TEXT("H40 — 지나간 경로가 20초간 좀비에게 표시"));

	Rule_State_Heartbeat = AddTag(TEXT("Rule.State.Heartbeat"),TEXT("H41 — 좀비 15m 이내 시 위치 점멸 표시"));
	State_Camp = AddTag(TEXT("State.Camp"),TEXT("4초 이상 정지하여 위치가 노출된 상태"));

#pragma endregion
}
