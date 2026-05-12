# VdjmRecorder Current Architecture

## 문서 목적
- 이 문서는 현재 `VdjmRecorder` 플러그인 구조를 나중에 다시 볼 때 빠르게 복기하기 위한 문서다.
- 특히 `구조`, `실행 흐름`, `플랫폼별 차이`, `의존성`, `의도와 현재 상태`를 함께 적어 둔다.
- 현재 기준은 local worktree에서 확인한 코드 상태다.

## 한 줄 요약
- 현재 Android 기준 구조는 `BridgeActor -> Resolver -> Resource -> Pipeline -> Unit -> Platform Encoder`가 실제 녹화 실행 경로이고, `Controller/EventManager/Observer`는 그 위에 오케스트레이션 레이어로 올라가며, `MetadataStore/AppState/MediaPreview`는 녹화 결과를 앱 콘텐츠로 다시 불러오는 레이어다.

## 핵심 의도
- `BridgeActor`는 월드/뷰포트/리소스/파이프라인 초기화와 실제 녹화 시작/정지의 플랫폼 실행체 역할을 한다.
- `Controller`는 외부 요청과 옵션 변경을 받아 `BridgeActor` 실행 문맥에 안전한 시점으로 전달하는 상위 제어 레이어다.
- `EventManager`는 브릿지를 직접 대체한 최종 녹화 엔진이라기보다, 외부에서 호출할 때 플로우/바인딩/분기를 담당하는 오케스트레이터로 보는 것이 맞다.
- `Pipeline/Unit` 분리는 플랫폼별 녹화 경로를 같은 개념으로 묶기 위한 추상화다.
- 이 추상화 덕분에 `Windows WMF`는 `NV12 compute -> readback -> encode` 다단계로 갈 수 있고, `Android`는 `surface submit` 기반 단일 unit 경로로 줄일 수 있다.

## 모듈/공통 의존성

### 모듈 레벨
- 모듈명: `VdjmRecorder`
- 주요 공통 의존성: `Core`, `RenderCore`, `RHI`, `Projects`, `CoreUObject`, `Engine`, `Json`, `JsonUtilities`, `AudioMixer`, `Slate`, `SlateCore`, `SlateRHIRenderer`
- Windows 전용 의존성: `AVCodecsCore`, Media Foundation 라이브러리(`mf`, `mfplat`, `mfreadwrite`, `mfuuid`)
- Android 전용 의존성: `Launch`, `AndroidRuntimeSettings`, `Vulkan`, `VulkanRHI`, NDK 라이브러리(`mediandk`, `android`, `log`, `EGL`, `GLESv3`, `vulkan`)

### 셰이더 매핑
- `VdjmRecorder` 모듈 시작 시 플러그인 셰이더 경로를 `/Plugin/VdjmMobileUi/...`로 매핑한다.
- NV12 변환 셰이더는 `Shaders/VdjmRecordShader.usf` 하나를 사용한다.

## 런타임 구조

### 1. BridgeActor 중심 초기화 체인
- `AVdjmRecordBridgeActor`는 BeginPlay 이후 초기화 체인을 돈다.
- 주요 단계:
  - `EInitializeWorldParts`
  - `EInitializeCurrentEnvironment`
  - `ECreateRecordResource`
  - `EPostResourceInitResolve`
  - `ECreatePipelines`
  - `EFinalizeInitialization`
  - `EComplete`
- 이 체인 동안 뷰포트/플레이어컨트롤러, 데이터 에셋, Resolver, Resource, Pipeline이 준비된다.

### 2. Resolver/Resource/Pipeline/Unit
- `UVdjmRecordEnvResolver`
  - DataAsset preset을 현재 플랫폼/품질/뷰포트 기준으로 해석한다.
  - 현재는 `BridgeActor`가 들고 있는 explicit override(`QualityTier`, `FrameRate`, `Bitrate`, `FileName`)를 우선 반영한다.
  - 최종 해상도, 프레임레이트, 비트레이트, 출력 경로를 결정한다.
- `UVdjmRecordResource`
  - 최종 실행값(`FinalFrameRate`, `FinalBitrate`, `FinalFilePath`)을 보유한다.
  - 최근 변경 기준으로는 resolver 결과를 부분 파일경로만이 아니라 runtime config 단위로 다시 동기화할 수 있다.
  - 실제 플랫폼별 Resource는 이를 상속한다.
