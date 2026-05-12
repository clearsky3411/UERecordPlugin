# VdjmWidgets Carousel Transition

## 목적

이 문서는 기존 `VdjmRecordMediaPreview*` carousel 실험을 중단하고, 새 `VdjmWidgets` carousel을 완성한 뒤 어떻게 사용할지 정리한다.

`PreviewManager` 초기화, loading widget, EventFlow 진행 방식은 기존 구조를 유지한다. 새 carousel 작업은 card 생성, card 상태, layout, input, swipe, active/visible/hidden/empty 표시 정책에만 집중한다.

## 왜 다시 만드는가

기존 legacy carousel 실험에서 확인된 문제는 다음과 같다.

- card가 center active carousel이 아니라 한쪽 방향으로 계속 쌓이는 line list처럼 보였다.
- visible card 개수와 registry 개수가 맞지 않거나, 실제 2개만 있어도 5개 slot이 표시되는 혼동이 있었다.
- screen 재진입 또는 refresh 후 위치가 흔들리고, 수동 refresh를 해야 돌아오는 경우가 있었다.
- active card만 영상 preview가 시작되고, visible sub card는 첫 프레임/썸네일 상태가 명확하지 않았다.
- swipe 입력이 button, touch release, panel hierarchy, mobile UMG input 처리와 엮이며 안정적이지 않았다.
- layout, input, swipe, media preview, registry refresh 책임이 한 legacy widget/file 안에 섞이면서 수정할수록 파편화가 커졌다.
- 실기기 packaging/test 주기가 길어 추측성 수정이 반복되면 비용이 너무 컸다.

따라서 새 carousel은 처음부터 책임 경계를 나눈다. Carousel은 관리하고, Card는 자기 상태와 media 표시를 처리한다.

## 건드리지 않는 것

다음 영역은 carousel 재작성 범위가 아니다.

- `StartPreviewManagerInit`, `AdvancePreviewManagerInitStep`, preview manager init/loading 흐름
- recorder bridge init loading widget
- EventFlow의 flow/session/signal 실행 방식
- metadata manifest/registry schema
- Android MediaStore 삭제와 앱 registry 삭제/동기화 정책
- server upload, cloud storage, retention policy

새 carousel은 기존에 준비된 registry/manifest 목록을 받아 화면에 배치하고 조작하는 UI 계층이다.

## 완성 기준

새 carousel 1차 완료 기준은 다음과 같다.

- refresh 시 manifest/registry snapshot을 읽고 card model 목록을 만든다.
- registry 개수에 맞춰 `Empty`, `Single`, `Pair`, `Window`, `Overflow` 같은 layout state를 선택한다.
- Carousel이 card widget을 생성, 재사용, 숨김, 파괴한다.
- Card는 `Active`, `Visible`, `Hidden`, `Empty`, `Waiting`, `Error` 상태 전환 API를 가진다.
- Active card는 loop preview를 담당한다.
- Visible card는 영상 재생이 아니라 첫 프레임 또는 thumbnail 표시 상태를 담당한다.
- Hidden card는 media resource를 멈추고 화면에서 빠진다.
- Empty card는 검은 화면이 아니라 placeholder UI를 표시한다.
- input/swipe는 carousel 소유 controller가 해석하고, card는 raw event 또는 card id만 전달한다.
- swipe/motion 계산은 carousel 전용 controller/policy가 처리한다.
- animation begin/update/end delegate를 제공해 BP가 transition animation을 붙일 수 있다.
- 기존 loading widget과 preview manager init 흐름을 변경하지 않는다.

## 완성 후 사용법

완성 후 BP/Flow 사용 흐름은 다음을 목표로 한다.

