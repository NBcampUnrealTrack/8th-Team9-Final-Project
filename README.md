# FallingApart

Unreal Engine **5.8** / C++ 프로젝트. 메인 모듈: `FallingApart`

이 문서는 팀 공통 **Git 규칙**과 **코드/에셋 컨벤션**을 정의합니다.
작업 시작 전 한 번 읽고, 헷갈릴 때 다시 찾아보는 용도입니다.

---

## 1. 시작하기

### 1.1 필수 설치

| 항목 | 버전 / 비고 |
|---|---|
| Unreal Engine | 5.8 |
| Visual Studio | 2022 (Game development with C++ 워크로드) |
| Git LFS | **필수** — 없으면 에셋이 깨진 텍스트로 받아집니다 |

### 1.2 클론

```bash
git lfs install          # 최초 1회만
git clone <repo-url>
cd FallingApart
git lfs pull
```

클론 후 `FallingApart.uproject` 우클릭 → **Generate Visual Studio project files** → `.sln` 빌드.

> ⚠️ `git lfs install` 을 안 한 상태로 클론하면 `.uasset` / `.umap` 이 포인터 텍스트로 받아집니다.
> 이 경우 `git lfs install` 후 `git lfs pull` 로 복구하세요.

---

## 2. Git 규칙

### 2.1 브랜치 전략

`main` 직접 커밋 금지. 항상 브랜치를 따서 **PR** 로 병합합니다.

```
main                 ← 항상 빌드 가능한 안정 브랜치 (보호)
 └─ feature/Map      ← 기능 단위 작업 브랜치
 └─ feature/Voice
 └─ fix/GameState-Crash
```

| 접두사 | 용도 | 예시 |
|---|---|---|
| `feature/` | 새 기능 / 신규 작업 | `feature/Map`, `feature/SteamVoice` |
| `fix/` | 버그 수정 | `fix/GameState-Crash` |
| `refactor/` | 동작 변경 없는 구조 개선 | `refactor/GameMode-Split` |
| `art/` | 에셋 / 리소스 위주 작업 | `art/Character-Anim` |

- 브랜치 이름은 **영문 PascalCase**, 단어 구분은 `-`.
- 한 브랜치 = 한 작업 단위. 오래 끌지 말고 작게 잘라서 자주 병합합니다.

### 2.2 커밋 메시지

```
[이름] - 작업 내용
```

예시:

```
[주홍] - 맵
[주홍] - GameState 점수 동기화 추가
[주홍] - 캐릭터 애니메이션 리타겟 에셋 추가
```

규칙:

- 한글 OK. **무엇을 했는지** 알 수 있게 적습니다.
- `수정`, `작업중`, `ㅇㅇ` 같은 내용 없는 메시지 금지.
- 하나의 커밋은 하나의 논리적 변경. C++ 수정과 무관한 에셋 대량 추가를 한 커밋에 섞지 않습니다.

### 2.3 PR (Pull Request)

1. 작업 브랜치 push
2. `main` 대상으로 PR 생성
3. 제목은 커밋과 동일한 형식 (`[이름] - 내용`)
4. 본문에 **변경 요약 / 테스트한 내용 / 확인 필요한 부분** 기재
5. 리뷰 후 병합

### 2.4 Git LFS

바이너리는 전부 LFS 로 관리합니다 (`.gitattributes` 참고).

- LFS 대상: `.uasset` `.umap` `.wav` `.mp3` `.png` `.fbx` `.mp4` `.ttf` `.pdf` 등
- 일반 Git: `.cpp` `.h` `.cs` `.ini` `.json` `.md` `.uproject`

**새로운 바이너리 확장자를 추가할 때는 반드시 `.gitattributes` 에 LFS 규칙을 먼저 추가**하고 커밋하세요.
규칙 없이 커밋되면 나중에 히스토리를 고치기 매우 번거롭습니다.

### 2.5 커밋하면 안 되는 것

`.gitignore` 로 이미 막혀 있지만 확인용:

```
Binaries/  Build/  DerivedDataCache/  Intermediate/  Saved/
.vs/  .idea/  .vscode/  *.user  *.suo
```

### 2.6 에셋 충돌 주의

`.uasset` / `.umap` 은 **머지가 불가능한 바이너리**입니다.

- 같은 레벨(`.umap`)이나 같은 BP 를 두 명이 동시에 수정하지 않도록 **작업 전 공유**합니다.
- 레벨 작업은 가능하면 한 명이, 아니면 서브레벨로 분리해서 진행합니다.
- 개인 실험용 에셋은 `Content/Developers/<이름>/` 아래에 두고, 공용 폴더를 더럽히지 않습니다.

---

## 3. 폴더 구조

### 3.1 C++ 소스

```
Source/FallingApart/
├─ Public/          # 헤더 (.h)
│   └─ Game/            FAGameMode.h, FAGameState.h
└─ Private/         # 구현 (.cpp)
    └─ Game/            FAGameMode.cpp, FAGameState.cpp
```

- **헤더는 `Public/`, 구현은 `Private/`** 에 두고, 두 트리의 하위 폴더 구조를 **동일하게** 맞춥니다.
- 기능별 하위 폴더로 분류합니다: `Game/`, `Character/`, `Player/`, `UI/`, `Audio/` …
- include 는 모듈 루트 기준 경로로 작성합니다.

```cpp
#include "Game/FAGameMode.h"   // O
#include "../Public/Game/FAGameMode.h"  // X
```

### 3.2 Content

```
Content/
├─ FallingApart/        # 공용 게임 에셋 (모든 정식 에셋은 여기)
│   ├─ Character/
│   │   ├─ Animation/
│   │   ├─ Mixamo/
│   │   └─ SM/  ├─ Material/  ├─ SkeletalMeshes/  └─ Texture/
│   ├─ Game/            # BP_FAGameMode, BP_FAGameState 등
│   └─ Level/           # L_TestLevel 등
└─ Developers/<이름>/   # 개인 실험/임시 에셋 (공유 대상 아님)
```

---

## 4. C++ 코드 컨벤션

기본은 **[Unreal Engine 코딩 표준](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine)** 을 따릅니다.

### 4.1 네이밍

| 대상 | 규칙 | 예시 |
|---|---|---|
| 프로젝트 접두사 | `FA` | `AFAGameMode` |
| `AActor` 파생 | `A` + `FA` + 이름 | `AFAGameState`, `AFACharacter` |
| `UObject` 파생 | `U` + `FA` + 이름 | `UFAAudioSubsystem` |
| 구조체 | `F` + 이름 | `FFAPlayerScore` |
| 인터페이스 | `I` + 이름 | `IFAInteractable` |
| enum class | `E` + 이름 | `EFAPlayerState` |
| bool 변수 | `b` + PascalCase | `bIsInfected` |
| 함수 / 변수 | PascalCase | `GetPlayerScore`, `CurrentRound` |

- 클래스 파일 이름은 접두사(`A`/`U`/`F`)를 **뺀** 이름과 일치시킵니다: `AFAGameMode` → `FAGameMode.h` / `.cpp`
- 축약어 남용 금지. `PlayerCount` (O) / `PlyrCnt` (X)

### 4.2 헤더 규칙

```cpp
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FAGameMode.generated.h"   // 항상 마지막 include

UCLASS()
class FALLINGAPART_API AFAGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AFAGameMode();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "FallingApart|Game")
	int32 MaxRound = 5;
};
```

- `#pragma once` 사용 (include guard 매크로 X)
- `.generated.h` 는 **반드시 마지막** include
- include 순서: `CoreMinimal.h` → 엔진 헤더 → 프로젝트 헤더 → `.generated.h`
- 헤더에서는 가능하면 전방 선언(forward declaration)을 쓰고, `.cpp` 에서 실제 헤더를 include 합니다.
- 접근 지정자 순서: `public` → `protected` → `private`

### 4.3 포맷팅

