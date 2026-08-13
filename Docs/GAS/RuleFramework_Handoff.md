# GAS 룰 프레임워크 인수인계

> 대상: 룰 카드를 추가·튜닝할 팀원
> 범위: 공통 룰 ◆ 14종 중 **첫 5종**(점프 불가 / 이동속도 -15% / 미끄러짐 / 자동 전진 / 경련·발작)
> 근거 문서: 《기술·GAS 설계문서 v1.1》 §1, 《룰 카드 도감 워크북 v1.1》 3_공통룰, 《밸런싱 워크북 v1.1》


---

## 0. 절대 원칙

> **모든 룰은 'Attribute 수정 + 태그 부여'로만 구현한다.**
> 새 C++ 클래스나 새 Tick 이 필요해지는 룰은, 그 룰의 설계를 의심할 것. (설계문서 §1)

---

## 1. 지금 무엇이 만들어져 있나

### C++ (전부 완료 · 빌드 통과)

| 경로 | 역할 |
|---|---|
| `Public/GAS/FAGameplayTags.h/.cpp` | 네이티브 태그 전량 등록. `FAGameplayTags::Get().Rule_Move_NoJump` 로 참조 |
| `Public/GAS/FAAbilitySystemComponent.h/.cpp` | 룰 부착·제거·4장 상한·진영별 일괄 제거 |
| `Public/GAS/FAGameplayAbility.h/.cpp` | 어빌리티 공통 베이스 |
| `Public/GAS/Abilities/FAGA_Jump.h/.cpp` | 점프 어빌리티 — 점프 불가 룰이 걸리는 지점 |
| `Public/Rule/FARuleTypes.h` | `FFARuleCardRow` (DataTable 행 구조체) |
| `Public/Rule/FARuleSettings.h/.cpp` | 프로젝트 설정에서 DT 위치·룰 상한 지정 |
| `Public/Player/FACharacterMovementComponent.h/.cpp` | 이동 룰이 실제로 몸에 느껴지는 곳 |
| `AttributeSet/FAAttributeSet.h/.cpp` | (기존) MoveSpeed / GroundFriction / AirControl 등 |

`AFACharacter` 가 `IAbilitySystemInterface` 를 구현하고 ASC·AttributeSet 을 소유한다.
ASC 를 PlayerState 가 아니라 **캐릭터**에 둔 이유: 라운드 중 리스폰이 없고(감염돼도 같은 폰이 좀비가 된다),
룰 효과의 소비자가 전부 캐릭터(CMC·카메라·PostProcess)라 참조가 짧아진다.

---

## 2. 태그 계층 (C++ 에 이미 등록됨 — ini 편집 금지)

태그는 전부 `FAGameplayTags.cpp` 에서 네이티브로 등록된다.
**에디터의 GameplayTag 설정 창에서 손으로 추가하지 말 것** — 중복 등록되고 ini 가 지저분해진다.

```
Rule.Team.Zombie / Rule.Team.Human      진영. GE 애셋이 아니라 코드가 붙인다 (§4 참고)

Rule.Move.NoJump                        Z01 / H01
Rule.Move.SpeedDown.S                   Z02 / H02
Rule.Move.LowFriction                   Z04 / H04
Rule.Move.ForceForward                  Z05 / H05
Rule.Move.Seizure                       Z07 / H06

State.Human / State.Zombie              현재 진영. 서버가 갈아끼우고 전원에게 복제된다
State.Stunned                           이동·점프 전면 봉인 (경련·착지 경직·QTE 패배 경직 공용)
State.Invulnerable                      감염 직후 2초 무적
State.Drafting / State.Grabbing / State.Grabbed / State.Contesting

Ability.Jump                            GA_Jump 식별

Data.Rule.Magnitude                     SetByCaller — DT 의 Magnitude 열이 여기로 들어온다
```

### 태그를 새로 추가할 때

1. `FAGameplayTags.h` 에 멤버 선언
2. `FAGameplayTags.cpp` 의 `AddAllTags()` 에 `AddTag()` 한 줄
3. DT 행의 `RuleTag` 에서 드롭다운으로 선택

---

## 3. 폴더 규약

