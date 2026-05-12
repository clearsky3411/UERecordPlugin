# Codex Handoff

Last updated: 2026-05-12

이 문서는 긴 대화 컨텍스트가 압축되거나 새 채팅방으로 이동할 때, 현재 작업 의도와 이어받을 기준을 빠르게 복원하기 위한 인계 문서다.

## 새 채팅방에서 먼저 할 일

새 채팅방 첫 메시지는 아래처럼 시작하면 된다.

```text
AGENTS.md와 Docs/Codex_Handoff.md를 먼저 읽고, 현재 VdjmMobileUi 작업 맥락을 이어서 진행해줘.
```

그 다음 필요에 따라 아래 문서를 같이 읽는다.

- `Docs/VdjmRecorder_Current_Architecture.md`
- `Docs/Current_Work_Checklist.md`
- `Docs/VdjmRecordEventNode_PropertyGuide.md`
- `Docs/VdjmRecordMediaPreview_Guide.md`
- `Docs/VdjmRecordMetadataSchema_v1.md`
- `Docs/VdjmRecordEventFlowDataAssetEditor_Guide.md`

## 문서 참조 우선순위

현재 구현/작업 판단은 아래 순서로 참조한다.

1. `Docs/Codex_Handoff.md`
2. `Docs/Current_Work_Checklist.md`
3. `Docs/VdjmRecorder_Current_Architecture.md`
4. 기능별 guide 문서
5. history/archive 문서

기능별 guide 문서는 다음 기준으로 본다.

- Event/DataAsset 작성: `Docs/VdjmRecordEventNode_PropertyGuide.md`
- FlowDataAsset 전용 editor 사용: `Docs/VdjmRecordEventFlowDataAssetEditor_Guide.md`
- Media preview/gallery/carousel: `Docs/VdjmRecordMediaPreview_Guide.md`
- Manifest/metadata JSON schema: `Docs/VdjmRecordMetadataSchema_v1.md`

`Docs/Android_Recording_Audio_GIF_Plan.md`는 2026-04-22 기준으로 업데이트가 중지된 history/archive 문서다. 현재 구현 기준으로 우선 참조하지 말고, 과거 계획과 의사결정 맥락을 확인할 때만 참고한다.

## 작업 환경

- Plugin root: `G:\Project\00Main\bg1LAb\VdjmBg1Lab\Plugins\VdjmMobileUi`
- Project file: `G:\Project\00Main\bg1LAb\VdjmBg1Lab\VdjmBg1Lab.uproject`
- UBT path: `E:\ueLauncher\5_6\UE_5.6\Engine\Build\BatchFiles`
- Current branch convention: `codex/*`
- 최근 사용 branch: `codex/record-controller`
- 작업 언어: 한국어

## 작업 원칙

- 구현 전 관련 코드와 문서를 먼저 확인하고 현재 상태를 5줄 이하로 요약한다.
- 큰 변경이나 2개 이상 파일에 영향이 있으면 계획을 먼저 제시한다.
- 기존 변경을 되돌리지 않는다. 워크트리가 dirty일 수 있다.
- 새 기능은 최소 변경으로 붙이고, 불필요한 리팩터링은 피한다.
- 새 파일은 `public/protected/private`, 함수/델리게이트/변수 순서 규칙을 지킨다.
- 로컬 변수와 파라미터는 camelCase, 함수는 PascalCase를 쓴다.
- 사용자가 원하는 철학은 "이벤트 구성만으로 앱 기능과 UX 흐름을 제어"하는 것이다.

## 핵심 설계 철학

- `BridgeActor`는 실제 Android 녹화 실행체다.
- `RecorderController`는 UI/외부 코드가 녹화 옵션, 시작/정지, 상태 확인을 조작하는 주 진입점이 되어야 한다.
- `EventManager`는 bridge를 직접 대체하는 것이 아니라 app/UX 오케스트레이션 레이어다.
- 상태 검증과 약참조 등록은 중요하다. 전역 참조는 `VdjmRecorderWorldContextSubsystem`에 둔다.
- `RuntimeSlotKey`는 현재 flow session 안에서만 공유되는 임시 객체 이름이다.
- `ContextKey`는 모든 flow/session에서 찾을 수 있는 world-global 약참조 이름이다.
- 반복 사용 UI는 `CreateWidgetAndRegisterContext -> ShowWidget -> LowerWidget` 흐름으로 재사용한다.
- DataAsset/JSON으로 flow를 구성하고, 나중에는 editor UX를 더 개선한다.