- `UVdjmRecordUnitPipeline`
  - Resource를 입력으로 받아 Unit 인스턴스를 만든다.
  - 플랫폼별 파이프라인이 어떤 Unit을 쓸지 결정한다.
- `UVdjmRecordUnit`
  - 실제 프레임 처리/인코딩 단계 단위다.

### 3. Controller/Observer/EventManager
- `UVdjmRecorderController`
  - 현재는 `EventManager`를 소유하는 상위 진입점이며, 옵션 적용/녹화 시작/정지는 가능한 한 이 레이어를 통해 들어가게 정리하는 중이다.
  - `FVdjmRecorderOptionRequest`를 메시지로 받아서 처리하며, 요청이 몰릴 때는 controller 내부에서 `latest wins` 방식으로 병합한다.
  - 브릿지가 아직 안전하게 적용할 수 없는 시점이면 요청을 pending으로 유지하고, 이후 tick에서 safe point에 재시도한다.
- `UVdjmRecorderStateObserver`
  - 현재는 `BridgeActor` 직접 감시기가 아니라 `EventManager`가 내보내는 coarse session state 감시기다.
  - `chainInit`의 세부 스텝은 더 이상 observer가 직접 다루지 않고, 필요 시 `EventManager` 디버그 delegate 쪽에서만 본다.
- `UVdjmRecordEventManager`
  - 브릿지 바인딩, FlowRuntime 실행, JSON 플로우 실행, coarse session state 관리까지 담당한다.
  - 앞으로 UI와 상위 호출은 이쪽으로 점차 모으는 방향이 자연스럽다.

### 4. MetadataStore/AppState/MediaPreview
- `UVdjmRecordMetadataStore`
  - 녹화가 완료된 후 `Manifest`, 내부 registry, Android MediaStore publish 작업을 관리한다.
  - 녹화 종료 직후에는 media artifact를 만들고, MediaStore 노출은 queue를 통해 처리한다.
  - 앱 시작 또는 갤러리 진입 시 `RefreshRegistryFromDisk` 계열 경로로 저장된 manifest 목록을 다시 읽어온다.
- `UVdjmRecordAppState`
  - 앱 전체 설정, 서버 endpoint, preview/gallery 상태처럼 "유저가 나갔다 들어와도 유지되어야 하는 런타임 TOC"를 담기 위한 상태 저장소다.
  - 개별 미디어의 진실은 manifest/registry가 맡고, AppState는 현재 선택/화면 상태/서비스 설정 같은 상위 상태를 맡는다.
- `UVdjmRecordMediaPreviewProbeWidget`
  - BridgeActor/EventFlow/Carousel 없이 `MetadataStore -> RegistryEntry -> MediaPlayer/MediaTexture -> Image` 경로만 검증하는 독립 테스트 위젯이다.
  - 실기기에서 MediaPlayer가 내부 파일 또는 content uri를 열 수 있는지 빠르게 확인하는 용도다.
- `UVdjmRecordMediaPreviewWidget`
  - 하나의 manifest/registry entry를 이미지 카드처럼 보여주는 preview 카드 베이스다.
  - `PreviewImage`, `InputButton`, `PreviewOverlay` 같은 optional bind 지점을 통해 BP 외형을 붙인다.
- `UVdjmRecordMediaPreviewCarouselWidget`
  - legacy registry/slot/window 확인용 최소 carousel 베이스다.
  - 실패한 active center/swipe/layout 실험은 제거했고, 새 gallery UX의 기준으로 더 이상 확장하지 않는다.
- `VdjmWidgets` 예정 계층
  - 새 gallery/carousel UX는 `VdjmRecordMediaPreview*` 확장이 아니라 별도 runtime module인 `VdjmWidgets`에서 다시 시작한다.
  - Carousel은 refresh, card 생성/재사용/파괴, layout state, active/visible/hidden/empty 배정, input/swipe 처리를 관리한다.
  - Card는 자기 상태 전환과 media 표시/재생/중지/placeholder를 소유한다.

### Media/Preview 트리거
1. 녹화 종료
   - `BridgeActor/Controller`가 stop을 처리한다.
   - artifact/manifest가 만들어진다.
   - `MetadataStore`가 registry를 갱신하고 MediaStore publish job을 진행한다.