- **들여쓰기: 탭** (Unreal 기본)
- 중괄호는 **새 줄**에 (Allman)
- 한 줄 길이는 대략 120자 이내
- 파일 끝에 개행 1줄

```cpp
void AFAGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bIsRoundActive)
	{
		StartRound();
	}
}
```

- 오버라이드 함수는 **항상 `Super::` 호출을 먼저** 합니다 (의도적으로 생략할 땐 주석으로 이유 명시).

### 4.4 UPROPERTY / UFUNCTION

- 블루프린트나 GC 가 참조해야 하는 `UObject*` 멤버는 **반드시 `UPROPERTY()`** 를 붙입니다. (안 붙이면 GC 로 날아갑니다)
- `Category` 는 `FallingApart|영역` 형태로 통일합니다: `"FallingApart|Audio"`, `"FallingApart|Game"`
- 에디터 노출 최소 권한 원칙: 값만 바꾸면 되는 건 `EditDefaultsOnly`, 읽기만 하면 `VisibleAnywhere`.

### 4.5 로그

```cpp
UE_LOG(LogTemp, Warning, TEXT("Round %d started"), CurrentRound);
```

- 문자열 리터럴은 항상 `TEXT()` 로 감쌉니다.
- 커밋 전 디버그용 `Warning` / 화면 출력(`AddOnScreenDebugMessage`) 로그는 정리합니다.

### 4.6 주석

- 한글 주석 OK. **왜(why)** 를 적습니다. 코드만 봐도 아는 내용은 적지 않습니다.
- 공개 API 는 헤더에 `/** */` 로 간단히 설명합니다.

```cpp
/** 감염자 수가 생존자 수 이상이면 라운드를 즉시 종료한다. */
void CheckRoundEndCondition();
```

---

## 5. 에셋 네이밍

`접두사_이름` 형식. 접두사는 **대문자**, 이름은 PascalCase.

| 접두사 | 종류 | 예시 |
|---|---|---|
| `BP_` | Blueprint 클래스 | `BP_FAGameMode` |
| `L_` | Level (`.umap`) | `L_TestLevel` |
| `SM_` | Static Mesh | `SM_Door` |
| `SK_` | Skeletal Mesh | `SK_Character` |
| `M_` | Material | `M_Character` |
| `MI_` | Material Instance | `MI_CharacterInfected` |
| `T_` | Texture | `T_CharacterSurvived` |
| `AS_` | Anim Sequence | `AS_Idle_Anim`, `AS_Run` |
| `ABP_` | Animation Blueprint | `ABP_Character` |
| `IK_` | IK Rig | `IK_Idle` |
| `RTG_` | IK Retargeter | `RTG_Mixamo_To_Character` |
| `S_` | Sound Wave | `S_DoorOpen` |
| `SC_` | Sound Cue | `SC_Footstep` |
| `WBP_` | Widget Blueprint | `WBP_MainMenu` |
| `DT_` | Data Table | `DT_CardList` |
| `DA_` | Data Asset | `DA_RoundConfig` |

규칙:

- 에셋 이름에 **공백, `+`, 한글 금지**. (외부에서 받은 에셋은 임포트 직후 리네임)
- C++ 클래스를 상속한 BP 는 클래스명 그대로: `AFAGameMode` → `BP_FAGameMode`
- 에셋 리네임/이동은 **반드시 에디터 안에서** 수행합니다. (탐색기에서 옮기면 참조가 깨집니다)

---

## 6. 작업 체크리스트

커밋 / PR 올리기 전에:

- [ ] 프로젝트가 빌드되는가
- [ ] 에디터에서 실행(PIE)이 정상인가
- [ ] 디버그 로그 / 임시 코드 정리했는가
- [ ] 에셋 이름이 컨벤션에 맞는가
- [ ] 개인 실험 에셋이 공용 폴더에 섞여 있지 않은가
- [ ] `git status` 에 `Binaries/`, `Intermediate/`, `Saved/` 가 잡히지 않는가
- [ ] 새 바이너리 확장자가 있다면 `.gitattributes` 에 LFS 규칙을 추가했는가