## 2026-05-12 Legacy Preview Cleanup

- 실패한 legacy carousel 실험 코드는 제거했다.
- 제거된 것: `UVdjmRecordMediaPreviewAreaRule`, `UVdjmRecordMediaPreviewCarouselPolicy`, `UVdjmRecordMediaPreviewSwipeMotionObject`, `UVdjmRecordMediaPreviewScreenWidget`, `AVdjmRecordMediaPreviewDebugActor`, direct pointer/swipe fallback, preview debug console command.
- 유지된 것: `AVdjmRecordMediaPreviewManagerActor`의 manifest/registry source 관리, preview slot/window 최소 기능, `UVdjmRecordMediaPreviewWidget`, `UVdjmRecordMediaPreviewCarouselWidget`, `UVdjmRecordMediaPreviewProbeWidget`.
- `AVdjmRecordMediaPreviewManagerActor`에는 EventFlow가 쓰는 명시 init API만 얇게 유지한다. `StartPreviewManagerInit -> AdvancePreviewManagerInitStep`를 event tick마다 진행하고, 내부 registry scan/copy/window apply를 단계별로 처리한다.
- 새 gallery/carousel UX는 기존 `VdjmRecordMediaPreview*` 확장이 아니라 새 `VdjmWidgets` 런타임 모듈/위젯 계층에서 다시 시작한다.

## VdjmWidgets 재작성 방향

- Carousel은 central manager다. manifest/registry refresh, card 생성/재사용/파괴, layout state 결정, active/visible/hidden/empty 배정, input/swipe 처리를 소유한다.
- Card는 자기 상태 전환과 media 표시를 소유한다. active는 반복 preview, visible은 첫 썸네일/첫 프레임, hidden은 media 중지/숨김, empty는 검은 화면이 아닌 placeholder를 담당한다.
- Carousel 내부 기능은 component처럼 작은 `UObject`들로 나눈다. source provider, card pool, layout policy, state policy, input controller, motion controller, preview coordinator를 후보로 둔다.
- 각 새 class는 책임, 금지사항, 필요한 전제조건을 주석으로 남긴다.
- 먼저 module skeleton과 책임 문서를 만들고, 이후 card state machine, carousel manager, source refresh, layout, input/swipe 순으로 작은 커밋을 쌓는다.

## EventFlow 현재 정의

### Flow와 Session

- root/main graph는 사실상 bootstrap flow다.
- subgraph는 실행 단위이며, UI 버튼이나 signal로 반복 실행할 수 있어야 한다.
- session은 flow 실행의 생명주기 단위다.
- runtime slot은 session-local이다.
- context는 world-global이다.
- main flow가 끝나도 subgraph signal branch는 살아 있어야 한다.

### Signal

- signal은 delegate, 반복 실행 trigger, flow control trigger의 역할을 한다.
- signal은 기본적으로 global하게 다루되, 도메인 규칙으로 tag를 분리한다.
- signal listener는 bind된 이후의 signal만 받는 것이 맞다.
- 이미 죽은 UObject listener는 실행하지 않고 정리하는 방향이 맞다.
- `WaitForSignalNode`는 pending signal을 소비해 flow를 진행할 수 있다.

### Branch/Subgraph

- `RegisterSubgraphSignalBranchNode`는 signal tag와 subgraph tag를 연결한다.
- `BranchTag`는 등록/교체/디버그용 branch identity다.
- `SubgraphTag`는 FlowDataAsset 안의 실제 subgraph 이름이다.
- `BranchCases`는 if/else-if처럼 0번부터 순서대로 검사하는 확장 포인트다.
- UI 버튼에서 subgraph를 직접 실행하기 위한 helper도 도입되었다.

### Widget Flow

- `CreateWidgetNode`는 widget 생성, viewport 추가, runtime slot 저장, destroy policy를 맡는다.
- `CreateWidgetAndRegisterContextNode`는 widget 생성 후 context 등록까지 맡는다.
- 반복 진입 UI에는 `bReuseRegisteredContext`, `bRefreshRuntimeSlotWhenReused`, `bRefreshContextRegistrationWhenReused`, `bSetVisibleWhenReused`를 사용한다.
- `ShowWidgetNode`, `LowerWidgetNode`, `RemoveWidgetNode`는 `LookupPolicy`로 runtime/context 조회 방식을 정한다.
- `LowerWidgetNode`가 정상적으로 `Collapsed`로 내렸는데 화면에 남는 경우, 내부 widget switcher나 parent UI 구조를 먼저 의심한다.

