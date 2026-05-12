# VdjmRecord Media Preview Guide

> 2026-05-12 기준: 이 문서는 legacy `VdjmRecordMediaPreview*` 계층의 기록/참고 문서다. 실패한 carousel 실험(`AreaRule`, `CarouselPolicy`, `SwipeMotionObject`, `ScreenWidget`, debug actor)은 코드에서 제거되었다. 새 gallery/carousel UX는 `VdjmWidgets` 모듈에서 다시 설계한다. 현재 이 문서에서 바로 사용 가능한 부분은 `PreviewManagerActor`의 registry/init 관리, `PreviewProbeWidget`, 기본 `PreviewWidget` 중심이다.

## 문서 목적
- 이 문서는 저장된 녹화 결과를 앱 안에서 다시 불러와 preview/gallery UI로 보여주는 방법을 설명한다.
- 목표는 BP 작업자가 C++ 내부를 전부 읽지 않아도 `어떤 위젯을 상속하고`, `무엇을 bind 하고`, `어떤 함수로 refresh 하는지` 알 수 있게 하는 것이다.
- 현재 preview 계층은 BridgeActor와 분리되어 있다. 녹화가 이미 끝난 media/manifest/registry를 읽어 화면에 보여주는 쪽이 기본 목적이다.

## 전체 개념

```text
녹화 완료
-> Artifact/Manifest 생성
-> MetadataStore registry 갱신
-> Android MediaStore publish
-> 앱 시작 또는 gallery 진입
-> AppState 로드
-> MetadataStore registry refresh
-> PreviewProbe 또는 PreviewCarousel이 RegistryEntry를 MediaPlayer로 연다
```

## 어떤 것을 쓰면 되는가

| 목적 | 사용할 클래스 | 설명 |
| --- | --- | --- |
| media open 단독 검증 | `UVdjmRecordMediaPreviewProbeWidget` | 저장된 registry entry를 하나 찾아서 `Image`에 바로 띄운다. |
| 카드 하나의 외형 제작 | `UVdjmRecordMediaPreviewWidget` | 하나의 media preview 카드다. BP에서 외형을 만든다. |
| 여러 카드를 carousel로 표시 | `UVdjmRecordMediaPreviewCarouselWidget` | preview widget을 자동 생성하고 slot/window를 관리한다. |
| SafeZone/Header/Footer 포함 화면 | `UVdjmRecordMediaPreviewScreenWidget` | 갤러리 화면 템플릿이다. |
| preview 배치 규칙 | `UVdjmRecordMediaPreviewAreaRule` | 자료구조와 layout rule이다. touch/swipe를 직접 처리하지 않는다. |

## 빠른 테스트: Probe Widget

### 목적
- 실기기에서 `MetadataStore -> RegistryEntry -> MediaPlayer -> MediaTexture -> Image` 경로가 되는지 확인한다.
- EventFlow, BridgeActor, Carousel 없이 media open만 검증한다.

### BP 만드는 법
1. Widget Blueprint를 만들고 parent class를 `UVdjmRecordMediaPreviewProbeWidget`으로 설정한다.
2. Image를 하나 만들고 이름을 `PreviewImage`로 둔다.
3. 옵션을 다음처럼 둔다.
   - `bAutoOpenOnConstruct = true`
   - `bUseLatestRegistryEntry = true`
   - `RegistryEntryIndex = -1`
4. 화면에 이 위젯을 붙인다.
5. 녹화가 하나 이상 존재하면 최신 registry entry를 찾아 자동으로 연다.

### 주요 함수
- `RefreshAndOpenLatestPreview`
  - registry를 refresh하고 최신 항목을 연다.
- `RefreshAndOpenPreviewAtIndex`
  - registry index를 지정해서 연다.
- `OpenPreviewFromRegistryEntry`
  - 이미 확보한 registry entry를 직접 연다.
- `StopProbePreview`
  - probe preview 재생을 멈춘다.

