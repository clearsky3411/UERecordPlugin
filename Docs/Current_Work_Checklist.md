# Current Work Checklist

## 사용 목적
- 이 문서는 현재 작업 순서를 한눈에 보려는 용도의 짧은 체크리스트다.
- 상세 배경과 구조는 `Android_Recording_Audio_GIF_Plan.md`, `VdjmRecorder_Current_Architecture.md`를 본다.
- Preview/Gallery 사용 순서는 `VdjmRecordMediaPreview_Guide.md`를 본다.
- Metadata/Registry JSON 계약은 `VdjmRecordMetadataSchema_v1.md`를 본다.
- EventFlow DataAsset 전용 에디터 사용법은 `VdjmRecordEventFlowDataAssetEditor_Guide.md`를 본다.

## 0. 기반 상태
- [x] `EventManager` 분리
- [x] `FlowRuntime` 도입
- [x] JSON 실행 경로 통합
- [x] `FlowFragment` 기반 코드 preset 조립 경로 추가
- [x] `FlowFragment -> transient FlowDataAsset` 생성 경로 추가
- [x] 기본 코드 preset helper 확장
- [x] `RecorderController` 초안 존재
- [x] `RecorderStateObserver` 초안 존재
- [x] `EVdjmRecordEventResultType`를 `E*` 규칙으로 정리
- [x] 이벤트 관련 파일을 `VdjmEvents` 폴더로 정리
- [x] `EventManager` coarse session state 추가
- [x] `MetadataStore` 기반 manifest/registry 1차 추가
- [x] Android MediaStore publish queue 1차 추가
- [x] `AppState` 기반 앱 상태 저장소 1차 추가
- [x] `MediaPreview` probe/carousel/screen widget 1차 추가

## 1. 현재 P0
- [x] `StateMachine Observer`를 EventManager/세션 기준으로 재정의
- [x] `RecorderController` 옵션 메시지/큐 리뉴얼 1차
- [x] Resource Option Apply Layer 1차 구현
- [x] `FlowRuntime` mutation/fragment 1차 구현
- [x] `FlowFragmentWrapper` 에디터 preview 경로 추가
- [x] `Log` test event 추가
- [x] `EventFlowEntryPoint` 레벨 시동기 추가
- [x] `EventFlowEntryPoint` 중복 시작 방지 추가
- [x] `EventFlowEntryPoint` pre-start widget / startup context 파라미터 추가
- [x] UI에서 flow signal로 대기 노드를 깨우는 경로 검증
- [x] Android 녹화 결과가 갤러리에 노출되는 경로 검증
- [x] Preview probe widget으로 저장된 media를 다시 여는 경로 검증
- [ ] UI 엔트리 정리
- [ ] 비UI 객체가 UI를 몰라도 되도록 연결 구조 정리
- [ ] Preview gallery screen BP 구성 및 carousel 동작 검증

## 1-1. 다음 Event TODO
- [x] `SpawnRecordBridgeActorWait` event 추가
- [x] `CreateWidget/AddToViewport` event 1차 추가
- [x] `CreateWidget` signal/time 기반 자동 제거 정책 추가
- [x] `CreateObject` primitive 추가
- [x] `SpawnActor` primitive 추가
- [x] `RegisterContextEntry` primitive 추가
- [x] `RegisterWidgetContext` primitive 추가
- [x] `WaitForSignal` primitive 추가
- [x] `Delay` primitive 추가
- [x] `EmitSignal` primitive 추가
- [x] `RemoveWidget` primitive 추가
- [x] `CreateObjectAndRegisterContext` composite 추가
- [x] `SpawnActorAndRegisterContext` composite 추가
- [x] `CreateWidgetAndRegisterContext` composite 추가
- [x] `EventManager` flow state/runtime edge metadata 1차 추가
- [x] `Sequence` child flow owner 1차 반영
- [x] `WidgetBase` flow control helper 추가
- [x] `EventFlowBlueprintLibrary` 범용 BP flow control helper 추가
- [x] `StartSubgraphSession`으로 named subgraph를 별도 session으로 실행하는 경로 추가
- [x] `RegisterSubgraphSignalBranch`로 signal 기반 subgraph branch rule 등록 경로 추가
- [x] `BranchCases` 0번부터 검사하는 if/else-if/else 형태와 duplicate policy 1차 추가
- [x] `FlowDataAsset` 전용 editor 모듈/JSON/Summary/Guide 탭 1차 추가
- [ ] `JumpToNext`와 label/subgraph jump 정책 구체화
- [ ] intro widget -> loading signal -> bridge init wait -> recorder widget attach flow preset 추가
- [ ] flow catalog/name table 설계