1. 기존 flow에서 `EnsureMediaPreviewManager`와 preview manager init/loading 흐름을 그대로 사용한다.
2. gallery screen을 열 때 새 `VdjmWidgets` screen 또는 owner widget을 생성한다.
3. screen 안에 새 carousel widget을 배치한다.
4. carousel은 preview manager 또는 source provider에서 registry snapshot을 읽는다.
5. screen open, gallery tab 진입, record artifact ready 이후 필요한 시점에 carousel refresh를 호출한다.
6. carousel refresh는 registry 재스캔이 아니라 현재 준비된 source snapshot을 card/window에 반영한다.
7. 강제 registry refresh가 필요하면 기존 preview manager/loading flow에서 먼저 처리하고, 완료 후 carousel refresh만 호출한다.

즉 사용자는 `PreviewManager 준비 -> Carousel Refresh -> Card State/Animation Delegate 바인딩` 순서로 쓴다.

## 기존 위젯 처리

새 carousel 완성 전까지 기존 객체는 다음처럼 취급한다.

- `AVdjmRecordMediaPreviewManagerActor`: 유지한다. registry/source 준비 경계로 사용한다.
- `UVdjmRecordMediaPreviewProbeWidget`: 유지한다. media open 단독 검증용이다.
- `UVdjmRecordMediaPreviewWidget`: legacy preview card로만 둔다. 새 carousel card의 기준으로 확장하지 않는다.
- `UVdjmRecordMediaPreviewCarouselWidget`: legacy slot/window 확인용으로만 둔다. 새 active/swipe/layout 기능을 더 추가하지 않는다.
- 기존 BP가 legacy widget을 상속 중이면 새 `VdjmWidgets` widget으로 교체할 때까지 보존한다.
- 새 gallery UX가 안정화되면 legacy carousel BP는 deprecated/fallback으로만 남기거나 제거 대상을 따로 정한다.

## 새 클래스 책임 초안

`UVdjmWidgetMediaCarouselWidget`

- card 생성/재사용/파괴를 총괄한다.
- refresh와 source snapshot 적용을 총괄한다.
- active source index와 visible window를 결정한다.
- layout, input, motion, state policy 객체를 소유한다.
- card 내부 media player를 직접 만지지 않고 card API만 호출한다.

`UVdjmWidgetMediaCardWidget`

- 자기 상태 전환을 소유한다.
- active preview loop, visible thumbnail/first-frame, hidden stop/release, empty placeholder 표시를 처리한다.
- carousel index/window/source 정책을 결정하지 않는다.
- registry refresh를 직접 하지 않는다.

`UVdjmWidgetMediaCarouselSource`

- preview manager 또는 다른 source에서 registry/manifest snapshot을 읽는다.
- card 생성, layout, swipe를 처리하지 않는다.

`UVdjmWidgetMediaCarouselCardPool`

- card widget 생성/재사용/숨김/파괴만 담당한다.
- 어떤 card가 active인지 결정하지 않는다.

`UVdjmWidgetMediaCarouselLayoutPolicy`

- count, active index, viewport, spacing option으로 slot transform을 계산한다.
- widget 생성이나 media 재생을 하지 않는다.

`UVdjmWidgetMediaCarouselStatePolicy`

- source/window/layout 결과를 card state target으로 변환한다.
- card 내부 구현에는 직접 접근하지 않는다.

`UVdjmWidgetMediaCarouselInputController`

- press, move, release, click, swipe action을 중앙 해석한다.
- registry refresh나 media playback을 직접 하지 않는다.

`UVdjmWidgetMediaCarouselMotionController`

- swipe offset, velocity, damping, snap target, lock state를 계산한다.
- 실제 card state 변경이나 source refresh를 하지 않는다.

## 다음 작업 순서

1. `VdjmWidgets` module skeleton을 만든다.
2. 새 carousel/card 책임과 option struct를 header 주석으로 고정한다.
3. `UVdjmWidgetMediaCardWidget` 상태 API를 먼저 만든다.
4. `UVdjmWidgetMediaCarouselWidget`이 card pool/source/layout/state/input/motion 객체를 소유하도록 skeleton을 만든다.
5. registry snapshot refresh와 line layout만 먼저 구현한다.
6. active/visible/hidden/empty 상태 적용을 붙인다.
7. swipe/motion/animation delegate를 붙인다.
8. 마지막에 사용법 문서를 BP 설정 순서까지 갱신한다.