2. 앱 시작 또는 갤러리 진입
   - `AppState`를 먼저 읽어 서비스 endpoint, 마지막 preview 위치 같은 상위 상태를 복원한다.
   - `EnsureMediaPreviewManager`가 PreviewManagerActor를 찾거나 스폰하고 world context/runtime slot에 등록한다.
   - `InitializeMediaPreviewManager`가 manager 초기화 완료 여부를 확인한다.
   - 이미 초기화가 끝났고 강제 refresh가 아니면 no-op 성공으로 진행한다.
   - 필요할 때만 `MetadataStore`가 저장된 manifest/registry를 다시 스캔한다.
   - `PreviewProbe` 또는 `PreviewCarousel`이 registry entry를 받아 MediaPlayer로 표시한다.
3. 사용자가 preview를 클릭
   - preview widget 또는 carousel owner가 선택된 manifest 정보를 외부로 알린다.
   - 이후 edit/play/delete/upload 같은 실제 행동은 preview 카드가 아니라 상위 화면/controller가 결정한다.

### Media/Preview 의존성
- Preview는 BridgeActor에 직접 의존하지 않는 것이 기본이다.
- Preview의 직접 의존성은 `MetadataStore`, `RegistryEntry`, `MediaPlayer`, `MediaTexture`, `Image Widget`이다.
- PreviewManager는 `VdjmRecorderWorldContextSubsystem`의 `MediaPreviewManager` key로 등록할 수 있으며, EventFlow runtime slot 기본 이름은 `media-preview-manager`다.
- 녹화 직후 자동 갱신은 `MetadataStore` 이벤트나 상위 화면의 refresh 호출로 연결한다.
- 서버 업로드/삭제/동기화는 preview widget 안에 넣지 않고, registry/app state/controller 계층에서 처리한다.
- `InitializeMediaPreviewManager`는 `StartPreviewManagerInit -> AdvancePreviewManagerInitStep`를 반복 호출하는 명시 초기화 경로다.

### Media/Preview 주의점
- `ProbeWidget`은 검증용으로 독립성이 강하고, 실제 갤러리 UX는 새 `VdjmWidgets` 계층에서 다시 만든다.
- legacy `AreaRule`, carousel policy, swipe motion, screen template, debug actor는 제거되었다.
- 내부 파일 경로와 Android content uri는 모두 가능하지만, 실제 열림 여부는 기기/플랫폼 MediaPlayer 정책에 영향을 받는다.
- manifest/registry가 없는 media는 앱 콘텐츠로 취급하지 않는 방향이 안전하다.
- Gallery/Carousel Construct에서 자동 refresh를 반복하는 방식은 피한다. 반복 진입은 manager의 초기화 완료 상태를 확인하고, 강제 refresh가 필요한 경우만 다시 스캔한다.

## Event Flow 구조

### 현재 구성
- 이벤트 관련 파일은 현재 `Source/VdjmRecorder/Public/VdjmEvents`, `Source/VdjmRecorder/Private/VdjmEvents` 폴더로 정리되어 있다.
- `UVdjmRecordEventManager`
- `UVdjmRecordEventFlowDataAsset`
- `UVdjmRecordEventFlowRuntime`
- `FVdjmRecordEventSubgraph`
- `FVdjmRecordSubgraphSignalBranch`
- `FVdjmRecordEventFlowFragment`
- `UVdjmRecordEventBase` 및 파생 노드들

### 현재 흐름
1. `EventManager` 생성
2. 월드에서 브릿지 자동 탐색/바인딩
3. Asset 또는 JSON으로부터 `FlowRuntime` 생성
4. Tick에서 현재 인덱스 이벤트 실행
5. 결과 타입에 따라 성공/실패/대기/점프/선택 분기 처리
6. root flow는 필요하면 `RegisterSubgraphSignalBranchNode`로 signal -> subgraph branch rule을 manager에 등록한다.
7. 필요하면 UI/Widget에서 `EmitLinkedFlowSignal`, `PauseLinkedEventFlow`, `ResumeLinkedEventFlow`, `StopLinkedEventFlow`로 현재 active flow를 제어한다.
8. 등록된 branch signal이 들어오면 manager가 named subgraph를 새 session으로 시작한다.