## 2. Media/Preview 현재 P1
- [x] `UVdjmRecordMediaPreviewProbeWidget`으로 registry media open 검증
- [x] 실패한 legacy carousel 실험 코드 제거
- [x] `UVdjmRecordMediaPreviewCarouselWidget`은 registry/slot/window 확인용 legacy 최소 기능으로 축소
- [x] PreviewManager 생성/등록 이벤트와 idempotent 명시 init 이벤트 추가
- [ ] `VdjmWidgets` runtime module skeleton 추가
- [ ] `VdjmWidgets` carousel/card 책임 문서 추가
- [ ] 새 carousel card state machine 설계/구현
- [ ] 새 carousel source refresh/card pool/layout/input/swipe controller 설계/구현
- [ ] Preview gallery BP 구성
- [ ] Preview 상태를 `AppState`에 저장/복원
- [ ] Registry item delete/hide/upload state 관리
- [ ] Preview clip 또는 원본 구간 반복 정책 정리

## 3. Metadata/Service 현재 P1
- [x] Manifest 객체/JSON 생성 1차 추가
- [x] 내부 registry refresh 1차 추가
- [x] Metadata 스키마 v1 문서화
- [ ] 권한 키(`canView`, `canEdit`, `ownerUserId`, developer/master token`) 구조 정리
- [ ] Registry 목록 화면 설계 및 BP helper 정리
- [ ] 서버 endpoint/app state 설정 UI 또는 event 경로 정리
- [ ] Upload manifest contract 정리
- [ ] 업로드 완료 후 local retention/delete 정책 정리

## 4. 다음 큰 단계
- [ ] Serverless 업로드 계약 정리
- [ ] 서비스 페이지 연동 설계 정리
- [ ] E2E 업로드/처리/노출 플로우 정리
- [ ] Edit 진입 시 manifest 기반 도구/옵션 복원
- [ ] Recording overwrite/new-copy 정책 정리

## 5. 검증
- [ ] UI 없이도 EventManager/Controller/Observer 초기화 가능
- [ ] UI에서 호출 시 월드 자동 연결 확인
- [ ] 상태 전이 로그 확인
- [ ] flow state / runtime edge metadata 확인
- [ ] intro widget animation 완료 후 signal emit -> flow 진행 확인
- [ ] pause/resume flow 제어 확인
- [ ] 옵션 적용/거절 로그 확인
- [ ] queue된 옵션의 safe apply 시점 확인
- [ ] Android Vulkan 경로 재검증
- [ ] Android OpenGL 경로 재검증
- [ ] Windows WMF 경로 재검증
- [ ] Preview gallery screen에서 최신 registry 목록 표시
- [ ] Preview media 반복 재생 확인
- [ ] AppState 저장 후 앱 재시작 시 마지막 preview 상태 복원

## 당장 다음 작업
- [ ] `VdjmWidgets` 모듈을 추가한다.
- [ ] 새 carousel/card class 이름과 책임 경계를 문서화한다.
- [ ] `VdjmWidgetMediaCardWidget`의 `Active/Visible/Hidden/Empty/Waiting/Error` 상태 API를 만든다.
- [ ] `VdjmWidgetMediaCarouselWidget`의 refresh/card pool/layout/state/input/motion 소유 구조를 만든다.
- [ ] legacy `VdjmRecordMediaPreview*`는 새 gallery UX의 기준으로 더 이상 확장하지 않는다.