```
Content/FallingApart/GAS/
├─ Abilities/   GA_Jump              (UFAGA_Jump 상속 BP)
├─ Effects/     GE_Rule_*            (룰 GE)
└─ Data/        DT_RuleCards         (룰 카드 표)
```

---

## 4. GameplayEffect 에디터 설정

### 4.0 모든 룰 GE 의 공통 규칙

| 항목 | 값 | 이유 |
|---|---|---|
| **Duration Policy** | `Infinite` | 룰은 라운드 끝까지 유지된다. 제거는 코드가 한다 |
| **Granted Tags** | Components → `Grant Tags to Target Actor` → `Add Tags` → **`Add to Inherited`** 에 해당 룰 태그 | UE5.3 부터 GE 의 태그 부여가 컴포넌트로 이동했다. 태그를 넣는 칸은 `Add to Inherited` 다. `Combined Tags` 는 읽기 전용 결과 표시라 입력이 안 된다 |
| **진영 태그** | **넣지 않는다** | 공통 룰은 GE 애셋 1개를 좀비/인간이 공유한다. `Rule.Team.*` 은 적용 시점에 `UFAAbilitySystemComponent::ApplyRuleCard()` 가 `DynamicGrantedTags` 로 붙인다. 애셋에 박아두면 공유가 깨진다 |
| **Stacking** | `None` | 같은 룰 중복 획득은 드래프트 후보 생성 단계에서 걸러진다 |

> **Granted Tags 를 빠뜨리면 룰이 아무 일도 하지 않는다.**
> CMC·어빌리티는 전부 "이 태그가 있는가"만 보기 때문이다. GE 를 만들고 나면 이것부터 확인할 것.

---

## 5. DataTable

### 5.1 만들기

1. `Content/FallingApart/GAS/Data` 에서 우클릭 → Miscellaneous → **Data Table**
2. Row Structure 에 **`FARuleCardRow`** 선택
3. 이름 `DT_RuleCards`

또는 저장소의 **`Docs/GAS/DT_RuleCards.csv`** 를 드래그해서 임포트하면 10행이 한 번에 들어간다
(임포트 시 Row Struct 로 `FARuleCardRow` 선택).

> CSV 의 `RuleEffect` 경로는 §3 폴더 규약을 전제로 미리 적어뒀다.
> 다른 경로에 만들었다면 임포트 후 에디터에서 다시 지정할 것 — 비어 있으면 해당 카드는 후보에서 자동 제외된다.

### 5.2 열 의미

| 열 | 설명 |
|---|---|
| **Row Name** | **곧 카드 ID 다** (`Z01`, `H02` …). 코드는 이 이름으로만 카드를 주고받는다 |
| DisplayName / ShortDesc | 카드 제목(8자 이내) / 한 줄 설명 |
| Team | `Zombie` / `Human`. 드래프트 후보 1차 필터이자 진영 태그 결정 근거 |
| Tier | `T1`/`T2`/`T3`. 회차별 티어 구성표가 이 값으로 고른다 |
| Type | `Penalty` / `Trade` / `Gamble` (도감의 P/T/G) |
| RuleTag | 정체성 태그. GE 의 Add Tags 와 **같은 값**이어야 한다 |
| RuleEffect | GE 애셋. 비어 있으면 그 카드는 후보에서 자동 제외 |
| ConflictTags | 동시 보유 불가. 하나라도 겹치면 후보에서 빠진다 |
| Magnitude | GE 의 SetByCaller 로 들어가는 수치. **곱연산은 배수** (-15% → 0.85) |
| DraftWeight | 같은 티어 안에서의 가중치. 0 이하면 아예 안 나온다 |
| bMVP | MVP 20종 여부 |

### 5.3 이번 5종의 값