## Recorder/Bridge 상태

- `BridgeActor::BeginPlay`에서 chain init을 자동 시작하지 않고, 외부 event가 준비 후 `StartRecordBridgeActor()`를 호출하는 방향으로 정리했다.
- `SpawnRecordBridgeActorWait`는 `EPrepareOnly`로 bridge를 미리 만들고 등록할 수 있다.
- loading widget에서 bridge chain init delegate에 bind하고, 이후 `StartRecordBridgeActorNode`로 시작하는 흐름이 목표다.
- `RecorderController`는 bridge/controller 상태 확인, 녹화 가능 여부, 옵션 요청, media/metadata 접근의 중심이 되어야 한다.
- 모바일 테스트에서 Android 녹화, MediaStore publish, gallery 노출은 성공한 적이 있다.
- output path 중복 결합 문제는 한동안 있었고, 날짜/시간 기반 naming 또는 normalize 정책이 후속 안정화 후보다.

## Metadata/AppState/Registry

- 녹화 결과의 콘텐츠 단위는 `UVdjmRecordMediaManifest`다.
- manifest JSON은 나중에 웹/스트리밍/권한/편집 진입의 명함 역할을 해야 한다.
- `UVdjmRecordMetadataStore`는 manifest 생성, publish queue, registry 관리의 중심이다.
- Android MediaStore publish는 background job으로 처리하고, UObject 갱신은 GameThread로 되돌리는 방향이다.
- AppState는 현재 화면 상태, service 설정, media registry TOC, preview 상태 같은 통합 런타임/지속 상태를 담는 역할이다.
- `content_uri`는 로컬 Android 노출용이고, 서비스/웹 playback은 장기적으로 cloud object storage/CDN/upload 정책과 분리해야 한다.

## Media Preview / Gallery

### 목적

- 저장된 manifest/registry를 읽어 gallery/lobby UI에 보여준다.
- BridgeActor와 분리되어야 한다.
- preview click은 edit/play/delete/upload 같은 상위 controller 동작으로 이어질 수 있다.

### 구성

- `AVdjmRecordMediaPreviewManagerActor`
  - registry를 읽고 preview slot/window를 관리한다.
  - world context key로 등록 가능하다.
  - chain init 구조가 도입되어 loading UI가 progress를 받을 수 있다.
- `UVdjmRecordMediaPreviewWidget`
  - 개별 preview card다.
  - `PreviewImage`와 필요 시 투명 `InputButton`을 bind한다.
  - media source는 manifest/registry에서 찾는다.
- `UVdjmRecordMediaPreviewCarouselWidget`
  - preview widget을 자동 생성하고 `PreviewWidgetHostPanel`에 배치한다.
  - `PreviewWidgetHostPanel`은 `UOverlay` 권장이다.
- `UVdjmRecordMediaPreviewScreenWidget`
  - SafeZone/Header/Carousel/Footer 형태의 gallery screen base다.

### 권장 위젯 계층

```text
SafeZone_Root
  Overlay_Root
    Background / Blur / Dim
    VerticalBox_Main
      HeaderArea
      SizeBox_CarouselArea
        Overlay_CarouselArea
          FixedBackLayer
          CarouselWidget
            PreviewWidgetHostPanel
          FixedFrontLayer
      FooterArea
```

`CarouselWidget`은 `UVdjmRecordMediaPreviewCarouselWidget` 기반 BP다. 그 안에 `PreviewWidgetHostPanel`이라는 `Overlay`를 두고, `PreviewWidgetClass`에 개별 card BP를 넣는다.

### 최근 Carousel 변경

2026-05-11 기준으로 carousel 배치는 부모 panel center 기준의 가상 `XYZ` 좌표계로 리워크되었다.

- `FVdjmRecordMediaPreviewLayoutSettings`
  - `XAxis`
  - `YAxis`
  - `ZAxis`
  - 기존 `LineDirection`, `CurveDirection`은 compatibility/advanced field로 남김
