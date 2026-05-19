# Vdjm Record Event Node Property Guide

## TOC
- [목적](#목적)
- [가장 중요한 작성 규칙](#가장-중요한-작성-규칙)
- [필수성 표기 기준](#필수성-표기-기준)
- [Key 개념 요약](#key-개념-요약)
- [RuntimeSlotKey와 ContextKey는 같아도 되나](#runtimeslotkey와-contextkey는-같아도-되나)
- [Widget LookupPolicy](#widget-lookuppolicy)
- [추천 이름 규칙](#추천-이름-규칙)
- [Widget 노드](#widget-노드)
- [Vcard Descriptor 노드](#vcard-descriptor-노드)
- [Context 노드](#context-노드)
- [Object / Actor 노드](#object--actor-노드)
- [Signal / Wait / Delay 노드](#signal--wait--delay-노드)
- [Subgraph 노드](#subgraph-노드)
- [Recorder / Bridge / Controller 노드](#recorder--bridge--controller-노드)
- [AppState / MediaPreview 노드](#appstate--mediapreview-노드)
- [Debug / Composite / Jump 노드](#debug--composite--jump-노드)
- [자주 쓰는 작성 패턴](#자주-쓰는-작성-패턴)
- [문제 생겼을 때 빠른 점검](#문제-생겼을-때-빠른-점검)

## 목적
이 문서는 `UVdjmRecordEventFlowDataAsset`에서 event node를 작성할 때 각 property에 무엇을 넣어야 하는지 빠르게 확인하기 위한 작성 가이드다.

기준 코드는 다음 파일이다.
- `Source/VdjmRecorder/Public/VdjmEvents/VdjmRecordEventNode.h`
- `Source/VdjmRecorder/Public/VdjmEvents/VdjmRecordEventManager.h`

## 가장 중요한 작성 규칙
- `EventTag`, `DebugName`, `CaseTag`는 작성/로그/디버그용 이름이다. 기능 라우팅이나 객체 조회에는 쓰지 않는다.
- `WidgetClass`, `ObjectClass`, `ActorClass`, `SignalTag`, `SubgraphTag`, `DescriptorKey`처럼 동작 대상이 되는 값은 `None`이면 대부분 실패하거나 아무 일도 하지 않는다.
- `RuntimeSlotKey`는 현재 flow session 안에서만 쓰는 local 임시 이름이다.
- `ContextKey`는 `VdjmRecorderWorldContextSubsystem`에 등록되는 world-global 약참조 이름이다.
- `SignalTag`는 event bus의 명령/알림 이름이고, `SubgraphTag`나 `DescriptorKey`는 실제 실행/적용 대상 이름이다. 둘은 비슷해 보여도 역할이 다르다.
- 반복해서 열고 닫을 UI는 매번 새로 만들기보다 `CreateWidgetAndRegisterContext -> ShowWidget -> LowerWidget` 구조로 잡는 것이 좋다.
- subgraph를 UI 버튼이나 animation signal로 열고 싶으면 root flow에서 먼저 `RegisterSubgraphSignalBranchNode`를 등록해야 한다.

## 필수성 표기 기준
각 property 표의 `필수` 열은 아래 의미로 읽는다. 이름이 비슷한 key라도 이 기준이 다르면 작성 의도가 다르다.

| 표기 | 의미 | None/빈 값일 때 |
|---|---|---|
| 필수 | 해당 노드의 주 기능을 실행하기 위한 최소 입력이다. | 보통 실패하거나 아무 일도 하지 않는다. |
| 조건부 필수 | 특정 옵션이나 정책을 켰을 때만 필요하다. | 해당 조건에서는 실패하거나 대기/제거/분기가 동작하지 않는다. |
| 강력 추천 | 없어도 실행은 가능하지만 후속 노드, 재사용, 디버깅이 어려워진다. | 즉시 실패하지 않아도 다음 단계에서 찾을 수 없을 수 있다. |
| 선택 | 기능을 보조하거나 기본값으로 충분한 값이다. | 기본 동작을 따른다. |
| 디버그 전용 | 로그, summary, 에디터 가독성만 위한 이름이다. | 기능에는 보통 영향이 없다. |

## Key 개념 요약
| 이름 | 역할 분류 | 의미 | 생존/조회 범위 | 필수성 | None일 때 |
|---|---|---|---:|---|---|
| `EventTag` | 디버그 전용 | event node 자체의 작성/디버그 태그 | DataAsset 작성 정보 | 선택 | 동작에는 보통 영향 없음 |
| `DebugName` | 디버그 전용 | descriptor attachment, UI item 같은 작성 단위의 표시 이름 | 작성 정보 | 선택 | 동작에는 영향 없음 |
| `CaseTag` | 디버그 전용 | branch case의 작성/로그용 이름 | branch rule 내부 | 선택 | branch 실행 조건에는 영향 없음 |
| `RuntimeSlotKey` | local runtime key | flow session 내부 임시 객체 이름 | 현재 session | 후속 runtime 조회 시 필수 | 저장/조회하지 않음 |
| `ContextKey` | global context key | world context subsystem 전역 조회 이름 | World | 전역 재사용/외부 조회 시 필수 | generic 등록 노드는 실패, 일부 ensure 노드는 표준 key 자동 사용 |
| `LookupPolicy` | 조회 정책 | runtime slot과 context 중 어디를 먼저 찾을지 결정 | 노드별 조회 | 모호한 조회를 줄이려면 중요 | 기본 조회 순서를 따름 |
| `SignalTag` | signal/command key | flow signal 이름 | EventManager | emit/wait/branch trigger에서는 필수 | wait/emit/branch는 보통 실패 또는 무효 |
| `EmitSignalTag` | signal/command key | event 성공 직후 자동 발행할 signal | EventManager | 선택 | 발행하지 않음 |
| `DestroySignalTag` | signal/command key | 위젯 자동 제거를 트리거할 signal | EventManager | `RemoveOnSignal`이면 조건부 필수 | 제거 대기가 불가능 |
| `StartSignalTag` | signal/command key | 준비 후 시작을 지시할 signal | EventManager | start 정책에 따라 조건부 필수 | 자동 시작하지 않음 |
| `BranchTag` | rule key | subgraph branch rule 식별자 | EventManager | 선택 | `SignalTag`를 key처럼 사용 |
| `SubgraphTag` | target key | DataAsset 안의 named subgraph 이름 | DataAsset | subgraph 실행 시 필수 | subgraph 실행 불가 |
| `DescriptorKey` | target key | registry 안의 Vcard descriptor 이름 | DescriptorRegistry | descriptor 적용 시 필수 | descriptor 적용 불가 |
| `FlowAsset` | asset reference | subgraph를 찾을 FlowDataAsset | Asset | 선택/조건부 | 현재 main flow asset 사용 |
| `WidgetClass` | class reference | 생성할 widget class | Class | widget 생성 시 필수 | 생성 실패 |
| `ObjectClass` | class reference | 생성할 object class | Class | object 생성 시 필수 | 생성 실패 |
| `ActorClass` | class reference | spawn할 actor class | Class | actor spawn 시 필수 | spawn 실패 |

역할별로 가장 중요한 차이는 다음과 같다.

```text
Debug only:
  EventTag, DebugName, CaseTag
  -> 사람이 읽고 찾기 위한 이름이다. 실행 대상을 고르지 않는다.

Local runtime:
  RuntimeSlotKey
  -> 현재 flow/session 안에서 다음 노드가 객체를 찾기 위한 임시 slot이다.

Global context:
  ContextKey
  -> 다른 session, widget, actor도 world context subsystem을 통해 찾을 수 있는 전역 key다.

Signal / command:
  SignalTag, EmitSignalTag, DestroySignalTag, StartSignalTag
  -> "언제/무엇을 알릴지"를 정한다.

Target:
  SubgraphTag, DescriptorKey, WidgetClass, ObjectClass, ActorClass
  -> "실제로 무엇을 실행/생성/적용할지"를 정한다.
```

## RuntimeSlotKey와 ContextKey는 같아도 되나
같아도 된다. 오히려 같은 객체를 "현재 session 안 이름"과 "world-global 이름"으로 동시에 다룰 때는 같은 문자열을 쓰는 편이 사람이 읽기 쉽다.

예시는 다음과 같다.
- `RuntimeSlotKey = preview-lobby`
- `ContextKey = preview-lobby`

다만 두 key의 의미는 다르다.
- `RuntimeSlotKey`는 현재 session 안의 임시 slot이다. subgraph session이 새로 열리면 이전 session의 runtime slot은 직접 보장되지 않는다.
- `ContextKey`는 world context subsystem에 약참조로 등록된다. 다른 session에서도 같은 key로 찾을 수 있다.

같게 쓰면 좋은 경우는 다음과 같다.
- 하나의 화면/객체를 같은 이름으로 계속 추적하고 싶다.
- 로비, 레코더 UI, preview manager처럼 재사용되는 핵심 객체다.
- 사람이 DataAsset summary를 볼 때 "이 slot과 이 context가 같은 대상"임을 바로 알고 싶다.

분리하는 편이 좋은 경우는 다음과 같다.
- 같은 객체를 session 안에서는 짧은 이름으로 쓰고, 전역에서는 더 명확한 도메인 이름으로 등록하고 싶다.
- 한 session 안에서 임시 객체를 여러 개 만들고, 그중 하나만 전역 등록하고 싶다.
- 같은 class의 여러 인스턴스를 만들고 전역 key 충돌을 피해야 한다.

주의할 점은 현재 `CreateWidgetAndRegisterContextNode`는 생성 후 `RuntimeSlotKey`로 위젯을 다시 찾아 `ContextKey`에 등록한다는 것이다. 그래서 이 노드는 `RuntimeSlotKey=None`이면 context 등록 대상으로 위젯을 찾기 어렵다. 이 노드를 쓸 때는 `WidgetClass`, `RuntimeSlotKey`, `ContextKey`를 한 세트로 채우는 것을 기본 규칙으로 둔다.

## Widget LookupPolicy
`ShowWidget`, `LowerWidget`, `RemoveWidget`은 위젯을 찾을 때 `LookupPolicy`로 조회 순서를 정한다. 기본값은 `ERuntimeSlotThenContext`다.

| 값 | 의미 | 추천 상황 |
|---|---|---|
| `ERuntimeSlotOnly` | 현재 flow session의 `RuntimeSlotKey`만 찾는다. | 같은 session 안에서만 쓰는 임시 위젯 |
| `EContextOnly` | world context만 찾는다. `ContextKey=None`이면 `RuntimeSlotKey`를 context key처럼 쓴다. | 다른 subgraph/session에서 만든 재사용 UI |
| `ERuntimeSlotThenContext` | runtime slot을 먼저 찾고, 없으면 context를 찾는다. | 기본값. 같은 session이면 빠르게 찾고, session이 달라도 fallback |
| `EContextThenRuntime` | context를 먼저 찾고, 없으면 runtime slot을 찾는다. | 전역 등록 객체를 우선 신뢰해야 하는 UI |

`ContextKey=None`이고 context 조회가 필요한 policy라면 `RuntimeSlotKey`를 fallback key로 쓴다. 그래서 `RuntimeSlotKey=preview-lobby`, `ContextKey=None`, `LookupPolicy=ERuntimeSlotThenContext`도 다른 session에서 `preview-lobby` context를 찾을 수 있다.

context에서 찾은 위젯에 `RuntimeSlotKey`가 지정되어 있으면, 해당 위젯을 현재 session의 runtime slot에도 다시 저장한다. 이 덕분에 subgraph 안에서 한 번 context로 가져온 뒤에는 같은 subgraph 안의 후속 `ShowWidget`/`LowerWidget`이 runtime slot처럼 다룰 수 있다.

subgraph 사이를 넘나드는 UI에는 보통 아래처럼 둔다.
```text
RuntimeSlotKey = preview-lobby
ContextKey = preview-lobby
LookupPolicy = ERuntimeSlotThenContext
```

## 추천 이름 규칙
| 대상 | 추천 예시 |
|---|---|
| Intro widget | `intro-widget` |
| Preview loading widget | `preview-loading` |
| Preview lobby widget | `preview-lobby` |
| Recorder loading widget | `recorder-loading` |
| Recorder main widget | `recorder-ui` |
| Recorder controller | `controller` |
| AppState store | `app-state` |
| MediaPreview manager | `media-preview-manager` |
| Open preview signal | `open-preview` |
| Open recorder signal | `open-recorder` |
| Intro done signal | `intro-done` |
| Preview loading done signal | `preview-loading-done` |
| Recorder loading done signal | `recorder-loading-done` |
| Preview subgraph | `subgraph-preview` |
| Recorder subgraph | `subgraph-recorder` |
| Preview branch | `branch-preview` |
| Recorder branch | `branch-recorder` |

## Widget 노드

### `UVdjmRecordEventCreateWidgetNode`
위젯을 만들고, 필요하면 viewport에 붙이고, runtime slot에 저장하는 primitive 노드다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `WidgetClass` | 필수 | 대상 WBP class | 생성할 `UUserWidget` class다. |
| `PlayerIndex` | 선택 | `0` | owning player를 찾을 index다. |
| `bRequireOwningPlayer` | 선택 | `false` | true면 player controller가 없을 때 실패한다. 모바일/PIE 테스트에서는 보통 false가 편하다. |
| `bReuseCreatedWidget` | 선택 | `true` | 같은 node instance가 다시 실행될 때 이전 widget을 재사용한다. |
| `bAddToViewport` | 선택 | 화면에 바로 보이면 `true` | true면 생성 후 `AddToViewport(ZOrder)`를 호출한다. 숨겨둘 화면이면 false도 가능하다. |
| `ZOrder` | 선택 | `0` | viewport 표시 순서다. overlay/로딩은 더 높은 값을 쓴다. |
| `RuntimeSlotKey` | 강력 추천 | `preview-lobby` 등 | 이후 `ShowWidget`, `LowerWidget`, `RemoveWidget`이 찾을 이름이다. |
| `EmitSignalTag` | 선택 | `None` | 생성 직후 자동으로 signal을 발행한다. |
| `DestroyPolicy` | 선택 | `ENone` | 자동 제거 정책이다. intro/loading처럼 끝 신호를 기다렸다 제거하려면 `ERemoveOnSignal`을 쓴다. |
| `DestroySignalTag` | 조건부 필수 | `intro-done` 등 | `DestroyPolicy=ERemoveOnSignal`일 때 기다릴 signal이다. |
| `DestroyConditionMode` | 선택 | `EConditional` | signal 대기 방식이다. 일반적으로 `EConditional`을 쓴다. |
| `DestroyDelaySeconds` | 조건부 | `0` | `ERemoveAfterDelay`일 때 제거까지 기다릴 시간이다. |
| `bClearRuntimeSlotOnDestroy` | 선택 | `true` | 자동 제거 시 runtime slot도 비운다. |
| `bUnregisterContextOnDestroy` | 선택 | `false` | 자동 제거 시 world context도 제거할지 정한다. |
| `DestroyContextKey` | 조건부 | 대상 context key | `bUnregisterContextOnDestroy=true`일 때 제거할 context key다. |

작성 예시는 다음과 같다.
```text
Intro:
WidgetClass = WBP_Intro
RuntimeSlotKey = intro-widget
bAddToViewport = true
DestroyPolicy = ERemoveOnSignal
DestroySignalTag = intro-done
DestroyConditionMode = EConditional
```

```text
Lobby:
WidgetClass = WBP_Lobby
RuntimeSlotKey = preview-lobby
bAddToViewport = true
DestroyPolicy = ENone
```

### `UVdjmRecordEventCreateWidgetAndRegisterContextNode`
`CreateWidgetNode` 실행 후 생성된 위젯을 world context에 등록하는 convenience 노드다.

| Property | 필수 | 추천 | 의미 |
|---|---:|---|---|
| `WidgetClass` | 필수 | 대상 WBP class | 생성할 위젯이다. |
| `RuntimeSlotKey` | 사실상 필수 | `preview-lobby` | 생성된 위젯을 다시 찾기 위해 필요하다. |
| `ContextKey` | 필수 | `preview-lobby` | world context에 등록할 전역 조회 key다. |
| 나머지 `CreateWidgetNode` property | 상황별 | 위 표 참고 | viewport 표시, destroy 정책, signal 발행 등은 동일하다. |

반복 사용 UI는 이 노드를 쓰는 편이 좋다. 단, 이미 context에 있는 위젯을 자동 재사용하는 전용 ensure 노드는 아직 별도로 없다. 현재는 첫 생성/등록 후, 이후에는 `ShowWidget`/`LowerWidget`으로 운용하는 방식을 권장한다.

### `UVdjmRecordEventShowWidgetNode`
runtime slot에 있는 위젯을 viewport에 올리거나, widget stack cursor 기준으로 최근 위젯을 올린다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 선택 | 직접 지정 시 대상 key | 특정 위젯을 올릴 때 쓴다. `None`이면 stack cursor를 사용한다. |
| `ContextKey` | 선택 | 재사용 UI의 전역 key | 다른 subgraph/session에서 등록된 위젯을 찾을 때 쓴다. `None`이면 `RuntimeSlotKey`를 context fallback key로 쓴다. |
| `LookupPolicy` | 선택 | `ERuntimeSlotThenContext` | runtime slot과 context 중 어디를 먼저 찾을지 정한다. |
| `CursorDelta` | 조건부 | `1` | `RuntimeSlotKey=None`일 때 cursor 이동량이다. |
| `ZOrder` | 선택 | `0` | 다시 `AddToViewport`할 때의 z order다. |
| `bLowerPreviousWidget` | 선택 | `true` | 새 위젯을 올리기 전 이전 cursor 위젯을 내린다. |
| `bSetVisibleOnShow` | 선택 | `true` | 표시 후 visibility를 visible로 맞춘다. |
| `bSucceedIfMissing` | 선택 | `false` | 대상이 없어도 성공 처리할지 정한다. |

### `UVdjmRecordEventLowerWidgetNode`
runtime slot의 위젯을 화면에서만 내린다. 객체를 파괴하지 않는다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 선택 | 직접 지정 시 대상 key | 특정 위젯을 내릴 때 쓴다. `None`이면 stack에서 최근 위젯부터 내린다. |
| `ContextKey` | 선택 | 재사용 UI의 전역 key | 다른 subgraph/session에서 등록된 위젯을 내릴 때 쓴다. |
| `LookupPolicy` | 선택 | `ERuntimeSlotThenContext` | runtime slot과 context 중 어디를 먼저 찾을지 정한다. |
| `LowerCount` | 조건부 | `1` | `RuntimeSlotKey=None`일 때 최근 위젯부터 몇 개를 내릴지 정한다. |
| `bMoveCursorAfterDirectLower` | 선택 | `true` | 직접 지정 위젯이 현재 cursor면 cursor를 이전 위젯으로 이동한다. |
| `bSucceedIfMissing` | 선택 | `false` | 대상이 없어도 성공 처리할지 정한다. |

### `UVdjmRecordEventRemoveWidgetNode`
runtime slot의 위젯을 parent에서 제거하고, 필요하면 slot/context도 정리한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 조건부 | 대상 key | 제거할 위젯을 찾을 runtime slot key다. `ContextKey`만 쓸 수도 있다. |
| `ContextKey` | 조건부 | 대상 context key | 제거할 위젯을 world context에서 찾거나, context도 같이 제거할 때 사용한다. |
| `LookupPolicy` | 선택 | `ERuntimeSlotThenContext` | runtime slot과 context 중 어디를 먼저 찾을지 정한다. |
| `bClearRuntimeSlot` | 선택 | `true` | 제거 후 runtime slot을 비운다. |
| `bUnregisterContext` | 선택 | `false` | 제거 후 world context도 해제한다. |
| `bSucceedIfMissing` | 선택 | `false` | 대상이 없어도 성공 처리할지 정한다. |

## Vcard Descriptor 노드
이 노드들은 `Source/VdjmVcard` 모듈에 있다. 목적은 flow graph가 특정 `UUserWidget`의 `NamedSlot` 또는 `PanelWidget`에 descriptor를 적용하거나 비우게 하는 것이다. 위젯은 `UVcardWidgetBase`를 상속할 필요가 없다. 대상 위젯은 `RuntimeSlotKey` 또는 `ContextKey`로 찾는다.

JSON class path는 다음처럼 쓴다.
```text
/Script/VdjmVcard.VcardEventApplyDescriptorNode
/Script/VdjmVcard.VcardEventClearDescriptorSlotNode
```

### `UVcardEventApplyDescriptorNode`
`DescriptorRegistryDataAsset`에서 `DescriptorKey`를 찾아 host widget의 named slot/panel에 적용한다. descriptor 안의 attachment가 실제 어떤 slot을 채울지 결정한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 조건부 | `vcard-root`, `preview-lobby` 등 | 현재 flow session 안에서 대상 host widget을 찾는 local key다. |
| `ContextKey` | 조건부 | 같은 이름 추천 | world context에서 대상 host widget을 찾는 global key다. |
| `LookupPolicy` | 선택 | `ERuntimeSlotThenContext` | runtime slot과 context 중 어디를 먼저 볼지 정한다. |
| `DescriptorRegistryDataAsset` | 필수 | Vcard descriptor registry DA | `DescriptorKey`를 찾을 중앙 registry다. |
| `DescriptorKey` | 필수 | `root-stage-creator-lobby` 등 | 실행할 descriptor map key다. |
| `FallbackTargetSlotName` | 선택 | 보통 `None` | descriptor attachment의 `TargetSlotName`이 비었을 때만 대신 쓴다. |
| `PayloadData` | 선택 | 필요 시 UObject | 생성된 위젯에 넘길 런타임 데이터다. |
| `bStoreContextResultInRuntimeSlot` | 선택 | `true` | context에서 찾은 host widget을 현재 session runtime slot에도 캐시한다. |
| `bSucceedIfMissing` | 선택 | 초기 제작 중이면 true 가능 | 대상/descriptor가 없어도 flow를 계속 진행할지 정한다. |
| `bLogResult` | 디버그 전용 | `true` | 적용 결과를 `LogVdjmVcard`에 남긴다. |

작성 예시는 다음과 같다.
```text
Apply creator lobby into root Stage:
  RuntimeSlotKey = vcard-root
  ContextKey = vcard-root
  LookupPolicy = ERuntimeSlotThenContext
  DescriptorRegistryDataAsset = DA_VcardDescriptorRegistry
  DescriptorKey = root-stage-creator-lobby
```

### `UVcardEventClearDescriptorSlotNode`
host widget 안의 `NamedSlot` 또는 `PanelWidget`을 비우거나 숨긴다. descriptor를 새로 적용하기 전에 특정 영역을 명시적으로 정리할 때 쓴다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 조건부 | 대상 host key | 현재 flow session에서 host widget을 찾는다. |
| `ContextKey` | 조건부 | 대상 context key | world context에서 host widget을 찾는다. |
| `LookupPolicy` | 선택 | `ERuntimeSlotThenContext` | 조회 순서다. |
| `TargetSlotName` | 필수 | `Stage`, `Modal`, `ToolContents` 등 | 비우거나 숨길 `NamedSlot`/`PanelWidget` 이름이다. |
| `bHideInsteadOfRemove` | 선택 | `false` | true면 내용은 유지하고 visibility만 `Collapsed`로 둔다. false면 slot content 또는 panel children을 제거한다. |
| `bSucceedIfMissing` | 선택 | 반복 호출이면 true | 대상 slot/panel이 없어도 성공 처리할지 정한다. |
| `bLogResult` | 디버그 전용 | `true` | 처리 결과를 `LogVdjmVcard`에 남긴다. |

## Context 노드

### `UVdjmRecordEventRegisterContextEntryNode`
UObject를 world context entry로 등록하거나 갱신한다. 기본값은 기존처럼 현재 flow session의 `RuntimeSlotKey`에서 객체를 찾아 `ContextKey`에 등록한다. `LookupPolicy`를 바꾸면 이미 등록된 context 객체를 다시 가져와 다른 key로 등록하거나, 현재 session의 runtime slot에 다시 꽂아 넣을 수 있다.

| Property | 필수 | 의미 |
|---|---:|---|
| `RuntimeSlotKey` | 조건부 | 등록할 객체를 찾을 session slot key다. context에서 찾은 객체를 현재 session에 다시 저장할 이름이기도 하다. |
| `SourceContextKey` | 선택 | source 객체를 가져올 world context key다. `None`이면 `ContextKey`, 그래도 없으면 `RuntimeSlotKey`를 source key처럼 쓴다. |
| `ContextKey` | 필수 | world-global 조회 key다. |
| `LookupPolicy` | 선택 | `RuntimeSlotKey`와 `SourceContextKey/ContextKey` 중 어디에서 source 객체를 찾을지 정한다. 기본값은 기존 호환을 위해 `ERuntimeSlotOnly`다. |
| `ExpectedClass` | 선택 | 등록 객체가 이 class인지 검증한다. |
| `bRefreshRuntimeSlot` | 선택 | context에서 찾은 객체를 `RuntimeSlotKey`에도 다시 저장한다. 다른 subgraph/session에서 가져온 객체를 현재 flow 안에서 계속 쓰기 좋다. |
| `bSucceedIfMissing` | 선택 | source 객체가 없어도 성공 처리한다. optional UI/context 갱신에 사용한다. |

### `UVdjmRecordEventRegisterWidgetContextNode`
widget을 world context entry로 등록하거나 갱신한다. `RegisterContextEntryNode`와 같은 source lookup 규칙을 쓰되, source 객체가 `UUserWidget`이 아니면 실패한다.

| Property | 필수 | 의미 |
|---|---:|---|
| `RuntimeSlotKey` | 조건부 | 등록할 widget을 찾을 session slot key다. context에서 찾은 widget을 현재 session에 다시 저장할 이름이기도 하다. |
| `SourceContextKey` | 선택 | source widget을 가져올 world context key다. `None`이면 `ContextKey`, 그래도 없으면 `RuntimeSlotKey`를 source key처럼 쓴다. |
| `ContextKey` | 필수 | world-global 조회 key다. |
| `LookupPolicy` | 선택 | `RuntimeSlotKey`와 `SourceContextKey/ContextKey` 중 어디에서 source widget을 찾을지 정한다. 기본값은 기존 호환을 위해 `ERuntimeSlotOnly`다. |
| `bRefreshRuntimeSlot` | 선택 | context에서 찾은 widget을 `RuntimeSlotKey`에도 다시 저장한다. |
| `bSucceedIfMissing` | 선택 | source widget이 없어도 성공 처리한다. |

예를 들어 recorder 화면을 전역 context에서 다시 가져와 현재 subgraph에서도 쓰고 싶다면 아래처럼 둔다.

```text
RegisterWidgetContextNode
RuntimeSlotKey = recoder-main
SourceContextKey = recoder-lobby
ContextKey = recoder-lobby
LookupPolicy = EContextOnly
bRefreshRuntimeSlot = true
bSucceedIfMissing = true
```

위 설정은 `recoder-lobby` context가 있으면 weak context 등록을 갱신하고 `recoder-main` runtime slot에도 다시 넣는다. 아직 생성되지 않았다면 `bSucceedIfMissing=true` 때문에 flow를 막지 않는다.

## Object / Actor 노드

### `UVdjmRecordEventCreateObjectNode`
일반 UObject를 생성하고 runtime slot에 저장한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `ObjectClass` | 필수 | 대상 class | 생성할 UObject class다. |
| `RuntimeSlotKey` | 필수에 가까움 | 객체 이름 | 다음 event가 찾을 이름이다. |
| `bReuseSlotObject` | 선택 | `true` | slot에 기존 객체가 있으면 재사용한다. |
| `OuterPolicy` | 선택 | `EBridgeActor` | 생성 객체의 outer를 정한다. bridge 의존 객체면 bridge actor가 자연스럽다. |

### `UVdjmRecordEventSpawnActorNode`
일반 Actor를 spawn하고 runtime slot에 저장한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `ActorClass` | 필수 | 대상 actor class | spawn할 actor class다. |
| `RuntimeSlotKey` | 필수에 가까움 | actor 이름 | 다음 event가 찾을 이름이다. |
| `bReuseSlotActor` | 선택 | `true` | slot에 기존 actor가 있으면 재사용한다. |

### Convenience composite
| Node | 추가 property | 작성 규칙 |
|---|---|---|
| `CreateObjectAndRegisterContextNode` | `ContextKey`, `ExpectedClass` | `ObjectClass`, `RuntimeSlotKey`, `ContextKey`를 같이 채운다. |
| `SpawnActorAndRegisterContextNode` | `ContextKey`, `ExpectedClass` | `ActorClass`, `RuntimeSlotKey`, `ContextKey`를 같이 채운다. |
| `CreateWidgetAndRegisterContextNode` | `ContextKey` | `WidgetClass`, `RuntimeSlotKey`, `ContextKey`를 같이 채운다. |

## Signal / Wait / Delay 노드

### `UVdjmRecordEventWaitForSignalNode`
특정 signal이 들어올 때까지 flow를 기다리게 한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `SignalTag` | 필수 | 기다릴 signal | 이 signal이 pending 상태거나 emit되면 다음 event로 진행한다. |
| `ConditionMode` | 선택 | `EConditional` 권장 | `ERunning`은 tick polling, `EConditional`은 manager 조건 대기 기반이다. |

권장 작성은 다음과 같다.
```text
SignalTag = intro-done
ConditionMode = EConditional
```

### `UVdjmRecordEventEmitSignalNode`
특정 signal을 발행한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `SignalTag` | 필수 | 발행할 signal | wait, callback, branch rule을 깨운다. |
| `SignalRoute` | 선택 | `0` | signal 전파 route다. 기본값이면 일반적인 전역/현재 흐름 처리로 사용한다. |

### `UVdjmRecordEventDelayNode`
지정 시간만큼 기다린다.

| Property | 필수 | 의미 |
|---|---:|---|
| `DelaySeconds` | 필수 | 대기 시간이다. |

## Subgraph 노드

### `UVdjmRecordEventStartSubgraphSessionNode`
지정 subgraph를 즉시 새 flow session으로 시작한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `FlowAsset` | 선택 | `None` | subgraph가 있는 asset이다. `None`이면 현재 main flow asset을 사용한다. |
| `SubgraphTag` | 필수 | `subgraph-preview` 등 | 시작할 subgraph 이름이다. |
| `bResetRuntimeStates` | 선택 | `true` | subgraph 실행 전 node runtime state를 초기화한다. |

### `UVdjmRecordEventRegisterSubgraphSignalBranchNode`
특정 signal이 들어오면 BranchCases를 0번부터 검사해 subgraph session을 시작하도록 manager에 등록한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `BranchTag` | 추천 | `branch-preview` | branch rule의 식별자다. `None`이면 `SignalTag`를 key로 쓴다. |
| `SignalTag` | 필수 | `open-preview` | 이 signal이 들어오면 branch case 검사를 시작한다. |
| `FlowAsset` | 선택 | `None` | target subgraph asset이다. `None`이면 현재 main flow asset이다. |
| `SubgraphTag` | 조건부 | 단일 case면 사용 | `BranchCases`가 비어 있을 때 자동으로 만들 기본 subgraph tag다. |
| `BranchCases` | 선택/권장 | if/else-if/else 구성 | 비어 있으면 `SubgraphTag` 기반 단일 case처럼 사용한다. |
| `bEnabled` | 선택 | `true` | branch rule 활성화 여부다. |
| `bTriggerOnce` | 선택 | `false` | 한 번 실행 후 branch를 제거할지 정한다. |
| `bReplaceExisting` | 선택 | `true` | 같은 key branch가 있으면 교체한다. |

### `FVdjmRecordSubgraphBranchCase`
| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `CaseTag` | 추천 | `preview-open-if-inactive` | 작성/디버그용 case 이름이다. |
| `MatchCondition` | 선택 | `EAlways` 또는 `EWhenBranchInactive` | branch가 실행될 조건이다. 0번부터 순서대로 검사한다. |
| `SubgraphTag` | 필수 | 실행할 subgraph | 매칭되었을 때 시작할 subgraph다. |
| `DuplicatePolicy` | 선택 | 화면 재진입은 `EEmitSignalToActiveSession` 또는 `EIgnoreAndSucceed` | 같은 branch가 이미 실행 중일 때의 처리다. |
| `ForwardSignalTag` | 조건부 | active session에 보낼 signal | `DuplicatePolicy=EEmitSignalToActiveSession`일 때 사용한다. |
| `bResetRuntimeStates` | 선택 | `true` | 시작 전 runtime node state를 초기화한다. |

반복 사용 UI에 추천하는 branch case는 다음과 같다.
```text
BranchTag = branch-preview
SignalTag = open-preview
BranchCases[0]:
  CaseTag = preview-if-inactive
  MatchCondition = EWhenBranchInactive
  SubgraphTag = subgraph-preview
  DuplicatePolicy = EIgnoreAndSucceed
BranchCases[1]:
  CaseTag = preview-if-active
  MatchCondition = EWhenBranchActive
  SubgraphTag = subgraph-preview
  DuplicatePolicy = EEmitSignalToActiveSession
  ForwardSignalTag = preview-show
```

### `UVdjmRecordEventUnregisterSubgraphSignalBranchNode`
manager에 등록된 branch rule을 제거한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `BranchTag` | 필수 | 제거할 branch key | 등록된 branch rule을 찾는 key다. |
| `bSucceedIfMissing` | 선택 | `true` | 없어도 성공 처리할지 정한다. |

## Recorder / Bridge / Controller 노드

### `UVdjmRecordEventSpawnRecordBridgeActorWait`
BridgeActor를 준비하고, 정책에 따라 start/init wait까지 수행한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `bReuseExistingBridgeActor` | 선택 | `true` | 이미 있는 bridge actor를 재사용한다. |
| `BridgeActorClass` | 조건부 | `AVdjmRecordBridgeActor` class | 없으면 기본/기존 bridge 경로를 사용한다. 명시하면 더 안전하다. |
| `EnvDataAssetPath` | 상황별 | recorder env asset | bridge init에 사용할 env data asset path다. |
| `bRequireLoadSuccess` | 선택 | `true` 권장 | env asset load 실패 시 event 실패 처리한다. |
| `StartPolicy` | 필수 | 상황별 | `EPrepareOnly`, `EStartImmediately`, `EWaitForSignal` 중 목적에 맞게 선택한다. |
| `StartSignalTag` | 조건부 | 시작 signal | `EWaitForSignal`일 때 기다릴 signal이다. |
| `ConditionMode` | 선택 | `EConditional` 또는 `ERunning` | signal/init 완료 대기 방식이다. |

권장 흐름은 다음과 같다.
```text
Bridge 준비만:
StartPolicy = EPrepareOnly

로딩 위젯 bind 후 시작:
StartRecordBridgeActorNode를 별도 event로 배치

signal 받고 시작:
StartPolicy = EWaitForSignal
StartSignalTag = bridge-start
ConditionMode = EConditional
```

### `UVdjmRecordEventStartRecordBridgeActorNode`
현재 EventManager에 bind된 BridgeActor의 `StartRecordBridgeActor()`를 호출한다.

작성할 property는 거의 없다. `SpawnRecordBridgeActorWait(EPrepareOnly)`로 bridge를 먼저 준비하고, loading widget이 delegate bind를 끝낸 뒤 이 노드를 실행하는 구성이 읽기 쉽다.

### `UVdjmRecordEventSetEnvDataAssetPathNode`
전역 bridge env data asset path를 설정한다.

| Property | 필수 | 의미 |
|---|---:|---|
| `EnvDataAssetPath` | 필수 | recorder env data asset path다. |
| `bRequireLoadSuccess` | 선택 | true면 asset load 검증 실패 시 event 실패다. |

### `UVdjmRecordEventEnsureRecorderControllerNode`
RecorderController를 찾거나 만들고 slot/context에 등록한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 추천 | `controller` | 현재 session에서 controller를 찾을 이름이다. |
| `ContextKey` | 선택 | `None` | `None`이면 표준 RecorderController context key를 자동 사용한다. |
| `bStoreRuntimeSlot` | 선택 | `true` | runtime slot에 저장한다. |
| `bRegisterContext` | 선택 | `true` | world context에 등록한다. |
| `LastErrorReason` | 읽기 전용 | - | 실패 이유 디버그용이다. |

### `UVdjmRecordEventSubmitRecorderOptionRequestNode`
RecorderController에 option request를 제출한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `OptionRequest` | 필수 | 변경할 옵션 | bitrate, fps, duration, output path 등 controller가 처리할 요청 값이다. |
| `bProcessPendingAfterSubmit` | 선택 | `true` | 제출 직후 pending request 처리를 시도한다. |
| `bSucceedIfQueued` | 선택 | `true` | 즉시 적용이 아니어도 queue에 들어가면 성공 처리한다. |
| `LastErrorReason` | 읽기 전용 | - | 실패 이유 디버그용이다. |

## AppState / MediaPreview 노드

### `UVdjmRecordEventLoadAppStateNode`
앱 전역 상태 JSON을 로드하고, 필요하면 records TOC를 refresh한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 추천 | `app-state` | session 안에서 AppStateStore를 찾는 이름이다. |
| `ContextKey` | 선택 | `None` | `None`이면 표준 AppStateStore context key를 자동 사용한다. |
| `bCreateIfMissing` | 선택 | `true` | 파일이 없으면 기본 상태를 만든다. |
| `bRefreshRecordsToc` | 선택 | `true` | manifest registry를 스캔해 records TOC를 갱신한다. |
| `bSaveAfterRefresh` | 선택 | `true` | refresh 후 저장한다. |
| `MaxManifestFilesPerStep` | 선택 | `8` | 한 step에서 처리할 manifest 수다. |
| `MaxRegistryEntryStateChecksPerStep` | 선택 | `64` | 한 step에서 상태 확인할 registry entry 수다. |
| `bStoreRuntimeSlot` | 선택 | `true` | runtime slot에 저장한다. |
| `bRegisterContext` | 선택 | `true` | world context에 등록한다. |

### `UVdjmRecordEventEnsureMediaPreviewManagerNode`
MediaPreviewManagerActor를 찾거나 스폰하고 slot/context에 등록한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 추천 | `media-preview-manager` | session 안에서 preview manager를 찾는 이름이다. |
| `ContextKey` | 선택 | `None` | `None`이면 표준 MediaPreviewManager context key를 자동 사용한다. |
| `bStoreRuntimeSlot` | 선택 | `true` | runtime slot에 저장한다. |
| `bRegisterContext` | 선택 | `true` | world context에 등록한다. |

### `UVdjmRecordEventInitializeMediaPreviewManagerNode`
MediaPreviewManagerActor의 명시 초기화/registry refresh를 수행한다.

| Property | 필수 | 기본/추천 | 의미 |
|---|---:|---|---|
| `RuntimeSlotKey` | 추천 | `media-preview-manager` | 먼저 조회할 preview manager slot이다. |
| `bFindOrSpawnIfMissing` | 선택 | `true` | slot에 없으면 자동으로 찾거나 스폰한다. |
| `bForceRefresh` | 선택 | `false` | 이미 초기화되어도 registry를 다시 refresh한다. |
| `bApplyCarouselWindowAfterInit` | 선택 | `true` | 초기화 후 carousel window를 적용한다. |
| `bSucceedWithEmptyRegistry` | 선택 | `true` | registry가 비어도 성공 처리한다. |
| `SlotCount` | 선택 | `5` | preview carousel slot 수다. |
| `ActiveSlotIndex` | 선택 | `2` | 활성 preview slot index다. |
| `InitialCenterSourceIndex` | 선택 | `INDEX_NONE` | 처음 중앙에 둘 registry source index다. |
| `bAutoStartCenterPreview` | 선택 | `true` | 중앙 preview를 자동 재생한다. |
| `MaxManifestFilesPerStep` | 선택 | `8` | manifest scan step 크기다. |
| `MaxRegistryEntryStateChecksPerStep` | 선택 | `64` | registry 상태 확인 step 크기다. |
| `MaxRegistryEntriesPerStep` | 선택 | `32` | registry entry 복사 step 크기다. |

## Debug / Composite / Jump 노드

### `UVdjmRecordEventLogNode`
| Property | 필수 | 의미 |
|---|---:|---|
| `Message` | 필수 | 출력할 로그 메시지다. |
| `bLogAsWarning` | 선택 | true면 warning으로 출력한다. |

### `UVdjmRecordEventSequenceNode`
| Property | 필수 | 의미 |
|---|---:|---|
| `Children` | 필수 | 순서대로 실행할 child event 목록이다. Sequence는 작은 manager처럼 child flow state를 가진다. |

### `UVdjmRecordEventJumpToNextNode` / `SelectorNode`
| Property | 필수 | 의미 |
|---|---:|---|
| `TargetClass` | 조건부 | 다음에 찾을 event class다. |
| `TargetTag` | 조건부 | 다음에 찾을 event tag다. |
| `bAbortIfNotFound` | 선택 | target을 못 찾으면 abort할지 정한다. |

## 자주 쓰는 작성 패턴

### Intro 후 제거
```text
CreateWidgetNode:
  WidgetClass = WBP_Intro
  RuntimeSlotKey = intro-widget
  bAddToViewport = true
  DestroyPolicy = ERemoveOnSignal
  DestroySignalTag = intro-done
  DestroyConditionMode = EConditional

Widget animation end:
  EmitFlowSignal("intro-done")
```

### Preview lobby 최초 진입
```text
Root:
  LoadAppState
  EnsureMediaPreviewManager
  RegisterSubgraphSignalBranchNode(open-preview -> subgraph-preview)
  EmitSignalNode(open-preview)

subgraph-preview:
  CreateWidgetAndRegisterContextNode
    WidgetClass = WBP_Lobby
    RuntimeSlotKey = preview-lobby
    ContextKey = preview-lobby
  InitializeMediaPreviewManagerNode
    RuntimeSlotKey = media-preview-manager
  ShowWidgetNode
    RuntimeSlotKey = preview-lobby
    ContextKey = preview-lobby
    LookupPolicy = ERuntimeSlotThenContext
```

### Recorder 화면 진입
```text
Root:
  RegisterSubgraphSignalBranchNode(open-recorder -> subgraph-recorder)

Button:
  EmitFlowSignal("open-recorder")

subgraph-recorder:
  LowerWidgetNode
    RuntimeSlotKey = preview-lobby
    ContextKey = preview-lobby
    LookupPolicy = ERuntimeSlotThenContext
    bSucceedIfMissing = true
  SpawnRecordBridgeActorWait
    StartPolicy = EPrepareOnly
  EnsureRecorderControllerNode
    RuntimeSlotKey = controller
  CreateWidgetAndRegisterContextNode
    WidgetClass = WBP_Recorder
    RuntimeSlotKey = recorder-ui
    ContextKey = recorder-ui
  StartRecordBridgeActorNode
```

## 문제 생겼을 때 빠른 점검
- `CreateWidgetAndRegisterContextNode`가 실패하면 `WidgetClass`, `RuntimeSlotKey`, `ContextKey`가 모두 채워져 있는지 먼저 본다.
- `ShowWidget`/`LowerWidget`이 실패하면 `RuntimeSlotKey`, `ContextKey`, `LookupPolicy`를 같이 본다.
- 다른 subgraph/session에서 찾아야 하는 객체라면 `CreateWidgetAndRegisterContextNode`로 먼저 등록하고, `LookupPolicy=ERuntimeSlotThenContext` 또는 `EContextOnly`를 사용한다.
- `WaitForSignalNode`가 진행하지 않으면 `SignalTag` 철자, `ConditionMode`, emit 시점, debug string의 pending signal을 본다.
- branch가 실행되지 않으면 root flow에서 `RegisterSubgraphSignalBranchNode`가 먼저 실행됐는지, `SignalTag`와 `SubgraphTag`가 정확한지 확인한다.
- subgraph 이름은 오타에 민감하다. 예를 들어 `subgraph-recoder`와 `subgraph-recorder`는 완전히 다른 이름이다.