### 현재 해석
- `EventManager`는 플로우와 분기 실행을 맡는다.
- 동시에 외부가 보기 쉬운 세션 상태(`ENew/EPreparing/EReady/ERecording/EFinalizing/ETerminated/EFailed`)를 coarse하게 정리한다.
- 실제 녹화 자체는 여전히 브릿지와 플랫폼 파이프라인이 수행한다.
- 즉, 지금 구조는 `EventManager가 브릿지 위에서 오케스트레이션을 담당하는 단계`다.
- 레벨에서 실제 시동은 `AVdjmRecordEventFlowEntryPoint` 같은 배치형 진입점이 맡을 수 있다.

### Flow 실행 상태
- 시동용 root flow는 한 번에 하나만 돈다.
- `Subgraph`는 DataAsset 안에 이름 붙여 저장한 flow template이고, 실행될 때는 별도 `FlowRuntime`과 별도 session으로 열린다.
- 같은 manager 안에서 root flow가 끝난 뒤에도 manager-owned branch registry는 남아 signal을 받을 수 있다.
- 대신 `Sequence` 같은 composite event는 child flow를 가지는 작은 실행 owner로 해석한다.
- 즉 `실행한다`는 행위 자체가 상태이며, `EventManager` 내부에서 `FlowHandle -> FlowExecutionState`로 관리한다.
- child에서 `Wait`가 발생하면 parent composite도 `Wait`로 남고, 상위 루트 flow도 자연스럽게 다음 tick까지 멈춘다.

### Subgraph Branch
- `StartSubgraphSessionNode`는 지정한 `SubgraphTag`를 즉시 새 session으로 시작한다.
- `RegisterSubgraphSignalBranchNode`는 `SignalTag`를 듣는 branch rule을 manager에 등록한다.
- branch rule은 main/root flow의 현재 index에 묶이지 않으므로, root flow가 종료된 뒤 UI 버튼 signal로도 subgraph를 시작할 수 있다.
- `BranchCases`는 0번부터 순서대로 검사하며, 첫 번째로 맞는 case만 실행한다.
- duplicate policy는 이미 같은 branch session이 살아 있을 때 `무시`, `새 session 시작`, `active session에 signal 전달`, `restart`, `fail` 중 하나로 처리한다.
- if/else-if/else를 만들 때는 앞쪽 case에 구체 조건을 두고, 마지막 case를 `EAlways` fallback으로 두면 된다.

### Runtime Edge Metadata
- 간선은 `FlowFragment / Json / DataAsset`에 저장되는 핵심 규칙이 아니다.
- 간선은 오직 runtime 중 `EventManager`가 만드는 메타데이터다.
- 현재는 흐름별로 `PendingEdgeDirective` 1개와 `LastObservedEdge` 1개만 유지한다.
- 간선 상태는 `EAdvance / ERepeat / EDiscard`이고, 의미는 각각 `진행 / 반복대기 / 버림`에 가깝다.
- 간선 종류는 1차로 `ENext / EJump / ESignal / ETerminal`만 둔다.
- event가 특수한 간선을 원하면 직접 생성하지 않고 `EventManager`에 directive를 요청하고, manager가 result 처리 시 그 요청을 소모한다.

### Fragment 계층
- `FVdjmRecordEventFlowFragment`는 코드에서 미리 조립하는 JSON 기반 flow 조각이다.
- `FVdjmRecordEventNodeFragment`는 개별 event node의 `class + properties + children` 조합을 표현한다.
- fragment는 `UVdjmRecordEventBase` UObject를 직접 들지 않는다.
- 대신 기존 flow JSON 스키마를 그대로 출력하고, runtime은 그 JSON을 다시 event node로 역직렬화한다.
- 이 덕분에 `DataAsset / Json / Fragment` 세 경로가 모두 같은 runtime 생성 경로로 합류한다.