- `FVdjmRecordMediaPreviewLayoutResult`
  - `VirtualPosition`
  - `ProjectedOffset`
  - `RenderTranslation`
- 의미
  - 외부에서는 `X/Y/Z`로 배치를 조작한다.
  - 내부에서는 UMG의 `RenderTranslation`, `Scale`, `Opacity`, `ZOrder`로 투영한다.
  - `Z`는 실제 3D가 아니라 priority/depth 추상값이다.
  - `RenderTranslation`은 parent center 기준 offset이다.
- 기본값
  - `XAxis=(1,0)`
  - `YAxis=(0,1)`
  - `ZAxis=(0,0)`
  - `ZAxis=(0,-1)` 같은 값으로 멀어진 카드가 위/뒤로 빠지는 느낌을 만들 수 있다.

### Carousel Widget 옵션/Active 정책

Carousel 관련 UX 옵션은 파편화를 줄이기 위해 `UVdjmRecordMediaPreviewCarouselWidget`에 우선 모은다.

- `SlotCount`: 화면에 생성/관리할 preview slot 수다. 최소 3개로 clamp한다.
- `ActiveSlotIndex`: active source를 배치할 기준 slot이다. 현재 active 카드 자체를 고정 의미로 가지는 값이 아니다.
- `bLoopCarousel`: false면 양 끝에서 slide가 멈추고, true면 source index가 순환한다.
- `bHandleCardClick`: card click을 carousel policy가 해석할지 결정한다.
- `bEnableSwipe`, `SwipeThresholdPixels`, `SwipeHorizontalBias`: press/release delta 기반 swipe 판정 옵션이다.
- `bAutoStartCenterPreview`: active source가 기준 slot에 배치된 뒤 해당 slot preview를 자동 시작할지 결정한다.
- `bAutoRefreshAfterRecordArtifactReady`: `AVdjmRecordBridgeActor::OnRecordArtifactReady` 이후 registry refresh/window 재적용을 자동 수행한다.
- `LayoutSettings.NormalizedSlotSpacing`: 0이면 화면 크기와 진행 축(`XAxis`) 기준으로 자동 간격을 계산한다. 수동 값을 넣으면 해당 percent를 사용한다.
- runtime에서는 실제 registry entry 수가 `SlotCount`보다 적으면 effective slot 수를 registry 수로 줄인다. 즉 녹화가 2개면 중복 5장을 만들지 않고 2장만 배치한다.
- refresh는 `OnCarouselRefreshStarted -> manifest/registry snapshot refresh -> previous active identity 복원 -> slot/card binding -> line layout -> OnCarouselRefreshFinished` 순서로 명시한다.
- refresh 때 이전 active `RecordId/MetadataFilePath/OutputFilePath`를 새 registry에서 다시 찾고, 못 찾으면 첫 위치로 fallback한다.
- `LayoutSettings` 원본 값은 runtime refresh 중 덮어쓰지 않는다. runtime용 visible slot/active slot은 `BuildRuntimeLayoutSettings()`에서 복사본으로 만든다.

Active 동작은 `CarouselPolicy -> CarouselWidget` 경로에서 중앙 처리한다.

- card click은 해당 slot의 `SourceRegistryIndex`를 active source로 선택한다.
- swipe는 `SlideNext/SlidePrevious`를 호출하고, 적용된 center source가 active source가 된다.
- active source는 `ActiveSlotIndex` 위치로 window를 다시 적용한다.
- active 적용이 끝나면 `OnActivePreviewChangeFinished`가 `FVdjmRecordMediaPreviewActivePayload`를 broadcast한다.
- Widget이 window를 적용하는 동안 manager의 `bAutoStartCenterPreview`를 잠시 끄고, Widget 쪽 완료 처리에서만 preview start/delegate broadcast를 수행한다.
- Preview input 좌표는 Android touch 좌표를 먼저 읽고, mouse/Slate cursor를 fallback으로 사용한다. Swipe가 안 잡히면 `PreviewInput Pressed/Released` 로그의 position delta를 먼저 확인한다.
- `UVdjmRecordMediaPreviewWidget`은 기본값 `bUseDirectPointerInput=true`로 `UButton` delegate 대신 widget의 touch/mouse press/release 좌표를 직접 기록한다. `InputButton`은 이때 `SelfHitTestInvisible`로 둬서 card 전체 swipe 좌표가 stale cursor 값으로 덮이지 않게 한다.
- Android에서 `NativeOnTouchEnded`가 누락되는 경우를 대비해 card tick에서 `GetInputTouchState`를 확인하고, touch가 더 이상 pressed가 아니면 마지막 유효 좌표로 `Released`를 확정한다. 이때 `PreviewInput TouchReleaseFallback` 로그가 찍힌다.
- Swipe는 `BeginSwipeInterpolation -> NativeTick transient offset 보간 -> EndSwipeInterpolation commit` 흐름이다. `bAnimateSwipe`, `SwipeAnimationDuration`, `SwipeAnimationEaseExponent`는 CarouselWidget 옵션이다.
- move는 `OnCarouselMoveStarted/OnCarouselMoveFinished`로 BP에 드러낸다. 즉시 이동과 swipe 이동 모두 같은 move delegate를 탄다.
- 현재 안정화 기준 layout은 curve를 빼고 line-first로 둔다. `XAxis` 방향으로 먼저 퍼지고, `ZAxis`/scale/opacity는 거리 기반 depth 표현에만 사용한다.