### 주의점
- Probe는 검증용 독립 위젯이다. 실제 gallery 화면을 최종 구성할 때는 `ScreenWidget + CarouselWidget + PreviewWidget` 조합을 기본으로 본다.
- `PreviewImage`를 bind하지 않으면 내부에서 기본 image를 만들 수 있지만, 실제 UI에서는 직접 bind하는 편이 좋다.

## 실제 Gallery 화면 구성

### 권장 위젯 계층

```text
WBP_RecordGalleryScreen : UVdjmRecordMediaPreviewScreenWidget
- SafeZone_Root
  - Overlay_Root
    - Background / Blur / Dim
    - VerticalBox_Main
      - HeaderArea
        - Title / Back / Refresh
      - SizeBox_CarouselArea
        - WBP_RecordMediaPreviewCarousel : UVdjmRecordMediaPreviewCarouselWidget
          - PreviewWidgetHostPanel
      - FooterArea
        - RecordId / Date / Edit / Play / Delete
```

### ScreenWidget bind 이름

| Bind 이름 | 타입 예시 | 목적 |
| --- | --- | --- |
| `SafeZone_Root` | `USafeZone` | 모바일 safe area 대응 |
| `Overlay_Root` | `UOverlay` | 배경, dim, main layer를 쌓는 root |
| `VerticalBox_Main` | `UVerticalBox` | header/carousel/footer 세로 배치 |
| `HeaderArea` | `UWidget` | 제목, back, refresh 영역 |
| `SizeBox_CarouselArea` | `USizeBox` | carousel 영역 높이 제어 |
| `Overlay_CarouselArea` | `UOverlay` | carousel 주변 fixed layer를 둘 때 사용 |
| `FixedBackLayer` | `UOverlay` | preview 뒤쪽 고정 UI |
| `PreviewWidgetHostPanel` | `UPanelWidget` | preview 카드들이 들어가는 panel |
| `FixedFrontLayer` | `UOverlay` | preview 앞쪽 고정 UI |
| `FooterArea` | `UWidget` | edit/play/delete 등 action 영역 |
| `CarouselWidget` | `UVdjmRecordMediaPreviewCarouselWidget` | 실제 carousel 동작 담당 |

### CarouselWidget bind 이름

| Bind 이름 | 타입 예시 | 목적 |
| --- | --- | --- |
| `PreviewWidgetHostPanel` | `UOverlay` 권장 | preview card를 자동 생성해서 넣는 공간 |

`PreviewWidgetHostPanel`은 `UOverlay`를 권장한다. 현재 ZOrder 처리는 overlay child re-add 방식으로 맞추는 경로가 있으므로, 다른 panel을 쓰면 겹침 순서가 의도와 다를 수 있다.

### Preview card bind 이름

| Bind 이름 | 타입 예시 | 목적 |
| --- | --- | --- |
| `PreviewOverlay` | `UOverlay` | 이미지와 입력 레이어를 쌓는 root |
| `PreviewImage` | `UImage` | `UMediaTexture` brush가 들어가는 실제 image |
| `InputButton` | `UButton` | 투명 버튼을 얹어 hover/click/touch를 상위로 전달 |

## BP 설정 순서

1. `WBP_RecordPreviewCard`를 만든다.
   - Parent: `UVdjmRecordMediaPreviewWidget`
   - `PreviewImage`를 반드시 만든다.
   - 필요하면 `InputButton`을 투명하게 덮는다.
2. `WBP_RecordMediaPreviewCarousel`을 만든다.
   - Parent: `UVdjmRecordMediaPreviewCarouselWidget`
   - `PreviewWidgetHostPanel`을 `Overlay`로 만든다.
   - `PreviewWidgetClass`에 `WBP_RecordPreviewCard`를 넣는다.
3. `WBP_RecordGalleryScreen`을 만든다.
   - Parent: `UVdjmRecordMediaPreviewScreenWidget`
   - 위 권장 계층대로 배치한다.
   - `CarouselWidget`에 `WBP_RecordMediaPreviewCarousel` 인스턴스를 넣는다.