### 현재 가능한 것
- fragment를 코드에서 체인해 preset을 만들 수 있다.
- fragment를 JSON으로 내보내고 바로 runtime으로 만들 수 있다.
- fragment를 transient `FlowDataAsset`으로도 바로 올릴 수 있다.
- runtime에 fragment를 append/insert 할 수 있다.
- tag 기준으로 기존 event를 fragment로 override 할 수 있다.
- 이 구조는 이후 subgraph, override, 코드 preset 확장에 그대로 이어질 수 있다.
- 현재 내장 preset helper는 bootstrap/reuse-spawn bridge/set-env/jump/log와 함께 object/actor/widget/context/signal 계열 primitive 조립 경로를 제공한다.
- `UVdjmRecordEventFlowFragmentWrapper`는 에디터에서 built-in preset을 골라 JSON / transient `FlowDataAsset` / runtime preview를 만들고, 현재 월드에서 flow 시작까지 시험하는 wrapper asset이다.

### Primitive / Composite 방향
- event는 가능한 한 작게 쪼갠다.
- event 사이의 임시 객체 전달은 `EventManager`의 runtime object slot을 사용하고, 월드 공유 참조는 `VdjmRecorderWorldContextSubsystem`에 올린다.
- 현재 primitive 기준은 다음과 같다.
  - `CreateObject`
  - `SpawnActor`
  - `RegisterContextEntry`
  - `RegisterWidgetContext`
  - `CreateWidget`
  - `RemoveWidget`
  - `WaitForSignal`
  - `Delay`
  - `EmitSignal`
  - `SetEnvDataAssetPath`
  - `EnsureMediaPreviewManager`
  - `InitializeMediaPreviewManager`
- composite는 `primitive를 자주 같이 쓰는 경우`만 얇게 감싼다.
  - `SpawnRecordBridgeActorWait`
  - `CreateObjectAndRegisterContext`
  - `SpawnActorAndRegisterContext`
  - `CreateWidgetAndRegisterContext`
- `SetEnvDataAssetPath`는 아직 world-global 설정이 아니라 recorder-specific primitive로 유지한다.
- `CreateWidget`은 기본적으로 즉시 성공하지만, 필요하면 `RemoveOnSignal` 또는 `RemoveAfterDelay` 정책으로 노드 안에서 위젯을 만들고 기다린 뒤 제거까지 수행할 수 있다.
- 명시 제거가 더 읽기 좋은 경우에는 `CreateWidget -> WaitForSignal/Delay -> RemoveWidget` 조합을 그대로 사용한다.

### Widget -> Flow 제어
- `UVdjmRecordEventWidgetBase`는 `EventManager`를 직접 들고 있거나, 없으면 `VdjmRecorderWorldContextSubsystem`에서 찾아온다.
- 위젯 블루프린트는 `EmitLinkedFlowSignal`을 호출해서 flow 안의 `WaitForSignal`을 깨울 수 있다.
- `CommonWidget` 계열처럼 `UVdjmRecordEventWidgetBase`를 상속하기 어려운 위젯은 `UVdjmRecordEventFlowBlueprintLibrary`를 쓴다.
- 이 경우 위젯 자신을 `WorldContextObject`로 넘기고 `EmitRecordFlowSignal`, `PauseRecordEventFlow`, `ResumeRecordEventFlow`, `StopRecordEventFlow`를 호출하면 된다.
- 예: intro widget animation 완료 시 `EmitLinkedFlowSignal("IntroFinished")` 또는 `EmitRecordFlowSignal(self, "IntroFinished")`를 호출하고, flow는 `WaitForSignal(IntroFinished) -> RemoveWidget(IntroWidgetSlot) -> 다음 이벤트` 순서로 진행한다.
- `PauseLinkedEventFlow`와 `ResumeLinkedEventFlow`는 전체 active flow를 잠시 멈추거나 재개한다. 현재는 루트 flow 하나를 기준으로 한다.
- `StartLinkedEventFlow`는 위젯에서 새 flow를 시작할 수 있게 열어둔 편의 함수지만, 이미 flow가 실행 중이면 manager 정책에 따라 실패할 수 있다.