### Empty State

빈 registry/manifest 상태를 BP에서 처리하기 위해 아래 함수가 추가되었다.

- `UVdjmRecordMediaPreviewAreaRule::GetActivePreviewItemCount()`
- `UVdjmRecordMediaPreviewAreaRule::HasAnyActivePreviewItem()`
- `UVdjmRecordMediaPreviewAreaRule::IsEmptyLayout()`
- `UVdjmRecordMediaPreviewCarouselWidget::GetActivePreviewItemCount()`
- `UVdjmRecordMediaPreviewCarouselWidget::IsPreviewCarouselEmpty()`
- `UVdjmRecordMediaPreviewCarouselWidget::ApplyEmptyStateVisibility(EmptyWidget, ContentWidget)`

## 최근 검증 상태

- Win64 Editor UBT build 성공.

```text
Build.bat VdjmBg1LabEditor Win64 Development -Project="G:\Project\00Main\bg1LAb\VdjmBg1Lab\VdjmBg1Lab.uproject" -WaitMutex
Result: Succeeded
```

- 2026-05-11 Carousel Widget active 정책 추가 후 Win64 Editor UBT build 성공.
- Carousel XYZ/active 리워크는 아직 모바일 실기기 UI 확인이 필요하다.
- 모바일에서 확인할 항목:
  - card가 `PreviewWidgetHostPanel` 중앙 기준으로 시작하는가
  - 좌우 카드 간격이 의도와 맞는가
  - scale/opacity/ZOrder가 자연스러운가
  - click한 카드가 active source가 되어 기준 slot으로 이동하는가
  - swipe가 실기기 press/release delta로 안정적으로 잡히는가
  - `OnActivePreviewChangeFinished` payload의 source/slot/widget 값이 BP에서 기대대로 들어오는가
  - empty registry일 때 empty UI 전환이 되는가
  - SafeZone/DPI/세로 해상도에서 위치가 깨지지 않는가

## 2026-05-11 실기기 테스트 결과

사용자가 Android 실기기에서 preview gallery/carousel 동작을 확인했다. 결과는 다음과 같다.

1. Carousel card 위치는 오른쪽으로 `0, 1, 2...` 계속 추가되는 형태로 보인다. 위치 기준 자체는 잡힌 듯하지만, 의도한 "center active card를 중심으로 window가 움직이는 carousel"이라기보다 가로 슬롯 나열에 가깝다.
2. 새로 녹화한 영상이 preview gallery에 바로 반영되지 않는다. 녹화 완료 후 `MetadataStore/Registry/AppState/PreviewManager/Carousel` refresh chain이 현재 화면에 즉시 연결되지 않은 상태로 본다.
3. Android 갤러리에서 영상을 삭제해도 앱 preview track은 여전히 남아 있다. 현재 preview source of truth가 Android Gallery/MediaStore DB가 아니라 앱 local manifest/registry 기반이라 생기는 결과로 보인다.
4. 위 3번은 당장 버그라고 단정하지 않는다. 의도된 구조일 수 있으므로, 추후 Media DB/MediaStore와 앱 manifest/registry를 어떻게 동기화할지 별도 정책으로 설계해야 한다.
5. preview video는 화면 진입 시 자동 로딩/재생되지 않고, card를 한 번 클릭해야 재생된다. 클릭한 영상만 preview되는 점은 좋지만, 현재 보는 center/active card는 gallery 진입 또는 refresh 완료 후 자동 preview되어야 한다.