4. 자동 refresh 옵션은 기본적으로 꺼둔다.
   - `CarouselWidget.bRefreshStoreOnConstruct = false`
   - `ScreenWidget.TemplateOptions.bAutoRefreshPreviewOnConstruct = false`
5. 화면 소유자 또는 EventFlow가 준비된 타이밍에 `RefreshPreviewScreen` 또는 carousel의 refresh 함수를 명시 호출한다.

`RefreshCarousel`은 기본적으로 manager의 초기화 완료 상태를 확인하고, 이미 초기화되어 있으면 registry 재스캔 없이 현재 manager 상태를 화면에 동기화한다. 강제로 다시 읽어야 하는 버튼이나 녹화 직후 갱신에서는 `RefreshCarouselWithPolicy(true, ErrorReason)` 또는 `InitializeMediaPreviewManager(bForceRefresh=true)`를 먼저 사용한다.

force refresh는 다음 경계에서만 사용한다.

- 앱 시작 또는 최초 gallery 초기화처럼 registry snapshot을 처음 구성할 때
- 녹화 완료 후 gallery로 이동해서 방금 생성된 manifest/registry entry를 반영해야 할 때
- 사용자가 refresh 버튼을 명시적으로 눌렀을 때

carousel next/previous, active card 변경, preview click은 force refresh를 사용하지 않는다. 이 동작들은 이미 로드된 registry snapshot 위에서 active center/window만 바꾼다.

## EventFlow에서 Preview Manager를 다루는 방법

Preview 쪽도 BridgeActor와 같은 원칙을 따른다. 객체 생성/등록과 실제 초기화/refresh를 분리한다.

권장 순서:

```text
LoadAppState
-> EnsureMediaPreviewManager
-> CreateWidget(GalleryScreen 또는 LoadingScreen)
-> InitializeMediaPreviewManager
-> RefreshPreviewScreen 또는 Carousel.RefreshCarousel
```

### `UVdjmRecordEventEnsureMediaPreviewManagerNode`
- `AVdjmRecordMediaPreviewManagerActor`를 찾거나 스폰한다.
- 기본 runtime slot은 `media-preview-manager`다.
- 기본 world context key는 `MediaPreviewManager`다.
- registry refresh는 하지 않는다. 오직 생성/등록만 담당한다.

### `UVdjmRecordEventInitializeMediaPreviewManagerNode`
- runtime slot 또는 world에서 PreviewManager를 찾아 명시 초기화한다.
- EventFlow 안에서는 한 번에 끝내지 않고 `ERunning`을 반환하면서 manager의 자동 init chain 완료를 기다린다.
- manager가 이미 초기화되어 있고 `bForceRefresh = false`이면 no-op 성공으로 끝난다.
- `bForceRefresh = true`이면 registry를 다시 디스크에서 읽는다.
- `bApplyCarouselWindowAfterInit = true`이면 `SlotCount`, `ActiveSlotIndex`, `InitialCenterSourceIndex` 기준으로 window를 적용한다.
- registry가 비어있는 상태도 첫 실행에서는 정상일 수 있으므로 기본은 `bSucceedWithEmptyRegistry = true`다.
- `MaxManifestFilesPerStep`은 `ERefreshRegistry` 단계에서 한 번에 읽고 등록할 manifest JSON 파일 수다.
- `MaxRegistryEntryStateChecksPerStep`은 registry entry의 파일 존재 상태를 한 번에 확인할 수다.
- `MaxRegistryEntriesPerStep`은 `ECopyRegistryEntries` 단계에서 한 번에 복사할 registry entry 수다. 녹화 결과가 많을수록 값을 낮춰 로딩 UI가 여러 frame에 걸쳐 반응할 수 있게 한다.