### 레벨 시동기
- `AVdjmRecordEventFlowEntryPoint`는 `AInfo` 기반 레벨 배치형 시동기다.
- 이 액터는 `FlowDataAsset`, 자동 시작 여부, 시작 지연, 시작 후 초기 signal, pre-start widget, startup context binding 같은 레벨 파라미터를 담는다.
- 목표는 "이 액터 하나만 레벨에 놓아도 EventManager 생성과 Flow 시작이 붙는 것"이다.
- 같은 월드에서 실제 flow 시작 owner는 하나만 잡히며, 다른 entry point는 중복 시작을 거절한다.
- `InitialSignalTags`는 flow 시작 "직후" 발행하는 signal 목록이다. 시작 전 주입이 아니라 `WaitForSignal`, `SpawnRecordBridgeActorWait(StartPolicy=WaitForSignal)` 같은 노드의 첫 문을 여는 용도다.
- 시작 전에 넣어야 하는 object/widget 참조는 runtime slot이 아니라 `VdjmRecorderWorldContextSubsystem`에 등록하는 방향을 기본으로 한다.
- pre-start widget은 flow 시작 전에 미리 생성해 화면 어두워짐/로딩 overlay 같은 진입 연출에 쓸 수 있고, 필요하면 world context에도 등록할 수 있다.

## Option 메시지 구조

### 현재 기준
- `FVdjmRecorderOptionRequest`는 옵션 변경 메시지다.
- 메시지 안에는 `QualityTier`, `FileName`, `FrameRate`, `Bitrate`에 대한 개별 action이 들어간다.
- 각 필드는 `Ignore / Set / Clear` 개념으로 동작한다.
- 요청 처리 정책도 메시지에 같이 담아서, 즉시 처리 시도 또는 queue only를 고를 수 있게 열어뒀다.

### 실제 적용 흐름
1. 외부(UI/코드)가 `FVdjmRecorderOptionRequest`를 만든다
2. `UVdjmRecorderController`에 넣는다
3. Controller가 기존 pending 요청과 병합한다
4. safe point면 바로 `BridgeActor`에 override를 반영한다
5. `BridgeActor -> Resolver -> Resource` 순서로 최종 runtime config를 재동기화한다
6. safe point가 아니면 pending 상태로 남고 다음 tick에 다시 처리한다

### 현재 의도
- 메시지는 순수 데이터로 유지한다.
- 처리 로직은 `RecorderController` 안에 둔다.
- `BridgeActor`는 override 값 저장과 플랫폼 실행 문맥 연결을 담당한다.
- `Resource`는 최종 실행값을 보유하는 단일 적용 지점 역할을 계속 강화하는 방향이다.
- undo는 바로 실행하지 않고, 우선 controller 내부 history 골격만 둔 뒤 실제 reverse request 정책이 정리되면 연결하는 방향으로 본다.

## Event TODO

- `UVdjmRecordEventJumpToNextNode`
  - 기존 selector 의미를 정리한 forward jump node다.
  - 현재는 다음 class/tag를 찾아 index jump 하는 성격이며, 이후 subgraph 규칙과 함께 direct goto / label jump 정책을 더 좁힐 여지가 있다.
- 다음 추가 후보 event
  - `WaitForBridgeInitComplete`
  - `CreateWidget`
  - `AddWidgetToViewport`
  - `RemoveWidget`
- 목표 샘플 flow
  - intro widget 표시
  - bridge actor 생성 또는 재사용
  - bridge chain init 완료까지 대기
  - 녹화용 widget attach 및 후속 interaction 연결

## 플랫폼별 실행 흐름

### Windows / WMF 경로
1. Resolver가 Windows preset 해석
2. `UVdjmRecordWMFResource` 생성
3. Resource가 `TexturePool`을 생성
4. `UVdjmRecordWMFUnitDefaultPipeline`이 Unit들을 생성
5. 일반적으로
   - `UVdjmRecordWMFCSUnit`
   - `UVdjmRecordWMFEncoderReadBackUnit`
   순서로 이어진다
6. Compute Shader가 원본 텍스처를 NV12 레이아웃 텍스처로 변환
7. ReadBack 후 `FVdjmWindowsEncoderImpl`로 전달
8. WMF SinkWriter가 `MFVideoFormat_NV12` 입력을 H264로 기록

### Android / Surface 경로
1. Resolver가 Android preset 해석
2. `UVdjmRecordAndroidResource` 생성
3. `UVdjmAndroidRecordPipeline`이 사실상 단일 unit(`UVdjmRecordAndroidUnit`)을 사용
4. Android unit가 `CreatePlatformVideoEncoder()`로 Android encoder를 만든다
5. `FVdjmAndroidEncoderImpl`가 `FVdjmAndroidRecordSession`을 구성한다
6. Session이 Video codec, Audio codec, Muxer, Graphic backend를 초기화한다
7. Graphic backend는 현재 RHI에 따라
   - `FVdjmAndroidEncoderBackendVulkan`
   - `FVdjmAndroidEncoderBackendOpenGL`
   를 사용한다