### 2026-05-11 코드 확인 메모

- `AVdjmRecordMediaPreviewManagerActor::ApplyCarouselWindow()`는 `sourceRegistryIndex = CenterSourceIndex - ActiveSlotIndex + SlotIndex`로 active slot에 center source를 넣는 구조다.
- `UVdjmRecordMediaPreviewAreaRule::ArrangeItems()`는 `SlotIndex - ActiveSlotIndex` 기준으로 render offset을 계산하므로, active slot 자체는 0 offset/center가 될 수 있다.
- `bAutoStartCenterPreview`는 manager init request와 manager config에 있으나, widget 등록/slot binding 타이밍과 맞지 않으면 `StartPreviewSlot(ActiveSlotIndex)`가 실제 preview를 시작하지 못할 수 있다.
- `InitializePreviewManagerFromDisk(false)`는 이미 initialized 상태에서 no-op 성공이 될 수 있으므로, 녹화 직후나 수동 refresh는 force refresh 경로를 명확히 써야 한다.
- force refresh는 자주 돌리는 기능이 아니다. 앱 시작/초기 gallery 진입, 녹화 완료 후 gallery 이동, 명시 수동 refresh처럼 registry source가 바뀔 수 있는 경계에서만 쓴다.
- carousel next/previous는 registry를 다시 읽지 않고, 현재 manager가 들고 있는 registry snapshot 위에서 active center/window만 이동해야 한다.

### 다음 수정 우선순위

1. 녹화 완료 후 현재 gallery 화면을 다시 열거나 앱을 재시작하지 않아도 preview gallery가 갱신되게 한다.
   - 후보 경로: record finalize complete -> metadata store registry refresh -> preview manager force refresh -> carousel refresh.
   - UI 버튼용 수동 refresh와 녹화 완료 후 자동 refresh를 분리하되, 내부 호출 경로는 공유한다.
2. Carousel을 단순 우측 나열이 아니라 center active card/window 방식으로 보정한다.
   - `ActiveSlotIndex`가 화면 중심 slot이 되도록 확인한다.
   - `ApplyCarouselWindow(centerSourceIndex, slotCount, activeSlotIndex)` 이후 active slot이 center에 오는지 검증한다.
   - slide/refresh 시 source registry index와 slot index의 관계를 다시 확인한다.
3. Carousel active 이동 옵션을 `CarouselWidget`에서 설정 가능하게 만든다.
   - 새 enum을 늘리지 않고 `bLoopCarousel` 같은 최소 bool 설정으로 시작한다.
   - 기본은 끝에 도달하면 멈추는 slide-window/clamp 방식으로 둔다.
   - `bLoopCarousel=true`이면 source index를 wrap해서 무한 carousel처럼 동작하게 한다.
   - 이 설정은 refresh 시 registry 재스캔 여부가 아니라 active center index 이동 방식과 window source index resolve 방식만 결정한다.
4. `bAutoStartCenterPreview`가 refresh 완료 후 center card에 대해 실제로 preview를 시작하게 한다.
   - slot binding이 끝난 뒤 center preview를 시작해야 한다.
   - media player start가 card click event에만 묶여 있지 않은지 확인한다.
5. Card action 중앙관리는 `UVdjmRecordMediaPreviewCarouselPolicy`에서 처리한다.
   - `InputButton`은 계속 card click/press/release trigger 역할을 한다.
   - preview widget은 입력 위치를 payload에 실어 manager로 올리고, carousel policy가 `Pressed/Released/Clicked`를 해석한다.
   - swipe는 우선 press/release screen delta로 판정한다. live drag follow animation은 후속 단계로 분리할 수 있다.
6. Gallery/MediaStore 삭제 동기화는 이번 작업 범위에서 제외한다.
   - 현재는 앱 local manifest/registry를 preview source of truth로 유지한다.
   - MediaStore/Media DB와의 reconcile은 추후 서버 저장/업로드 정책과 함께 다시 설계한다.

## 현재까지 큰 흐름