### Registry scan chunk 구조
- `UVdjmRecordMetadataStore`는 `StartRegistryScanFromDisk`와 `AdvanceRegistryScanFromDisk`로 manifest scan을 단계적으로 진행한다.
- 파일 목록 수집은 UE 파일 API 특성상 시작 시점에 한 번 수행한다.
- 무거운 작업인 manifest JSON 파싱, registry entry upsert, 파일 존재 상태 확인은 설정된 step 크기만큼 나눠 처리한다.
- 기존 `RefreshRegistryFromDisk`는 호환용 동기 wrapper로 유지된다. 새 EventFlow 로딩 UX에서는 `LoadAppState`와 `InitializeMediaPreviewManager`가 scan runner를 사용해 `ERunning`으로 흐름을 잠시 잡는다.
- `SaveRegistry`는 scan 완료 시점에 한 번 수행한다. 지금은 안정성을 위해 GameThread 동기 저장으로 두고, registry JSON이 더 커지면 별도 save queue로 분리한다.

### `UVdjmRecordEventLoadAppStateNode`
- AppState JSON을 로드하고 필요하면 records TOC를 갱신한다.
- `bRefreshRecordsToc = true`이면 MetadataStore registry scan을 chunk 방식으로 진행하고, scan 완료 후 `RefreshRecordsTocFromMetadataStore`를 호출한다.
- 이 노드도 scan 중에는 `ERunning`을 반환하므로 로딩 위젯은 EventFlow가 멈춘 것이 아니라 AppState/registry TOC를 준비 중이라고 보면 된다.
- `MaxManifestFilesPerStep`, `MaxRegistryEntryStateChecksPerStep`으로 AppState 단계의 scan 부하를 조절한다.

### PreviewManager init 상태 연결
- Loading UI는 `media-preview-manager` runtime slot 또는 world context의 `MediaPreviewManager`에서 manager를 가져온다.
- `StartPreviewManagerInit`는 초기화 작업을 시작만 한다. actor tick이 자동으로 `AdvancePreviewManagerInitStep`을 이어가지는 않는다.
- EventFlow에서는 `InitializeMediaPreviewManagerNode`가 실행될 때마다 `AdvancePreviewManagerInitStep`을 한 번씩 호출해 `ERunning` 흐름으로 loading UI를 유지한다.
- BP에서 직접 제어하려면 timer, widget tick, flow tick 같은 명시 호출 경계에서 `AdvancePreviewManagerInitStep`을 반복 호출한다.
- 즉 loading widget은 delegate/progress를 읽고, 실제 진행 타이밍은 EventFlow 또는 화면 owner가 컨트롤한다.
- `OnPreviewManagerInitStarted`는 preview init 시작 시 호출된다.
- `OnPreviewManagerInitStepChanged`는 `EEnsureMetadataStore`, `ERefreshRegistry`, `ECopyRegistryEntries`, `EApplyCarouselWindow`, `EFinalizeInitialization`, `EComplete` 같은 단계 변화 또는 entry copy 진행 때 호출된다.
- 진행률 UI는 `GetPreviewManagerInitProgress`, `GetPreviewManagerInitProcessedCount`, `GetPreviewManagerInitPendingCount`, `GetCurrentPreviewManagerInitStep`를 읽는다.
- `OnPreviewManagerInitFinished`에서 성공/실패와 에러 메시지를 받아 로딩창을 닫거나 에러 UI로 전환한다.

이 구조 덕분에 같은 화면을 다시 들어가도 매번 무거운 init를 반복하지 않고, 명시 refresh 버튼이나 녹화 완료 후 갱신이 필요할 때만 강제 refresh할 수 있다.

## 주요 트리거

### 녹화 완료 후
- `MetadataStore`가 manifest/registry를 갱신한다.
- MediaStore publish가 성공하면 registry entry의 publication 정보가 채워진다.
- gallery 화면은 이 registry를 다시 읽어 preview를 구성한다.