8. 프레임은 CPU readback 없이 codec input surface 쪽으로 직접 제출된다

## NV12 구조를 위해 왜 이렇게 나뉘어 있는가
- Windows WMF 경로는 `NV12` 입력을 전제로 설계되어 있다.
- 그래서 `Compute Shader -> NV12 텍스처 -> ReadBack -> WMF Encode` 구조가 필요하다.
- `Shaders/VdjmRecordShader.usf`는 다음 의도를 가진다:
  - 상단 영역에 Y plane 저장
  - 하단 절반 영역에 UV interleaved plane 저장
  - 결과적으로 `height * 3 / 2` 구조의 NV12 레이아웃을 하나의 텍스처/버퍼처럼 다루게 한다
- 이 때문에 `Pipeline/Unit` 분리는 단순한 추상화 취향이 아니라, `NV12 전처리 + 인코더 요구 포맷`을 맞추기 위한 구조적 필요에서 나왔다.
- 반대로 Android는 `MediaCodec input surface` 기반이라 현재는 같은 구조를 강제로 거치지 않는다.

## Pipeline/Unit 구조의 원래 의도
- 원래 의도는 플랫폼마다 같은 단계 수를 강제하는 것이 아니다.
- 의도는 다음 둘을 동시에 만족하는 것이다:
  - 공통 추상화 유지
  - 플랫폼별 최적 경로 허용
- 그래서
  - Windows는 다단계 파이프라인
  - Android는 단일 혹은 소수 단계 파이프라인
  으로 자연스럽게 갈 수 있다.

## 플랫폼별 현재 상태 평가

### Android Vulkan
- 현재 코드 양과 상태 관리가 가장 자세하다.
- `Session -> Vulkan backend -> swapchain/input surface -> present` 흐름이 구체적으로 구현되어 있다.
- 현재 Android 경로 중 가장 주력으로 보인다.

### Android OpenGL
- EGL 공유 컨텍스트 기반 경로가 구현되어 있다.
- 다만 시작 시점보다 `Running()` 시점의 `EnsureEGLContextReady()`에 많이 의존하고 있어 실기기 재검증이 필요하다.
- 구현은 존재하지만 Vulkan만큼 강하게 검증되었다고 단정하긴 어렵다.

### Windows WMF
- 구조 의도는 매우 명확하다.
- `NV12 compute`, `readback`, `WMF encoder` 조합도 잘 드러난다.
- 다만 수정 중인 흔적과 방어 코드, 분기 흔적이 많아서 현재는 회귀 위험이 높은 편으로 보는 것이 안전하다.
- 즉, "구조는 맞는데 재검증이 반드시 필요한 상태"로 기록해 둔다.

## Event Flow Signal Bus

### 목적
- Event Flow의 signal은 두 가지 역할을 동시에 가진다.
- 첫 번째는 `WaitForSignalNode`가 소비하는 flow 진행/대기용 pending signal이다.
- 두 번째는 `BindFlowSignal`/`BindRecordFlowSignal`로 등록된 위젯, 액터, UObject가 반응할 수 있는 live callback signal bus다.
- 세 번째는 `RegisterSubgraphSignalBranchNode`가 등록한 manager-owned branch rule을 깨워 named subgraph session을 시작하는 trigger다.
- 이 구조의 목적은 flow와 UI 객체가 서로를 직접 참조하지 않고도 같은 signal tag로 상호작용하게 하는 것이다.

