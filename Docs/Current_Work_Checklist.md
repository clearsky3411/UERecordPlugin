# Current Work Checklist

## 사용 목적
- 이 문서는 현재 작업 순서를 한눈에 보려는 용도의 짧은 체크리스트다.
- 상세 배경과 구조는 `Android_Recording_Audio_GIF_Plan.md`, `VdjmRecorder_Current_Architecture.md`를 본다.

## 0. 기반 상태
- [x] `EventManager` 분리
- [x] `FlowRuntime` 도입
- [x] JSON 실행 경로 통합
- [x] `FlowFragment` 기반 코드 preset 조립 경로 추가
- [x] `RecorderController` 초안 존재
- [x] `RecorderStateObserver` 초안 존재
- [x] `EVdjmRecordEventResultType`를 `E*` 규칙으로 정리
- [x] 이벤트 관련 파일을 `VdjmEvents` 폴더로 정리
- [x] `EventManager` coarse session state 추가

## 1. 현재 P0
- [x] `StateMachine Observer`를 EventManager/세션 기준으로 재정의
- [x] `RecorderController` 옵션 메시지/큐 리뉴얼 1차
- [x] Resource Option Apply Layer 1차 구현
- [x] `FlowRuntime` mutation/fragment 1차 구현
- [ ] UI 엔트리 정리
- [ ] 비UI 객체가 UI를 몰라도 되도록 연결 구조 정리

## 2. 현재 P1
- [ ] Thumbnail/GIF 후처리 인터페이스 설계
- [ ] Thumbnail/GIF 결과 메타 구조 정의
- [ ] Metadata 스키마 v1 정리
- [ ] 권한 키(`canView`, `canEdit`, `ownerUserId`) 구조 정리

## 3. 최종 단계
- [ ] Serverless 업로드 계약 정리
- [ ] 서비스 페이지 연동 설계 정리
- [ ] E2E 업로드/처리/노출 플로우 정리

## 4. 검증
- [ ] UI 없이도 EventManager/Controller/Observer 초기화 가능
- [ ] UI에서 호출 시 월드 자동 연결 확인
- [ ] 상태 전이 로그 확인
- [ ] 옵션 적용/거절 로그 확인
- [ ] queue된 옵션의 safe apply 시점 확인
- [ ] Android Vulkan 경로 재검증
- [ ] Android OpenGL 경로 재검증
- [ ] Windows WMF 경로 재검증