### Gallery 화면 진입
- `AppState`를 먼저 읽어 endpoint, 마지막 preview index 같은 상위 상태를 복원한다.
- PreviewManager가 초기화 완료 상태인지 확인한다.
- 이미 초기화되어 있으면 기본적으로 refresh를 건너뛴다.
- 앱 시작/첫 진입, 녹화 완료 직후 이동, 수동 refresh처럼 registry source가 바뀔 수 있는 경우에만 `bForceRefresh` 또는 화면의 refresh 버튼으로 `MetadataStore`를 다시 refresh한다.
- Carousel이 registry entry 수에 맞춰 preview widget을 만들거나 재사용한다.
- AreaRule이 slot 위치, scale, opacity, ZOrder를 계산한다.

### Carousel active/window 이동
- Active card는 `ActiveSlotIndex`에 배치된 center source entry다.
- `SlideNext`/`SlidePrevious`는 registry를 다시 읽지 않고 `CenterSourceIndex`와 slot window만 변경한다.
- 기본은 끝에 도달하면 더 움직이지 않는 slide-window/clamp 방식이다.
- `CarouselWidget.bLoopCarousel = true`이면 source index를 wrap해서 무한 carousel처럼 동작한다.
- `bLoopCarousel`은 layout 축, spacing, scale 같은 `LayoutSettings`와 분리한다.

### Carousel action policy
- `UVdjmRecordMediaPreviewCarouselPolicy`는 carousel 전용 입력/action 해석 객체다.
- Card의 `InputButton`은 계속 `Pressed`, `Released`, `Clicked`를 발생시키고, preview widget은 해당 action과 screen position을 `PreviewManager` notify payload로 올린다.
- Carousel은 manager의 `OnPreviewNotify`를 policy에 연결한다.
- Policy는 자기 carousel이 소유한 card action만 처리한다.
- `Pressed` 위치와 `Released` 위치의 delta가 threshold를 넘으면 swipe로 보고 `SlideNext` 또는 `SlidePrevious`를 호출한다.
- Swipe로 판정된 입력 뒤의 `Clicked`는 무시한다.
- Swipe가 아니면 card click으로 보고 해당 slot preview를 시작한다.
- 현재 구현은 release 시점에 한 칸 넘기는 방식이다. 손가락을 따라 실시간으로 움직이는 live drag animation은 별도 follow 단계에서 붙일 수 있다.

### Preview 클릭
- preview card는 선택 사실과 manifest/registry 정보를 상위 owner에게 알려주는 역할만 한다.
- 실제 edit/play/delete/upload는 gallery screen 또는 controller 계층에서 수행한다.

## AreaRule의 역할

`UVdjmRecordMediaPreviewAreaRule`은 carousel의 자료구조와 배치 알고리즘이다. 직접 손가락 입력이나 애니메이션을 처리하지 않는다.

### 가상 XYZ 좌표계

AreaRule은 실제 3D 위젯을 만들지 않는다. 대신 부모 panel의 center를 원점으로 보고, 각 preview card를 가상 좌표 `X/Y/Z`로 계산한 뒤 UMG의 2D 표현으로 투영한다.

- `X`는 기본적으로 좌우 slot 위치다.
- `Y`는 곡선 배치나 위/아래 흐름에 쓰는 보조 축이다.
- `Z`는 외부에서는 깊이처럼 보이지만 내부에서는 priority에 가깝다. 기본적으로 scale, opacity, ZOrder 계산에 반영된다.
- `XAxis`, `YAxis`, `ZAxis`는 가상 좌표를 화면 2D offset으로 바꾸는 축 벡터다.
- `RenderTranslation`은 부모 panel center 기준 offset이다. `PreviewWidgetHostPanel`을 `Overlay`로 두고 child slot을 center alignment로 쓰는 것을 기본으로 한다.