### 기본 사용 규칙
- signal을 받을 객체는 먼저 `BindRecordFlowSignal` 또는 `UVdjmRecordEventManager::BindFlowSignal`로 특정 `SignalTag`에 callback을 등록한다.
- 위젯의 `Construct` 또는 context 적용 직후가 일반적인 등록 지점이다.
- 일반 `UUserWidget`, `CommonUserWidget`, actor, UObject 모두 같은 bind API를 쓴다.
- callback signal은 live-only다. bind 이전에 emit된 signal은 callback으로 replay하지 않는다.
- 위젯이 사라질 때는 `UnbindRecordFlowSignal` 또는 `UnbindRecordFlowSignalsForObject`를 호출하는 것이 안전하다.
- `OnFlowSignalBroadcast`는 전체 감시/debug용으로 남기고, 일반 UX 반응은 tag 단위 bind API를 기본 경로로 둔다.
- subgraph branch는 callback bind와 다르게 manager에 등록된 규칙이므로, 등록 이후 main flow가 끝나도 signal을 받을 수 있다.
- `GetEventFlowDebugString`은 현재 등록된 `SubgraphSignalBranches`, active session handle, last matched case를 함께 출력한다.

### UX 애니메이션 흐름 예시
- Flow:
  - `CreateWidget(Intro, RuntimeSlotKey="intro")`
  - `EmitSignal("intro.open")`
  - `WaitForSignal("intro.done")`
  - `EmitSignal("intro.lower.start")`
  - `WaitForSignal("intro.lower.done")`
  - `LowerWidget(RuntimeSlotKey="intro")`
- Widget:
  - `Construct`에서 `BindRecordFlowSignal(Self, Self, "intro.open", CustomEvent_OpenIntro)` 호출
  - `Construct`에서 `BindRecordFlowSignal(Self, Self, "intro.lower.start", CustomEvent_LowerIntro)` 호출
  - `CustomEvent_OpenIntro`에서 등장 애니메이션 실행
  - 등장 애니메이션 완료 시 `EmitRecordFlowSignal(Self, "intro.done")`
  - `CustomEvent_LowerIntro`에서 퇴장 애니메이션 실행
  - 퇴장 애니메이션 완료 시 `EmitRecordFlowSignal(Self, "intro.lower.done")`
  - `Destruct`에서 `UnbindRecordFlowSignalsForObject(Self, Self)` 호출

### 주의점
- callback listener는 signal emit 시점에 이미 bind된 객체에게만 전달된다.
- flow 진행용 pending signal은 남을 수 있지만, callback signal은 과거 이벤트를 replay하지 않는다.
- 따라서 UX callback signal은 반드시 위젯 생성 및 bind 이후에 emit해야 한다.
- callback은 exact `SignalTag`에만 반응하므로 listener가 직접 모든 tag를 필터링할 필요가 없다.
- 같은 객체가 같은 tag로 다시 bind하면 기존 bind를 덮어쓴다.

## 현재 수정할 때 먼저 봐야 하는 포인트

### 상태/오케스트레이션
- `VdjmEvents/VdjmRecordEventManager.*`
- `VdjmEvents/VdjmRecordEventFlowDataAsset.*`
- `VdjmEvents/VdjmRecordEventFlowRuntime.*`
- `VdjmEvents/VdjmRecordEventNode.*`
- `VdjmRecorderController.*`
- `VdjmRecorderStateObserver.*`

### Media Manifest/Registry/AppState
- `VdjmRecordAppState.h/.cpp`
- `VdjmRecordMediaPreview.h/.cpp`
- `VdjmRecorderCore.h/.cpp`
- `VdjmRecordBridgeActor.h/.cpp`
- `VdjmRecorderController.h/.cpp`

### 플랫폼 공통 구조
- `VdjmRecorderCore.h/.cpp`
- `VdjmRecordBridgeActor.h/.cpp`
- `VdjmRecordTypes.h/.cpp`

### Android 경로
- `VdjmAndroidCore.h/.cpp`
- `VdjmRecoderAndroidEncoder.h/.cpp`
- `VdjmAndroidEncoderBackendVulkan.h/.cpp`
- `VdjmAndroidEncoderBackendOpenGL.h/.cpp`

### Windows WMF 경로
- `VdjmWMFCore.h/.cpp`
- `VdjmRecorderWndEncoder.h`
- `VdjmRecorderWndEncorder.cpp`

### 셰이더/NV12
- `VdjmRecordShader.h/.cpp`
- `Shaders/VdjmRecordShader.usf`

## 다음 문서와의 관계
- 작업 순서 확인: `Current_Work_Checklist.md`
- 전체 계획과 장기 설계: `Android_Recording_Audio_GIF_Plan.md`