1. EventFlow 기반을 만들어 BridgeActor 초기화와 widget attach를 외부 flow로 제어하도록 정리했다.
2. RuntimeSlot/Context/Signal/Subgraph 개념을 도입했다.
3. DataAsset JSON export/import/editor toolkit 초안을 추가했다.
4. Android 녹화 경로, MediaStore publish, manifest/metadata store를 붙였다.
5. AppState와 MediaRegistry/TOC 기반을 만들었다.
6. PreviewManagerActor와 Carousel/Probe/Screen widget 계층을 만들었다.
7. Carousel 배치가 단순 2D line/curve에서 center-origin virtual XYZ projection으로 바뀌었다.

## 다음 작업 후보

### P0: 모바일 확인

- Android package/install 후 gallery lobby 화면에서 carousel 위치를 확인한다.
- 문제 발생 시 `LayoutSettings` 값을 먼저 조정한다.
- 그래도 구조 문제가 있으면 `AreaRule::ArrangeItems()`의 projection 계산을 본다.

### P1: Gesture Owner

- swipe/touch gesture owner를 별도 객체 또는 screen owner 쪽에 둔다.
- AreaRule은 gesture를 직접 알지 않고 `BeginInteraction`, `SetTransientOffset`, `MoveCenterBy`, `CommitCenterMove`, `CancelCenterMove`, `EndInteraction`만 받는다.

### P1: Gallery State

- 현재 선택된 preview index, center source index, 마지막 화면 상태를 AppState에 저장/복원한다.
- 앱을 나갔다 들어와도 이전 gallery 상태로 복원하는 방향이다.

### P1: Recorder UI 연결

- lobby에서 `open-recoder` signal 또는 subgraph helper로 recorder subgraph를 실행한다.
- recorder 화면은 `recoder-lobby` context key로 재사용한다.
- recorder loading은 bridge chain init progress와 연결한다.

### P2: Server/Upload

- 로컬/Tailscale 테스트 서버는 가능하다.
- 장기적으로는 GCP Cloud Storage + signed URL + CDN 또는 serverless API 구조를 검토한다.
- 앱 내부 manifest의 `publication`과 cloud locator 정책을 분리한다.

## 주의할 점

- `recoder` 오타 tag가 일부 DataAsset에 들어가 있을 수 있다. 당장 바꾸면 existing asset flow가 깨질 수 있으므로 주의한다.
- `RemoveFromParent`는 construct 재호출과 재사용 정책에 영향을 준다. 반복 사용 UI는 `Collapsed/Visible` 재사용을 우선한다.
- `ContextKey`는 weak reference다. 등록된 UObject가 GC되면 다시 ensure/register해야 한다.
- `PreviewWidgetHostPanel`이 `Overlay`가 아니면 ZOrder 처리 결과가 달라질 수 있다.
- DataAsset에 저장된 `LayoutSettings`는 새 필드 기본값이 바로 반영되지 않을 수 있다. BP/DataAsset을 열어 값 확인 후 저장하는 것이 안전하다.
- 워크트리는 dirty일 수 있으며, 이전 변경을 되돌리지 않는다.

## 관련 주요 파일

- `Source/VdjmRecorder/Public/VdjmEvents/VdjmRecordEventNode.h`
- `Source/VdjmRecorder/Private/VdjmEvents/VdjmRecordEventNode.cpp`
- `Source/VdjmRecorder/Public/VdjmEvents/VdjmRecordEventManager.h`
- `Source/VdjmRecorder/Private/VdjmEvents/VdjmRecordEventManager.cpp`
- `Source/VdjmRecorder/Public/VdjmEvents/VdjmRecordEventFlowRuntime.h`
- `Source/VdjmRecorder/Private/VdjmEvents/VdjmRecordEventFlowRuntime.cpp`
- `Source/VdjmRecorder/Public/VdjmRecordMediaPreview.h`
- `Source/VdjmRecorder/Private/VdjmRecordMediaPreview.cpp`
- `Source/VdjmRecorder/Public/VdjmRecordAppState.h`
- `Source/VdjmRecorder/Private/VdjmRecordAppState.cpp`
- `Source/VdjmRecorder/Public/VdjmRecorderController.h`
- `Source/VdjmRecorder/Private/VdjmRecorderController.cpp`
- `Source/VdjmRecorder/Public/VdjmRecorderCore.h`
- `Source/VdjmRecorder/Private/VdjmRecorderCore.cpp`
- `Source/VdjmRecorderEditor/`