기본 모양은 `XAxis=(1,0)`, `YAxis=(0,1)`, `ZAxis=(0,0)`이다. 즉 카드는 좌우로 벌어지고, 곡선은 아래 방향으로 휘며, `Z`는 화면 이동보다 깊이/우선순위 표현에 먼저 쓰인다. `ZAxis`를 `(0,-1)` 같은 방향으로 주면 멀어진 카드가 위쪽/뒤쪽으로 들어가는 느낌을 만들 수 있다.

주요 기능:
- preview/fixed item 추가
- item hide/show/delete/lower/lock/unlock
- center 이동 예약/커밋/취소
- transient offset 적용
- layout 결과 계산
- rule state export/import
- 빈 상태 판단

swipe gesture owner가 생기면 보통 다음 순서로 호출한다.

```text
BeginInteraction
-> SetTransientOffset
-> MoveCenterBy
-> CommitCenterMove 또는 CancelCenterMove
-> EndInteraction
```

### 빈 상태 처리

registry/manifest가 하나도 없거나 preview slot이 모두 empty이면 gallery는 별도 empty UI를 보여주는 편이 좋다.

- `UVdjmRecordMediaPreviewAreaRule::IsEmptyLayout()`은 활성 preview item이 없으면 true다.
- `UVdjmRecordMediaPreviewAreaRule::GetActivePreviewItemCount()`는 현재 표시 가능한 preview item 수를 반환한다.
- `UVdjmRecordMediaPreviewCarouselWidget::IsPreviewCarouselEmpty()`는 BP에서 바로 쓸 수 있는 carousel 단축 함수다.
- `UVdjmRecordMediaPreviewCarouselWidget::ApplyEmptyStateVisibility(EmptyWidget, ContentWidget)`을 호출하면 empty widget과 content widget의 visibility를 한 번에 전환한다.

## 확장 규칙

- Preview는 BridgeActor를 직접 찾지 않는다. 저장된 media를 보는 기능은 녹화 실행체와 분리한다.
- 개별 preview widget에 upload/delete/edit 로직을 많이 넣지 않는다. preview는 표시와 입력 notify에 집중한다.
- media의 진실은 manifest/registry에 둔다. AppState는 현재 화면 상태와 서비스 설정 같은 상위 상태만 맡는다.
- 새로운 배치 방식은 `AreaRule` 파생 또는 교체로 처리한다.
- 새로운 입력 방식은 gesture owner나 screen owner에서 처리하고, AreaRule에는 이동 요청만 보낸다.
- UI 표시용 문자열과 실제 상태 값은 가능하면 분리한다.

## 주의점

- Android의 내부 파일 경로와 content uri는 기기 정책에 영향을 받는다. ProbeWidget으로 먼저 확인한다.
- Carousel/Screen의 Construct 자동 refresh는 기본 비활성화되어 있다. 화면 소유자나 EventFlow에서 명시 호출하는 것을 기본으로 한다.
- `PreviewWidgetHostPanel`이 overlay가 아니면 ZOrder 처리 결과가 다를 수 있다.
- manifest가 없는 영상은 앱 콘텐츠로 취급하지 않는 편이 안전하다.
- gallery가 registry를 갱신하지 않으면 방금 녹화한 media가 바로 보이지 않을 수 있다.
- preview 재생은 원본 media의 특정 구간 반복을 기본으로 보며, 별도 preview clip 파일은 추후 정책으로 분리할 수 있다.

## 지금 테스트할 것

1. 녹화를 1개 이상 만든다.
2. `UVdjmRecordMediaPreviewProbeWidget` 기반 BP를 띄운다.
3. 최신 media가 image에 뜨고 반복 재생되는지 본다.
4. `WBP_RecordGalleryScreen`을 만든다.
5. `RefreshPreviewScreen` 호출 후 preview card들이 생성되는지 본다.
6. AppState 저장/복원은 다음 단계에서 붙인다.

## 다음 구현 후보

- carousel touch/swipe gesture owner
- preview 선택 상태를 AppState에 저장/복원
- registry item delete/hide/upload state 관리
- upload 완료 후 local retention/delete 정책
- manifest 기반 edit 진입과 recorder option 복원