| Row | 이름 | Team | Tier | RuleTag | Magnitude |
|---|---|---|---|---|---|
| Z01 | 굳은 관절 | Zombie | T1 | `Rule.Move.NoJump` | – |
| H01 | 삔 발목 | Human | T1 | `Rule.Move.NoJump` | – |
| Z02 | 시체의 다리 | Zombie | T1 | `Rule.Move.SpeedDown.S` | **0.85** |
| H02 | 지친 다리 | Human | T1 | `Rule.Move.SpeedDown.S` | **0.85** |
| Z04 | 미끄러짐 | Zombie | T2 | `Rule.Move.LowFriction` | **0.3** |
| H04 | 젖은 바닥 | Human | T2 | `Rule.Move.LowFriction` | **0.3** |
| Z05 | 전진 강박 | Zombie | T3 | `Rule.Move.ForceForward` | – |
| H05 | 공황 질주 | Human | T3 | `Rule.Move.ForceForward` | – |
| Z07 | 경련 | Zombie | **T2** | `Rule.Move.Seizure` | – |
| H06 | 발작 | Human | **T3** | `Rule.Move.Seizure` | – |

> 경련만 진영별 티어가 다르다(도감 워크북 그대로). 인간이 1.5초 굳는 게 더 치명적이라는 판단이다.
> 같은 GE 애셋을 쓰면서 티어만 다르게 두는 게 가능한 이유가 "GE 1개 + 행 2개" 구조다.

### 5.4 프로젝트 설정에 물리기

**Project Settings → Game → Falling Apart | Rule**

| 항목 | 값 |
|---|---|
| Rule Card Table | `DT_RuleCards` |
| Max Active Rules | `4` (밸런싱 워크북 0_기준) |

여기를 비워두면 드래프트가 **더미 룰 풀로 폴백**한다(로그 경고 1회). 게임은 계속 돌지만 GE 는 안 붙는다.

---

## 6. 다음 사람이 이어서 할 일

### 6.1 남은 공통 룰 9종

| 룰 | 태그 | 구현 방식 | 난이도 |
|---|---|---|---|
| 공중 마비 | `Rule.Move.NoAirControl` | AirControl Attribute → 0. **CMC 는 이미 준비됨. GE 만 만들면 끝** | 즉시 |
| 시야각 축소 | `Rule.Vision.NarrowFOV` | FieldOfView Attribute (AttributeSet 에 추가 필요) | 낮음 |
| 흑백 | `Rule.Vision.Desaturate` | 로컬 PostProcess. 태그 이벤트로 토글 | 낮음 |
| 안개 | `Rule.Vision.Fog` | SightRadius + PP | 중간 |
| 착지 경직 | `Rule.Move.LandStun` | `Landed` 이벤트 → `State.Stunned` 1초 GE. **CMC 정지 처리 재사용** | 낮음 |
| 명멸 | `Rule.Vision.Blackout` | Periodic GE — **§4.5 구조 그대로 복사** (Period 6 / 0.7초 암전) | 낮음 |
| 좌우 입력 반전 | `Rule.Input.FlipLR` | Periodic 토글 | 중간 |
| 1인칭 강제 | `Rule.Vision.ForceFPS` | 시점 토글 잠금 + FOV -10% | 중간 |
| 상하 반전 | `Rule.Vision.FlipY` | 입력 Y축 반전 | 낮음 |

시야 계열은 전부 **로컬 PostProcess** 다. 서버는 태그만 복제하고 화면 처리는 클라이언트가 한다(설계문서 §1.4).
ASC 의 `RegisterGameplayTagEvent` 로 태그를 감지해 PP 파라미터를 토글하는 공통 컴포넌트를 하나 만들면
시야 룰 12종이 전부 그 위에 얹힌다 — **개별로 만들지 말 것.**

### 6.2 프레임워크 쪽 남은 구멍

- **좀비 속도 우위 5%** (MoveSpeed 630) — 진영 배정 시 초기화 GE 가 필요하다. 지금은 전원 600.
- **드래프트 후보 필터** — `ConflictTags` 충돌 제외와 `DraftWeight` 가중 랜덤이 아직 미적용
  (`UFARuleDraftSubsystem::GenerateCandidates` 는 현재 균등 랜덤).
- **연속 드래프트** — `StartChainDraft` 는 아직 로그만 찍는다. 제거 장수 N 은 이미 정확히 넘어온다.
- **HUD 룰 슬롯** — `UFAAbilitySystemComponent::OnActiveRulesChanged` 델리게이트가 준비돼 있다.
  `GetActiveRuleIds()` 로 보유 순서대로 받아서 DT 에서 아이콘·이름을 찾아 그리면 된다.
